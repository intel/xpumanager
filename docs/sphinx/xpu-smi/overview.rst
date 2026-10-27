Overview
========

``xpu-smi`` is the command-line interface for Intel(R) XPU Manager.
It is built on top of the Intel Level-Zero driver stack and provides local GPU
management for Intel(R) Arc Pro Series GPUs.

Global Options
--------------

These options are available at the top level (without a subcommand):

.. option:: -h, --help

   Print the help message and exit.

.. option:: -v, --version

   Display version information, including CLI version, build ID, and Level Zero version.

.. option:: --list-gpus

   Print one line per GPU (index, name, UUID) and exit.

.. option:: --query-gpu=<fields>

   Single-shot or looping per-field query. ``<fields>`` is a comma-separated list of
   field names (e.g. ``temperature.gpu,power.draw``). Run ``xpu-smi --query-gpu`` with
   no fields to list all available field names.

   Related options used with ``--query-gpu``:

   .. option:: --id <n>, --device <n>

      Select GPU by index or PCI BDF address.

   .. option:: --loop[=<sec>]

      Repeat query every ``<sec>`` seconds (default: ``1``).

   .. option:: --loop-ms=<ms>

      Repeat query every ``<ms>`` milliseconds. Takes precedence over ``--loop``.

   .. option:: --count=<n>, -c <n>

      Cap the number of loop iterations. Default: infinite.

   .. option:: --format=<spec>

      Output format: ``csv[,noheader][,nounits]``.

   .. option:: -f <path>, --file=<path>

      Redirect output to a file instead of stdout.

Synopsis
--------

.. code-block:: text

   xpu-smi [Options]
   xpu-smi -v
   xpu-smi -h
   xpu-smi --list-gpus
   xpu-smi --query-gpu=<fields> [--id <n>] [--loop[=<sec>]|--loop-ms=<ms>] [--count=<n>] [--format=csv[,...]]
   xpu-smi <command> [command-options]

Default Output
--------------

Running ``xpu-smi`` with no arguments displays a GPU status summary of all detected devices,
followed by the processes using them.

``Name`` is the device name the driver reports, without the words that every Intel device
name shares: the trademark signs, a leading ``Intel`` (kept when only ``Graphics`` or ``GPU``
would be left), a ``Data Center GPU`` prefix and a trailing ``Graphics`` or ``GPU`` after the
model number. So ``Intel(R) Arc(TM) Pro B60 Graphics`` shows as ``Arc Pro B60``, and a name
still too long for the column wraps at a space. A driver
that has no product name for a device reports ``Intel(R) Graphics [0x<PCI device ID>]``, shown as
``Graphics [0x<PCI device ID>]``; a newer driver may name it. :doc:`discovery` and
``--query-gpu=name`` show the full name.

``GPU-Util`` is the utilization of the busiest engine on the device: the fraction of the
sample window during which that engine was executing work. It is not the average across
all engines — a workload that saturates the compute engine while leaving the media and
copy engines idle reports close to 100%, because the compute engine is the bottleneck.
Use :doc:`stats` for the per-engine-class breakdown behind that figure.

.. note::

   Engine utilization is read from the GPU's busyness counters, which the kernel exposes
   only to callers holding ``CAP_PERFMON`` (or ``CAP_SYS_ADMIN`` on older kernels).
   ``GPU-Util`` shows ``N/A`` when they are inaccessible — typically an unprivileged user,
   or a container started without the capability. Add it with
   ``docker run --cap-add=PERFMON ...``, or run the container privileged. ``N/A`` means
   "not measurable here", as opposed to ``0%``, which means the engines were idle.

Command Categories
------------------

