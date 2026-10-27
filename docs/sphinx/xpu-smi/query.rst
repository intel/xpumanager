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

.. option:: --verbose

   Print every row, including those a device does not report, which are otherwise left out.
   A value the driver or OS says the device does not support reads ``Not Supported``.

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
     - The fan speed, on devices with fans
   * - ``COMPUTE``
     - Scheduler mode, and the number of tiles, slices, sub-slices per slice and EUs
   * - ``PERFORMANCE``
     - Whether each clock throttle reason is active, the time throttled, and the performance
       factors, where the driver reports them
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
       GPU IP Version                        : 0x05004000
       Serial Number                         : unknown
       GPU UUID                              : 00000000-0000-0003-0000-0000e20b8086
       Device Index                          : 0
       Minor Number                          : 0
       GFX Firmware Version                  : N/A (requires read-write access to /dev/mei1)
       Platform Info
           Slot Number                       : N/A
       Device State                          : normal
       GPU Virtualization Mode
           Virtualization Mode               : physical
           Enabled VFs                       : 0
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
                   Max                       : N/A (requires CAP_SYS_ADMIN)
                   Current                   : N/A (requires CAP_SYS_ADMIN)
                   Host Max                  : 5
               Link Width
                   Max                       : N/A (requires CAP_SYS_ADMIN)
                   Current                   : N/A (requires CAP_SYS_ADMIN)
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
           Video                             : 0.00 %
           Video Enhancement                 : 0.00 %
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
           Memory Power Draw                 : 8.38 W
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


Fields
------

Every row the report can print. **Row** is the label path in the text report, with the path to
the value in ``--json`` output (under each entry of ``gpus``) below it. **Details** gives the
``--display`` section that selects the row, its unit, and its source: the ``--query-gpu`` field
that reads the same value, or the :doc:`discovery` property it comes from. A row reads ``N/A``
when the device, driver or platform cannot supply its value; **Meaning** says when that is
expected.

Report header
~~~~~~~~~~~~~

Printed once, above the devices. JSON keys are at the top level.

.. list-table::
   :header-rows: 1
   :widths: 30 25 45

   * - Row
     - Details
     - Meaning
   * - Timestamp

       ``timestamp``
     - 
     - When sampling began, as ISO 8601 UTC.
   * - UMD Version

       ``umd_version``
     - Source: ``driver_version``
     - Level Zero user-mode GPU driver version.
   * - KMD Version

       ``kmd_version``
     - Source: ``kernel_driver_version``
     - Kernel-mode driver version that the driver module declares; ``N/A`` for a driver that declares none, such as the in-tree driver, and on Windows.
   * - Level Zero Version

       ``level_zero_version``
     - Source: Level Zero loader
     - Version of the Level Zero loader.
   * - Attached GPUs

       ``attached_gpus``
     - 
     - Number of devices in the report.
   * - GPU <address>

       ``pci_bus_id``
     - Source: ``pci.bus_id``
     - Starts each device; the value is its PCI address. In JSON each device is an entry of ``gpus``.

