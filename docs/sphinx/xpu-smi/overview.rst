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

Running ``xpu-smi`` with no arguments displays a GPU status summary of all detected devices.

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
     - Update GPU firmware (GFX, FDO, GFX_DATA, OP_CODE, OP_DATA, AMC)
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
     - GPU core and memory temperatures. Shortcut ``p`` also enables POWER.
   * - ``POWER``
     - ``p``
     - Power draw, per-tile power, energy consumed, power limits. Shortcut ``p`` also enables TEMPERATURE.
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
     - Driver version string
   * - ``vbios_version``
     - —
     - VBIOS version string
   * - ``pci.bus_id``
     - —
     - PCI BDF address
   * - ``pci.device_id``
     - —
     - PCI device ID
   * - ``pci.sub_device_id``
     - —
     - PCI sub-device ID
   * - ``temperature.gpu``
     - C
     - GPU core temperature
   * - ``temperature.memory``
     - C
     - Memory temperature
   * - ``power.draw``
     - W
     - Total GPU power draw
   * - ``power.draw.gpu``
     - W
     - Per-tile GPU power draw
   * - ``power.limit``
     - W
     - Current power limit
   * - ``power.max_limit``
     - W
     - Maximum power limit
   * - ``energy.consumed``
     - J
     - Total energy consumed
   * - ``utilization.gpu``
     - %
     - Overall GPU utilization
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
   * - ``clocks.throttle.reason``
     - —
     - Active clock throttle reason flags
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
   * - ``ras.non_compute.errors.total``
     - —
     - RAS non-compute (memory) error counter
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
