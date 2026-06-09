Config
======

Get and change GPU device settings, including frequency range, power limits,
standby mode, scheduler mode, ECC state, fan control, and reset operations.

Synopsis
--------

.. code-block:: text

   xpu-smi config -d [deviceId]
   xpu-smi config --device [deviceId]
   xpu-smi config --device [deviceId] --frequencyrange [minFrequency,maxFrequency]
   xpu-smi config --device [deviceId] --powerlimit [powerValue]
   xpu-smi config --device [deviceId] --powerlimit [powerValue] --powertype [sustain|peak|burst]
   xpu-smi config --device [deviceId] --standby [standbyMode]
   xpu-smi config --device [deviceId] --scheduler [schedulerMode]
   xpu-smi config --device [deviceId] --memoryecc [0|1]
   xpu-smi config --device [deviceId] --pciedowngrade [0|1]
   xpu-smi config --device [deviceId] --fanspeed [percent|default]
   xpu-smi config --device [deviceId] --fanid [id] --fanspeed [percent|default]
   xpu-smi config --device [deviceId] --fancurve [temp:speed,...]
   xpu-smi config --device [deviceId] --fanid [id] --fancurve [temp:speed,...]
   xpu-smi config --device [deviceId] --fancurve-rpm [temp:rpm,...]
   xpu-smi config --device [deviceId] --fanid [id] --fancurve-rpm [temp:rpm,...]
   xpu-smi config --device [deviceId] --reset
   xpu-smi config --device [pciBdfAddress] --coldreset
   xpu-smi config --device [deviceId] --clear-ras-errors

Options
-------

.. option:: -h, --help

   Print this help message and exit.

.. option:: -j, --json

   Print result in JSON format.

.. option:: -d <deviceId>, --device <deviceId>, --id <deviceId>

   The device ID or PCI BDF address to configure.

.. option:: --frequencyrange <minFreq,maxFreq>

   Set the GPU tile-level core frequency range in MHz. Provide the minimum and
   maximum frequencies separated by a comma.

.. option:: --powerlimit <watts>

   Set the device-level power limit in watts.

.. option:: --powertype <type>

   Device-level power limit type. Must be combined with ``--powerlimit``.
   Valid values: ``sustain``, ``peak``, ``burst``.

.. option:: --standby <mode>

   Set the tile-level standby mode. Valid values: ``default``, ``never``.

   .. note::

      Standby mode support varies by GPU platform. Not all modes may be
      available on every device.

.. option:: --scheduler <mode>

   Set the tile-level scheduler mode. Accepted formats:

   - ``timeout,<watchdogTimeout>`` — Watchdog timeout mode; ``watchdogTimeout`` in microseconds.
   - ``timeslice,<interval>,<yieldTimeout>`` — Timeslice mode; both values in microseconds.
   - ``exclusive`` — Exclusive mode; no additional arguments.

   The valid range for all time values (us) is ``5000`` to ``100,000,000``.

   .. note::

      Scheduler mode support varies by GPU platform. On Intel Xe GPUs,
      ``timeslice`` is the most commonly supported mode. Not all modes may
      be available on every device.

.. option:: --memoryecc <0|1>

   Enable or disable memory ECC. ``0`` = disable, ``1`` = enable. A device reboot
   may be required for the change to take effect.

.. option:: --pciedowngrade <0|1>

   Enable or disable PCIe generation downgrade. ``0`` = disable, ``1`` = enable.

.. option:: --fanspeed <value>

   Set the fan speed percentage (``0``–``100``) or ``default`` for automatic control.

   Examples::

      --fanspeed 50
      --fanspeed 50%
      --fanspeed default
      --fanid 1 --fanspeed 50

.. option:: --fancurve <temp:speed,...>

   Set a fan curve in percent. Provide comma-separated ``temperature:speed`` pairs.

   Examples::

      --fancurve 40:25,60:50,80:90
      --fancurve 40:25%,60:50%,80:90%

.. option:: --fancurve-rpm <temp:rpm,...>

   Set a fan curve in RPM. Provide comma-separated ``temperature:rpm`` pairs.

   Examples::

      --fancurve-rpm 40:1200,70:2800
      --fancurve-rpm 40:1200rpm,70:2800rpm

.. option:: --fanid <id>

   Fan target: ``-1`` for all fans (default), or ``0``..``N-1`` for a specific fan.

.. option:: --reset

   Reset the device using Secondary Bus Reset (SBR).

   .. note::

      For Intel Max Series GPUs with SR-IOV enabled, ensure virtual functions are
      removed before performing a reset.

.. option:: --coldreset

   Perform a cold reset of the device via PCIe slot power cycle.

   .. important::

      The device **must** be addressed by PCI BDF address (e.g., ``0000:4d:00.0``),
      not by device ID. By default, xpu-smi will refuse to cold-reset if other
      PCI devices share the same PCIe slot. Use ``--force-reset-gpus`` to override
      this check and reset regardless of other devices on the slot.

.. option:: --ignore-gpu-user-processes

   Proceed with ``--coldreset`` even if processes currently have the GPU open.

.. option:: --force-reset-gpus

   Override the safety check and proceed with ``--coldreset`` even if other PCI
   devices share the PCIe slot.

.. option:: --clear-ras-errors

   Clear all RAS (Reliability, Availability, Serviceability) error counters for
   the device.

Examples
--------

Show current configuration for device 0:

.. code-block:: shell

   xpu-smi config --device 0

Set frequency range for device 0:

.. code-block:: shell

   xpu-smi config --device 0 --frequencyrange 300,1200

Set power limit to 250W for device 0:

.. code-block:: shell

   xpu-smi config --device 0 --powerlimit 250

Enable memory ECC on device 0:

.. code-block:: shell

   xpu-smi config --device 0 --memoryecc 1

Set standby mode to never for device 0:

.. code-block:: shell

   xpu-smi config --device 0 --standby never

Set fan speed to 75% for device 0:

.. code-block:: shell

   xpu-smi config --device 0 --fanspeed 75

Set a fan curve for device 0:

.. code-block:: shell

   xpu-smi config --device 0 --fancurve 40:25,60:50,80:90

Cold reset device at PCI BDF 0000:4d:00.0:

.. code-block:: shell

   xpu-smi config --device 0000:4d:00.0 --coldreset

Clear RAS error counters for device 0:

.. code-block:: shell

   xpu-smi config --device 0 --clear-ras-errors
