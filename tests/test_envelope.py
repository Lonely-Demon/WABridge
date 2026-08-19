import struct
import unittest
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).parents[1] / "shared" / "protocol"))
from envelope import Envelope, ProtocolError, decode, encode, HEADER, HEADER_SIZE  # noqa: E402


class EnvelopeTests(unittest.TestCase):
    def test_round_trip(self):
        original = Envelope(channel=1, kind=1, flags=1, request_id=42, payload=b"hello")
        self.assertEqual(decode(encode(original)), original)

    def test_rejects_truncated_header(self):
        with self.assertRaises(ProtocolError):
            decode(b"WB")

    def test_rejects_bad_magic(self):
        frame = bytearray(encode(Envelope(1, 1, 1, 1, b"x")))
        frame[0:2] = b"XX"
        with self.assertRaises(ProtocolError):
            decode(bytes(frame))

    def test_rejects_unknown_channel(self):
        frame = HEADER.pack(0x5742, 1, 99, 1, 1, 1, 1) + b"x"
        with self.assertRaises(ProtocolError):
            decode(frame)

    def test_rejects_zero_request_id(self):
        frame = HEADER.pack(0x5742, 1, 1, 1, 1, 0, 1) + b"x"
        with self.assertRaises(ProtocolError):
            decode(frame)

    def test_rejects_oversized_payload_before_allocation(self):
        frame = HEADER.pack(0x5742, 1, 1, 1, 1, 1, 65 * 1024)
        with self.assertRaises(ProtocolError):
            decode(frame)

    def test_rejects_declared_length_mismatch(self):
        frame = HEADER.pack(0x5742, 1, 1, 1, 1, 1, 2) + b"x"
        with self.assertRaises(ProtocolError):
            decode(frame)


if __name__ == "__main__":
    unittest.main()
