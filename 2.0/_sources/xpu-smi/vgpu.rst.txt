vGPU
====

Create and remove virtual GPUs in SR-IOV (Single Root I/O Virtualization)
configuration. Allows partitioning a physical GPU into multiple virtual GPU
instances for use in virtualized environments.

.. note::

   This command is supported on Linux only.

Synopsis
--------

.. code-block:: text

   xpu-smi vgpu --precheck
   xpu-smi vgpu -d [deviceId] -c -n [vGpuNumber] --lmem [vGpuMemorySize]
   xpu-smi vgpu --device [deviceId] -c -n [vGpuNumber] --lmem [vGpuMemorySize]
   xpu-smi vgpu --device [pciBdfAddress] -c -n [vGpuNumber] --lmem [vGpuMemorySize]
   xpu-smi vgpu --device [deviceId] -r
   xpu-smi vgpu --device [pciBdfAddress] -r
   xpu-smi vgpu --device [deviceId] -l
   xpu-smi vgpu --device [pciBdfAddress] -l
   xpu-smi vgpu --device [deviceId] -s
   xpu-smi vgpu --device [pciBdfAddress] -s

Options
-------

.. option:: -h, --help

   Print this help message and exit.

.. option:: -j, --json

   Print result in JSON format.

.. option:: -d <deviceId>, --device <deviceId>, --id <deviceId>

   Device ID or PCI BDF address of the physical GPU to operate on.

.. option:: --precheck

   Check whether BIOS settings are configured correctly and the system is ready
   to create virtual GPUs. Run this before attempting to create vGPUs.

.. option:: -c, --create

   Create virtual GPUs on the specified physical GPU. Requires ``-n`` and ``--lmem``.

.. option:: -n <count>, --number <count>

   The number of virtual GPUs to create.

.. option:: --lmem <MiB>

   The local memory size (in MiB) to allocate to each virtual GPU.

   Example: ``--lmem 500``

.. option:: -r, --remove

   Remove all virtual GPUs on the specified physical GPU.

.. option:: -l, --list

   List all virtual GPUs on the specified physical GPU.

.. option:: -s, --stats

   Show statistics data for all virtual GPUs on the specified physical GPU.

Examples
--------

Check system readiness for vGPU creation:

.. code-block:: shell

   xpu-smi vgpu --precheck

Create 4 virtual GPUs on device 0, each with 500 MiB of local memory:

.. code-block:: shell

   xpu-smi vgpu --device 0 -c -n 4 --lmem 500

List all virtual GPUs on device 0:

.. code-block:: shell

   xpu-smi vgpu --device 0 -l

Show statistics for all virtual GPUs on device 0:

.. code-block:: shell

   xpu-smi vgpu --device 0 -s

Remove all virtual GPUs from device 0:

.. code-block:: shell

   xpu-smi vgpu --device 0 -r
