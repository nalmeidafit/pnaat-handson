#!/usr/bin/env python3
"""Build artifact server + MQTT OTA trigger for development.

Run from the esp32 project root after `idf.py build`:
    python tools/ota_deploy.py

The script:
1. serves build/pnaat_esteira.bin over a temporary HTTP server;
2. publishes {"cmd":"OTA_UPDATE","url":"http://..."};
3. prints pnaat/ota/status messages;
4. exits after success/failure/timeout.
"""

from __future__ import annotations

import http.server
import json
import socket
import sys
import threading
import time
from pathlib import Path

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("Instale a dependência: python -m pip install -r tools/requirements.txt")
    raise SystemExit(2)

PROJECT_DIR = Path(__file__).resolve().parents[1]
BUILD_DIR = PROJECT_DIR / "build"
FIRMWARE = BUILD_DIR / "pnaat_esteira.bin"
CONFIG_FILE = Path(__file__).with_name("ota_config.json")
EXAMPLE_FILE = Path(__file__).with_name("ota_config.example.json")


def load_config():
    if not CONFIG_FILE.exists():
        print(f"Crie {CONFIG_FILE.name} a partir de {EXAMPLE_FILE.name} e informe o broker/credenciais.")
        raise SystemExit(2)
    return json.loads(CONFIG_FILE.read_text(encoding="utf-8"))


def local_ip_for(remote_host: str, remote_port: int) -> str:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        sock.connect((remote_host, remote_port))
        return sock.getsockname()[0]
    finally:
        sock.close()


class FirmwareHandler(http.server.SimpleHTTPRequestHandler):
    completed = threading.Event()

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(BUILD_DIR), **kwargs)

    def log_message(self, fmt, *args):
        print("[HTTP] " + fmt % args)

    def do_GET(self):
        try:
            super().do_GET()
        finally:
            if self.path.split("?", 1)[0].endswith("/pnaat_esteira.bin"):
                self.completed.set()


def make_client(config, done_event):
    try:
        client = mqtt.Client(
            mqtt.CallbackAPIVersion.VERSION2,
            client_id="pnaat-ota-deploy"
        )
    except Exception:
        client = mqtt.Client(client_id="pnaat-ota-deploy")

    username = config.get("username", "")
    password = config.get("password", "")

    if username:
        client.username_pw_set(username, password)

    prefix = config.get("topic_prefix", "pnaat")
    status_topic = f"{prefix}/ota/status"

    def on_connect(c, userdata, flags, reason_code, properties=None):
        print(f"[MQTT] conectado: {reason_code}")
        c.subscribe(status_topic, qos=1)

    def on_message(c, userdata, msg):
        text = msg.payload.decode("utf-8", errors="replace")
        print(f"[MQTT] {msg.topic}: {text}")
        try:
            data = json.loads(text)
            status = data.get("status", "")
            if status in {"SUCCESS_REBOOTING", "FAILED", "START_FAILED"}:
                done_event.set()
        except Exception:
            pass

    client.on_connect = on_connect
    client.on_message = on_message
    return client


def main():
    if not FIRMWARE.exists():
        print(f"Firmware não encontrado: {FIRMWARE}")
        print("Execute primeiro: idf.py build")
        return 2

    config = load_config()
    broker = config["broker"]
    mqtt_port = int(config.get("mqtt_port", 1883))
    http_port = int(config.get("http_port", 8070))
    timeout = int(config.get("timeout_seconds", 180))
    prefix = config.get("topic_prefix", "pnaat")

    host_ip = local_ip_for(broker, mqtt_port)
    url = f"http://{host_ip}:{http_port}/{FIRMWARE.name}"

    print(f"[OTA] firmware: {FIRMWARE}")
    print(f"[OTA] URL temporária: {url}")

    server = http.server.ThreadingHTTPServer(("0.0.0.0", http_port), FirmwareHandler)
    server_thread = threading.Thread(target=server.serve_forever, daemon=True)
    server_thread.start()

    done = threading.Event()
    client = make_client(config, done)

    try:
        client.connect(broker, mqtt_port, 20)
        client.loop_start()
        time.sleep(1.0)

        command_topic = f"{prefix}/esteira/comando"
        command = json.dumps({"cmd": "OTA_UPDATE", "url": url})

        info = client.publish(command_topic, command, qos=1, retain=False)
        info.wait_for_publish()
        print(f"[MQTT] comando OTA enviado para {command_topic}")

        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if done.wait(timeout=0.25):
                return 0

        print("[OTA] timeout esperando conclusão.")
        return 1

    finally:
        try:
            client.loop_stop()
            client.disconnect()
        except Exception:
            pass

        server.shutdown()
        server.server_close()


if __name__ == "__main__":
    raise SystemExit(main())
