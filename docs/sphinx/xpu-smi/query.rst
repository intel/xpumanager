Query
=====

Print a report of the main attributes of each GPU, as an indented ``Key : Value`` listing:
identity, PCI link, clock throttle reasons, memory, utilization, ECC, retired pages,
temperatures, power and clocks, and the processes using the device. Use :doc:`discovery`
for static detail such as compute resources and firmware components, and ``--query-gpu``
or :doc:`dump` for any single metric. The report can be restricted to chosen sections, written as JSON,
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
PCI part of the report, and memory utilization appears under both ``MEMORY`` and
``UTILIZATION``. A section or heading with nothing to show is left out. Without
``--display`` every section is printed except ``SUPPORTED_CLOCKS``, which must be named
(or ``ALL`` given) because it lists every clock the GPU can be set to.

.. list-table::
   :widths: 22 78
   :header-rows: 1

   * - Section
     - Report content
   * - ``IDENTITY``
     - Product name, index, serial number, UUID, VBIOS (GFX firmware) version, board part
       number, device state and the PCI address
   * - ``MEMORY``
     - Memory total, used and free, and memory utilization
   * - ``UTILIZATION``
     - GPU, memory, compute, render, media and copy engine utilization
   * - ``TEMPERATURE``
     - GPU core and memory temperatures and their configured shutdown and slowdown
       temperatures
   * - ``POWER``
     - Power draw and the current, default, minimum and maximum power limit
   * - ``CLOCK``
     - Current graphics, memory and media clocks and the maximum graphics and media clocks
   * - ``PCI``
     - PCI address fields and IDs, link generation and width (maximum and current), replays
       and throughput
   * - ``ECC``
     - ECC mode now and after the next reset, and aggregate error counts
   * - ``EU_ARRAY``
     - Nothing of its own in this report; the GPU utilization row uses the EU array. Use
       ``--query-gpu eu.active,eu.stall,eu.idle`` for the EU array figures
   * - ``FAN``
     - Fan speed
   * - ``COMPUTE``
     - Scheduler mode
   * - ``PERFORMANCE``
     - Whether each clock throttle reason is active
   * - ``PAGE_RETIREMENT``
     - Number of memory pages the driver has taken offline
   * - ``SUPPORTED_CLOCKS``
     - The clocks the GPU can be set to, summarised as ``low-high step N`` when evenly
       spaced. Printed only when named
   * - ``PIDS``
     - Processes using the device
   * - ``ALL``
     - Everything above, including ``SUPPORTED_CLOCKS``

Sample Output
-------------

From an Intel Arc B580 run by a non-root user; ``...`` marks rows left out here.

.. code-block:: text

   ==============XPU-SMI LOG==============

   Timestamp                                 : 2026-10-22T17:28:53Z
   UMD Version                               : 1.17.39980
   KMD Version                               : N/A
   Level Zero Version                        : 1.33.1

   Attached GPUs                             : 1
   GPU 0000:03:00.0
       Product Name                          : Intel(R) Arc(TM) B580 Graphics
       Serial Number                         : unknown
       GPU UUID                              : 00000000-0000-0003-0000-0000e20b8086
       Device Index                          : 0
       VBIOS Version                         : N/A
       Device State                          : normal
       PCI
           Bus                               : 0x03
           Device                            : 0x00
           Domain                            : 0x0000
           Device Id                         : 0xE20B
           Bus Id                            : 0000:03:00.0
           Sub System Id                     : 0x1100
           GPU Link Info
               PCIe Generation
                   Max                       : N/A
                   Current                   : N/A
               Link Width
                   Max                       : N/A
                   Current                   : N/A
           Replays Since Reset               : N/A
           Tx Throughput                     : 84 kB/s
           Rx Throughput                     : 1582 kB/s
       Fan Speed                             : N/A
       Clocks Event Reasons
           Thermal Limit                     : Not Active
           Average Power Cap                 : Not Active
           ...
       FB Memory Usage
           Total                             : 12216 MiB
           Used                              : 218.62 MiB
           Free                              : 11997.38 MiB
       Scheduler Mode                        : timeslice
       Utilization
           GPU                               : 0.00 %
           Memory                            : 1.79 %
           Compute                           : 0.00 %
           Render                            : 0.00 %
           Media                             : 0.00 %
           Copy                              : 0.00 %
       ECC Mode
           Current                           : Disabled
           Pending                           : N/A
       ECC Errors
           Aggregate
               Correctable                   : N/A
               Uncorrectable                 : N/A
       Retired Pages
           Offline Pages                     : N/A
       Temperature
           GPU Current Temp                  : 33.00 C
           GPU Shutdown Temp                 : 130 C
           GPU Slowdown Temp                 : 105 C
           Memory Current Temp               : 52.00 C
           Memory Shutdown Temp              : 100 C
           Memory Slowdown Temp              : 85 C
       GPU Power Readings
           Average Power Draw                : 58.88 W
           Current Power Limit               : 380.00 W
           Default Power Limit               : N/A
           Min Power Limit                   : N/A
           Max Power Limit                   : N/A
       Clocks
           Graphics                          : 400 MHz
           Memory                            : 9500 MHz
           Media                             : 1200 MHz
       Max Clocks
           Graphics                          : 2850 MHz
           Media                             : 1500 MHz
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
- GPU utilization comes from the EU array when the hardware metrics stream is available,
  which adds about 100 ms after the measurement window, and otherwise from the busiest
  engine.
- As root, the VBIOS version is read from the GSC firmware interface, which takes about
  200 ms per device on some platforms. Devices are read in parallel, and the read overlaps
  the measurement window. On a device with an AMC the board part number is read from the
  AMC.
- The slowdown and shutdown temperatures are the configured thresholds the
  :doc:`health` command compares against, not sensor readings.
- Attributes that other vendors' GPUs have and Intel GPUs do not, such as persistence
  mode, partitioning and accounting, are not part of the report.
- On Windows the kernel-mode driver version reads ``N/A``.

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
               "fb_memory_usage": {
                   "total": "12216",
                   "used": "218.62109375",
                   "free": "11997.37890625"
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
