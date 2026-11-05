Health
======

Check GPU hardware component health. Reports the status of key GPU subsystems
(temperature, power, frequency) and indicates whether they are operating
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

   The device ID or PCI BDF address to query. Accepts a comma-separated list to
   query several devices at once (e.g., ``-d 0,1,4``).

   .. note::

      Separate per-device output is only produced when ``--list`` is used.
      Without ``--list``, results for a multi-device selection are not shown
      per device.

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
        - GPU Frequency

   Any other value is rejected as an invalid component ID, including:

   * other numbers, such as ``0``, ``5``, ``6``, ``999`` or ``-1``
   * letters, such as ``abc``
   * mixed letters and digits, such as ``4x`` or ``a4``
   * decimals, such as ``4.0``
   * an empty value

   .. note::

      GPU Frequency was component ID 6 in earlier releases; it is now ID 4.
      Use ``-c 4`` for GPU Frequency health. ``-c 6`` is now rejected as
      invalid.

      ID 4 was GPU Memory in earlier releases. GPU Memory health is no longer
      available: the driver reports memory health only for HBM memory, which
      the supported devices do not have.

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

Show GPU frequency health for device addressed by PCI BDF:

.. code-block:: shell

   xpu-smi health --device 0000:4d:00.0 -c 4
