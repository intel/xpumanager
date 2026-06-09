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
   information about the given device.

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
        - OPROM Code Firmware Name
      * - 44
        - OPROM Code Firmware Version
      * - 45
        - OPROM Data Firmware Name
      * - 46
        - OPROM Data Firmware Version

.. option:: --listamcversions

   Show all AMC firmware versions.

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
