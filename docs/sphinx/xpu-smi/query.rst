Query
=====

Print a full report of every attribute ``xpu-smi`` knows about each GPU, as an indented
``Key : Value`` listing. The report can be restricted to chosen sections, written as JSON,
repeated at an interval, or saved to a file.

Synopsis
--------

.. code-block:: text

   xpu-smi query
   xpu-smi query --display [sections] --device [deviceIds]
   xpu-smi query --json
   xpu-smi query --loop [seconds] --count [count]
   xpu-smi query --file [filename]

Options
-------

.. option:: -h, --help

   Print this help message and exit.

.. option:: -j, --json

   Print the report as JSON instead of text. See `JSON Output`_.

.. option:: -d <deviceIds>, --device <deviceIds>, --id <deviceIds>

   Device IDs or PCI BDF addresses to report on. Multiple IDs can be separated by a comma.
   Use ``-1`` to select all devices. Without this option every device is reported.

.. option:: --display <sections>

   Comma-separated list of sections to print, from the table below. Names are
   case-insensitive. Sections always print in the order of the full report, whatever order
   they are listed in. Without this option everything is printed.

.. option:: --loop <seconds>

   Print the report again every ``<seconds>`` seconds until interrupted with Ctrl+C. On a
   terminal the whole report is drawn once and then only the lines whose values changed are
   rewritten, so unchanged data stays on screen. This needs every line of the report to fit
   in the window. If it does not, the report is printed once and a notice explains why, and
   nothing more is printed until the window is made large enough, at which point live
   updates start. Making the window smaller again prints the report once more. Use
   ``--display`` to select fewer sections if you want live updates in a smaller window.
   When output goes to a file or a pipe, each report is appended in full.

.. option:: --loop-ms <milliseconds>

   Like ``--loop``, with the interval in milliseconds. Takes precedence over ``--loop``.

.. option:: --count <count>

   Number of reports to print. Without ``--loop`` or ``--loop-ms`` a ``--count`` above 1
   repeats once a second. A count of 1 prints a single report.

.. option:: -f <path>, --file <path>

   Write the report to ``<path>`` instead of the terminal. An existing file is overwritten.

Sections
--------

``--display`` accepts the names below. A field that belongs to a section is printed
wherever it sits in the report, so ``--display PCI`` prints the PCI address fields from the
PCI part of the report, and a few fields appear under more than one section: memory
utilization under both ``MEMORY`` and ``UTILIZATION``, and the EU array under both
``UTILIZATION`` and ``EU_ARRAY``. A section or heading with nothing to show is left out.

.. list-table::
   :widths: 22 78
   :header-rows: 1

   * - Section
     - Report content
   * - ``IDENTITY``
     - Product name, index, type, stepping, state, health, serial number, UUID, vendor, DRM
       device, driver and firmware versions, compute resources (tiles, slices, EUs, engines)
       and the PCI address
   * - ``MEMORY``
     - Memory total, used and free, utilization, read and write bandwidth, type, vendor,
       channels, bus width, largest allocation and health
   * - ``UTILIZATION``
     - GPU, compute, render, media and copy engine utilization and the EU array
   * - ``TEMPERATURE``
     - GPU core, memory and composite temperatures, and the configured throttle and
       shutdown temperatures
   * - ``POWER``
     - Power draw, energy consumed, the current and maximum power limit, the limit at each
       level (sustained, burst, peak, default, minimum) and the thermal design power
   * - ``CLOCK``
     - Current, requested, efficient and TDP-limited clocks, the maximum and minimum
       clocks, the allowed frequency range and the core clock rate
   * - ``PCI``
     - PCI address fields, IDs, slot, link generation and width (maximum and current), link
       status, maximum bandwidth, replays and throughput
   * - ``ECC``
     - ECC mode now and after the next reset, the action that change needs, and error
       counters
   * - ``EU_ARRAY``
     - EU array active, stall and idle percentages
   * - ``FAN``
     - Fan speed and mode
   * - ``COMPUTE``
     - Scheduler mode
   * - ``PERFORMANCE``
     - Whether each clock throttle reason is active, and the standby mode
   * - ``PAGE_RETIREMENT``
     - Number of memory pages the driver has taken offline
   * - ``SUPPORTED_CLOCKS``
     - The clocks the GPU can be set to, summarised as ``low-high step N`` when evenly spaced
   * - ``PIDS``
     - Processes using the device
   * - ``ALL``
     - Everything above

Sample Output
-------------

The report below is shortened; ``...`` marks parts left out.

