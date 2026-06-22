Dump
====

Dump device statistics data continuously at a configurable interval. Suitable for
time-series data collection and high-frequency sampling.

Synopsis
--------

.. code-block:: text

   xpu-smi dump -d [deviceIds] --metrics [metricsSpec] --interval [seconds] --number [count]
   xpu-smi dump --device [deviceIds] --metrics [metricsSpec] --interval [seconds] --number [count]
   xpu-smi dump --device [deviceIds] --metrics [metricsSpec] --file [filename] --loop-ms [milliseconds] --time [seconds]

Options
-------

.. option:: -h, --help

   Print this help message and exit.

.. option:: -j, --json

   Print result in JSON format.

.. option:: -d <deviceIds>, --device <deviceIds>, --id <deviceIds>

   Device IDs or PCI BDF addresses to query. Use ``-1`` to select all devices.
   Multiple IDs can be separated by a comma.

.. option:: --metrics <metricsSpec>, --select <metricsSpec>

   Metrics to collect. Accepts any of the following forms, or a comma-separated mix:

   - **Legacy numeric IDs**: ``0``, ``1``, ``0,1,2`` (see Metrics Reference below)
   - **Group names**: ``POWER``, ``UTILIZATION``, ``TEMPERATURE``, ``MEMORY``, ``CLOCK``, ``PCI``, ``ECC``, ``EU_ARRAY``, ``FAN``, ``ALL``
   - **Single-char shortcuts**: ``p`` (Power + Temperature), ``u`` (Utilization), ``m`` (Memory), ``c`` (Clock), ``t`` (PCI), ``e`` (ECC), ``x`` (EU_ARRAY), ``f`` (Fan)
   - **Multi-char combos**: ``pu`` (Power + Temperature + Utilization), ``pum`` (Power + Temperature + Utilization + Memory)

   .. note::

      Dot-notation field names (e.g. ``temperature.gpu``) are not accepted by ``--metrics``.
      Use ``--query-gpu`` at the top level for field-name queries.

.. option:: --interval <seconds>, --delay <seconds>, --loop <seconds>

   Sampling interval in seconds between dumps. Default: ``1``. Maximum: ``20``.

.. option:: --number <count>, --count <count>

   Number of samples to dump. If omitted, dumps continuously until the user
   terminates the operation (Ctrl-C).

.. option:: --loop-ms <milliseconds>

   Millisecond-precision sampling interval. Overrides ``--interval`` when present.
   Use for sub-second sampling. Recommend pairing with ``--file``.

   .. note::

      A warning is emitted when ``--loop-ms`` is below 50 ms and power metrics are selected,
      as the power measurement window may be too short for accurate readings.

.. option:: --file <filename>, -f <filename>

   Write output to a file instead of stdout.

.. option:: --time <seconds>

   Total dump duration in seconds. Cannot be combined with ``--number``.

.. option:: --date

   Include date (``YYYY/MM/DD``) prefix in each timestamp column.

.. option:: --format <spec>

   Output format for CSV output. Comma-separated flags:

   - ``csv`` — CSV output (default when ``--file`` is used)
   - ``noheader`` — suppress the header row
   - ``nounits`` — omit unit suffixes (e.g. ``(W)``, ``(C)``) from headers

   Example: ``--format csv,noheader,nounits``

Metrics Reference
-----------------

.. list-table::
   :widths: 8 92
   :header-rows: 1

   * - ID
     - Metric
   * - 0
     - GPU Utilization (%), per tile or device. Device-level is the average value of tiles for multi-tile devices.
   * - 1
     - GPU Power (W), per tile or device
   * - 2
     - GPU Frequency (MHz), per tile or device
   * - 3
     - GPU Core Temperature (Celsius), per tile or device
   * - 4
     - GPU Memory Temperature (Celsius), per tile or device
   * - 5
     - GPU Memory Utilization (%), per tile or device
   * - 6
     - GPU Memory Read (kB/s), per tile or device
   * - 7
     - GPU Memory Write (kB/s), per tile or device
   * - 8
     - GPU Energy Consumed (J), per tile or device
   * - 9
     - GPU EU Array Active (%), per tile or device. At least one thread is active. Device-level is the average value of tiles for multi-tile devices.
   * - 10
     - GPU EU Array Stall (%), per tile or device. At least one thread is loaded but the EU is stalled. Device-level is the average value of tiles for multi-tile devices.
   * - 11
     - GPU EU Array Idle (%), per tile or device. Device-level is the average value of tiles for multi-tile devices.
   * - 12
     - Reset Counter
   * - 13
     - Programming Errors
   * - 14
     - Driver Errors
   * - 15
     - Cache Errors Correctable
   * - 16
     - Cache Errors Uncorrectable
   * - 17
     - GPU Memory Bandwidth Utilization (%), per tile or device. Device-level is the average value of tiles for multi-tile devices.
   * - 18
     - GPU Memory Used (MiB), per tile or device
   * - 19
     - PCIe Read (kB/s), per device
   * - 20
     - PCIe Write (kB/s), per device
   * - 21
     - Compute engine utilizations (%), per tile
   * - 22
     - Render engine utilizations (%), per tile
   * - 23
     - Media decoder engine utilizations (%), per tile
   * - 24
     - Media encoder engine utilizations (%), per tile
   * - 25
     - Copy engine utilizations (%), per tile
   * - 26
     - Media enhancement engine utilizations (%), per tile
   * - 27
     - 3D engine utilizations (%), per tile
   * - 28
     - GPU Memory Errors Correctable. Device-level is the sum value of tiles for multi-tile devices.
   * - 29
     - GPU Memory Errors Uncorrectable. Device-level is the sum value of tiles for multi-tile devices.
   * - 30
     - Compute Engine Group Utilization (%), per tile
   * - 31
     - Render Engine Group Utilization (%), per tile
   * - 32
     - Media Engine Group Utilization (%), per tile
   * - 33
     - Copy Engine Group Utilization (%), per tile
   * - 34
     - Throttle reason, per tile
   * - 35
     - Media Engine Frequency (MHz), per tile or device

Examples
--------

Dump GPU utilization and power for all devices every second, 10 times:

.. code-block:: shell

   xpu-smi dump --device -1 --metrics 0,1 --interval 1 --number 10

Dump using group name shortcuts:

.. code-block:: shell

   xpu-smi dump --device 0 --metrics pu --interval 1

Dump frequency and temperature for device 0 every 2 seconds:

.. code-block:: shell

   xpu-smi dump --device 0 --metrics CLOCK,TEMPERATURE --interval 2

Millisecond-precision dump to file:

.. code-block:: shell

   xpu-smi dump --device 0 --metrics POWER,UTILIZATION --file metrics.csv --loop-ms 100 --time 60

Dump with date in timestamp, CSV with no header or units:

.. code-block:: shell

   xpu-smi dump --device 0 --metrics 0,1 --date --format csv,noheader,nounits
