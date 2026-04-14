/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "sysman_state.h"
#include "sysman_state_cyaml.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <unistd.h>
#include <libgen.h>
#include <poll.h>
#include <time.h>
#include <sys/inotify.h>
#include <sys/eventfd.h>

sysman_state_t g_sysman_state;
static char g_config_path[PATH_MAX];

// Guards access to g_sysman_state and g_config_path.
static pthread_mutex_t g_state_lock = PTHREAD_MUTEX_INITIALIZER;

// ------------------------------------------------------------------
// Config file watcher
// ------------------------------------------------------------------

static pthread_t g_watch_thread;
// Guards the entire start/stop lifecycle; serialises concurrent callers.
static pthread_mutex_t g_watch_lifecycle_mu = PTHREAD_MUTEX_INITIALIZER;
static int g_watch_thread_started; // protected by g_watch_lifecycle_mu
static int g_stop_efd = -1;		   // eventfd; written by sysman_watch_stop()

// Condition variable used to signal that the watcher thread has registered
// the inotify watch and is ready to receive events.
static pthread_mutex_t g_watch_ready_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_watch_ready_cv = PTHREAD_COND_INITIALIZER;

typedef enum
{
	CONFIG_WATCH_PENDING = 0,
	CONFIG_WATCH_READY = 1,
	CONFIG_WATCH_ERROR = -1
} watch_ready_t;

static watch_ready_t g_watch_ready_val = CONFIG_WATCH_PENDING;

// Path snapshot taken at sysman_watch_start() time.
static char g_watch_path[PATH_MAX];

static void *watch_thread_fn(void *arg)
{
	(void)arg;

	// dirname/basename modify their argument in-place; strdup to get mutable copies.
	char *dirc = strdup(g_watch_path);
	char *fnamec = strdup(g_watch_path);
	if (!dirc || !fnamec) {
		fprintf(stderr, "stub watcher: OOM in strdup\n");
		goto error;
	}

	const char *dname = dirname(dirc);
	const char *fname = basename(fnamec);

	int ifd = inotify_init1(IN_NONBLOCK);
	if (ifd < 0) {
		perror("stub watcher: inotify_init1");
		goto error;
	}
	int wd = inotify_add_watch(ifd, dname, IN_CLOSE_WRITE | IN_MOVED_TO);
	if (wd < 0) {
		perror("stub watcher: inotify_add_watch");
		close(ifd);
		goto error;
	}

	// Signal that the watch is registered; sysman_watch_start() may now return.
	pthread_mutex_lock(&g_watch_ready_mu);
	g_watch_ready_val = CONFIG_WATCH_READY;
	pthread_cond_signal(&g_watch_ready_cv);
	pthread_mutex_unlock(&g_watch_ready_mu);

	{
		// Poll both the inotify fd and the stop eventfd with no timeout.
		// Writing to g_stop_efd in sysman_watch_stop() wakes us immediately.
		enum
		{
			FD_INOTIFY = 0,
			FD_STOP = 1
		};
		struct pollfd fds[2] = {
			[FD_INOTIFY] = {.fd = ifd, .events = POLLIN},
			[FD_STOP] = {.fd = g_stop_efd, .events = POLLIN},
		};
		char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));

		while (true) {
			int r = poll(fds, ARRAY_SIZE(fds), -1);
			if (r < 0) {
				if (errno == EINTR)
					continue;
				perror("stub watcher: poll");
				break;
			}

			if (fds[FD_STOP].revents & POLLIN)
				break; // stop signal

			if (!(fds[FD_INOTIFY].revents & POLLIN))
				continue;

			ssize_t len = read(ifd, buf, sizeof(buf));
			if (len <= 0)
				continue;

			// Scan events; fire on the watched filename.
			bool matched = false;
			for (char *p = buf; p < buf + len;) {
				struct inotify_event *ev = (struct inotify_event *)p;
				if (ev->len > 0 && strcmp(ev->name, fname) == 0)
					matched = true;
				p += sizeof(*ev) + ev->len;
			}
			if (!matched)
				continue;

			// Debounce: drain any follow-up events within 100 ms, then reload.
			struct timespec debounce = {.tv_sec = 0, .tv_nsec = 100 * 1000 * 1000};
			nanosleep(&debounce, NULL);
			while (poll(&fds[FD_INOTIFY], 1, 0) > 0 && (fds[FD_INOTIFY].revents & POLLIN)) {
				ssize_t r = read(ifd, buf, sizeof(buf));
				if (r < 0) {
					if (errno == EINTR)
						continue; // interrupted, re-poll and retry
					break;		  // non-retryable error
				} else if (r == 0) {
					break; // EOF
				}
			}

			// Check stop signal before reloading.
			if (poll(&fds[FD_STOP], 1, 0) > 0 && (fds[FD_STOP].revents & POLLIN))
				break;

			sysman_state_load(g_watch_path);
		}
	}

	inotify_rm_watch(ifd, wd);
	close(ifd);
	free(dirc);
	free(fnamec);
	return NULL;

