Introduction
============

Introduction for XPU Manager


What does XPUM provide?
~~~~~~~~~~~~~~~~~~~~~~~

XPUM can provide intel GPU management facilities

- libxpum.so: core library, a shared library, provides GPU management functions
- xpumd: a daemon, which is a wrapper of core lib and provides grpc service
- xpumcli: a command line tool, you can manage GPU locally with it, it communicates with xpumd by grpc
- restful API server: a http server provides restful API, you can manage GPU remotely through it, it communicates with xpumd by grpc

XPUM work mode
~~~~~~~~~~~~~~

- Integrate mode: you can integrate "libxpum.so" into your program, and manage GPU by calling functions it provides.
- Local mode: manage GPU by command line tool xpumcli
- Remote mode: restful server default not enabled.


Features
~~~~~~~~

- device management: basic device info
- group management: manage GPUs in group mode
- firmware flash
- diagnostics
- health
- statistics: telemetry and job profiling
