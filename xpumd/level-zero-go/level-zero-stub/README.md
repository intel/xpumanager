# level-zero-stub

A shared-library stub (`libze_stub.so`) that implements the Level Zero Sysman
API, plus the parts of the Level Zero loader (`zel*`) API that the Sysman
bindings need. The stub driver is intended for use in unit and integration
tests.

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

// Return a copy of the config path from the most recent sysman_state_load call.
// NOTE: This may differ from the currently active config if that load failed.
// The caller is responsible for free()ing the returned string.
// Returns NULL on allocation failure.
// Thread-safe.
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

> **IMPORTANT:** The stub driver is stateless and idempotent. The state is
> owned by the configuration file alone. No Level-Zero API call may modify the
> state, and every call must return exactly and only what the loaded
> configuration says. That is, the same call with the same arguments always
> returns the same thing.
>
> In other words, the stub driver does not remember what was written (through
> setters), does not allocate or free anything that an API call created or
> deleted, and does not consume the data that it hands out. Setters validate
> their arguments and return. Values that an API returns are either configured
> in the YAML or derived from the configuration and the arguments of the call
> where applicable. Anything that a real driver tracks at run time is expressed
> as an error injected with `ReturnValues` or as configured state, not as stub
> state. Also, the stub driver does not cross-check the properties / configuration
> between functions: e.g. `X.Properties` (returned by `XGetProperties()`) does
> not affect how `X.GetState()` behaves (this is solely determined by `X.State`
> from the configuration file). A call that must report an unsupported feature
> is told so with `UnsupportedFeatures` or `ReturnValues`, or an empty value
> for nullable fields.
>
> This keeps the tests reproducible and independent of their execution order,
> and keeps the configuration file a complete description of what a test sees.
>
> **NOTE:** Functionality for plugging in custom state handlers (e.g. to enable
> round-trip testing of setters and getters, or to simulate a device that
> changes state over time) is planned for as a future enhancement. The
> idempotent behavior described above is also an essential enabler for this.

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
