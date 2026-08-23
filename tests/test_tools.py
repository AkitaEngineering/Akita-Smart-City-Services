from __future__ import annotations

import importlib.util
import subprocess
import sys
import tempfile
import unittest
import re
from pathlib import Path


REPOSITORY = Path(__file__).resolve().parents[1]


def load_module(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Could not load {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


installer = load_module("install_meshtastic_module", REPOSITORY / "tools" / "install_meshtastic_module.py")
subscriber = load_module("mqtt_test_subscriber", REPOSITORY / "tools" / "mqtt_test_subscriber.py")


class InstallerTests(unittest.TestCase):
    def test_meshtastic_pin_matches_ci(self) -> None:
        version = (REPOSITORY / "meshtastic.version").read_text(encoding="utf-8").strip()
        self.assertRegex(version, re.compile(r"^[0-9a-f]{40}$"))
        workflow = (REPOSITORY / ".github" / "workflows" / "ci.yml").read_text(encoding="utf-8")
        self.assertIn(f"ref: {version}", workflow)

    def test_generated_protocol_contains_relay_origin_field(self) -> None:
        schema = (REPOSITORY / "proto" / "SmartCity.proto").read_text(encoding="utf-8")
        generated = (REPOSITORY / "src" / "generated_proto" / "SmartCity.pb.h").read_text(encoding="utf-8")
        self.assertIn("uint32 origin_node = 5;", schema)
        self.assertIn("uint32_t origin_node;", generated)
        self.assertIn("akita_smart_city_SensorData_origin_node_tag 5", generated)
        self.assertIn("uint32 expires_at_utc = 7;", schema)
        self.assertIn("uint32_t expires_at_utc;", generated)

    def test_insert_once_is_idempotent_and_requires_anchor(self) -> None:
        block = "inserted\n"
        first = installer.insert_once("before\nanchor\nafter\n", "anchor", block, after=True)
        self.assertEqual(installer.insert_once(first, "anchor", block, after=True), first)
        with self.assertRaises(RuntimeError):
            installer.insert_once("missing", "anchor", block, after=True)

    def test_installer_retains_all_role_environments(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            firmware = Path(temporary)
            modules = firmware / "src" / "modules"
            modules.mkdir(parents=True)
            (modules / "Modules.cpp").write_text(
                '#include "configuration.h"\n\nvoid setupModules()\n{\n}\n', encoding="utf-8"
            )
            (firmware / "platformio.ini").write_text("[platformio]\n", encoding="utf-8")

            commands = [
                ("sensor", "--sensor", "bme280"),
                ("aggregator",),
                ("gateway",),
            ]
            for role in commands:
                subprocess.run(
                    [sys.executable, str(REPOSITORY / "tools" / "install_meshtastic_module.py"), str(firmware),
                     "--base-env", "tbeam", "--role", role[0], *role[1:]],
                    check=True,
                    capture_output=True,
                    text=True,
                )

            variants = firmware / "variants" / "ascs"
            self.assertEqual(
                {path.name for path in variants.glob("*.ini")},
                {"ascs-tbeam-sensor.ini", "ascs-tbeam-aggregator.ini", "ascs-tbeam-gateway.ini"},
            )
            for environment_file in variants.glob("*.ini"):
                self.assertIn("-fno-strict-aliasing", environment_file.read_text(encoding="utf-8"))
            modules_text = (modules / "Modules.cpp").read_text(encoding="utf-8")
            self.assertEqual(modules_text.count('#include "modules/ascs/AkitaSmartCityServices.h"'), 1)
            self.assertEqual(modules_text.count("new AkitaSmartCityServices();"), 1)

    def test_installer_rejects_environment_injection_before_writing(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            firmware = Path(temporary)
            modules = firmware / "src" / "modules"
            modules.mkdir(parents=True)
            (modules / "Modules.cpp").write_text(
                '#include "configuration.h"\n\nvoid setupModules()\n{\n}\n', encoding="utf-8"
            )
            (firmware / "platformio.ini").write_text("[platformio]\n", encoding="utf-8")
            result = subprocess.run(
                [sys.executable, str(REPOSITORY / "tools" / "install_meshtastic_module.py"), str(firmware),
                 "--base-env", "tbeam\n-DINJECTED", "--role", "aggregator"],
                capture_output=True,
                text=True,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse((modules / "ascs").exists())


class TelemetryValidatorTests(unittest.TestCase):
    def valid_payload(self) -> dict[str, object]:
        return {
            "node_id": "a1b2c3d4",
            "sensor_id": "BME280-1",
            "timestamp_utc": 1,
            "sequence_num": 2,
            "readings": {"temperature_c": 21.5},
        }

    def test_accepts_production_schema(self) -> None:
        self.assertTrue(subscriber.valid_telemetry(self.valid_payload()))

    def test_rejects_non_finite_boolean_and_unsafe_values(self) -> None:
        for invalid_reading in (float("nan"), float("inf"), True):
            payload = self.valid_payload()
            payload["readings"] = {"temperature_c": invalid_reading}
            self.assertFalse(subscriber.valid_telemetry(payload))

        payload = self.valid_payload()
        payload["sensor_id"] = "bad/topic"
        self.assertFalse(subscriber.valid_telemetry(payload))

    def test_rejects_json_extensions_and_out_of_range_counters(self) -> None:
        payload = self.valid_payload()
        payload["unexpected"] = True
        self.assertFalse(subscriber.valid_telemetry(payload))

        payload = self.valid_payload()
        payload["sequence_num"] = 0x1_0000_0000
        self.assertFalse(subscriber.valid_telemetry(payload))


if __name__ == "__main__":
    unittest.main()