Identity
~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 30 25 45

   * - Row
     - Details
     - Meaning
   * - Product Name

       ``product_name``
     - Section: ``IDENTITY``

       Source: ``name``
     - Device name the driver reports. ``Intel(R) Graphics [0x<device ID>]`` when the driver has no product name for the device.
   * - Product Brand

       ``product_brand``
     - Section: ``IDENTITY``

       Source: discovery Vendor Name
     - Brand the driver reports, e.g. ``Intel(R) Corporation``.
   * - Stepping

       ``stepping``
     - Section: ``IDENTITY``

       Source: discovery Stepping
     - Silicon stepping, e.g. ``A0``.
   * - GPU IP Version

       ``gpu_ip_version``
     - Section: ``IDENTITY``

       Source: ``ip_version``
     - Graphics IP version the driver reports, in hex (``ze_device_ip_version_ext_t``). It identifies the architecture: newer devices have higher values.
   * - Serial Number

       ``serial_number``
     - Section: ``IDENTITY``

       Source: ``serial``
     - Board serial number: from the AMC on a card that has one, otherwise the OEM serial number held by the GSC (root only). ``unknown`` when neither holds one.
   * - GPU UUID

       ``gpu_uuid``
     - Section: ``IDENTITY``

       Source: ``uuid``
     - Device UUID.
   * - Device Index

       ``device_index``
     - Section: ``IDENTITY``

       Source: ``index``
     - Index ``--device`` and the other commands accept for the device.
   * - Minor Number

       ``minor_number``
     - Section: ``IDENTITY``

       Source: DRM device
     - Number of the device's DRM card node, e.g. ``1`` for ``/dev/dri/card1``. ``N/A`` on Windows.
   * - GFX Firmware Version

       ``gfx_firmware_version``
     - Section: ``IDENTITY``

       Source: ``vbios_version``
     - Version of the GFX firmware the device runs, read from the GSC. Needs read-write access to the device's ``/dev/mei*`` node; without it the row says which node.
   * - Board Part Number

       ``board_part_number``
     - Section: ``IDENTITY``

       Source: discovery Part Number
     - Part number of the board, read from the AMC. Present only on a device with an AMC.
   * - Platform Info > Slot Number

       ``platform_info.slot_number``
     - Section: ``PCI``

       Source: discovery PCI Slot
     - Physical slot the platform firmware names for the device, e.g. ``GPU Board Slot2``. ``N/A`` when the firmware names none, as on most desktop boards.
   * - Device State

       ``device_state``
     - Section: ``IDENTITY``

       Source: device
     - ``normal``, or ``survivability`` for a device whose firmware needs recovery.
   * - GPU Virtualization Mode > Virtualization Mode

       ``gpu_virtualization_mode.virtualization_mode``
     - Section: ``IDENTITY``

       Source: PCI function
     - ``physical`` for a physical function, ``virtual`` for an SR-IOV virtual function.
   * - GPU Virtualization Mode > Enabled VFs

       ``gpu_virtualization_mode.enabled_vfs``
     - Section: ``IDENTITY``

       Source: ``vf.enabled``
     - SR-IOV virtual functions enabled on the physical function. Left out where the device or driver does not support SR-IOV.
   * - GPU Recovery Action

       ``gpu_recovery_action``
     - Section: ``IDENTITY``

       Source: ``recovery_action``
     - What the driver says the device needs to keep working: ``None``, ``Reset`` (for example a wedged device) or ``Reset to repair``. A device in survivability mode shows ``Firmware update and GPU reset``.
   * - Row
     - Details
     - Meaning
   * - PCI > Bus

       ``pci.bus``
     - Section: ``IDENTITY``, ``PCI``

       Source: ``pci.bus_id``
     - Bus number of the PCI address, in hex.
   * - PCI > Device

       ``pci.device``
     - Section: ``IDENTITY``, ``PCI``

       Source: ``pci.bus_id``
     - Device number of the PCI address, in hex.
   * - PCI > Domain

       ``pci.domain``
     - Section: ``IDENTITY``, ``PCI``

       Source: ``pci.bus_id``
     - Domain of the PCI address, in hex.
   * - PCI > Base Classcode

       ``pci.base_classcode``
     - Section: ``PCI``

       Source: ``pci.base_class``
     - PCI base class, in hex: ``0x03`` display controller, ``0x12`` processing accelerator.
   * - PCI > Sub Classcode

       ``pci.sub_classcode``
     - Section: ``PCI``

       Source: ``pci.sub_class``
     - PCI subclass, in hex.
   * - PCI > Device Id

       ``pci.device_id``
     - Section: ``IDENTITY``, ``PCI``

       Source: ``pci.device_id``
     - PCI device ID, in hex.
   * - PCI > Bus Id

       ``pci.bus_id``
     - Section: ``IDENTITY``, ``PCI``

       Source: ``pci.bus_id``
     - Full PCI address, ``DDDD:BB:DD.F``.
   * - PCI > Sub System Id

       ``pci.sub_system_id``
     - Section: ``IDENTITY``, ``PCI``

       Source: ``pci.sub_device_id``
     - PCI subsystem device ID, in hex. ``N/A`` on Windows.
   * - PCI > GPU Link Info > PCIe Generation > Max

       ``pci.gpu_link_info.pcie_generation.max``
     - Section: ``PCI``

       Source: ``pcie.link.gen.max``
     - Highest PCIe generation the link supports. The driver reads it from PCI configuration space, which needs ``CAP_SYS_ADMIN``; ``N/A`` when the driver does not know it.
   * - PCI > GPU Link Info > PCIe Generation > Current

       ``pci.gpu_link_info.pcie_generation.current``
     - Section: ``PCI``

       Source: ``pcie.link.gen.current``
     - PCIe generation the link runs at now. Needs ``CAP_SYS_ADMIN``, as Max does.
   * - PCI > GPU Link Info > PCIe Generation > Host Max

       ``pci.gpu_link_info.pcie_generation.host_max``
     - Section: ``PCI``

       Source: ``pcie.link.gen.host_max``
     - Maximum PCIe generation of the root port the device is under, the CPU end of the path (Linux sysfs). A switch in between can limit the link further.
   * - PCI > GPU Link Info > Link Width > Max

       ``pci.gpu_link_info.link_width.max``
     - Section: ``PCI``

       Source: ``pcie.link.width.max``
     - Widest the link can be, in lanes. Needs ``CAP_SYS_ADMIN``, as the generation does.
   * - PCI > GPU Link Info > Link Width > Current

       ``pci.gpu_link_info.link_width.current``
     - Section: ``PCI``

       Source: ``pcie.link.width.current``
     - Lanes the link uses now.
   * - PCI > GPU Link Info > Link Width > Host Max

       ``pci.gpu_link_info.link_width.host_max``
     - Section: ``PCI``

       Source: ``pcie.link.width.host_max``
     - Maximum PCIe link width of the root port the device is under (Linux sysfs).
   * - PCI > PCIe Errors > Correctable

       ``pci.pcie_errors.correctable``
     - Section: ``PCI``

       Source: ``pcie.errors.correctable``
     - PCIe correctable errors the device's function reported since it was enumerated (AER, Linux). Left out where the function has no AER counters, as the B580 GPU function, whose switch ports carry them.
   * - PCI > PCIe Errors > Non-Fatal

       ``pci.pcie_errors.non_fatal``
     - Section: ``PCI``

       Source: ``pcie.errors.non_fatal``
     - PCIe uncorrectable non-fatal errors since enumeration (AER, Linux).
   * - PCI > PCIe Errors > Fatal

       ``pci.pcie_errors.fatal``
     - Section: ``PCI``

       Source: ``pcie.errors.fatal``
     - PCIe uncorrectable fatal errors since enumeration (AER, Linux).
   * - PCI > PCIe Errors > Replay Number Rollovers

       ``pci.pcie_errors.replay_number_rollovers``
     - Section: ``PCI``

       Source: ``pcie.errors.replay_rollovers``
     - Times the link's REPLAY_NUM counter rolled over, a correctable link error (AER, Linux).
   * - PCI > PCIe Errors > Replay Timer Timeouts

       ``pci.pcie_errors.replay_timer_timeouts``
     - Section: ``PCI``

       Source: ``pcie.errors.replay_timeouts``
     - Times the link's replay timer expired, a correctable link error (AER, Linux).
   * - PCI > Tx Throughput

       ``pci.tx_throughput``
     - Section: ``PCI``

       Unit: kB/s

       Source: ``pcie.tx.throughput.kbs``
     - Data the device sent over PCIe, averaged over the sampling window. ``0`` on an idle link; ``N/A`` when the driver cannot read the PCIe counters.
   * - PCI > Rx Throughput

       ``pci.rx_throughput``
     - Section: ``PCI``

       Unit: kB/s

       Source: ``pcie.rx.throughput.kbs``
     - Data the device received over PCIe, as for Tx Throughput.
   * - PCI > Atomic Caps Inbound

       ``pci.atomic_caps_inbound``
     - Section: ``PCI``

       Source: ``pcie.atomic.completer``
     - PCIe AtomicOp requests the device completes, from Device Capabilities 2: e.g. ``FETCHADD_32 SWAP_32 CAS_32``, or ``None``. Reading it needs ``CAP_SYS_ADMIN`` (Linux).
   * - PCI > Replays Since Reset

       ``pci.replays_since_reset``
     - Section: ``PCI``

       Source: ``pcie.replay.counter``
     - PCIe replays since the last reset, from Level Zero. Printed where the driver supplies it; ``--verbose`` prints ``Not Supported`` where it says the device does not. CRI's driver has no replay counter; the PCIe Errors rows are the Linux alternative.
   * - PCI > Atomic Requests

       ``pci.atomic_requests``
     - Section: ``PCI``

       Source: ``pcie.atomic.requests``
     - Whether the device may send PCIe AtomicOp requests (Device Control 2): ``Enabled`` or ``Disabled``. Needs ``CAP_SYS_ADMIN`` (Linux).

