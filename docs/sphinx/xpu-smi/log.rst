Log
===

Collect GPU diagnostic logs and write them to a file. Gathers a comprehensive
snapshot of system and GPU state useful for troubleshooting and bug reports.

.. note::

   This command is supported on Linux only.

Synopsis
--------

.. code-block:: text

   xpu-smi log -f [fileName]

Options
-------

.. option:: -h, --help

   Print this help message and exit.

.. option:: -f <fileName>, --file <fileName>

   The output file to write all collected diagnostic data into.

Collected Information
---------------------

The log command collects the following information:

**System snapshots** (from ``/proc``):

- CPU info, interrupt table, memory info, loaded modules, kernel version

**OS and kernel logs**:

- ``/var/log/syslog``, ``kern*.log`` files, kernel messages (dmesg)

**GPU driver info**:

- Kernel version, ``xe`` graphics driver module path and load status

**DRM and debugfs**:

- Files from ``/sys/class/drm`` and ``/sys/kernel/debug/dri`` (if available)

**Intel package inventory**:

- Installed Intel GPU stack packages (``dpkg`` on Debian/Ubuntu, ``rpm`` on RHEL/SUSE)

**System command outputs** (where tools are available):

- ``lspci -v -xxx``, ``dmidecode``, ``lsusb``
- ``xpu-smi discovery``
- ``clinfo`` (OpenCL platform info)
- ``vainfo`` (video acceleration info)

Examples
--------

Collect diagnostic logs to a file:

.. code-block:: shell

   xpu-smi log -f gpu_debug.log
