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
   xpu-smi topology --p2p [capability]

Options
-------

.. option:: -h, --help

   Print this help message and exit.

.. option:: -j, --json

   Print result in JSON format.

.. option:: -d <deviceId>, --device <deviceId>, --id <deviceId>

   The device ID or PCI BDF address to query. Accepts a comma-separated list to
   query several devices at once (e.g., ``-d 0,1,4``).

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
        - Connected within a NUMA node
      * - SYS
        - Worst-case connectivity (cross-NUMA or topology unknown)

   Symbols are assigned by a priority-ordered decision chain, best (fastest)
   connectivity first:

   #. **S** — self (same device and tile).
   #. **MDF** — both GPU tiles on the same physical device.
   #. **PIX** — PCIe paths share ≥3 common bridge ancestors (same PCIe switch).
   #. **PXB** — PCIe paths share exactly 2 common ancestors (same root port).
   #. **PHB** — PCIe paths share exactly 1 common ancestor (same host bridge).
   #. **NODE** — no common PCIe ancestor (or a PCIe path is unavailable) but both
      devices resolve to the same NUMA node.
   #. **SYS** — everything else: different NUMA nodes, or NUMA/topology unknown.

   .. note::

      When two devices share a NUMA node but sit under different PCIe root
      complexes (no common PCIe ancestor), they are classified as ``NODE``
      rather than ``SYS``. NUMA locality is used as the tiebreaker whenever the
      PCIe paths do not share an ancestor.

.. option:: --p2p <capability>

   Print the peer-to-peer (P2P) capability matrix between GPU devices for the
   requested capability.

   The capability argument selects which P2P property to probe:

   .. list-table::
      :widths: 15 85
      :header-rows: 1

      * - Value
        - Meaning
      * - ``r``
        - P2P read/write access (``w`` accepted as an alias; Level Zero reports
          this as a single unified capability)
      * - ``n``
        - MDF fabric connectivity
      * - ``a``
        - P2P atomic operations
      * - ``p``
        - PCIe P2P access

   The matrix uses the following symbols:

   .. list-table::
      :widths: 15 85
      :header-rows: 1

      * - Symbol
        - Meaning
      * - X
        - Self
      * - OK
        - Capability supported
      * - NS
        - Not supported
      * - ?
        - Query failed (driver or device error)

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

Print the P2P read/write capability matrix between GPUs:

.. code-block:: shell

   xpu-smi topology --p2p r
