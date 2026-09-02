Crashlog
========

Manage Intel Crash Log collection for a GPU.

The command drives the Intel Crash Log CLI (``iclg``) for enable, disable,
trigger, clear, and extract operations. It resolves an xpu-smi device index or
PCI BDF address to the ``pmt:<bdf>`` source string that ``iclg`` expects, then
runs ``iclg`` for each selected device.

.. note::

   This command is supported on Linux only.

   It requires the ``iclg`` utility, which is not part of xpu-smi. Install it
   from the Intel Crash Log project at https://github.com/intel/crashlog. If
   ``iclg`` is not found in ``PATH``, the command reports an error and does
   nothing. Most operations also require root privileges.

Synopsis
--------

.. code-block:: text

   xpu-smi crashlog --enable  -d [deviceId]
   xpu-smi crashlog --disable -d [deviceId]
   xpu-smi crashlog --trigger -d [deviceId]
   xpu-smi crashlog --clear   -d [deviceId]
   xpu-smi crashlog --extract -d [deviceId] --output [dir]
   xpu-smi crashlog --extract -d [deviceId] --output [dir] --decode

Exactly one of ``--enable``, ``--disable``, ``--trigger``, ``--clear``, or
``--extract`` must be given. ``--output`` and ``--decode`` are valid only with
``--extract``.

Options
-------

.. option:: -h, --help

   Print this help message and exit.

.. option:: --enable

   Enable crash log collection on the device.

.. option:: --disable

   Disable crash log collection on the device.

.. option:: --trigger

   Trigger an on-demand crash log collection.

.. option:: --clear

   Clear the crash log storage.

.. option:: --extract

   Extract crash log records to a directory. Each path ``iclg`` writes is
   reported in the command output.

.. option:: --output <dir>

   Output directory for ``--extract``. Defaults to the current directory. It
   must be an existing directory: a single source can produce several records,
   and ``iclg`` names them uniquely only when writing into a directory.

.. option:: --decode

   With ``--extract``, also decode each extracted record to JSON, written next
   to the binary record as ``<file>.json``.

.. option:: -d <deviceId>, --device <deviceId>, --id <deviceId>

   The device ID or PCI BDF address. Accepts a comma-separated list to act on
   several devices at once (e.g., ``-d 0,1``). Required.

.. option:: -j, --json

   Print result in JSON format.

Device Support
--------------

Not every GPU exposes an Intel Crash Log source. Before acting, the command
queries the sources ``iclg`` reports and rejects any device that is not among
them, reporting it as ``unsupported`` rather than falsely reporting success. The
exit status reflects the outcome: success when every device succeeded, an
unsupported-feature error when every failure was an unsupported device, and a
general error otherwise.

Examples
--------

Enable crash log collection on device 0:

.. code-block:: shell

   xpu-smi crashlog --enable -d 0

Trigger an on-demand collection on device 0:

.. code-block:: shell

   xpu-smi crashlog --trigger -d 0

Extract the crash log for device 0 into ``/tmp`` and decode it to JSON:

.. code-block:: shell

   xpu-smi crashlog --extract -d 0 --output /tmp --decode

Clear the crash log storage on devices 0 and 1, in JSON format:

.. code-block:: shell

   xpu-smi crashlog --clear -d 0,1 -j