Clocks Event Reasons
~~~~~~~~~~~~~~~~~~~~

Each row is ``Active`` when that reason holds the GPU clock back, otherwise ``Not Active``. ``clocks.throttle.reason`` gives the active ones as a single list.

.. list-table::
   :header-rows: 1
   :widths: 30 25 45

   * - Row
     - Details
     - Meaning
   * - Fan Speed

       ``fan_speed``
     - Section: ``FAN``

       Unit: %

       Source: ``fan.speed``
     - Fan speed, averaged across the device's fans. Printed where the driver supplies it; ``--verbose`` prints ``Not Supported`` where it says the device does not.
   * - Performance Factor > Compute

       ``performance_factor.compute``
     - Section: ``PERFORMANCE``

       Source: ``performance.factor.compute``
     - How the hardware trades frequency toward the compute engines, 0-100; 50 is balanced. Printed where the driver supplies it; ``--verbose`` prints ``Not Supported`` where it says the device does not. xe disables it.
   * - Performance Factor > Media

       ``performance_factor.media``
     - Section: ``PERFORMANCE``

       Source: ``performance.factor.media``
     - The same for the media engines. Printed where the driver supplies it; ``--verbose`` prints ``Not Supported`` where it says the device does not.

PCI
~~~

.. list-table::
   :header-rows: 1
   :widths: 30 25 45

   * - Clocks Event Reasons > Thermal Limit

       ``clocks_event_reasons.thermal_limit``
     - Section: ``PERFORMANCE``

       Source: ``clocks.event_reasons.thermal_limit``
     - The thermal limit.
   * - Clocks Event Reasons > Average Power Cap

       ``clocks_event_reasons.average_power_cap``
     - Section: ``PERFORMANCE``

       Source: ``clocks.event_reasons.average_power_cap``
     - The average (sustained) power limit.
   * - Clocks Event Reasons > Burst Power Cap

       ``clocks_event_reasons.burst_power_cap``
     - Section: ``PERFORMANCE``

       Source: ``clocks.event_reasons.burst_power_cap``
     - The burst power limit.
   * - Clocks Event Reasons > Current Limit

       ``clocks_event_reasons.current_limit``
     - Section: ``PERFORMANCE``

       Source: ``clocks.event_reasons.current_limit``
     - The electrical current limit.
   * - Clocks Event Reasons > PSU Alert

       ``clocks_event_reasons.psu_alert``
     - Section: ``PERFORMANCE``

       Source: ``clocks.event_reasons.psu_alert``
     - A power supply alert.
   * - Clocks Event Reasons > SW Range

       ``clocks_event_reasons.sw_range``
     - Section: ``PERFORMANCE``

       Source: ``clocks.event_reasons.sw_range``
     - The frequency range set in software, e.g. with ``config --frequencyrange``.
   * - Clocks Event Reasons > HW Range

       ``clocks_event_reasons.hw_range``
     - Section: ``PERFORMANCE``

       Source: ``clocks.event_reasons.hw_range``
     - The frequency range the hardware allows.
   * - Clocks Event Reasons > Voltage

       ``clocks_event_reasons.voltage``
     - Section: ``PERFORMANCE``

       Source: ``clocks.event_reasons.voltage``
     - The voltage limit.
   * - Clocks Event Reasons > Thermal Alert

       ``clocks_event_reasons.thermal_alert``
     - Section: ``PERFORMANCE``

       Source: ``clocks.event_reasons.thermal_alert``
     - A thermal alert.
   * - Clocks Event Reasons > Power Alert

       ``clocks_event_reasons.power_alert``
     - Section: ``PERFORMANCE``

       Source: ``clocks.event_reasons.power_alert``
     - A power alert.
   * - Clocks Event Reasons Counters > Throttled Time

       ``clocks_event_reasons_counters.throttled_time``
     - Section: ``PERFORMANCE``

       Unit: us

       Source: ``clocks.throttle_time``
     - Time the GPU clock has been held back by any throttle reason, accumulated by the driver. Printed where the driver supplies it; ``--verbose`` prints ``Not Supported`` where it says the device does not. The Linux runtime does not count it.

