Discovery
=========

Discover the GPU devices installed on this machine and provide device information.

Synopsis
--------

.. code-block:: text

   xpu-smi discovery
   xpu-smi discovery -d [deviceId]
   xpu-smi discovery --device [deviceId]
   xpu-smi discovery --device [pciBdfAddress]
   xpu-smi discovery --device [deviceId] -j
   xpu-smi discovery --dump [propertyIds]

Options
-------

.. option:: -h, --help

   Print this help message and exit.

.. option:: -j, --json

   Print result in JSON format.

.. option:: -d <deviceId>, --device <deviceId>, --id <deviceId>

   Device ID or PCI BDF address to query. When specified, displays more detailed
   information about the given device. Accepts a comma-separated list to query
   several devices at once (e.g., ``-d 0,1,4`` or ``-d 0,0000:4d:00.0``).

.. option:: --pf, --physicalFunction

   Display the physical functions only.

.. option:: --vf, --virtualFunction

   Display the virtual functions only.

.. option:: --dump <propertyIds>

   Dump one or more device properties in CSV format. Accepts a comma-separated list
   of property IDs. Use ``-1`` to dump all properties.

   .. list-table:: Property IDs
      :widths: 10 90
      :header-rows: 1

      * - ID
        - Property
      * - 1
        - Device ID
      * - 2
        - Device Name
      * - 3
        - Vendor Name
      * - 4
        - SOC UUID
      * - 5
        - Serial Number
      * - 6
        - Core Clock Rate
      * - 7
        - Stepping
      * - 8
        - Driver Version
      * - 9
        - GFX Firmware Version
      * - 10
        - GFX Data Firmware Version
      * - 11
        - PCI BDF Address
      * - 12
        - PCI Slot
      * - 13
        - PCIe Generation
      * - 14
        - PCIe Max Link Width
      * - 15
        - OAM Socket ID
      * - 16
        - Memory Physical Size
      * - 17
        - Number of Memory Channels
      * - 18
        - Memory Bus Width
      * - 19
        - Number of EUs
      * - 20
        - Number of Media Engines
      * - 21
        - Number of Media Enhancement Engines
      * - 22
        - GFX Firmware Status
      * - 23
        - PCI Vendor ID
      * - 24
        - PCI Device ID
      * - 25
        - Number of Tiles
      * - 26
        - Number of Slices
      * - 27
        - Number of Sub-slices per Slice
      * - 28
        - Number of EUs per Sub-slice
      * - 29
        - Number of Threads per EU
      * - 30
        - Physical EU SIMD Width
      * - 31
        - Max Command Queue Priority
      * - 32
        - Max Hardware Contexts
      * - 33
        - Max Memory Alloc Size
      * - 34
        - Memory Free Size
      * - 35
        - Memory ECC State
      * - 36
        - Kernel Version
      * - 37
        - DRM Device
      * - 38
        - Device Type
      * - 39
        - SKU Type
      * - 40
        - PCIe Max Bandwidth
      * - 41
        - AMC Firmware Name
      * - 42
        - AMC Firmware Version
      * - 43
        - GFX PSCBIN Firmware Name
      * - 44
        - GFX PSCBIN Firmware Version
      * - 45
        - OPROM Code Firmware Name
      * - 46
        - OPROM Code Firmware Version
      * - 47
        - OPROM Data Firmware Name
      * - 48
        - OPROM Data Firmware Version
      * - 49
        - Part Number
      * - 50
        - Memory Type
      * - 51
        - Memory Vendor
      * - 52
        - Memory Date Code
      * - 53
        - Memory IC/Die Info

.. option:: --listamcversions

   Show all AMC firmware versions.

Device Properties
-----------------

``xpu-smi discovery --device <deviceId>`` prints the properties of a single device,
grouped and ordered as follows:

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Group
     - Properties (in display order)
   * - Basic Device Information
     - Device Type, Device Name, Device State, Recovery Action, PCI Device ID,
       Vendor Name, SOC UUID, Serial Number, Part Number, Core Clock Rate,
       Stepping, SKU Type
   * - Driver and Firmware
     - Driver Version, Kernel Version, GFX Firmware Name, GFX Firmware Version,
       GFX Firmware Status
   * - PCIe Information
     - PCI BDF Address, PCI Slot, PCIe Generation, PCIe Max Link Width,
       PCIe Max Bandwidth
   * - Memory Information
     - Memory Type, Memory Physical Size, Memory Vendor, Memory Date Code,
       Memory IC/Die Info, Max Mem Alloc Size, ECC State,
       Number of Memory Channels, Memory Bus Width, Max Hardware Contexts,
       Max Command Queue Priority
   * - EU and Architecture Information
     - Number of EUs, Number of Tiles, Number of Slices,
       Number of Sub Slices per Slice, Number of Threads per EU,
       Physical EU SIMD Width, Number of Media Engines,
       Number of Media Enhancement Engines
   * - AMC Firmware Information
     - AMC Firmware Name, AMC Firmware Version

Properties that the platform does not report are shown as ``N/A``, ``unknown``, or
are omitted.

.. note::

   **Memory Type** is resolved from two Level Zero sources, because neither is
   complete on its own: the sysman memory type is per memory module and is the
   only source that names a product-specific type such as ``LPDDR5X``, while the
   core device memory extension type distinguishes memory generations the sysman
   enumeration cannot express (it has a single ``HBM`` value, where the core type
   separates HBM2/HBM2E/HBM3/HBM3E/HBM4). Whichever source names the memory more
   specifically is reported, so the property is populated on platforms whose
   sysman layer reports no type at all. It reads ``unknown`` when neither source
   names a type.

   **Memory Vendor**, **Memory Date Code** and **Memory IC/Die Info** report
   ``unknown`` on all currently supported platforms: no Level Zero interface
   (core, sysman, or the Intel sysman extensions), no kernel-mode driver
   interface (sysfs or debugfs) and no ``igsc`` entry point exposes memory
   manufacturer, date-code or IC/die data. Populating them requires a new driver
   or firmware interface; the properties are present so that no CLI change is
   needed once such an interface exists.

Examples
--------

List all GPUs:

.. code-block:: shell

   xpu-smi discovery

Show detailed info for device 0:

.. code-block:: shell

   xpu-smi discovery --device 0

Show detailed info in JSON format:

.. code-block:: shell

   xpu-smi discovery --device 0 -j

Dump device name and PCI BDF for all devices in CSV format:

.. code-block:: shell

   xpu-smi discovery --dump 2,11

Dump all properties in CSV format:

.. code-block:: shell

   xpu-smi discovery --dump -1

List all AMC firmware versions:

.. code-block:: shell

   xpu-smi discovery --listamcversions
