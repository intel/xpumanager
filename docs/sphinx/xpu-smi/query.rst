Query
=====

Print a report of the main attributes of each GPU, as an indented ``Key : Value`` listing:
identity, PCI link, clock throttle reasons, memory, utilization, ECC, temperatures, power and
clocks, and the processes using the device. Use :doc:`discovery`
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
   rewritten, so unchanged data stays on screen. A line wider than the window wraps onto the
   rows below it. This needs the whole report, wrapped lines included, to fit in the window.
   If it does not, the report is printed once and a notice explains why, and nothing more is
   printed until the window is made large enough, at which point live updates start. Making the window smaller again prints the report once more. Use
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
(or ``ALL`` given) because it lists every clock the frequency range can be set to.

.. list-table::
   :widths: 22 78
   :header-rows: 1

   * - Section
     - Report content
   * - ``IDENTITY``
     - Product name and brand, stepping, index, DRM minor number, serial number, UUID, GFX
       firmware version, board part number, device state, whether the device is a physical or
       an SR-IOV virtual function, the recovery action it needs, and the PCI address
   * - ``MEMORY``
     - Memory total, used and free, memory type, number of channels and bus width, and memory
       utilization
   * - ``UTILIZATION``
     - GPU, memory, compute, render, media and copy engine utilization
   * - ``TEMPERATURE``
     - GPU core and memory temperatures and their configured shutdown and slowdown
       temperatures
   * - ``POWER``
     - Power draw and the current power limit
   * - ``CLOCK``
     - Current graphics, memory and media clocks, the graphics clock the driver requested
       (Applications Clocks), and the maximum graphics and media clocks
   * - ``PCI``
     - PCI address fields, class code and IDs, the physical slot, link generation and width
       (maximum and current), and throughput
   * - ``ECC``
     - ECC mode and aggregate error counts, in total and by RAS category: memory, PCIe, SoC
       internal, compute, fabric and scale errors (xe), or cache and non-compute errors, resets,
       programming errors and driver errors (i915)
   * - ``EU_ARRAY``
     - Nothing in this report. Use ``--query-gpu eu.active,eu.stall,eu.idle`` for the EU array
       figures
   * - ``FAN``
     - Nothing in this report. Use ``--query-gpu fan.speed`` for the fan speed
   * - ``COMPUTE``
     - Scheduler mode, and the number of tiles, slices, sub-slices per slice and EUs
   * - ``PERFORMANCE``
     - Whether each clock throttle reason is active
   * - ``SUPPORTED_CLOCKS``
     - The GPU core clocks the minimum and maximum of ``config --frequencyrange`` can be set
       to, summarised as ``low-high step N`` when evenly spaced. The memory clock cannot be
       set, so it has no list. Printed only when named
   * - ``PIDS``
     - Processes using the device
   * - ``ALL``
     - Everything above, including ``SUPPORTED_CLOCKS``

Sample Output
-------------

From an Intel Arc B580 run by a non-root user.

