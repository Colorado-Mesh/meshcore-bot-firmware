import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("artifacts", ROOT / "scripts" / "check-release-artifacts.py")
artifacts = importlib.util.module_from_spec(spec)
spec.loader.exec_module(artifacts)


class ArtifactChecks(unittest.TestCase):
    def test_all_targets_required(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            (directory / "board-v0.2.0.bin").write_bytes(b"firmware")
            artifacts.check_artifacts(directory, ["board"])
            with self.assertRaises(ValueError):
                artifacts.check_artifacts(directory, ["board", "missing"])

    def test_installable_assets_required(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            firmware = directory / "board-v0.2.0.bin"
            firmware.write_bytes(b"firmware")
            with self.assertRaises(ValueError):
                artifacts.check_artifacts(directory, ["board"], {"board": ["-merged.bin", ".bin"]})
            (directory / "board-v0.2.0-merged.bin").write_bytes(b"merged")
            artifacts.check_artifacts(directory, ["board"], {"board": ["-merged.bin", ".bin"]})
            firmware.write_bytes(b"")
            with self.assertRaises(ValueError):
                artifacts.check_artifacts(directory, ["board"])

    def test_stale_version_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            (directory / "board-v0.2.0-d9296435.bin").write_bytes(b"firmware")
            artifacts.check_artifacts(directory, ["board"], version="v0.2.0")
            with self.assertRaises(ValueError):
                artifacts.check_artifacts(directory, ["board"], version="v0.3.0")


if __name__ == "__main__":
    unittest.main()
