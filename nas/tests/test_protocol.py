import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "gateway"))

from protocol import FrameParser, crc16_ccitt, encode_frame, parse_frame  # noqa: E402


class ProtocolTests(unittest.TestCase):
    def test_crc_known_vector(self):
        self.assertEqual(crc16_ccitt(b"123456789"), 0x29B1)

    def test_round_trip(self):
        payload = '{"seq":1,"state":"NORMAL","t":[25.0,26.5]}'
        frame = encode_frame(payload)
        self.assertTrue(frame.startswith(b"$"))
        self.assertTrue(frame.endswith(b"\n"))
        parsed = parse_frame(frame)
        self.assertIsNotNone(parsed)
        assert parsed is not None
        self.assertTrue(parsed.crc_ok)
        self.assertEqual(parsed.payload, payload)
        self.assertEqual(parsed.data["state"], "NORMAL")
        self.assertEqual(parsed.data["t"], [25.0, 26.5])

    def test_bad_crc_is_rejected(self):
        frame = encode_frame('{"seq":2}')[:-6] + b"*0000\n"
        parsed = parse_frame(frame)
        self.assertIsNotNone(parsed)
        assert parsed is not None
        self.assertFalse(parsed.crc_ok)
        self.assertIsNone(parsed.data)
        self.assertEqual(parsed.error, "crc_mismatch")

    def test_parser_handles_chunks_and_noise(self):
        frames = [encode_frame('{"seq":%d}' % i) for i in range(4)]
        blob = b"boot noise\r\n" + b"".join(frames)
        parser = FrameParser()
        parsed = []
        for index in range(0, len(blob), 3):
            parsed.extend(parser.feed(blob[index : index + 3]))
        self.assertEqual([frame.data["seq"] for frame in parsed], [0, 1, 2, 3])
        self.assertTrue(all(frame.crc_ok for frame in parsed))

    def test_non_json_payload_is_valid_crc(self):
        parsed = parse_frame(encode_frame("PING"))
        self.assertIsNotNone(parsed)
        assert parsed is not None
        self.assertTrue(parsed.crc_ok)
        self.assertIsNone(parsed.data)
        self.assertEqual(parsed.error, "not_json")

    def test_oversized_garbage_does_not_grow_forever(self):
        parser = FrameParser(max_buffer=32)
        parser.feed(b"x" * 4096)
        self.assertEqual(len(parser._buffer), 0)


if __name__ == "__main__":
    unittest.main()
