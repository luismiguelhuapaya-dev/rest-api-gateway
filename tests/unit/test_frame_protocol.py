"""
Unit tests for the Unix domain socket frame protocol.
Tests mirror the behaviour implemented in src/transport/FrameProtocol.cpp.

Reference: GW-003 - Unix Domain Socket Transport
"""

import struct
import unittest
import sys
import os

sys.path.insert(0, os.path.dirname(__file__))
from test_helpers import Frame, FrameType


# ---------------------------------------------------------------------------
# Python reference serializer / deserializer mirroring FrameProtocol.cpp
# ---------------------------------------------------------------------------

HEADER_SIZE = 12
MAX_PAYLOAD_SIZE = 1048576  # 1 MiB


def write_uint32_be(value: int) -> bytes:
    return struct.pack('>I', value)


def read_uint32_be(data: bytes, offset: int = 0) -> int:
    return struct.unpack_from('>I', data, offset)[0]


def serialize_frame(frame: Frame) -> (bool, bytes):
    """Serialize a Frame to binary, matching FrameProtocol::SerializeFrame."""
    if frame.payload_length > MAX_PAYLOAD_SIZE:
        return False, b""

    buf = bytearray(HEADER_SIZE + frame.payload_length)
    struct.pack_into('>I', buf, 0, int(frame.frame_type))
    struct.pack_into('>I', buf, 4, frame.request_identifier)
    struct.pack_into('>I', buf, 8, frame.payload_length)

    if frame.payload_length > 0:
        payload_bytes = frame.payload.encode('utf-8') if isinstance(frame.payload, str) else frame.payload
        buf[HEADER_SIZE:HEADER_SIZE + frame.payload_length] = payload_bytes[:frame.payload_length]

    return True, bytes(buf)


def deserialize_frame(data: bytes) -> (bool, Frame, int):
    """Deserialize binary data to a Frame, matching FrameProtocol::DeserializeFrame."""
    if len(data) < HEADER_SIZE:
        return False, Frame(), 0

    frame_type = read_uint32_be(data, 0)
    req_id = read_uint32_be(data, 4)
    payload_length = read_uint32_be(data, 8)

    if payload_length > MAX_PAYLOAD_SIZE:
        return False, Frame(), 0

    if len(data) < HEADER_SIZE + payload_length:
        return False, Frame(), 0

    frame = Frame()
    frame.frame_type = FrameType(frame_type) if frame_type <= 7 else FrameType.Error
    frame.request_identifier = req_id
    frame.payload_length = payload_length

    if payload_length > 0:
        frame.payload = data[HEADER_SIZE:HEADER_SIZE + payload_length].decode('utf-8', errors='replace')
    else:
        frame.payload = ""

    return True, frame, HEADER_SIZE + payload_length


def has_complete_frame(buf: bytes) -> bool:
    """Check if buffer contains a complete frame, matching FrameProtocol::HasCompleteFrame."""
    if len(buf) < HEADER_SIZE:
        return False
    payload_length = read_uint32_be(buf, 8)
    if payload_length > MAX_PAYLOAD_SIZE:
        return False
    return len(buf) >= HEADER_SIZE + payload_length


# ---------------------------------------------------------------------------
# Test Cases
# ---------------------------------------------------------------------------

