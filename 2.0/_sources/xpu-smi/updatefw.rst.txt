UpdateFW
========

Update GPU firmware. Supports direct GPU firmware types (GFX, GFX_DATA, FDO,
OP_CODE, OP_DATA) as well as AMC (Add-in card Management Controller) firmware
on supported server platforms.

Synopsis
--------

.. code-block:: text

   xpu-smi updatefw -d [deviceId] -t GFX -f [imageFilePath]
   xpu-smi updatefw --device [deviceId] -t GFX -f [imageFilePath]
   xpu-smi updatefw --device [pciBdfAddress] -t GFX -f [imageFilePath]
   xpu-smi updatefw --device [deviceId] -t FDO -f [imageFilePath]
   xpu-smi updatefw -t AMC -f [imageFilePath]

Options
-------

.. option:: -h, --help

   Print this help message and exit.

.. option:: -j, --json

   Print result in JSON format.

.. option:: -d <deviceId>, --device <deviceId>, --id <deviceId>

   The device ID or PCI BDF address to update. Accepts a comma-separated list to
   update several devices at once (e.g., ``-d 0,1,4``). If not specified, the
   firmware image is applied to all compatible devices.

.. option:: -t <type>, --type <type>

   The firmware type to update. Valid values:

   .. list-table::
      :widths: 20 80
      :header-rows: 1

      * - Type
        - Description
      * - ``GFX``
        - Graphics firmware
      * - ``GFX_DATA``
        - Graphics data firmware
      * - ``FDO``
        - Field-deployable firmware
      * - ``AMC``
        - Add-in card Management Controller firmware
      * - ``OP_CODE``
        - Option ROM code
      * - ``OP_DATA``
        - Option ROM data

.. option:: -f <path>, --file <path>

   Path to the firmware image file on the local filesystem.

.. option:: -y, --assumeyes

   Assume yes to all confirmation prompts (non-interactive mode).

.. option:: --force

   Force the GFX firmware update even if the existing firmware is up to date.
   This option applies to GFX firmware only.

Examples
--------

Update GFX firmware on device 0:

.. code-block:: shell

   xpu-smi updatefw --device 0 -t GFX -f /path/to/gfx_firmware.bin

Update GFX firmware on device addressed by PCI BDF:

.. code-block:: shell

   xpu-smi updatefw --device 0000:4d:00.0 -t GFX -f /path/to/gfx_firmware.bin

Update FDO firmware on device 0 without confirmation:

.. code-block:: shell

   xpu-smi updatefw --device 0 -t FDO -f /path/to/fdo_firmware.bin -y

Update AMC firmware on all compatible devices:

.. code-block:: shell

   xpu-smi updatefw -t AMC -f /path/to/amc_firmware.bin

Force GFX firmware update even if up to date:

.. code-block:: shell

   xpu-smi updatefw --device 0 -t GFX -f /path/to/gfx_firmware.bin --force

Obtaining Firmware Binaries for Arc Pro (Battlemage) GPUs
---------------------------------------------------------

Firmware images for Intel(R) Arc Pro Series (Battlemage / BMG) GPUs are
distributed through the Linux Vendor Firmware Service (LVFS). To gather the
binaries required by the ``updatefw`` command:

#. Browse the LVFS catalog and search for your device at
   `LVFS Arc Battlemage search <https://fwupd.org/lvfs/search?value=Arc+Battlemage>`_,
   then navigate to the exact Arc Pro GPU SKU you want to update.

#. On that device's page, click **Download archive** to download the cabinet
   (``.cab``) file for that BMG GPU.

#. Use the ``cabextract`` tool to extract the contents of the ``.cab`` archive.
   It contains multiple firmware files — typically ``fwcode``, ``fwdata``,
   ``opromcode``, and ``opromdata`` for your specific device. Choose the file
   matching the firmware type you want to update.

   .. code-block:: shell

      cabextract firmware-archive.cab

#. Confirm the type is supported by ``xpu-smi`` — run ``xpu-smi updatefw --help``
   to list the supported firmware types (see the ``-t`` option above for the
   mapping). If the type is supported, feed the extracted file to the update
   command:

   .. code-block:: shell

      xpu-smi updatefw --device 0 -t GFX -f /path/to/fwcode

.. note::

   The file names inside the cabinet map to the ``updatefw`` firmware types
   (for example, ``fwcode`` / ``fwdata`` correspond to the GFX code and data
   images). Only the types listed under the ``-t`` option can be flashed with
   ``xpu-smi``.
