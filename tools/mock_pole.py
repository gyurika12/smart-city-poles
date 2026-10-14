#!/usr/bin/env python3
"""Ψεύτικη κολόνα: μιλάει το ίδιο HTTP API με το firmware (KolonaApi.h).

Χρησιμεύει για ανάπτυξη και tests του integration χωρίς ESP32.

    python3 tools/mock_pole.py                    # http://127.0.0.1:8080
    python3 tools/mock_pole.py --port 8081 --id col2 --index 2
    python3 tools/mock_pole.py --token secret     # το POST /led θέλει Bearer token
    python3 tools/mock_pole.py --mdns             # ανακοινώνεται και με mDNS
                                                  # (χρειάζεται: pip install zeroconf)

Μετά:
    curl http://127.0.0.1:8080/info
    curl http://127.0.0.1:8080/state
    curl -X POST -d mode=ON http://127.0.0.1:8080/led
"""
import argparse
import json
import math
import random
import socket
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs

API_VERSION = 1
FW_VERSION = "2.1.0-mock"


def entities(pid):
    """Ίδια λίστα με το registerApi() της COL1."""
    t = f"esp32s3/{pid}"
    return [
        dict(key="temperature", name="Θερμοκρασία", platform="sensor", topic=f"{t}/sensors/temperature",
             unit="°C", device_class="temperature", state_class="measurement"),
        dict(key="humidity", name="Υγρασία", platform="sensor", topic=f"{t}/sensors/humidity",
             unit="%", device_class="humidity", state_class="measurement"),
        dict(key="pressure", name="Πίεση", platform="sensor", topic=f"{t}/sensors/pressure",
             unit="hPa", device_class="atmospheric_pressure", state_class="measurement", scale=0.01),
        dict(key="lux", name="Φωτεινότητα", platform="sensor", topic=f"{t}/sensors/lux",
             unit="lx", device_class="illuminance", state_class="measurement"),
        dict(key="distance_tof", name="Απόσταση (laser)", platform="sensor", topic=f"{t}/sensors/distance_tof_mm",
             unit="mm", device_class="distance", state_class="measurement"),
        dict(key="presence", name="Παρουσία", platform="binary_sensor", topic=f"{t}/sensors/laser_trigger",
             payload_on="ON", payload_off="OFF", device_class="occupancy"),
        dict(key="rain", name="Βροχή", platform="binary_sensor", topic=f"{t}/sensors/rain",
             payload_on="RAIN", payload_off="DRY", device_class="moisture"),
        dict(key="led_state", name="Λάμπα αναμμένη", platform="binary_sensor", topic=f"{t}/led/state",
             payload_on="ON", payload_off="OFF", device_class="light"),
        dict(key="led_pwm", name="PWM λάμπας", platform="sensor", topic=f"{t}/led/pwm",
             state_class="measurement"),
        dict(key="led_mode", name="Λειτουργία λάμπας", platform="text", topic=f"{t}/led/mode"),
    ]