Memory and compute
~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 30 25 45

   * - Row
     - Details
     - Meaning
   * - FB Memory Usage > Total

       ``fb_memory_usage.total``
     - Section: ``MEMORY``

       Unit: MiB

       Source: ``memory.total``
     - Device memory the driver manages.
   * - FB Memory Usage > Reserved

       ``fb_memory_usage.reserved``
     - Section: ``MEMORY``

       Unit: MiB

       Source: ``memory.reserved``
     - Memory the driver keeps back: installed size minus Total. Used includes it, so Total is Used plus Free.
   * - FB Memory Usage > Used

       ``fb_memory_usage.used``
     - Section: ``MEMORY``

       Unit: MiB

       Source: ``memory.used``
     - Device memory in use.
   * - FB Memory Usage > Free

       ``fb_memory_usage.free``
     - Section: ``MEMORY``

       Unit: MiB

       Source: ``memory.free``
     - Device memory free.
   * - BAR Memory > Total

       ``bar_memory.total``
     - Section: ``PCI``

       Unit: MiB

       Source: ``pci.bar.memory.size``
     - Size of the device's largest memory BAR, the window through which the host reaches its local memory. Nothing reports how much of it is in use.
   * - BAR Memory > Resizable BAR

       ``bar_memory.resizable_bar``
     - Section: ``PCI``

       Source: ``pci.bar.resizable``
     - Whether Resizable BAR is on for that BAR: ``Enabled`` or ``Disabled``. The driver reads it from PCI configuration space, which needs ``CAP_SYS_ADMIN``.
   * - Memory Info > Memory Type

       ``memory_info.memory_type``
     - Section: ``MEMORY``

       Source: discovery Memory Type
     - Kind of device memory, e.g. ``GDDR6`` or ``LPDDR5X``.
   * - Memory Info > Number of Memory Channels

       ``memory_info.number_of_memory_channels``
     - Section: ``MEMORY``

       Source: discovery Number of Memory Channels
     - Memory channels. ``N/A`` when the driver does not know them.
   * - Memory Info > Memory Bus Width

       ``memory_info.memory_bus_width``
     - Section: ``MEMORY``

       Unit: bits

       Source: discovery Memory Bus Width
     - Width of the memory bus. ``N/A`` when the driver does not know it.
   * - Scheduler Mode

       ``scheduler_mode``
     - Section: ``COMPUTE``

       Source: ``scheduler.mode``
     - How the device schedules work: ``timeout``, ``timeslice`` or ``exclusive``, as set with ``config``.
   * - Compute Resources > Number of Tiles

       ``compute_resources.number_of_tiles``
     - Section: ``COMPUTE``

       Source: discovery Number of Tiles
     - Tiles (sub-devices) on the device.
   * - Compute Resources > Number of Slices

       ``compute_resources.number_of_slices``
     - Section: ``COMPUTE``

       Source: discovery Number of Slices
     - Slices in each tile.
   * - Compute Resources > Number of Sub-slices per Slice

       ``compute_resources.number_of_sub_slices_per_slice``
     - Section: ``COMPUTE``

       Source: discovery Number of Sub-slices per Slice
     - Sub-slices in each slice.
   * - Compute Resources > Number of EUs

       ``compute_resources.number_of_eus``
     - Section: ``COMPUTE``

       Source: discovery Number of EUs
     - Execution units on the device.

Utilization
~~~~~~~~~~~

Measured over the sampling window. Engine utilization needs ``CAP_PERFMON``; without it the values come from the processes' DRM file statistics where the kernel provides them.

