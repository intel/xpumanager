#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

"""
gRPC client wrapper for the xpuinfo exporter local socket.

Provides a high-level Python interface to the WatchDeviceHealth streaming
RPC, with connection management, timeout support, and clean shutdown.
"""

import logging
from typing import Iterator, Optional

import grpc

# Generated stubs live alongside the exporter PR; we also ship a copy in
# this package so the validation tool is self-contained.
from . import deviceinfo_pb2 as pb2
from . import deviceinfo_pb2_grpc as pb2_grpc
from .config import SocketConfig

log = logging.getLogger(__name__)


class GrpcClient:
    """Thin wrapper around the DeviceInfo gRPC stub."""

    def __init__(self, config: Optional[SocketConfig] = None) -> None:
        self._cfg = config or SocketConfig()
        self._channel: Optional[grpc.Channel] = None
        self._stub: Optional[pb2_grpc.DeviceInfoStub] = None

    # ------------------------------------------------------------------
    # Connection management
    # ------------------------------------------------------------------

    def connect(self) -> None:
        """Open the gRPC channel to the Unix socket."""
        target = self._cfg.target
        log.info("Connecting to %s", target)
        self._channel = grpc.insecure_channel(target)
        self._stub = pb2_grpc.DeviceInfoStub(self._channel)

        # Verify the channel can connect within the timeout.
        try:
            grpc.channel_ready_future(self._channel).result(
                timeout=self._cfg.connect_timeout_s
            )
        except grpc.FutureTimeoutError:
            self.close()
            raise ConnectionError(
                f"Timed out connecting to {target} after {self._cfg.connect_timeout_s}s"
            ) from None
        log.info("Connected to %s", target)

    def close(self) -> None:
        """Close the gRPC channel."""
        if self._channel is not None:
            self._channel.close()
            self._channel = None
            self._stub = None

    def __enter__(self) -> "GrpcClient":
        self.connect()
        return self

    def __exit__(self, *exc) -> None:
        self.close()

    # ------------------------------------------------------------------
    # Streaming RPCs
    # ------------------------------------------------------------------

    def watch_device_health(
        self,
        timeout: Optional[float] = None,
    ) -> Iterator[pb2.DeviceHealthResponse]:
        """Start the WatchDeviceHealth server-stream.

        Yields ``DeviceHealthResponse`` messages until the stream ends,
        the *timeout* expires, or the caller breaks out of the loop.
        """
        if self._stub is None:
            raise RuntimeError("Not connected – call connect() first")

        effective_timeout = timeout or self._cfg.stream_timeout_s
        request = pb2.WatchDeviceHealthRequest()
        stream = self._stub.WatchDeviceHealth(request, timeout=effective_timeout)
        yield from stream

    # ------------------------------------------------------------------
    # One-shot helpers
    # ------------------------------------------------------------------

    def snapshot(self, timeout: Optional[float] = None) -> pb2.DeviceHealthResponse:
        """Return the first DeviceHealthResponse from the stream.

        Useful for a quick device-inventory check without long-running
        streaming.
        """
        if self._stub is None:
            raise RuntimeError("Not connected – call connect() first")
        request = pb2.WatchDeviceHealthRequest()
        stream = self._stub.WatchDeviceHealth(
            request, timeout=timeout or self._cfg.connect_timeout_s
        )
        for response in stream:
            stream.cancel()
            return response
        raise RuntimeError("Stream ended with no responses")