error:
	free(dirc);
	free(fnamec);
	pthread_mutex_lock(&g_watch_ready_mu);
	g_watch_ready_val = CONFIG_WATCH_ERROR;
	pthread_cond_signal(&g_watch_ready_cv);
	pthread_mutex_unlock(&g_watch_ready_mu);
	return NULL;
}

// Must be called with g_watch_lifecycle_mu held.
static void sysman_watch_stop_locked(void)
{
	if (!g_watch_thread_started)
		return;
	// Wake the thread immediately by writing to the stop eventfd.
	// Even if the thread already exited on its own, we still must join
	// it and release the eventfd.
	eventfd_write(g_stop_efd, 1);
	g_watch_thread_started = 0;
	pthread_join(g_watch_thread, NULL);
	close(g_stop_efd);
	g_stop_efd = -1;
}

// Must be called with both g_state_lock AND g_watch_lifecycle_mu held.
static int sysman_watch_start_locked(void)
{
	if (g_config_path[0] == '\0') {
		fprintf(stderr, "stub watcher: no config path known; call sysman_state_load first\n");
		return -1;
	}
	snprintf(g_watch_path, sizeof(g_watch_path), "%s", g_config_path);

	if (g_watch_thread_started) {
		fprintf(stderr, "stub watcher: already watching\n");
		return -1;
	}

	g_stop_efd = eventfd(0, EFD_NONBLOCK);
	if (g_stop_efd < 0) {
		perror("stub watcher: eventfd");
		return -1;
	}

	pthread_mutex_lock(&g_watch_ready_mu);
	g_watch_ready_val = CONFIG_WATCH_PENDING;
	pthread_mutex_unlock(&g_watch_ready_mu);

	int rc = pthread_create(&g_watch_thread, NULL, watch_thread_fn, NULL);
	if (rc != 0) {
		fprintf(stderr, "stub watcher: pthread_create: %s\n", strerror(rc));
		close(g_stop_efd);
		g_stop_efd = -1;
		return -1;
	}
	g_watch_thread_started = 1;

	// Wait until the thread has registered the inotify watch (or failed).
	pthread_mutex_lock(&g_watch_ready_mu);
	while (g_watch_ready_val == CONFIG_WATCH_PENDING)
		pthread_cond_wait(&g_watch_ready_cv, &g_watch_ready_mu);
	watch_ready_t ready = g_watch_ready_val;
	pthread_mutex_unlock(&g_watch_ready_mu);

	if (ready == CONFIG_WATCH_ERROR) {
		// inotify watch failed; clean up the thread and eventfd.
		sysman_watch_stop_locked();
		return -1;
	}
	return 0;
}

// ------------------------------------------------------------------
// Memory allocation helpers
// ------------------------------------------------------------------