.. list-table::
   :header-rows: 1
   :widths: 30 25 45

   * - Row
     - Details
     - Meaning
   * - Utilization > GPU

       ``utilization.gpu``
     - Section: ``UTILIZATION``

       Unit: %

       Source: ``utilization.gpu``
     - Share of the window the busiest engine was busy: from the engine counters, otherwise from the per-process engine times in fdinfo. Never from the EU array, whose metric stream is slow to open; ``--query-gpu eu.active`` reports that figure.
   * - Utilization > Memory

       ``utilization.memory``
     - Section: ``MEMORY``, ``UTILIZATION``

       Unit: %

       Source: ``utilization.memory``
     - Memory bandwidth used, as a share of the most the memory allows.
   * - Utilization > Compute

       ``utilization.compute``
     - Section: ``UTILIZATION``

       Unit: %

       Source: ``utilization.compute``
     - Busiest compute engine. ``N/A`` on a device without compute engines.
   * - Utilization > Render

       ``utilization.render``
     - Section: ``UTILIZATION``

       Unit: %

       Source: ``utilization.render``
     - Busiest render engine. ``N/A`` on a device without render engines, such as a compute-only accelerator.
   * - Utilization > Media

       ``utilization.media``
     - Section: ``UTILIZATION``

       Unit: %

       Source: ``utilization.media``
     - Busiest media engine.
   * - Utilization > Video

       ``utilization.video``
     - Section: ``UTILIZATION``

       Unit: %

       Source: ``utilization.video``
     - Busiest video decode/encode engine. Linux runs decode and encode on one engine class, so they share this figure. From the engine counters, otherwise fdinfo; never the EU array.
   * - Utilization > Video Enhancement

       ``utilization.video_enhancement``
     - Section: ``UTILIZATION``

       Unit: %

       Source: ``utilization.video_enhancement``
     - Busiest video enhancement engine (scaling, color conversion).
   * - Utilization > Copy

       ``utilization.copy``
     - Section: ``UTILIZATION``

       Unit: %

       Source: ``utilization.copy``
     - Busiest copy engine.

ECC
~~~

Error counts come from the driver's RAS counters and need root. A category the device does not report is left out of the text report (``null`` in JSON); ``--verbose`` prints it.