.. list-table::
   :widths: 20 15 15 50
   :header-rows: 1

   * - Command
     - Linux
     - Windows
     - Description
   * - :doc:`discovery`
     - Yes
     - Yes
     - Discover GPU devices and display device properties
   * - :doc:`topology`
     - Yes
     - Yes
     - Display system topology and CPU/GPU connectivity
   * - :doc:`stats`
     - Yes
     - Yes
     - Display GPU performance statistics
   * - :doc:`dump`
     - Yes
     - Yes
     - Continuously dump raw GPU telemetry to stdout or file
   * - :doc:`health`
     - Yes
     - No
     - Check GPU component health status
   * - :doc:`config`
     - Yes
     - Yes
     - Get and change GPU settings
   * - :doc:`updatefw`
     - Yes
     - Yes
     - Update GPU firmware (GFX, FDO, GFX_DATA, OP_CODE, OP_DATA, AMC, COMPOSITE)
   * - :doc:`amc`
     - Yes
     - No
     - AMC (Add-in Card Management Controller) operations
   * - :doc:`vgpu`
     - Yes
     - No
     - Create and manage virtual GPUs (SR-IOV)
   * - :doc:`ps`
     - Yes
     - No
     - List processes using GPU resources
   * - :doc:`crashlog`
     - Yes
     - No
     - Manage Intel Crash Log collection via the ``iclg`` utility
   * - :doc:`log`
     - Yes
     - No
     - Collect GPU debug logs

Output Formats
--------------

Most commands support two output formats:

- **Text** (default): Human-readable tabular output
- **JSON** (``-j`` / ``--json``): Machine-readable JSON, suitable for scripting and automation
- **XML** (``topology -f <file>``): System topology exported as an XML file (see :doc:`topology`)

Device Identification
---------------------

Most commands accept a device identifier via ``-d`` / ``--device`` / ``--id`` (subcommands) or ``--device`` / ``--id`` (``--query-gpu``). Devices can be
addressed by either:

- **Device ID**: An integer index assigned by the driver (e.g., ``0``, ``1``)
- **PCI BDF address**: Bus/Device/Function notation (e.g., ``0000:4d:00.0``)

A comma-separated list combining either form selects multiple devices in a
single invocation (e.g., ``-d 0,1,4`` or ``-d 0,0000:4d:00.0``). Most
subcommands that accept ``--device`` parse this list; duplicate entries are
de-duplicated and the requested order is preserved. Some operations still
require a single device or behave differently for multiple devices:

- ``amc`` operates on a single device only and does not accept a list.
- ``config --reset`` / ``config --coldreset`` act on a single device.
- ``health`` produces separate per-device output only when ``--list`` is used.

Refer to each subcommand's page for its exact device-selection behavior.

--query-gpu Field Reference
----------------------------

``--query-gpu`` accepts a comma-separated list of field names. Fields are grouped by
metric category. Run ``xpu-smi --query-gpu`` with no argument to print the live list.

**Metric Groups (for** ``--metrics`` **)**

.. list-table::
   :widths: 18 10 72
   :header-rows: 1

   * - Group Name
     - Shortcut
     - Description
   * - ``IDENTITY``
     - —
     - Device identity fields (name, index, UUID, serial, driver/vbios version, PCI IDs)
   * - ``MEMORY``
     - ``m``
     - Memory total/used/free, read/write bandwidth, bandwidth utilization
   * - ``UTILIZATION``
     - ``u``
     - GPU, compute, render, media, copy engine utilization (%)
   * - ``TEMPERATURE``
     - ``p``
     - GPU core, memory and composite temperatures, configured throttle/shutdown thresholds. Shortcut ``p`` also enables POWER.
   * - ``POWER``
     - ``p``
     - Card- and GPU-domain power draw, energy counters, power limits. Shortcut ``p`` also enables TEMPERATURE.
   * - ``CLOCK``
     - ``c``
     - Current and max graphics/media clock frequencies, throttle reason
   * - ``PCI``
     - ``t``
     - PCIe link gen/width (max and current), TX/RX throughput, replay counter
   * - ``ECC``
     - ``e``
     - ECC mode, corrected/uncorrected error counts, RAS error categories
   * - ``EU_ARRAY``
     - ``x``
     - EU active/stall/idle percentages (Intel Xe only)
   * - ``FAN``
     - ``f``
     - Fan speed (%)
   * - ``COMPUTE``
     - —
     - Scheduler mode
   * - ``PERFORMANCE``
     - —
     - Whether each clock throttle reason is active
   * - ``SUPPORTED_CLOCKS``
     - —
     - GPU clocks the frequency range can be set to
   * - ``ALL``
     - —
     - All metrics from all groups