static void free_engine_groups(sysman_device_state_t *dev)
{
	if (dev->engine_groups) {
		for (uint32_t j = 0; j < dev->engine_groups_count; j++) {
			free(dev->engine_groups[j].activity_ext);
			memset(&dev->engine_groups[j], 0, sizeof(dev->engine_groups[j]));
		}
		free(dev->engine_groups);
	}
}

static void free_fabric_ports(sysman_device_state_t *dev)
{
	if (dev->fabric_ports) {
		for (uint32_t j = 0; j < dev->fabric_ports_count; j++) {
			free(dev->fabric_ports[j].properties);
			free(dev->fabric_ports[j].link_type);
			free(dev->fabric_ports[j].config);
			free(dev->fabric_ports[j].state);
			free(dev->fabric_ports[j].throughput);
			free(dev->fabric_ports[j].error_counters);
			memset(&dev->fabric_ports[j], 0, sizeof(dev->fabric_ports[j]));
		}
		free(dev->fabric_ports);
	}
}

static void free_fans(sysman_device_state_t *dev)
{
	if (dev->fans) {
		for (uint32_t j = 0; j < dev->fans_count; j++) {
			free(dev->fans[j].properties);
			free(dev->fans[j].config);
			memset(&dev->fans[j], 0, sizeof(dev->fans[j]));
		}
		free(dev->fans);
	}
}

static void free_firmwares(sysman_device_state_t *dev)
{
	if (dev->firmwares) {
		for (uint32_t j = 0; j < dev->firmwares_count; j++) {
			free(dev->firmwares[j].properties);
			memset(&dev->firmwares[j], 0, sizeof(dev->firmwares[j]));
		}
		free(dev->firmwares);
	}
}

static void free_frequency_domains(sysman_device_state_t *dev)
{
	if (dev->frequency_domains) {
		for (uint32_t j = 0; j < dev->frequency_domains_count; j++) {
			free(dev->frequency_domains[j].properties);
			free(dev->frequency_domains[j].range);
			free(dev->frequency_domains[j].available_clocks);
			free(dev->frequency_domains[j].state);
			free(dev->frequency_domains[j].throttle_time);
			memset(&dev->frequency_domains[j], 0, sizeof(dev->frequency_domains[j]));
		}
		free(dev->frequency_domains);
	}
}

static void free_leds(sysman_device_state_t *dev)
{
	if (dev->leds) {
		for (uint32_t j = 0; j < dev->leds_count; j++) {
			free(dev->leds[j].properties);
			free(dev->leds[j].state);
			memset(&dev->leds[j], 0, sizeof(dev->leds[j]));
		}
		free(dev->leds);
	}
}

static void free_memory_modules(sysman_device_state_t *dev)
{
	if (dev->memory_modules) {
		for (uint32_t j = 0; j < dev->memory_modules_count; j++) {
			free(dev->memory_modules[j].properties);
			free(dev->memory_modules[j].state);
			free(dev->memory_modules[j].bandwidth);
			memset(&dev->memory_modules[j], 0, sizeof(dev->memory_modules[j]));
		}
		free(dev->memory_modules);
	}
}

static void free_overclock(sysman_device_state_t *dev)
{
	if (!dev->overclock)
		return;
	if (dev->overclock->domains) {
		for (uint32_t j = 0; j < dev->overclock->domains_count; j++) {
			sysman_oc_t *e = &dev->overclock->domains[j];
			free(e->properties);
			free(e->vf_properties);
			if (e->control_infos) {
				for (uint32_t k = 0; k < e->control_infos_count; k++) {
					free(e->control_infos[k].properties);
					free(e->control_infos[k].current_value);
					free(e->control_infos[k].pending_value);
					free(e->control_infos[k].state);
					free(e->control_infos[k].pending_action);
					memset(&e->control_infos[k], 0, sizeof(e->control_infos[k]));
				}
				free(e->control_infos);
			}
			memset(e, 0, sizeof(*e));
		}
		free(dev->overclock->domains);
	}
	free(dev->overclock->controls);
	free(dev->overclock->state);
	free(dev->overclock);
	dev->overclock = NULL;
}

