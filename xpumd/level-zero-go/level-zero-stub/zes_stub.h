/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef ZES_STUB_H
#define ZES_STUB_H

// Load state from path, or from SYSMAN_STUB_CONFIG environment variable
// if path is NULL. If neither is set, the state is left empty.
// Returns 0 on success, -1 on error.
// Note: if SYSMAN_STUB_CONFIG is set at library load time, state is loaded and
// the inotify watcher started automatically.
int sysman_state_load(const char *yaml_path);

// Free all dynamic memory, zero the state, re-init handles, and clear the
// config path. Thread-safe.
void sysman_state_reset(void);

// Return a copy of the effective config path used by the last successful
// sysman_state_load. The caller is responsible for free()ing the returned
// string. Returns NULL on allocation failure. Thread-safe.
char *sysman_get_config_path(void);

// Start a background inotify thread watching the currently-loaded config file.
// Automatically resets state and reloads from the watched file when it changes.
// Returns 0 on success, -1 if no config file path is known or it is already
// being watched.
int sysman_watch_start(void);

// Stop the background watcher thread. Blocks until the thread has exited.
void sysman_watch_stop(void);

#endif // ZES_STUB_H
