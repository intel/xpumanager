Health
======

Check GPU hardware component health. Reports the status of key GPU subsystems
(temperature, power, memory, frequency) and indicates whether they are operating
within normal parameters.

.. note::

   This command is supported on Linux only.

Synopsis
--------

.. code-block:: text

   xpu-smi health -l
   xpu-smi health -l -j
   xpu-smi health -d [deviceId]
   xpu-smi health --device [deviceId]
   xpu-smi health --device [pciBdfAddress]
   xpu-smi health --device [deviceId] -j
   xpu-smi health --device [pciBdfAddress] -j
   xpu-smi health --device [deviceId] -c [componentTypeId]
   xpu-smi health --device [pciBdfAddress] -c [componentTypeId] -j

Options
-------

.. option:: -h, --help

   Print this help message and exit.

.. option:: -j, --json

   Print result in JSON format.

.. option:: -l, --list

   Display health information for all detected GPU devices.

.. option:: -d <deviceId>, --device <deviceId>, --id <deviceId>

   The device ID or PCI BDF address to query.

.. option:: -c <componentTypeId>, --component <componentTypeId>

   Filter health status to a specific component type. Valid component IDs:

   .. list-table::
      :widths: 10 90
      :header-rows: 1

      * - ID
        - Component
      * - 1
        - GPU Core Temperature
      * - 2
        - GPU Memory Temperature
      * - 3
        - GPU Power
      * - 4
        - GPU Memory
      * - 6
        - GPU Frequency

Examples
--------

List health status for all devices:

.. code-block:: shell

   xpu-smi health -l

List health in JSON format:

.. code-block:: shell

   xpu-smi health -l -j

Show all component health for device 0:

.. code-block:: shell

   xpu-smi health --device 0

Show only GPU Core Temperature health for device 0:

.. code-block:: shell

   xpu-smi health --device 0 -c 1

Show memory health for device addressed by PCI BDF:

.. code-block:: shell

   xpu-smi health --device 0000:4d:00.0 -c 4