static void free_ecc(sysman_device_state_t *dev)
{
	if (!dev->ecc)
		return;
	free(dev->ecc->state);
	free(dev->ecc);
	dev->ecc = NULL;
}

static void free_performance_domains(sysman_device_state_t *dev)
{
	if (dev->performance_domains) {
		for (uint32_t j = 0; j < dev->performance_domains_count; j++) {
			free(dev->performance_domains[j].properties);
			memset(&dev->performance_domains[j], 0, sizeof(dev->performance_domains[j]));
		}
		free(dev->performance_domains);
	}
}

static void free_power_domains(sysman_device_state_t *dev)
{
	if (dev->power_domains) {
		for (uint32_t j = 0; j < dev->power_domains_count; j++) {
			free(dev->power_domains[j].properties);
			free(dev->power_domains[j].energy_counter);
			free(dev->power_domains[j].limits);
			free(dev->power_domains[j].energy_threshold);
			memset(&dev->power_domains[j], 0, sizeof(dev->power_domains[j]));
		}
		free(dev->power_domains);
	}
}

static void free_psus(sysman_device_state_t *dev)
{
	if (dev->psus) {
		for (uint32_t j = 0; j < dev->psus_count; j++) {
			free(dev->psus[j].properties);
			free(dev->psus[j].state);
			memset(&dev->psus[j], 0, sizeof(dev->psus[j]));
		}
		free(dev->psus);
	}
}

static void free_ras_error_sets(sysman_device_state_t *dev)
{
	if (dev->ras_error_sets) {
		for (uint32_t j = 0; j < dev->ras_error_sets_count; j++) {
			free(dev->ras_error_sets[j].properties);
			free(dev->ras_error_sets[j].config);
			free(dev->ras_error_sets[j].state);
			free(dev->ras_error_sets[j].state_exp);
			memset(&dev->ras_error_sets[j], 0, sizeof(dev->ras_error_sets[j]));
		}
		free(dev->ras_error_sets);
	}
}

static void free_schedulers(sysman_device_state_t *dev)
{
	if (dev->schedulers) {
		for (uint32_t j = 0; j < dev->schedulers_count; j++) {
			free(dev->schedulers[j].properties);
			free(dev->schedulers[j].timeout_mode_properties);
			free(dev->schedulers[j].timeslice_mode_properties);
			memset(&dev->schedulers[j], 0, sizeof(dev->schedulers[j]));
		}
		free(dev->schedulers);
	}
}

static void free_standby_domains(sysman_device_state_t *dev)
{
	if (dev->standby_domains) {
		for (uint32_t j = 0; j < dev->standby_domains_count; j++) {
			free(dev->standby_domains[j].properties);
			memset(&dev->standby_domains[j], 0, sizeof(dev->standby_domains[j]));
		}
		free(dev->standby_domains);
	}
}

static void free_temperature_sensors(sysman_device_state_t *dev)
{
	if (dev->temperature_sensors) {
		for (uint32_t j = 0; j < dev->temperature_sensors_count; j++) {
			free(dev->temperature_sensors[j].properties);
			free(dev->temperature_sensors[j].config);
			memset(&dev->temperature_sensors[j], 0, sizeof(dev->temperature_sensors[j]));
		}
		free(dev->temperature_sensors);
	}
}

static void free_diagnostics(sysman_device_state_t *dev)
{
	if (dev->diagnostics) {
		for (uint32_t j = 0; j < dev->diagnostics_count; j++) {
			free(dev->diagnostics[j].properties);
			free(dev->diagnostics[j].tests);
			memset(&dev->diagnostics[j], 0, sizeof(dev->diagnostics[j]));
		}
		free(dev->diagnostics);
	}
}