.. list-table::
   :header-rows: 1
   :widths: 30 25 45

   * - Row
     - Details
     - Meaning
   * - ECC Mode > Current

       ``ecc_mode.current``
     - Section: ``ECC``

       Source: ``ecc.mode.current``
     - ``Enabled`` or ``Disabled``.
   * - ECC Mode > Pending

       ``ecc_mode.pending``
     - Section: ``ECC``

       Source: ``ecc.mode.pending``
     - ECC state after the next reset: ``Enabled`` or ``Disabled``. Printed where the driver supplies it; ``--verbose`` prints ``Not Supported`` where it says the device does not. CRI's ECC is not configurable.
   * - ECC Errors > Aggregate > Correctable

       ``ecc_errors.aggregate.correctable``
     - Section: ``ECC``

       Source: ``ecc.errors.corrected.aggregate.total``
     - Correctable errors across every hardware RAS category.
   * - ECC Errors > Aggregate > Uncorrectable

       ``ecc_errors.aggregate.uncorrectable``
     - Section: ``ECC``

       Source: ``ecc.errors.uncorrected.aggregate.total``
     - Uncorrectable errors across every hardware RAS category.
   * - ECC Errors > Aggregate > Memory Correctable

       ``ecc_errors.aggregate.memory_correctable``
     - Section: ``ECC``

       Source: ``ras.memory.errors.correctable``
     - Correctable device memory errors. Left out where the device does not report the category; xe reports these netlink RAS categories, i915 the cache, non-compute, reset, programming and driver ones.
   * - ECC Errors > Aggregate > Memory Uncorrectable

       ``ecc_errors.aggregate.memory_uncorrectable``
     - Section: ``ECC``

       Source: ``ras.memory.errors.uncorrectable``
     - Uncorrectable device memory errors. Left out where the device does not report the category; xe reports these netlink RAS categories, i915 the cache, non-compute, reset, programming and driver ones.
   * - ECC Errors > Aggregate > PCIe Correctable

       ``ecc_errors.aggregate.pcie_correctable``
     - Section: ``ECC``

       Source: ``ras.pcie.errors.correctable``
     - Correctable PCIe subsystem errors. Left out where the device does not report the category; xe reports these netlink RAS categories, i915 the cache, non-compute, reset, programming and driver ones.
   * - ECC Errors > Aggregate > PCIe Uncorrectable

       ``ecc_errors.aggregate.pcie_uncorrectable``
     - Section: ``ECC``

       Source: ``ras.pcie.errors.uncorrectable``
     - Uncorrectable PCIe subsystem errors. Left out where the device does not report the category; xe reports these netlink RAS categories, i915 the cache, non-compute, reset, programming and driver ones.
   * - ECC Errors > Aggregate > SoC Internal Correctable

       ``ecc_errors.aggregate.soc_internal_correctable``
     - Section: ``ECC``

       Source: ``ras.soc_internal.errors.correctable``
     - Correctable SoC internal errors. Left out where the device does not report the category; xe reports these netlink RAS categories, i915 the cache, non-compute, reset, programming and driver ones.
   * - ECC Errors > Aggregate > SoC Internal Uncorrectable

       ``ecc_errors.aggregate.soc_internal_uncorrectable``
     - Section: ``ECC``

       Source: ``ras.soc_internal.errors.uncorrectable``
     - Uncorrectable SoC internal errors. Left out where the device does not report the category; xe reports these netlink RAS categories, i915 the cache, non-compute, reset, programming and driver ones.
   * - ECC Errors > Aggregate > Compute Correctable

       ``ecc_errors.aggregate.compute_correctable``
     - Section: ``ECC``

       Source: ``ras.compute.errors.correctable``
     - Correctable compute engine errors. Left out where the device does not report the category; xe reports these netlink RAS categories, i915 the cache, non-compute, reset, programming and driver ones.
   * - ECC Errors > Aggregate > Compute Uncorrectable

       ``ecc_errors.aggregate.compute_uncorrectable``
     - Section: ``ECC``

       Source: ``ras.compute.errors.uncorrectable``
     - Uncorrectable compute engine errors. Left out where the device does not report the category; xe reports these netlink RAS categories, i915 the cache, non-compute, reset, programming and driver ones.
   * - ECC Errors > Aggregate > Fabric Correctable

       ``ecc_errors.aggregate.fabric_correctable``
     - Section: ``ECC``

       Source: ``ras.fabric.errors.correctable``
     - Correctable fabric interconnect errors. Left out where the device does not report the category; xe reports these netlink RAS categories, i915 the cache, non-compute, reset, programming and driver ones.
   * - ECC Errors > Aggregate > Fabric Uncorrectable

       ``ecc_errors.aggregate.fabric_uncorrectable``
     - Section: ``ECC``

       Source: ``ras.fabric.errors.uncorrectable``
     - Uncorrectable fabric interconnect errors. Left out where the device does not report the category; xe reports these netlink RAS categories, i915 the cache, non-compute, reset, programming and driver ones.
   * - ECC Errors > Aggregate > Scale Correctable

       ``ecc_errors.aggregate.scale_correctable``
     - Section: ``ECC``

       Source: ``ras.scale.errors.correctable``
     - Correctable scale fabric errors. Left out where the device does not report the category; xe reports these netlink RAS categories, i915 the cache, non-compute, reset, programming and driver ones.
   * - ECC Errors > Aggregate > Scale Uncorrectable

       ``ecc_errors.aggregate.scale_uncorrectable``
     - Section: ``ECC``

       Source: ``ras.scale.errors.uncorrectable``
     - Uncorrectable scale fabric errors. Left out where the device does not report the category; xe reports these netlink RAS categories, i915 the cache, non-compute, reset, programming and driver ones.
   * - ECC Errors > Aggregate > Cache Correctable

       ``ecc_errors.aggregate.cache_correctable``
     - Section: ``ECC``

       Source: ``ras.cache.errors.correctable``
     - Correctable cache errors.
   * - ECC Errors > Aggregate > Cache Uncorrectable

       ``ecc_errors.aggregate.cache_uncorrectable``
     - Section: ``ECC``

       Source: ``ras.cache.errors.uncorrectable``
     - Uncorrectable cache errors.
   * - ECC Errors > Aggregate > Non-Compute Correctable

       ``ecc_errors.aggregate.non_compute_correctable``
     - Section: ``ECC``

       Source: ``ras.non_compute.errors.correctable``
     - Correctable errors outside the compute engines.
   * - ECC Errors > Aggregate > Non-Compute Uncorrectable

       ``ecc_errors.aggregate.non_compute_uncorrectable``
     - Section: ``ECC``

       Source: ``ras.non_compute.errors.uncorrectable``
     - Uncorrectable errors outside the compute engines.
   * - ECC Errors > Aggregate > Resets

       ``ecc_errors.aggregate.resets``
     - Section: ``ECC``

       Source: ``ras.reset``
     - Device resets.
   * - ECC Errors > Aggregate > Programming Errors

       ``ecc_errors.aggregate.programming_errors``
     - Section: ``ECC``

       Source: ``ras.programming.errors``
     - Errors caused by the work submitted, such as an invalid instruction.
   * - ECC Errors > Aggregate > Driver Errors

       ``ecc_errors.aggregate.driver_errors``
     - Section: ``ECC``

       Source: ``ras.driver.errors``
     - Errors the driver detected in itself.
   * - ECC Errors > Offline Pages

       ``ecc_errors.offline_pages``
     - Section: ``ECC``

       Source: ``memory.offline_pages``
     - Device memory pages the driver has retired after errors. Left out where the driver has no page-offline support.
   * - ECC Errors > Memory Repair

       ``ecc_errors.memory_repair``
     - Section: ``ECC``

       Source: ``memory.repair_status``
     - Whether the device has had a repair performed, such as remapping faulty memory: ``Performed`` or ``Not Performed``. Left out where the driver does not track repairs.

Temperature
~~~~~~~~~~~

The shutdown and slowdown rows are configured thresholds, the ones the ``health`` command compares against, not sensor readings.

