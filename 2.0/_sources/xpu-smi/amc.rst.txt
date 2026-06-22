AMC
===

AMC (Add-in Card Management Controller) operations. Supports GPU resets via AMC,
reading real-time sensor data, and retrieving files from the GPU.

.. note::

   This command is supported on Linux only. Some operations may require
   root privileges.

Synopsis
--------

.. code-block:: text

   xpu-smi amc --gpureset
   xpu-smi amc --gpureset -y
   xpu-smi amc --gpureset -d [deviceId]
   xpu-smi amc --gpureset --device [deviceId]
   xpu-smi amc --gpureset --device [deviceId] -y
   xpu-smi amc --sensor --device [deviceId] -s [sensorId]
   xpu-smi amc --sensor --device [deviceId] -s [sensorId] -j
   xpu-smi amc --file --device [deviceId] --filetype [fileType] --filename [outputFile]

Options
-------

.. option:: -h, --help

   Print this help message and exit.

.. option:: -j, --json

   Print result in JSON format.

.. option:: -d <deviceId>, --device <deviceId>, --id <deviceId>

   Specify the device ID or PCI BDF address.

.. option:: --gpureset

   Reset GPU(s) via AMC. If ``--device`` is not specified, all GPUs are reset.

.. option:: --sensor

   Read real-time sensor readings from the AMC. Requires ``-s`` to specify
   which sensor to read.

.. option:: -s <sensorId>, --sensorid <sensorId>

   Specify the sensor ID to read. Valid sensor IDs:

   .. list-table::
      :widths: 10 90
      :header-rows: 1

      * - ID
        - Sensor
      * - 1
        - Temperature Sensor 0 from Add-In-Card
      * - 2
        - Temperature Sensor 1 from Add-In-Card
      * - 3
        - VR Voltage from Add-In-Card
      * - 4
        - VR Current from Add-In-Card
      * - 5
        - VR Power from Add-In-Card
      * - 6
        - VR Temperature Sensor
      * - 7
        - Total Board Power

.. option:: --file

   Read a file from the GPU via AMC. Requires ``--filetype`` and optionally
   ``--filename``.

.. option:: --filetype <id>

   Specify the type of file to retrieve. Valid file type IDs:

   .. list-table::
      :widths: 10 90
      :header-rows: 1

      * - ID
        - Description
      * - 0
        - GPU logs

.. option:: --filename <path>

   Output file path for the retrieved file. If not specified, defaults to
   ``amc_file_<timestamp>.txt``.

.. option:: -y, --yes

   Skip the confirmation prompt.

Examples
--------

Reset all GPUs via AMC (with confirmation):

.. code-block:: shell

   xpu-smi amc --gpureset

Reset GPU on device 0 via AMC without confirmation:

.. code-block:: shell

   xpu-smi amc --gpureset --device 0 -y

Read total board power sensor for device 0:

.. code-block:: shell

   xpu-smi amc --sensor --device 0 -s 7

Read temperature sensor 0 and output as JSON:

.. code-block:: shell

   xpu-smi amc --sensor --device 0 -s 1 -j

Retrieve GPU logs from device 0:

.. code-block:: shell

   xpu-smi amc --file --device 0 --filetype 0 --filename gpu_logs.txt