static void free_device(sysman_device_state_t *dev)
{
	free(dev->properties);
	free(dev->state);
	free(dev->pci.properties);
	free(dev->pci.state);
	free(dev->pci.bars);
	free(dev->pci.stats);
	free(dev->processes);
	free(dev->unsupported_features);
	free_ecc(dev);
	free_engine_groups(dev);
	free_fabric_ports(dev);
	free_fans(dev);
	free_firmwares(dev);
	free_frequency_domains(dev);
	free_leds(dev);
	free_memory_modules(dev);
	free_overclock(dev);
	free_performance_domains(dev);
	free_power_domains(dev);
	free_psus(dev);
	free_ras_error_sets(dev);
	free_schedulers(dev);
	free_standby_domains(dev);
	free_temperature_sensors(dev);
	free_diagnostics(dev);
	memset(dev, 0, sizeof(*dev));
}

static void free_driver(sysman_drivers_state_t *drv)
{
	for (uint32_t i = 0; i < drv->devices_count; i++)
		free_device(&drv->devices[i]);
	free(drv->devices);
	free(drv->extension_properties);
	free(drv->unsupported_features);
	memset(drv, 0, sizeof(*drv));
}

static void free_system_state(sysman_system_state_t *sys)
{
	for (uint32_t i = 0; i < sys->drivers_count; i++)
		free_driver(&sys->drivers[i]);
	free(sys->drivers);
	memset(sys, 0, sizeof(*sys));
}

static const cyaml_config_t cyaml_cfg = {
	.log_fn = cyaml_log,
	.mem_fn = cyaml_mem,
	.log_level = CYAML_LOG_WARNING,
	.flags = CYAML_CFG_IGNORE_UNKNOWN_KEYS,
};

static bool parse_uuid(const char str[SYSMAN_UUID_STR_SIZE], uint8_t (*id)[16])
{
	if (!str || str[0] == '\0') {
		memset(id, 0, sizeof(*id));
		return true;
	}

	if (strlen(str) != SYSMAN_UUID_STR_SIZE - 1)
		return false;

	unsigned int b[16];
	int n = sscanf(str, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", &b[0], &b[1], &b[2],
				   &b[3], &b[4], &b[5], &b[6], &b[7], &b[8], &b[9], &b[10], &b[11], &b[12], &b[13], &b[14], &b[15]);
	if (n != 16)
		return false;

	for (int i = 0; i < 16; i++)
		(*id)[i] = (uint8_t)b[i];
	return true;
}

// Special post-parse handler for UUID fields. Resolves UUID strings parsed
// from YAML into the binary fields in the ze/zes structs.
// Must be called on the freshly parsed tree before it is donated to g_sysman_state.
static bool resolve_uuids(sysman_state_t *state)
{
	for (uint32_t d = 0; d < state->system.drivers_count; d++) {
		sysman_drivers_state_t *drv = &state->system.drivers[d];
		for (uint32_t i = 0; i < drv->devices_count; i++) {
			sysman_device_properties_info_t *p = drv->devices[i].properties;
			if (!p)
				continue;
			// Copy all Core fields parsed into .core.ze back to .base.core,
			// then overlay the parsed UUID string onto .base.core.uuid.id.
			p->base.core = p->core.ze;
			if (!parse_uuid(p->core.uuid.id, &p->base.core.uuid.id)) {
				fprintf(stderr, "stub: invalid Core.Uuid '%s' in device %u of driver %u\n", p->core.uuid.id, i, d);
				return false;
			}
			if (!parse_uuid(p->uuid.id, &p->extended_properties.uuid.id)) {
				fprintf(stderr, "stub: invalid Uuid '%s' in device %u of driver %u\n", p->uuid.id, i, d);
				return false;
			}
		}
	}
	return true;
}

// ------------------------------------------------------------------
// Internal state management functions
// ------------------------------------------------------------------