.. list-table::
   :header-rows: 1
   :widths: 30 25 45

   * - Row
     - Details
     - Meaning
   * - Temperature > GPU Current Temp

       ``temperature.gpu_current_temp``
     - Section: ``TEMPERATURE``

       Unit: C

       Source: ``temperature.gpu``
     - GPU core temperature. ``N/A`` on a device that exposes no temperature sensor.
   * - Temperature > GPU Shutdown Temp

       ``temperature.gpu_shutdown_temp``
     - Section: ``TEMPERATURE``

       Unit: C

       Source: ``temperature.gpu.threshold.shutdown``
     - GPU core temperature at which the device shuts down: the threshold configured for the device, otherwise xpu-smi's default (130 C) lowered to the driver's maximum for the sensor (125 C on CRI).
   * - Temperature > GPU Slowdown Temp

       ``temperature.gpu_slowdown_temp``
     - Section: ``TEMPERATURE``

       Unit: C

       Source: ``temperature.gpu.threshold.throttle``
     - GPU core temperature at which the clock is throttled.
   * - Temperature > GPU Max Operating Temp

       ``temperature.gpu_max_operating_temp``
     - Section: ``TEMPERATURE``

       Unit: C

       Source: ``temperature.gpu.max``
     - Highest temperature the driver says the GPU sensor can operate at; 125 C on CRI. Left out where the driver reports none.
   * - Temperature > Memory Current Temp

       ``temperature.memory_current_temp``
     - Section: ``TEMPERATURE``

       Unit: C

       Source: ``temperature.memory``
     - Memory temperature. ``N/A`` on a device that exposes no memory sensor.
   * - Temperature > Memory Shutdown Temp

       ``temperature.memory_shutdown_temp``
     - Section: ``TEMPERATURE``

       Unit: C

       Source: ``temperature.memory.threshold.shutdown``
     - Memory temperature at which the device shuts down: the configured threshold, otherwise xpu-smi's default (100 C) lowered to the driver's maximum, if lower.
   * - Temperature > Memory Slowdown Temp

       ``temperature.memory_slowdown_temp``
     - Section: ``TEMPERATURE``

       Unit: C

       Source: ``temperature.memory.threshold.throttle``
     - Memory temperature at which the clock is throttled.
   * - Temperature > Memory Max Operating Temp

       ``temperature.memory_max_operating_temp``
     - Section: ``TEMPERATURE``

       Unit: C

       Source: ``temperature.memory.max``
     - The same for the memory sensor.

Power and clocks
~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 30 25 45

   * - Row
     - Details
     - Meaning
   * - GPU Power Readings > Average Power Draw

       ``gpu_power_readings.average_power_draw``
     - Section: ``POWER``

       Unit: W

       Source: ``power.draw``
     - Power the card drew, averaged over the sampling window.
   * - GPU Power Readings > Instantaneous Power Draw

       ``gpu_power_readings.instantaneous_power_draw``
     - Section: ``POWER``

       Unit: W

       Source: ``power.draw.instant``
     - Card power at one moment, as the driver reports it (``zesPowerGetUsage``). Needs a runtime and loader that support it; on CRI it comes from telemetry, which needs root.
   * - GPU Power Readings > GPU Domain Power Draw

       ``gpu_power_readings.gpu_domain_power_draw``
     - Section: ``POWER``

       Unit: W

       Source: ``power.draw.gpu``
     - The GPU power domain's share, averaged over the sampling window.
   * - GPU Power Readings > Current Power Limit

       ``gpu_power_readings.current_power_limit``
     - Section: ``POWER``

       Unit: W

       Source: ``power.limit``
     - Power limit in force: the sustained limit, or the burst or peak limit on a device without one.
   * - GPU Power Readings > Burst Power Limit

       ``gpu_power_readings.burst_power_limit``
     - Section: ``POWER``

       Unit: W

       Source: ``power.limit.burst``
     - Burst power limit (PL2), on devices that report one.
   * - GPU Power Readings > Peak Power Limit

       ``gpu_power_readings.peak_power_limit``
     - Section: ``POWER``

       Unit: W

       Source: ``power.limit.peak``
     - Peak power limit (PL4), on devices that report one. Current Power Limit shows it on a device with no sustained or burst limit.
   * - GPU Power Readings > Default Power Limit

       ``gpu_power_readings.default_power_limit``
     - Section: ``POWER``

       Unit: W

       Source: ``power.limit.default``
     - Default power limit from the device properties. Printed where the driver supplies it; ``--verbose`` prints ``Not Supported`` where it says the device does not. CRI's driver reports it as unknown, so ``--verbose`` shows ``N/A``.
   * - GPU Power Readings > Min Power Limit

       ``gpu_power_readings.min_power_limit``
     - Section: ``POWER``

       Unit: W

       Source: ``power.limit.min``
     - Lowest power limit that can be set. Printed where the driver supplies it; ``--verbose`` prints ``Not Supported`` where it says the device does not.
   * - GPU Power Readings > Max Power Limit

       ``gpu_power_readings.max_power_limit``
     - Section: ``POWER``

       Unit: W

       Source: ``power.max_limit``
     - Highest power limit that can be set. Printed where the driver supplies it; ``--verbose`` prints ``Not Supported`` where it says the device does not.
   * - GPU Power Readings > Memory Power Draw

       ``gpu_power_readings.memory_power_draw``
     - Section: ``POWER``

       Unit: W

       Source: ``power.draw.memory.instant``
     - Memory power domain at one moment, on devices with one (the B580 has one; CRI does not).
   * - Clocks > Graphics

       ``clocks.graphics``
     - Section: ``CLOCK``

       Unit: MHz

       Source: ``clocks.current.graphics``
     - Current GPU core clock.
   * - Clocks > Memory

       ``clocks.memory``
     - Section: ``CLOCK``

       Unit: MHz

       Source: ``clocks.current.memory``
     - Current memory clock. ``N/A`` when the driver does not know it, including a reading of 0 MHz, as on CRI.
   * - Clocks > Media

       ``clocks.media``
     - Section: ``CLOCK``

       Unit: MHz

       Source: ``clocks.current.media``
     - Current media engine clock; can read ``0`` while the media engine is idle.
   * - Applications Clocks > Graphics

       ``applications_clocks.graphics``
     - Section: ``CLOCK``

       Unit: MHz

       Source: ``clocks.requested.graphics``
     - GPU core clock the driver last requested, which the GPU runs at unless a clock event reason holds it back.
   * - Max Clocks > Graphics

       ``max_clocks.graphics``
     - Section: ``CLOCK``

       Unit: MHz

       Source: ``clocks.max.graphics``
     - Highest GPU core clock the device supports.
   * - Max Clocks > Media

       ``max_clocks.media``
     - Section: ``CLOCK``

       Unit: MHz

       Source: ``clocks.max.media``
     - Highest media engine clock the device supports.
   * - Supported Clocks > Graphics

       ``supported_clocks.graphics``
     - Section: ``SUPPORTED_CLOCKS``

       Unit: MHz

       Source: ``clocks.supported.graphics``
     - GPU core clocks the minimum and maximum of ``config --frequencyrange`` can be set to, as ``low-high step N`` when evenly spaced. Printed only when ``SUPPORTED_CLOCKS`` or ``ALL`` is selected.