.. code-block:: text

   ==============XPU-SMI LOG==============

   Timestamp                                 : 2026-10-21T13:28:16Z
   UMD Version                               : 1.17.39980
   KMD Version                               : N/A
   Level Zero Version                        : 1.33.1

   Attached GPUs                             : 1
   GPU 0000:03:00.0
       Product Name                          : Intel(R) Arc(TM) B580 Graphics
       Device Index                          : 0
       Device Type                           : Discrete GPU
       SKU Type                              : N/A
       Stepping                              : A0
       Device State                          : normal
       Device Health                         : N/A
       Serial Number                         : unknown
       GPU UUID                              : 00000000-0000-0003-0000-0000e20b8086
       Vendor Name                           : Intel(R) Corporation
       DRM Device                            : /dev/dri/card0
       ...
       Memory Usage
           Total                             : 12216 MiB
           Used                              : 218.62 MiB
           Free                              : 11997.38 MiB
           Memory Utilization                : 1.79 %
           Read Bandwidth                    : 479141.91 kB/s
           Write Bandwidth                   : 887930.75 kB/s
           Bandwidth Utilization             : 2.34 %
           Memory Type                       : GDDR6
       ...
       Clocks Event Reasons
           Thermal Limit                     : Not Active
           Average Power Cap                 : Not Active
           Burst Power Cap                   : Not Active
           ...
       Power Readings
           Power Draw                        : 59.67 W
           GPU Power Draw                    : 0.34 W
           Current Power Limit               : 380.00 W
           Max Power Limit                   : N/A
           Energy Consumed                   : 133850.88 J
           GPU Energy Consumed               : 172.12 J
           Power Limits
               Sustained                     : N/A
               Burst                         : N/A
               Peak                          : 380.00 W
               Default                       : N/A
               Min                           : N/A
       Clocks
           Graphics                          : 400 MHz
           Media                             : 0 MHz
           Memory                            : 9500 MHz
           Memory Voltage                    : 1.35 V
           Core Clock Rate                   : 2850 MHz
           Requested Graphics                : 400 MHz
           Efficient Graphics                : 400 MHz
           TDP Graphics                      : 2850 MHz
       Supported Clocks
           Graphics                          : 400-2850 step 50 MHz
       Processes
           None

Reading the Report
------------------

- A value the device, driver or platform cannot supply prints as ``N/A``. This includes
  values that need more privilege than the caller has, such as engine utilization without
  ``CAP_PERFMON``, and the ECC state pending a reset.
- Measured values (power draw, utilization, throughput, bandwidth) are averaged over a
  200 ms window that the first report waits for. In a loop each later report averages over
  the time since the previous one.
- Values with a unit are shown to two decimals. JSON keeps the full precision.
- Selecting ``UTILIZATION``, ``EU_ARRAY`` or ``ALL`` also samples the EU array, which
  takes one extra measurement window. The EU array reads ``N/A`` when the hardware metrics
  stream is unavailable, for instance because another process holds it.
- The throttle and shutdown temperatures are the configured thresholds the
  :doc:`health` command compares against, not sensor readings.
- Attributes that other vendors' GPUs have and Intel GPUs do not, such as persistence
  mode, partitioning and accounting, are not part of the report.
- On Windows the kernel and kernel-mode driver versions, PCI slot and DRM device read
  ``N/A``.

A device in survivability mode is listed with only its state and the recovery action
(``Firmware update and GPU reset``).

JSON Output
-----------

``--json`` prints one object. Its keys are the lower-case, underscore-separated form of the
labels in the text report. A value that is not available is ``null``, numbers are strings
in the unit shown in the text report, and ``processes`` is an array.

.. code-block:: text

   {
       "timestamp": "2026-10-21T13:28:16Z",
       "umd_version": "1.17.39980",
       "kmd_version": null,
       "level_zero_version": "1.33.1",
       "attached_gpus": 1,
       "gpus": [
           {
               "pci_bus_id": "0000:03:00.0",
               "product_name": "Intel(R) Arc(TM) B580 Graphics",
               "memory_usage": {
                   "total": "12216",
                   "used": "218.62109375"
               },
               "processes": []
           }
       ]
   }

Examples
--------

Report on every device:

.. code-block:: text

   xpu-smi query

Report only memory and ECC for device 0:

.. code-block:: text

   xpu-smi query --device 0 --display MEMORY,ECC

Save a report to a file:

.. code-block:: text

   xpu-smi query --file report.txt

Print the power and temperature sections every 5 seconds, 12 times, as JSON:

.. code-block:: text

   xpu-smi query --display POWER,TEMPERATURE --json --loop 5 --count 12

See Also
--------

- :doc:`discovery` for static device properties
- :doc:`stats` for statistics collected over an interval
- :doc:`dump` for metrics in columns, one row per sample
