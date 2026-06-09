PS
==

List status of processes currently using GPU resources.

.. note::

   This command is supported on Linux only.

Synopsis
--------

.. code-block:: text

   xpu-smi ps
   xpu-smi ps -d [deviceId]
   xpu-smi ps --device [deviceId]
   xpu-smi ps --device [deviceId] -j

Output Columns
--------------

.. list-table::
   :widths: 15 85
   :header-rows: 1

   * - Column
     - Description
   * - PID
     - Process ID
   * - Command
     - Process command name
   * - DeviceID
     - Device ID the process is using
   * - SHR
     - Size of shared device memory mapped into the process, in kB (may not be available on all platforms)
   * - MEM
     - Device memory allocated by this process, in kB (may not be available on all platforms)

Options
-------

.. option:: -h, --help

   Print this help message and exit.

.. option:: -j, --json

   Print result in JSON format.

.. option:: -d <deviceId>, --device <deviceId>, --id <deviceId>

   The device ID or PCI BDF address. If not specified, processes from all GPUs
   are listed.

Examples
--------

List all processes using any GPU:

.. code-block:: shell

   xpu-smi ps

List processes using device 0:

.. code-block:: shell

   xpu-smi ps --device 0

List processes using device 0 in JSON format:

.. code-block:: shell

   xpu-smi ps --device 0 -j