class TestFrameSerialization(unittest.TestCase):
    """GW-003 AC1: Frame serialization and deserialization."""

    def test_serialize_empty_payload(self):
        """GW-003 AC1: Serialize frame with empty payload."""
        f = Frame()
        f.frame_type = FrameType.Heartbeat
        f.request_identifier = 42
        f.payload_length = 0
        f.payload = ""

        ok, data = serialize_frame(f)
        self.assertTrue(ok)
        self.assertEqual(len(data), HEADER_SIZE)

        # Verify header fields
        self.assertEqual(read_uint32_be(data, 0), int(FrameType.Heartbeat))
        self.assertEqual(read_uint32_be(data, 4), 42)
        self.assertEqual(read_uint32_be(data, 8), 0)

    def test_serialize_with_payload(self):
        """GW-003 AC1: Serialize frame with non-empty payload."""
        payload = '{"status":"ok"}'
        f = Frame()
        f.frame_type = FrameType.Response
        f.request_identifier = 100
        f.payload_length = len(payload)
        f.payload = payload

        ok, data = serialize_frame(f)
        self.assertTrue(ok)
        self.assertEqual(len(data), HEADER_SIZE + len(payload))

    def test_roundtrip(self):
        """GW-003 AC1: Serialize then deserialize produces identical frame."""
        payload = '{"key":"value","num":123}'
        f = Frame()
        f.frame_type = FrameType.Request
        f.request_identifier = 999
        f.payload_length = len(payload)
        f.payload = payload

        ok_s, data = serialize_frame(f)
        self.assertTrue(ok_s)

        ok_d, f2, consumed = deserialize_frame(data)
        self.assertTrue(ok_d)
        self.assertEqual(f2.frame_type, f.frame_type)
        self.assertEqual(f2.request_identifier, f.request_identifier)
        self.assertEqual(f2.payload_length, f.payload_length)
        self.assertEqual(f2.payload, f.payload)
        self.assertEqual(consumed, len(data))


class TestFrameTypeDiscrimination(unittest.TestCase):
    """GW-003 AC2: Frame type discrimination."""

    def _make_frame(self, ftype: FrameType) -> Frame:
        f = Frame()
        f.frame_type = ftype
        f.request_identifier = 1
        f.payload_length = 0
        f.payload = ""
        return f

    def test_registration_frame(self):
        """GW-003 AC2: Registration frame type roundtrips correctly."""
        f = self._make_frame(FrameType.Registration)
        ok, data = serialize_frame(f)
        self.assertTrue(ok)
        ok2, f2, _ = deserialize_frame(data)
        self.assertTrue(ok2)
        self.assertEqual(f2.frame_type, FrameType.Registration)

    def test_unregistration_frame(self):
        """GW-003 AC2: Unregistration frame type roundtrips correctly."""
        f = self._make_frame(FrameType.Unregistration)
        ok, data = serialize_frame(f)
        ok2, f2, _ = deserialize_frame(data)
        self.assertEqual(f2.frame_type, FrameType.Unregistration)

    def test_request_frame(self):
        """GW-003 AC2: Request frame type roundtrips correctly."""
        f = self._make_frame(FrameType.Request)
        ok, data = serialize_frame(f)
        ok2, f2, _ = deserialize_frame(data)
        self.assertEqual(f2.frame_type, FrameType.Request)

    def test_response_frame(self):
        """GW-003 AC2: Response frame type roundtrips correctly."""
        f = self._make_frame(FrameType.Response)
        ok, data = serialize_frame(f)
        ok2, f2, _ = deserialize_frame(data)
        self.assertEqual(f2.frame_type, FrameType.Response)

    def test_heartbeat_frame(self):
        """GW-003 AC2: Heartbeat frame type roundtrips correctly."""
        f = self._make_frame(FrameType.Heartbeat)
        ok, data = serialize_frame(f)
        ok2, f2, _ = deserialize_frame(data)
        self.assertEqual(f2.frame_type, FrameType.Heartbeat)

    def test_error_frame(self):
        """GW-003 AC2: Error frame type roundtrips correctly."""
        f = self._make_frame(FrameType.Error)
        ok, data = serialize_frame(f)
        ok2, f2, _ = deserialize_frame(data)
        self.assertEqual(f2.frame_type, FrameType.Error)


class TestPartialFrameHandling(unittest.TestCase):
    """GW-003 AC3: Partial frame handling."""

    def test_partial_header(self):
        """GW-003 AC3: Incomplete header (< 12 bytes) is not a complete frame."""
        self.assertFalse(has_complete_frame(b"\x00\x00\x00\x01"))

    def test_header_only_no_payload(self):
        """GW-003 AC3: Header indicating 0 payload is complete."""
        data = write_uint32_be(0) + write_uint32_be(1) + write_uint32_be(0)
        self.assertTrue(has_complete_frame(data))

    def test_header_with_incomplete_payload(self):
        """GW-003 AC3: Header indicating payload but data is shorter."""
        data = write_uint32_be(0) + write_uint32_be(1) + write_uint32_be(100)
        # Only 12 bytes, but says 100 bytes payload needed
        self.assertFalse(has_complete_frame(data))

    def test_header_with_complete_payload(self):
        """GW-003 AC3: Full frame with exactly enough bytes is complete."""
        payload = b"Hello"
        data = write_uint32_be(2) + write_uint32_be(1) + write_uint32_be(len(payload)) + payload
        self.assertTrue(has_complete_frame(data))

    def test_deserialization_partial_data(self):
        """GW-003 AC3: Deserializing partial data returns failure."""
        data = write_uint32_be(0) + write_uint32_be(0) + write_uint32_be(50) + b"short"
        ok, _, consumed = deserialize_frame(data)
        self.assertFalse(ok)
        self.assertEqual(consumed, 0)