Multi-char combos expand character-by-character (e.g. ``pu`` = POWER + TEMPERATURE + UTILIZATION, ``put`` adds PCI, ``pum`` adds MEMORY).

**Available Field Names**

.. list-table::
   :widths: 42 12 46
   :header-rows: 1

   * - Field
     - Unit
     - Description
   * - ``timestamp``
     - —
     - Sample timestamp
   * - ``name`` (alias: ``gpu_name``)
     - —
     - GPU device name
   * - ``index``
     - —
     - GPU device index
   * - ``uuid``
     - —
     - GPU UUID
   * - ``serial``
     - —
     - Serial number
   * - ``driver_version``
     - —
     - Level Zero user-mode driver version string
   * - ``kernel_driver_version``
     - —
     - Kernel-mode driver release version (``modinfo`` ``version``), declared only
       by the out-of-tree driver installed via DKMS. The key is omitted when the
       driver declares no version
   * - ``vbios_version``
     - —
     - GFX firmware version (shown as GFX Firmware Version in the query report)
   * - ``pci.bus_id``
     - —
     - PCI BDF address
   * - ``pci.device_id``
     - —
     - PCI device ID
   * - ``pci.base_class``
     - —
     - PCI base class code, in hex: ``0x03`` display controller, ``0x12`` processing accelerator
   * - ``pci.sub_class``
     - —
     - PCI subclass code, in hex
   * - ``recovery_action``
     - —
     - What the device needs to keep working: ``None``, ``Reset``, or ``Reset to repair``
   * - ``pci.sub_device_id``
     - —
     - PCI subsystem device ID (Linux only)
   * - ``temperature.gpu``
     - C
     - GPU core temperature
   * - ``temperature.memory``
     - C
     - Memory temperature
   * - ``temperature.composite``
     - C
     - Composite temperature
   * - ``temperature.gpu.threshold.throttle``
     - C
     - Configured GPU core temperature at which the clock is throttled
   * - ``temperature.gpu.threshold.shutdown``
     - C
     - Configured GPU core temperature at which the device shuts down
   * - ``temperature.memory.threshold.throttle``
     - C
     - Configured memory temperature at which the clock is throttled
   * - ``temperature.memory.threshold.shutdown``
     - C
     - Configured memory temperature at which the device shuts down
   * - ``power.draw``
     - W
     - Card-domain power draw, averaged over the sampling interval
   * - ``power.draw.gpu``
     - W
     - GPU-domain power draw, averaged over the sampling interval: compute engines only,
       excluding memory and other card subsystems. ``N/A`` on devices that expose no GPU
       power domain
   * - ``power.draw.instant``
     - W
     - Card-domain power draw at this moment, as the driver reports it; needs a runtime and loader
       that support it, and root on CRI
   * - ``power.limit``
     - W
     - Current power limit
   * - ``power.limit.burst``
     - W
     - Burst power limit (PL2), on devices that report one
   * - ``power.limit.peak``
     - W
     - Peak power limit (PL4), on devices that report one
   * - ``power.max_limit``
     - W
     - Maximum power limit
   * - ``energy.consumed``
     - J
     - Cumulative card-domain energy counter; the energy counterpart of ``power.draw``
   * - ``energy.consumed.gpu``
     - J
     - Cumulative GPU-domain energy counter; the energy counterpart of ``power.draw.gpu``.
       ``N/A`` on devices that expose no GPU power domain
   * - ``utilization.gpu``
     - %
     - Overall GPU utilization: the busiest engine on the device
   * - ``utilization.compute``
     - %
     - Compute engine utilization
   * - ``utilization.render``
     - %
     - Render engine utilization
   * - ``utilization.media``
     - %
     - Media engine utilization
   * - ``utilization.copy``
     - %
     - Copy engine utilization
   * - ``utilization.memory``
     - %
     - Memory bandwidth utilization
   * - ``memory.total``
     - MiB
     - Total memory
   * - ``memory.reserved``
     - MiB
     - Memory the driver keeps back: installed size minus ``memory.total``; included in
       ``memory.used``
   * - ``memory.used``
     - MiB
     - Used memory
   * - ``memory.free``
     - MiB
     - Free memory
   * - ``memory.read.bandwidth``
     - kB/s
     - Memory read bandwidth
   * - ``memory.write.bandwidth``
     - kB/s
     - Memory write bandwidth
   * - ``memory.bandwidth.utilization``
     - %
     - Memory bandwidth utilization
   * - ``clocks.current.graphics`` (alias: ``clocks.current.sm``)
     - MHz
     - Current graphics/SM clock
   * - ``clocks.current.media`` (alias: ``clocks.current.video``)
     - MHz
     - Current media/video clock
   * - ``clocks.max.graphics`` (alias: ``clocks.max.sm``)
     - MHz
     - Max graphics clock
   * - ``clocks.max.media`` (alias: ``clocks.max.video``)
     - MHz
     - Max media clock
   * - ``clocks.supported.graphics``
     - MHz
     - GPU core clocks the minimum and maximum of ``config --frequencyrange`` can be set
       to: ``low-high step N`` when evenly spaced, otherwise a ``;`` separated list. The GPU
       cannot be pinned to one of them, and there is no list for the memory clock, which
       cannot be set
   * - ``clocks.requested.graphics``
     - MHz
     - GPU core clock the driver last requested from the hardware
   * - ``clocks.throttle.reason``
     - —
     - Active clock throttle reason flags
   * - ``clocks.event_reasons.<reason>``
     - —
     - Whether one throttle reason is active (``Active`` or ``Not Active``). One field per
       reason: ``thermal_limit``, ``average_power_cap``, ``burst_power_cap``, ``current_limit``,
       ``psu_alert``, ``sw_range``, ``hw_range``, ``voltage``, ``thermal_alert``, ``power_alert``
   * - ``pcie.link.gen.max``
     - —
     - Maximum PCIe generation
   * - ``pcie.link.gen.current``
     - —
     - Current PCIe generation
   * - ``pcie.link.width.max``
     - —
     - Maximum PCIe link width
   * - ``pcie.link.width.current``
     - —
     - Current PCIe link width
   * - ``pci.bar.memory.size``
     - MiB
     - Size of the device's largest memory BAR, the window through which the host reaches its local memory
   * - ``pci.bar.resizable``
     - —
     - Resizable BAR on that BAR: ``Enabled`` or ``Disabled``; reading it needs ``CAP_SYS_ADMIN``
   * - ``pcie.link.gen.host_max``
     - —
     - Maximum PCIe generation of the root port the device is under (Linux); a switch in between can limit the link further
   * - ``pcie.link.width.host_max``
     - —
     - Maximum PCIe link width of the root port the device is under (Linux)
   * - ``pcie.errors.correctable``
     - —
     - PCIe correctable errors since the device was enumerated (AER, Linux)
   * - ``pcie.errors.non_fatal``
     - —
     - PCIe uncorrectable non-fatal errors since enumeration (AER, Linux)
   * - ``pcie.errors.fatal``
     - —
     - PCIe uncorrectable fatal errors since enumeration (AER, Linux)
   * - ``pcie.errors.replay_rollovers``
     - —
     - PCIe REPLAY_NUM rollovers since enumeration (AER, Linux)
   * - ``pcie.errors.replay_timeouts``
     - —
     - PCIe replay timer timeouts since enumeration (AER, Linux)
   * - ``pcie.atomic.completer``
     - —
     - PCIe AtomicOp requests the device completes, e.g. ``FETCHADD_32 SWAP_32 CAS_32``; needs root on Linux
   * - ``pcie.atomic.requests``
     - —
     - Whether the device may send PCIe AtomicOp requests: ``Enabled`` or ``Disabled``; needs root on Linux
   * - ``pcie.tx.throughput``
     - MB/s
     - PCIe transmit throughput
   * - ``pcie.rx.throughput``
     - MB/s
     - PCIe receive throughput
   * - ``pcie.tx.throughput.kbs``
     - kB/s
     - PCIe transmit throughput (kB/s)
   * - ``pcie.rx.throughput.kbs``
     - kB/s
     - PCIe receive throughput (kB/s)
   * - ``pcie.replay.counter``
     - —
     - PCIe replay error counter
   * - ``ecc.mode.current``
     - —
     - ECC mode (enabled/disabled)
   * - ``ecc.errors.corrected.aggregate.total``
     - —
     - Total corrected ECC errors
   * - ``ecc.errors.uncorrected.aggregate.total``
     - —
     - Total uncorrected ECC errors
   * - ``ecc.errors.aggregate.total``
     - —
     - Total ECC errors (corrected + uncorrected)
   * - ``ras.reset``
     - —
     - RAS reset error counter
   * - ``ras.programming.errors``
     - —
     - RAS programming error counter
   * - ``ras.driver.errors``
     - —
     - RAS driver error counter
   * - ``ras.cache.errors.correctable``
     - —
     - RAS correctable cache error counter
   * - ``ras.cache.errors.uncorrectable``
     - —
     - RAS uncorrectable cache error counter
   * - ``ras.non_compute.errors.correctable``
     - —
     - RAS correctable non-compute error counter
   * - ``ras.non_compute.errors.uncorrectable``
     - —
     - RAS uncorrectable non-compute error counter
   * - ``ras.non_compute.errors.total``
     - —
     - RAS non-compute error counter (correctable + uncorrectable)
   * - ``ras.memory.errors.correctable``
     - —
     - RAS correctable device memory error counter (xe netlink RAS category)
   * - ``ras.memory.errors.uncorrectable``
     - —
     - RAS uncorrectable device memory error counter (xe netlink RAS category)
   * - ``ras.pcie.errors.correctable``
     - —
     - RAS correctable PCIe subsystem error counter (xe netlink RAS category)
   * - ``ras.pcie.errors.uncorrectable``
     - —
     - RAS uncorrectable PCIe subsystem error counter (xe netlink RAS category)
   * - ``ras.soc_internal.errors.correctable``
     - —
     - RAS correctable SoC internal error counter (xe netlink RAS category)
   * - ``ras.soc_internal.errors.uncorrectable``
     - —
     - RAS uncorrectable SoC internal error counter (xe netlink RAS category)
   * - ``ras.compute.errors.correctable``
     - —
     - RAS correctable compute engine error counter (xe netlink RAS category)
   * - ``ras.compute.errors.uncorrectable``
     - —
     - RAS uncorrectable compute engine error counter (xe netlink RAS category)
   * - ``ras.fabric.errors.correctable``
     - —
     - RAS correctable fabric interconnect error counter (xe netlink RAS category)
   * - ``ras.fabric.errors.uncorrectable``
     - —
     - RAS uncorrectable fabric interconnect error counter (xe netlink RAS category)
   * - ``ras.scale.errors.correctable``
     - —
     - RAS correctable scale fabric error counter (xe netlink RAS category)
   * - ``ras.scale.errors.uncorrectable``
     - —
     - RAS uncorrectable scale fabric error counter (xe netlink RAS category)
   * - ``eu.active``
     - %
     - EU array active percentage (Intel Xe only)
   * - ``eu.stall``
     - %
     - EU array stall percentage (Intel Xe only)
   * - ``eu.idle``
     - %
     - EU array idle percentage (Intel Xe only)
   * - ``fan.speed``
     - %
     - Fan speed percentage
   * - ``scheduler.mode``
     - —
     - Scheduler mode: timeout, timeslice or exclusive

The ``ecc.errors.*`` and ``ras.*`` error counters read the same RAS categories as ``stats -r``;
the ``ecc.errors.*`` totals add up every hardware category, i.e. all but resets, programming and
driver errors. A counter shows ``N/A`` when the device reports none of the categories it counts, as
opposed to ``0``, which means no errors were counted.