.. code-block:: text

   ==============XPU-SMI LOG==============

   Timestamp                                 : 2026-10-23T14:50:00Z
   UMD Version                               : 1.17.39980
   KMD Version                               : N/A
   Level Zero Version                        : 1.33.1

   Attached GPUs                             : 1
   GPU 0000:03:00.0
       Product Name                          : Intel(R) Arc(TM) B580 Graphics
       Product Brand                         : Intel(R) Corporation
       Stepping                              : A0
       Serial Number                         : unknown
       GPU UUID                              : 00000000-0000-0003-0000-0000e20b8086
       Device Index                          : 0
       Minor Number                          : 0
       GFX Firmware Version                  : N/A
       Platform Info
           Slot Number                       : N/A
       Device State                          : normal
       GPU Virtualization Mode
           Virtualization Mode               : physical
       GPU Recovery Action                   : None
       PCI
           Bus                               : 0x03
           Device                            : 0x00
           Domain                            : 0x0000
           Base Classcode                    : 0x03
           Sub Classcode                     : 0x00
           Device Id                         : 0xE20B
           Bus Id                            : 0000:03:00.0
           Sub System Id                     : 0x1100
           GPU Link Info
               PCIe Generation
                   Max                       : N/A
                   Current                   : N/A
                   Host Max                  : 5
               Link Width
                   Max                       : N/A
                   Current                   : N/A
                   Host Max                  : 16
           Tx Throughput                     : 102 kB/s
           Rx Throughput                     : 1623 kB/s
       Clocks Event Reasons
           Thermal Limit                     : Not Active
           Average Power Cap                 : Not Active
           Burst Power Cap                   : Not Active
           Current Limit                     : Not Active
           PSU Alert                         : Not Active
           SW Range                          : Not Active
           HW Range                          : Not Active
           Voltage                           : Not Active
           Thermal Alert                     : Not Active
           Power Alert                       : Not Active
       FB Memory Usage
           Total                             : 12216 MiB
           Reserved                          : 0 MiB
           Used                              : 218.62 MiB
           Free                              : 11997.38 MiB
       BAR Memory
           Total                             : 16384 MiB
       Memory Info
           Memory Type                       : GDDR6
           Number of Memory Channels         : 6
           Memory Bus Width                  : 192
       Scheduler Mode                        : timeslice
       Compute Resources
           Number of Tiles                   : 1
           Number of Slices                  : 5
           Number of Sub-slices per Slice    : 4
           Number of EUs                     : 160
       Utilization
           GPU                               : 0.00 %
           Memory                            : 1.79 %
           Compute                           : 0.00 %
           Render                            : 0.00 %
           Media                             : 0.00 %
           Copy                              : 0.00 %
       ECC Mode
           Current                           : Disabled
       ECC Errors
           Aggregate
               Correctable                   : N/A
               Uncorrectable                 : N/A
       Temperature
           GPU Current Temp                  : 30.00 C
           GPU Shutdown Temp                 : 130 C
           GPU Slowdown Temp                 : 105 C
           Memory Current Temp               : 48.00 C
           Memory Shutdown Temp              : 100 C
           Memory Slowdown Temp              : 85 C
       GPU Power Readings
           Average Power Draw                : 59.34 W
           Instantaneous Power Draw          : 68.62 W
           GPU Domain Power Draw             : 0.06 W
           Current Power Limit               : 380.00 W
           Peak Power Limit                  : 380.00 W
       Clocks
           Graphics                          : 400 MHz
           Memory                            : 9500 MHz
           Media                             : 1200 MHz
       Applications Clocks
           Graphics                          : 400 MHz
       Max Clocks
           Graphics                          : 2850 MHz
           Media                             : 1500 MHz
       Processes
           None


Reading the Report
------------------

- A value the device, driver or platform cannot supply prints as ``N/A``. This includes
  values that need more privilege than the caller has, such as engine utilization without
  ``CAP_PERFMON`` and the ECC error counts as a non-root user. The report prints no
  diagnostic for such a value; set ``XPU_SMI_LOG_LEVEL=DBG`` to see why one is missing.
- Measured values (power draw, utilization, throughput, bandwidth) are averaged over a
  200 ms window that the first report waits for. In a loop each later report averages over
  the time since the previous one.
- Values with a unit are shown to two decimals. JSON keeps the full precision.
- PCI: Host Max is the link capability of the root port the device is under, the CPU end of the
  path; a switch in between can limit the link further. PCIe Errors are the device's AER
  counters since it was enumerated, and Atomic Caps Inbound and Atomic Requests come from its
  PCI Express capability, which needs root. All of these are Linux only, and left out where the
  device or OS does not report them.
- GPU Power Readings: Average Power Draw is the card's draw over the window, Instantaneous Power
  Draw the driver's reading at one moment, and GPU Domain Power Draw the GPU domain's share.
  Current Power Limit is the sustained limit, or the burst or peak limit on a device without
  one; the burst and peak limits are listed on devices that report them.
- BAR Memory is the device's largest memory BAR, through which the host reaches device memory.
  Resizable BAR says whether it was resized; the driver reads that from PCI configuration space.
- FB Memory Usage: Reserved is memory the driver keeps back (installed size minus Total). Used
  counts it, so Total is Used plus Free. A memory clock the driver reports as 0 reads ``N/A``.
- GPU utilization is the busiest engine's share of the window. An engine counts as busy for all
  the time it has work scheduled, so it can read higher than ``eu.active`` under the same
  workload.
- Applications Clocks shows the graphics clock the driver last requested, which the GPU runs
  at unless a throttle reason holds it back. Minor Number is the number of the device's DRM
  card node, and reads ``N/A`` on Windows. Slot Number is the physical PCI slot the platform
  firmware names, and reads ``N/A`` on systems whose firmware names none.
- GPU Recovery Action says what the driver reports the device needs: ``None``, ``Reset`` (for
  example a wedged device), or ``Reset to repair``.
- As root, the GFX firmware version is read from the GSC firmware interface, which takes about
  200 ms per device on some platforms. Devices are read in parallel, and the read overlaps
  the measurement window. On a device with an AMC the serial number and the board part number
  are read from the AMC, one after the other.
- The slowdown and shutdown temperatures are the configured thresholds the
  :doc:`health` command compares against, not sensor readings.
- On Windows the kernel-mode driver version reads ``N/A``.
- Rows only some devices or drivers supply, such as the RAS categories under ECC Errors, are
  left out of the text report on a device without them, so it does not fill with ``N/A``. The
  JSON output keeps every key, as ``null``.

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
                   "reserved": "0",
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
