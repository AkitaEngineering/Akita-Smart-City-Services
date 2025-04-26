#!/usr/bin/env python3

"""
Simple MQTT Test Subscriber for Akita Smart City Services (ASCS)

Subscribes to the ASCS MQTT topics and prints received messages.
Helpful for verifying that Gateway nodes are publishing data correctly.
"""

import paho.mqtt.client as mqtt
import json
import argparse
import sys
import os
import time

# --- Configuration ---
DEFAULT_BROKER = "localhost"
DEFAULT_PORT = 1883
DEFAULT_BASE_TOPIC = "akita/smartcity" # Should match gateway's 'mqtt_topic' config

# --- Argument Parsing ---
parser = argparse.ArgumentParser(description="ASCS MQTT Test Subscriber")
parser.add_argument("-b", "--broker", default=DEFAULT_BROKER, help=f"MQTT broker address (default: {DEFAULT_BROKER})")
parser.add_argument("-p", "--port", type=int, default=DEFAULT_PORT, help=f"MQTT broker port (default: {DEFAULT_PORT})")
parser.add_argument("-t", "--topic", default=DEFAULT_BASE_TOPIC, help=f"Base MQTT topic to subscribe to (default: {DEFAULT_BASE_TOPIC})")
parser.add_argument("-u", "--username", default=None, help="MQTT username (optional)")
parser.add_argument("-P", "--password", default=None, help="MQTT password (optional)")
parser.add_argument("-v", "--verbose", action="store_true", help="Print verbose output")

args = parser.parse_args()

# --- MQTT Callback Functions ---

def on_connect(client, userdata, flags, rc):
    """Callback when the client connects to the MQTT broker."""
    if rc == 0:
        print(f"Connected successfully to MQTT Broker: {args.broker}:{args.port}")
        # Subscribe to the base topic and all subtopics (# wildcard)
        subscribe_topic = f"{args.topic}/#"
        client.subscribe(subscribe_topic)
        print(f"Subscribed to topic: {subscribe_topic}")
    else:
        print(f"Connection failed with error code: {rc}")
        # Specific error codes can be checked here (e.g., bad credentials, broker unavailable)
        if rc == 5:
            print("Authentication error. Check username/password.")
        sys.exit(1) # Exit if connection fails

def on_disconnect(client, userdata, rc):
    """Callback when the client disconnects."""
    print(f"Disconnected from MQTT Broker (rc: {rc}).")
    if rc != 0:
        print("Unexpected disconnection. Attempting to reconnect...")
        # Basic reconnect logic (Paho library handles some reconnection automatically)
        # For robust reconnect, more sophisticated logic might be needed

def on_message(client, userdata, msg):
    """Callback when a message is received."""
    print("-" * 40)
    print(f"Received message on topic: {msg.topic}")
    try:
        payload_str = msg.payload.decode("utf-8")
        # Attempt to parse payload as JSON for pretty printing
        try:
            payload_json = json.loads(payload_str)
            print("Payload (JSON):")
            print(json.dumps(payload_json, indent=2))
        except json.JSONDecodeError:
            print("Payload (Raw):")
            print(payload_str)

    except Exception as e:
        print(f"Error processing message payload: {e}")
        print(f"Raw payload bytes: {msg.payload}")
    print("-" * 40)

# --- Main Execution ---

# Create MQTT client instance
client = mqtt.Client()

# Assign callback functions
client.on_connect = on_connect
client.on_disconnect = on_disconnect
client.on_message = on_message

# Set username and password if provided
if args.username:
    client.username_pw_set(args.username, args.password)

print(f"Attempting to connect to {args.broker}:{args.port}...")

# Connect to the broker
try:
    client.connect(args.broker, args.port, 60) # 60-second keepalive
except Exception as e:
    print(f"Error connecting to MQTT broker: {e}")
    sys.exit(1)

# Start the MQTT network loop in a non-blocking way
client.loop_start()

# Keep the script running until interrupted
try:
    while True:
        time.sleep(1) # Keep main thread alive
except KeyboardInterrupt:
    print("\nDisconnecting...")
    client.loop_stop()
    client.disconnect()
    print("Exited.")

