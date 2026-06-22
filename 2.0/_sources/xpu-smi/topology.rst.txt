Topology
========

Get the system topology, showing how GPUs are interconnected and connected to the CPU.

Synopsis
--------

.. code-block:: text

   xpu-smi topology -d [deviceId]
   xpu-smi topology --device [deviceId]
   xpu-smi topology --device [pciBdfAddress]
   xpu-smi topology --device [deviceId] -j
   xpu-smi topology -f [filename]
   xpu-smi topology -m

Options
-------

.. option:: -h, --help

   Print this help message and exit.

.. option:: -j, --json

   Print result in JSON format.

.. option:: -d <deviceId>, --device <deviceId>, --id <deviceId>

   The device ID or PCI BDF address to query.

.. option:: -f <filename>, --file <filename>

   Generate the system topology with GPU info and write it to an XML file.

.. option:: -m, --matrix

   Print the CPU/GPU topology matrix showing interconnect types between all devices.

   The matrix uses the following symbols:

   .. list-table::
      :widths: 15 85
      :header-rows: 1

      * - Symbol
        - Meaning
      * - S
        - Self
      * - MDF
        - Connected with Multi-Die Fabric Interface
      * - PIX
        - Connected via PCIe switch (≥3 common bridge ancestors)
      * - PXB
        - Connected via multiple PCIe bridges (2 common ancestors)
      * - PHB
        - Connected via PCIe host bridge (1 common ancestor)
      * - NODE
        - Connected with PCIe within a NUMA node
      * - SYS
        - Connected with PCIe between NUMA nodes

Examples
--------

Show topology for device 0:

.. code-block:: shell

   xpu-smi topology --device 0

Show topology in JSON format:

.. code-block:: shell

   xpu-smi topology --device 0 -j

Export topology to XML file:

.. code-block:: shell

   xpu-smi topology -f system_topology.xml

Print the full CPU/GPU interconnect matrix:

.. code-block:: shell

   xpu-smi topology -m
