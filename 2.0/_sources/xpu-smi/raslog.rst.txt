RAS Log
=======

Collect GPU RAS (Reliability, Availability, Serviceability) hardware error records.
By default the read is non-destructive (peek): records remain in the hardware error
buffer and can be read again by subsequent calls.  Use ``--drain`` to consume records
from the buffer; a ``--file`` destination is required in that mode to avoid data loss.

.. note::

   This command is supported on Linux only.

Synopsis
--------

.. code-block:: text

   xpu-smi raslog [-t cper]
   xpu-smi raslog [-t cper] -j
   xpu-smi raslog [-t cper] -f [fileName]
   xpu-smi raslog [-t cper] -f [fileName] --drain
   xpu-smi raslog [-t cper] [-f fileName] --instance [name]
   xpu-smi raslog [-t cper] [-f fileName] --buffer-size-kb [kb]

Options
-------

.. option:: -h, --help

   Print this help message and exit.

.. option:: -j, --json

   Print status in JSON format. The raw CPER binary is still written to the
   file specified by ``--file``; the JSON output describes the collection
   result and per-record metadata only.

.. option:: -t <type>, --type <type>

   The hardware log type to collect. Currently the only supported type is
   ``cper`` (Common Platform Error Record), which is also the default.

.. option:: -f <fileName>, --file <fileName>

   The file to write the raw hardware log data into. Optional in the default
   peek mode; required with ``--drain`` since consumed records cannot be
   recovered. When provided, the path is validated before any data is read.

.. option:: --drain

   Read and consume records from the hardware error buffer (destructive mode).
   Records will not be returned by subsequent reads. ``--file`` is required
   in this mode to ensure no records are lost.

.. option:: --instance <name>

   Collect from the named hardware trace instance instead of the global error
   buffer. Requires driver support; an error is returned if named instances
   are not available on the current system. Each invocation creates a fresh
   collection instance; if an instance with the same name is already active,
   the command fails with a ``HANDLE_OBJECT_IN_USE`` error.

.. option:: --buffer-size-kb <kb>

   Request a specific total error buffer size in kilobytes across all
   per-CPU buffers. The driver splits and rounds the value as needed.
   If omitted, the driver default size is used.

Output
------

Without ``--json``, a single status line is printed. The exact wording depends on
whether ``--file`` and ``--drain`` were supplied:

.. code-block:: text

   # peek (default), no --file
   CPER buffer (4096 bytes, 3 records) collected (records not consumed).

   # peek with --file
   CPER buffer (4096 bytes, 3 records) copied (records not consumed) to file: cper.bin

   # --drain with --file
   CPER buffer (4096 bytes, 3 records) written to file: cper.bin

With ``--json``, a JSON object is printed to stdout:

.. code-block:: json

   {
       "status": "OK",
       "bytes": 4096,
       "buffer_bytes": 4096,
       "records": 3,
       "file": "cper.bin",
       "cper_records": [
           {
               "offset": 0,
               "length": 1400,
               "bdf": "0000:4d:00.0",
               "uuid": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
               "timestamp_ns": 1749123456789012,
               "record_type": "ERROR_CORRECTED"
           }
       ]
   }

.. list-table:: JSON fields
   :widths: 20 80
   :header-rows: 1

   * - Field
     - Description
   * - ``status``
     - ``"OK"`` on a complete read; ``"PARTIAL"`` if some records were too
       large to fit in the buffer and were dropped.
   * - ``bytes``
     - Bytes written to the output file. Zero when ``--file`` is not provided.
   * - ``buffer_bytes``
     - Bytes collected from the hardware error buffer, regardless of whether a
       file was written. Equal to ``bytes`` when a file was written.
   * - ``records``
     - Number of error records collected.
   * - ``file``
     - Path of the output file. Present only when ``--file`` was provided.
   * - ``warning``
     - Present only when ``status`` is ``"PARTIAL"``. Describes the reason
       records were dropped.
   * - ``cper_records``
     - Array of per-record metadata objects (see below).

.. list-table:: Per-record metadata (``cper_records`` entries)
   :widths: 20 80
   :header-rows: 1

   * - Field
     - Description
   * - ``offset``
     - Byte offset of this record within the collected CPER blob. When ``--file``
       is provided this matches the byte offset within the output file.
   * - ``length``
     - Length of this record in bytes.
   * - ``bdf``
     - PCI Bus/Device/Function address of the GPU that reported the error,
       in ``domain:bus:device.function`` format.
   * - ``uuid``
     - UUID identifying the GPU device that reported the error.
   * - ``timestamp_ns``
     - Timestamp of the error event in nanoseconds. The reference point is
       implementation-defined and only consistent across records of the same
       info log instance.
   * - ``record_type``
     - Severity of the record: ``UNKNOWN``, ``INFORMATIONAL``,
       ``ERROR_CORRECTED``, ``ERROR_RECOVERABLE``, or ``ERROR_FATAL``.

Error output
------------

On failure, a JSON error object is printed to stdout when ``--json`` is active:

.. code-block:: json

   {
       "ze_result": 2013265923,
       "error": "CPER hardware log is not supported on this system or build"
   }

.. list-table:: Error JSON fields
   :widths: 20 80
   :header-rows: 1

   * - Field
     - Description
   * - ``ze_result``
     - The ``ze_result_t`` error code as a decimal integer.  Common values:
       ``2013265923`` (``ZE_RESULT_ERROR_UNSUPPORTED_FEATURE``),
       ``2147483646`` (``ZE_RESULT_ERROR_UNKNOWN``).
   * - ``error``
     - Human-readable description of the failure.
   * - ``file``
     - Present only for file I/O errors; path of the output file that could
       not be opened or written.

Examples
--------

Peek at hardware error records (non-destructive, no file required):

.. code-block:: shell

   xpu-smi raslog

Peek and print a JSON summary:

.. code-block:: shell

   xpu-smi raslog -j

Peek and save the raw binary to a file:

.. code-block:: shell

   xpu-smi raslog -f cper.bin

Drain (consume) records into a file:

.. code-block:: shell

   xpu-smi raslog --drain -f cper.bin

Drain and print a JSON summary:

.. code-block:: shell

   xpu-smi raslog --drain -f cper.bin -j

Collect from a named trace instance:

.. code-block:: shell

   xpu-smi raslog -f cper.bin --instance xpu-collection

Collect with a specific buffer size:

.. code-block:: shell

   xpu-smi raslog -f cper.bin --buffer-size-kb 8192