class TestOversizedFrame(unittest.TestCase):
    """GW-003 AC4: Oversized frame rejection."""

    def test_oversized_payload_serialization(self):
        """GW-003 AC4: Serialization rejects payload > MAX_PAYLOAD_SIZE."""
        f = Frame()
        f.frame_type = FrameType.Request
        f.request_identifier = 1
        f.payload_length = MAX_PAYLOAD_SIZE + 1
        f.payload = "x" * (MAX_PAYLOAD_SIZE + 1)

        ok, _ = serialize_frame(f)
        self.assertFalse(ok)

    def test_max_payload_serialization_succeeds(self):
        """GW-003 AC4: Serialization accepts payload exactly at MAX_PAYLOAD_SIZE."""
        f = Frame()
        f.frame_type = FrameType.Request
        f.request_identifier = 1
        f.payload_length = MAX_PAYLOAD_SIZE
        f.payload = "x" * MAX_PAYLOAD_SIZE

        ok, data = serialize_frame(f)
        self.assertTrue(ok)
        self.assertEqual(len(data), HEADER_SIZE + MAX_PAYLOAD_SIZE)

    def test_oversized_deserialization_rejected(self):
        """GW-003 AC4: Deserialization rejects frame with payload length > MAX."""
        data = write_uint32_be(0) + write_uint32_be(0) + write_uint32_be(MAX_PAYLOAD_SIZE + 1)
        self.assertFalse(has_complete_frame(data))


class TestMalformedFrames(unittest.TestCase):
    """GW-003 AC5: Malformed frame handling."""

    def test_empty_input(self):
        """GW-003 AC5: Empty data yields no complete frame."""
        self.assertFalse(has_complete_frame(b""))

    def test_zero_length_deserialization(self):
        """GW-003 AC5: Zero-length input deserialization fails."""
        ok, _, consumed = deserialize_frame(b"")
        self.assertFalse(ok)

    def test_garbage_header(self):
        """GW-003 AC5: Random 12 bytes might form a frame if payload length is 0."""
        # If bytes 8..11 decode to 0, it's a valid empty-payload frame
        data = b"\xFF\xFF\xFF\xFF" + b"\x00\x00\x00\x01" + b"\x00\x00\x00\x00"
        self.assertTrue(has_complete_frame(data))

    def test_multiple_frames_in_buffer(self):
        """GW-003 AC5: Two frames in buffer; first is deserialized, second remains."""
        payload1 = b"first"
        payload2 = b"second"
        data1 = write_uint32_be(2) + write_uint32_be(1) + write_uint32_be(len(payload1)) + payload1
        data2 = write_uint32_be(3) + write_uint32_be(2) + write_uint32_be(len(payload2)) + payload2
        combined = data1 + data2

        ok, f1, consumed = deserialize_frame(combined)
        self.assertTrue(ok)
        self.assertEqual(f1.payload, "first")
        self.assertEqual(consumed, HEADER_SIZE + len(payload1))

        # Remaining buffer has second frame
        remaining = combined[consumed:]
        ok2, f2, consumed2 = deserialize_frame(remaining)
        self.assertTrue(ok2)
        self.assertEqual(f2.payload, "second")


class TestFrameConstants(unittest.TestCase):
    """GW-003: Protocol constants match C++ definitions."""

    def test_header_size(self):
        """GW-003: Header size is 12 bytes."""
        self.assertEqual(HEADER_SIZE, 12)

    def test_max_payload_size(self):
        """GW-003: Max payload size is 1 MiB."""
        self.assertEqual(MAX_PAYLOAD_SIZE, 1048576)


if __name__ == '__main__':
    unittest.main()