Processes
~~~~~~~~~

One record per process using the device; ``None`` when there is none. In JSON, ``processes`` is an array of objects with the keys below.

.. list-table::
   :header-rows: 1
   :widths: 30 25 45

   * - Row
     - Details
     - Meaning
   * - Processes > Process ID

       ``processes[].process_id``
     - Section: ``PIDS``

       Source: ``ps``
     - Process ID.
   * - Processes > Type

       ``processes[].type``
     - Section: ``PIDS``

       Source: ``ps``
     - Engine the process uses: ``C`` compute, ``G`` render or 3D, ``M`` media, ``DMA`` copy, ``Other``; ``C`` for a process holding memory without naming an engine.
   * - Processes > Name

       ``processes[].name``
     - Section: ``PIDS``

       Source: ``ps``
     - Command name.
   * - Processes > Used GPU Memory

       ``processes[].used_gpu_memory``
     - Section: ``PIDS``

       Unit: MiB

       Source: ``ps``
     - Device memory the process holds.

Reading the Report
------------------

- A value the device, driver or platform cannot supply prints as ``N/A``. Where the value needs
  something the caller lacks, the text report says what, for example
  ``N/A (requires CAP_SYS_ADMIN)``: ``CAP_SYS_ADMIN`` for the PCIe link generation and width,
  the atomic capabilities and Resizable BAR, which come from PCI configuration space;
  ``CAP_PERFMON`` for engine utilization; read-write access to the device's ``/dev/mei*`` node
  for the GFX firmware version. The ECC error counts also read ``N/A`` as a non-root user. The
  report prints no diagnostic for such a value; set ``XPU_SMI_LOG_LEVEL=DBG`` to see why one is
  missing.
- Measured values (power draw, utilization, throughput, bandwidth) are averaged over a
  200 ms window that the first report waits for. In a loop each later report averages over
  the time since the previous one.
- Values with a unit are shown to two decimals. JSON keeps the full precision.
- GPU IP Version is the graphics IP version the driver reports, in hex. It identifies the
  architecture: newer devices have higher values.
- Offline Pages counts the device memory pages the driver has retired after errors, on drivers
  with page-offline support. Memory Repair says whether the device has had a repair performed,
  such as remapping faulty memory. It is left out where the driver does not track repairs.
- PCI: Host Max is the link capability of the root port the device is under, the CPU end of the
  path; a switch in between can limit the link further. PCIe Errors are the device's AER
  counters since it was enumerated, and Atomic Caps Inbound and Atomic Requests come from its
  PCI Express capability, which needs root. All of these are Linux only, and left out where the
  device or OS does not report them.
- Fan Speed, the Performance Factors, Throttled Time, PCIe Replays Since Reset, ECC Mode >
  Pending, the default, minimum and maximum power limits and Memory Power Draw come from Level
  Zero calls that many drivers do not implement; CRI's driver supplies none of them. They are
  printed where the driver supplies a value, and with ``--verbose`` otherwise.
- GPU Power Readings: Average Power Draw is the card's draw over the window, Instantaneous Power
  Draw the driver's reading at one moment, and GPU Domain Power Draw the GPU domain's share.
  Current Power Limit is the sustained limit, or the burst or peak limit on a device without
  one; the burst and peak limits are listed on devices that report them.
- BAR Memory is the device's largest memory BAR, through which the host reaches device memory.
  Resizable BAR says whether it was resized; the driver reads that from PCI configuration space.
- FB Memory Usage: Reserved is memory the driver keeps back (installed size minus Total). Used
  counts it, so Total is Used plus Free. A memory clock the driver reports as 0 reads ``N/A``.
- Utilization comes from the Sysman engine counters, or from the per-process engine times in
  fdinfo where those are unavailable; never from the EU array, whose metric stream is slow to
  open. Video and Video Enhancement split Media into its decode/encode engines (which Linux
  reports as one) and its enhancement engines, on devices that have them.
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
  :doc:`health` command compares against, not sensor readings. Without a threshold configured
  for the device, the default shutdown temperature is lowered to the driver's maximum for that
  sensor, as on CRI (125 C). The max operating temperatures are what the driver reports for each
  sensor, on devices that report one.
- On Windows the kernel-mode driver version reads ``N/A``.
- Rows only some devices or drivers supply, such as the RAS categories under ECC Errors, are
  left out of the text report on a device without them, so it does not fill with ``N/A``. The
  JSON output keeps every key, as ``null``. ``--verbose`` prints them all, and ``Not Supported``
  for a value the driver or OS says the device does not support; a value missing for another or
  an unknown reason stays ``N/A``.

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
