#!/usr/bin/env python3
"""TLS MQTT subscriber with ASCS telemetry validation."""

from __future__ import annotations

import argparse
import json
import os
import re
import ssl
import sys
import math
from typing import Any

import paho.mqtt.client as mqtt


BASE_TOPIC = re.compile(r"^[^#+/]+(?:/[^#+/]+)*$")
NODE_ID = re.compile(r"^[0-9A-Fa-f]{8}$")
IDENTIFIER = re.compile(r"^[A-Za-z0-9_.-]{1,64}$")


def valid_telemetry(value: Any) -> bool:
    if not isinstance(value, dict) or set(value) != {"node_id", "sensor_id", "timestamp_utc", "sequence_num", "readings"}:
        return False
    if not isinstance(value["node_id"], str) or not NODE_ID.fullmatch(value["node_id"]):
        return False
    if not isinstance(value["sensor_id"], str) or not IDENTIFIER.fullmatch(value["sensor_id"]):
        return False
    if (not isinstance(value["timestamp_utc"], int) or isinstance(value["timestamp_utc"], bool) or
            not 1 <= value["timestamp_utc"] <= 0xFFFFFFFF):
        return False
    if (not isinstance(value["sequence_num"], int) or isinstance(value["sequence_num"], bool) or
            not 1 <= value["sequence_num"] <= 0xFFFFFFFF):
        return False
    readings = value["readings"]
    return isinstance(readings, dict) and 0 < len(readings) <= 64 and all(
        isinstance(key, str) and IDENTIFIER.fullmatch(key) and
        isinstance(reading, (int, float)) and not isinstance(reading, bool) and math.isfinite(reading)
        for key, reading in readings.items()
    )


def valid_telemetry_topic(base_topic: str, topic: str, value: Any) -> bool:
    if not valid_telemetry(value):
        return False
    prefix = f"{base_topic}/sensor/"
    if not topic.startswith(prefix):
        return False
    parts = topic[len(prefix):].split("/")
    if len(parts) != 3 or not parts[0].isdigit() or parts[0].startswith("0"):
        return False
    service_id = int(parts[0])
    return (1 <= service_id <= 0xFFFFFFFF and parts[1] == value["node_id"].lower() and
            parts[2] == value["sensor_id"])


def arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Subscribe to and validate ASCS MQTT telemetry")
    parser.add_argument("--broker", required=True)
    parser.add_argument("--port", type=int, default=8883)
    parser.add_argument("--topic", default="akita/smartcity")
    parser.add_argument("--username", required=True)
    parser.add_argument("--ca-file")
    result = parser.parse_args()
    if not 1 <= result.port <= 65535:
        parser.error("--port must be between 1 and 65535")
    if not BASE_TOPIC.fullmatch(result.topic):
        parser.error("--topic must be a concrete MQTT base topic")
    if not os.environ.get("ASCS_MQTT_PASSWORD"):
        parser.error("ASCS_MQTT_PASSWORD must be set in the environment")
    return result


def main() -> int:
    args = arguments()
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.username_pw_set(args.username, os.environ["ASCS_MQTT_PASSWORD"])
    context = ssl.create_default_context(cafile=args.ca_file)
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    client.tls_set_context(context)

    def on_connect(current: mqtt.Client, _userdata: Any, _flags: mqtt.ConnectFlags,
                   reason_code: mqtt.ReasonCode, _properties: mqtt.Properties | None) -> None:
        if reason_code.is_failure:
            print(f"MQTT connection rejected: {reason_code}", file=sys.stderr)
            current.disconnect()
            return
        topic = f"{args.topic}/sensor/#"
        result, _message_id = current.subscribe(topic, qos=1)
        if result != mqtt.MQTT_ERR_SUCCESS:
            print(f"Subscription failed with code {result}", file=sys.stderr)
            current.disconnect()
        else:
            print(f"Subscribed to {topic}")

    def on_message(_client: mqtt.Client, _userdata: Any, message: mqtt.MQTTMessage) -> None:
        if len(message.payload) > 65536:
            print(f"Rejected oversized payload on {message.topic}", file=sys.stderr)
            return
        try:
            decoded = json.loads(message.payload.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as error:
            print(f"Rejected invalid JSON on {message.topic}: {error}", file=sys.stderr)
            return
        if not valid_telemetry_topic(args.topic, message.topic, decoded):
            print(f"Rejected invalid ASCS schema or topic identity on {message.topic}", file=sys.stderr)
            return
        print(json.dumps({"topic": message.topic, "payload": decoded}, separators=(",", ":")))

    client.on_connect = on_connect
    client.on_message = on_message
    client.reconnect_delay_set(min_delay=1, max_delay=30)
    try:
        client.connect(args.broker, args.port, keepalive=60)
        client.loop_forever(retry_first_connection=False)
    except (OSError, mqtt.MQTTException) as error:
        print(f"MQTT client failed: {error}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        client.disconnect()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