class Pole:
    def __init__(self, pid, name, index, token):
        self.id, self.name, self.index, self.token = pid, name, index, token
        self.start = time.time()
        self.mode = "AUTO"
        self.mac = "AA:BB:CC:%02X:%02X:%02X" % (index, random.randint(0, 255), random.randint(0, 255))

    def info(self):
        t = f"esp32s3/{self.id}"
        return {
            "api": API_VERSION, "id": self.id, "name": self.name, "index": self.index,
            "model": "Smart Pole ESP32-S3", "manufacturer": "Smart City Poles", "fw": FW_VERSION,
            "mac": self.mac, "hostname": f"kolona-{self.id}", "auth": bool(self.token),
            "mqtt": {"status_topic": f"{t}/status", "wave_topic": "esp32s3/wave/event",
                     "payload_online": "online", "payload_offline": "offline"},
            "led": {"command_topic": f"{t}/led/cmd", "state_topic": f"{t}/led/mode",
                    "options": ["AUTO", "ON", "OFF"], "http": "/led"},
            "entities": entities(self.id),
        }

    def state(self):
        now = time.time()
        k = now / 60
        present = (int(now) % 20) < 3                       # κάποιος περνάει κάθε 20 s
        pwm = {"ON": 255, "OFF": 0}.get(self.mode, 255 if present else 128)
        raw = {
            "temperature": f"{21 + 2 * math.sin(k):.1f}",
            "humidity": f"{55 + 5 * math.cos(k):.1f}",
            "pressure": f"{101300 + 50 * math.sin(k / 3):.0f}",   # σε Pa, όπως η COL1
            "lux": f"{max(0.0, 12 + 3 * math.sin(k * 7)):.1f}",
            "distance_tof": str(150 if present else 820),
            "presence": "ON" if present else "OFF",
            "rain": "DRY",
            "led_state": "ON" if pwm > 0 else "OFF",
            "led_pwm": str(pwm),
            "led_mode": self.mode,
        }
        return {
            "id": self.id, "uptime_s": int(now - self.start), "rssi": -58, "ip": "127.0.0.1",
            "heap_free": 180000, "heap_max_block": 110000, "mqtt": False,
            "values": {key: {"value": v, "age_s": 0} for key, v in raw.items()},
        }


def make_handler(pole):
    class Handler(BaseHTTPRequestHandler):
        def _json(self, code, obj):
            body = json.dumps(obj, ensure_ascii=False).encode()
            self.send_response(code)
            self.send_header("Content-Type", "application/json")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self):
            if self.path == "/info":
                return self._json(200, pole.info())
            if self.path == "/state":
                return self._json(200, pole.state())
            self._json(404, {"error": "not found"})

        def do_POST(self):
            if self.path.split("?")[0] != "/led":
                return self._json(404, {"error": "not found"})
            if pole.token and self.headers.get("Authorization") != f"Bearer {pole.token}":
                return self._json(401, {"error": "unauthorized"})
            body = self.rfile.read(int(self.headers.get("Content-Length") or 0)).decode()
            query = self.path.split("?", 1)[1] if "?" in self.path else ""
            args = parse_qs(body) | parse_qs(query)
            mode = (args.get("mode") or [""])[0].upper()
            if mode not in ("AUTO", "ON", "OFF"):
                return self._json(400, {"error": "mode must be AUTO, ON or OFF"})
            pole.mode = mode
            self._json(200, {"ok": True, "mode": mode})

        def log_message(self, fmt, *args):
            print(f"[{pole.id}] {self.address_string()} {fmt % args}")

    return Handler


def advertise(pole, port):
    from zeroconf import ServiceInfo, Zeroconf  # pip install zeroconf

    ip = socket.gethostbyname(socket.gethostname())
    info = ServiceInfo(
        "_kolona._tcp.local.", f"kolona-{pole.id}._kolona._tcp.local.",
        addresses=[socket.inet_aton(ip)], port=port,
        properties={"id": pole.id, "api": str(API_VERSION), "fw": FW_VERSION},
        server=f"kolona-{pole.id}.local.",
    )
    zc = Zeroconf()
    zc.register_service(info)
    print(f"mDNS: kolona-{pole.id}._kolona._tcp στο {ip}:{port}")
    return zc


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=8080)
    ap.add_argument("--id", default="col1")
    ap.add_argument("--name", default=None)
    ap.add_argument("--index", type=int, default=1)
    ap.add_argument("--token", default="")
    ap.add_argument("--mdns", action="store_true")
    a = ap.parse_args()

    pole = Pole(a.id, a.name or f"Κολώνα {a.index}", a.index, a.token)
    zc = advertise(pole, a.port) if a.mdns else None
    print(f"Ψεύτικη κολόνα {pole.id} στο http://{a.host}:{a.port}  (Ctrl+C για τέλος)")
    try:
        ThreadingHTTPServer((a.host, a.port), make_handler(pole)).serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        if zc:
            zc.close()


if __name__ == "__main__":
    main()
