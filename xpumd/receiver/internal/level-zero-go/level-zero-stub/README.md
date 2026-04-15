# level-zero-stub

A shared-library stub (`libze_stub.so`) that implements the Level Zero Sysman
API. The stub driver is intended for use in unit and integration tests.

## Requirements

- gcc
- libyaml
- libcyaml
- libpthread

## Control API

The stub driver exposes a control API (declared in `zes_stub.h`) for
loading a configuration and managing the background file watcher.

```c
// Load state from path, or from SYSMAN_STUB_CONFIG environment variable
// if path is NULL. If neither is set, the state is left empty.
// Returns 0 on success, -1 on error.
// Note: if SYSMAN_STUB_CONFIG is set at library load time, state is loaded and
// the inotify watcher started automatically.
int sysman_state_load(const char *path);

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
```

## State handling

The stub driver state is initialized from a YAML file that describes the
driver/device/component hierarchy and, optionally, per-function return-value
overrides used to inject errors. The stub sets an inotify watch on the
configuration file so that the state can be dynamically updated by replacing
the file with a new version.

> **NOTE:** The configuration file must be replaced atomically (e.g. write to a
> temporary file and then rename).

The `SYSMAN_STUB_CONFIG` environment variable is used in startup to specify the
path to the configuration file. If it is not set, the stub starts with an empty
state.

## Configuration file format

An example:

```yaml
Drivers:
  - Devices:
      - EngineGroups:
          - Properties:
              Type: 0         # ZES_ENGINE_GROUP_ALL
              ExtendedProperties:
                CountOfVirtualFunctionInstance: 4
            ReturnValues:    # component-level error injection
              zesEngineGetProperties: 0
        ReturnValues:        # device-level error injection
          zesDeviceGetProperties: 0

ReturnValues:                  # global error injection
  zesInit: 0
  zesDriverGet: 0
```

See [`example-config.yaml`](example-config.yaml) for a complete example with
all supported fields.