// Must be called with g_state_lock held.
static void sysman_state_reset_locked(void)
{
	free_system_state(&g_sysman_state.system);
	memset(&g_sysman_state, 0, sizeof(g_sysman_state));
}

// Must be called with g_state_lock held.
static int sysman_state_load_locked(const char *path)
{
	const char *resolved = path;
	if (!resolved)
		resolved = getenv("SYSMAN_STUB_CONFIG");
	if (!resolved) {
		sysman_state_reset_locked();
		g_config_path[0] = '\0';
		return 0;
	}

	sysman_state_t *parsed = NULL;
	cyaml_err_t err = cyaml_load_file(resolved, &cyaml_cfg, &sysman_state_schema, (cyaml_data_t **)&parsed, NULL);
	if (err != CYAML_OK) {
		fprintf(stderr, "stub: YAML parse error in '%s': %s\n", resolved, cyaml_strerror(err));
		return -1;
	}
	if (!parsed) {
		fprintf(stderr, "stub: YAML produced empty state in '%s'\n", resolved);
		return -1;
	}

	if (!resolve_uuids(parsed)) {
		free_system_state(&parsed->system);
		free(parsed);
		return -1;
	}

	// We have a successfully parsed new state; free the old state before
	// replacing it with the new one.
	sysman_state_reset_locked();

	// Donate the parsed tree: take ownership of all cyaml-allocated memory
	// by copying the top-level struct and NOT calling cyaml_free().
	// The free(parsed) below releases only the top-level shell; all inner
	// allocations are now owned by g_sysman_state and freed by free_device().
	g_sysman_state = *parsed;
	free(parsed);

	// Record the effective path for callers (e.g. the Go file watcher).
	snprintf(g_config_path, sizeof(g_config_path), "%s", resolved);

	return 0;
}

// ------------------------------------------------------------------
// Public API
// ------------------------------------------------------------------

void sysman_state_lock(void) { pthread_mutex_lock(&g_state_lock); }

void sysman_state_unlock(void) { pthread_mutex_unlock(&g_state_lock); }

int sysman_state_load(const char *path)
{
	sysman_state_lock();
	int rc = sysman_state_load_locked(path);
	sysman_state_unlock();
	return rc;
}

void sysman_state_reset(void)
{
	sysman_state_lock();
	sysman_state_reset_locked();
	g_config_path[0] = '\0';
	sysman_state_unlock();
}

char *sysman_get_config_path(void)
{
	pthread_mutex_lock(&g_state_lock);
	char *pathc = strdup(g_config_path);
	pthread_mutex_unlock(&g_state_lock);
	return pathc;
}

int sysman_watch_start(void)
{
	sysman_state_lock();
	pthread_mutex_lock(&g_watch_lifecycle_mu);
	int rc = sysman_watch_start_locked();
	pthread_mutex_unlock(&g_watch_lifecycle_mu);
	sysman_state_unlock();
	return rc;
}

void sysman_watch_stop(void)
{
	pthread_mutex_lock(&g_watch_lifecycle_mu);
	sysman_watch_stop_locked();
	pthread_mutex_unlock(&g_watch_lifecycle_mu);
}

// ------------------------------------------------------------------
// Library auto-init / auto-cleanup
// ------------------------------------------------------------------

// If SYSMAN_STUB_CONFIG is set when the shared library is loaded, automatically
// load the specified config file and start the inotify watcher so the stub
// is usable without any explicit sysman_state_load() call.
__attribute__((constructor)) static void sysman_auto_init(void)
{
	const char *path = getenv("SYSMAN_STUB_CONFIG");
	if (!path)
		return;
	if (sysman_state_load(path) == 0)
		sysman_watch_start();
}

// Stop the watcher (if running) when the library is unloaded.
__attribute__((destructor)) static void sysman_auto_cleanup(void) { sysman_watch_stop(); }
