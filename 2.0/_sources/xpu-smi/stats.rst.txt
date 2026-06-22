Stats
=====

List GPU device statistics. Displays a snapshot of current performance metrics
including utilization, power, frequency, temperature, memory, and error counters.

Synopsis
--------

.. code-block:: text

   xpu-smi stats
   xpu-smi stats -d [deviceId]
   xpu-smi stats --device [deviceId]
   xpu-smi stats --device [pciBdfAddress]
   xpu-smi stats --device [deviceId] -j
   xpu-smi stats --device [deviceId] -e
   xpu-smi stats --device [deviceId] -e -j
   xpu-smi stats --device [deviceId] -r
   xpu-smi stats --device [deviceId] -r -j
   xpu-smi stats --device [deviceId] --samples [count] --interval [milliseconds]
   xpu-smi stats --device [deviceId] --list-offline-pages

Options
-------

.. option:: -h, --help

   Print this help message and exit.

.. option:: -j, --json

   Print result in JSON format.

.. option:: -d <deviceId>, --device <deviceId>, --id <deviceId>

   The device ID or PCI BDF address to query. If omitted, statistics for all
   detected devices are displayed.

.. option:: -e, --eu

   Show EU (Execution Unit) array metrics: Active, Stall, and Idle percentages.

.. option:: -r, --ras

   Show RAS (Reliability, Availability, Serviceability) error metrics, including
   correctable and uncorrectable error counters per category.

.. option:: --list-offline-pages

   List offline memory pages. This option is exclusive — no other stats are
   shown when it is used.

.. option:: --samples <count>

   Number of samples to collect before computing the displayed statistics.
   Default: ``2``.

.. option:: --interval <milliseconds>

   Sampling interval in milliseconds between samples. Default: ``100``.

Output Metrics
--------------

The following metrics are reported per device and per tile where applicable:

.. list-table::
   :widths: 40 60
   :header-rows: 1

   * - Metric
     - Notes
   * - GPU Utilization (%)
     - Per tile; device average for multi-tile
   * - Compute / Render / Media / Copy Engine Utilization (%)
     - Per tile
   * - EU Array Active / Stall / Idle (%)
     - Per tile; shown with ``-e``
   * - GPU Power (W)
     - Per tile and device
   * - GPU Frequency (MHz)
     - Per tile and device
   * - Media Frequency (MHz)
     - Per tile and device
   * - GPU Core Temperature (°C)
     - Per tile
   * - GPU Memory Temperature (°C)
     - Per tile
   * - GPU VR Temperature (°C)
     - Per tile (voltage regulator)
   * - Fan Speed (%)
     - Per fan
   * - GPU Memory Read / Write (kB/s)
     - Per tile
   * - GPU Memory Bandwidth (%)
     - Per tile
   * - GPU Memory Used (MiB)
     - Per tile
   * - PCIe Read / Write (kB/s)
     - Device level
   * - Fabric RX / TX (kB/s)
     - Device level; shown when fabric ports present
   * - RAS Error Counters
     - Per category (Reset, Programming, Driver, Cache, Mem); shown with ``-r``
   * - Energy Consumed (J)
     - Device level (total over measurement window)
   * - Offline Memory Pages
     - Count; shown with ``--list-offline-pages``

Examples
--------

Show stats for all GPUs:

.. code-block:: shell

   xpu-smi stats

Show stats for device 0:

.. code-block:: shell

   xpu-smi stats --device 0

Show stats including EU metrics in JSON format:

.. code-block:: shell

   xpu-smi stats --device 0 -e -j

Show RAS error counters for device 0:

.. code-block:: shell

   xpu-smi stats --device 0 -r

List offline memory pages:

.. code-block:: shell

   xpu-smi stats --device 0 --list-offline-pages

Collect 10 samples at 200ms intervals:

.. code-block:: shell

   xpu-smi stats --device 0 --samples 10 --interval 200
