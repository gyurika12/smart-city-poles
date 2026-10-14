# Pole HTTP API (v1)

Every pole (firmware 2.1.0+, `KolonaCore/src/KolonaApi.h`) runs a small HTTP
server on port 80 and announces itself with mDNS, so Home Assistant can set it
up from its IP address — or find it on its own.

Η προδιαγραφή του τοπικού API των κολονών. Το υλοποιεί το firmware
(`KolonaApi.h`) και η ψεύτικη κολόνα `tools/mock_pole.py`.

## Discovery (mDNS)

| | |
|---|---|
| Hostname | `kolona-<id>.local` (e.g. `kolona-col1.local`) |
| Service | `_kolona._tcp`, port 80 |
| TXT records | `id=col1`, `api=1`, `fw=2.1.0` |

## `GET /info` — who the pole is and what it measures

```json
{
  "api": 1,
  "id": "col1",
  "name": "Κολώνα 1",
  "index": 1,
  "model": "Smart Pole ESP32-S3",
  "manufacturer": "Smart City Poles",
  "fw": "2.1.0",
  "mac": "34:85:18:AA:BB:CC",
  "hostname": "kolona-col1",
  "auth": false,
  "mqtt": {
    "status_topic": "esp32s3/col1/status",
    "wave_topic": "esp32s3/wave/event",
    "payload_online": "online",
    "payload_offline": "offline"
  },
  "led": {
    "command_topic": "esp32s3/col1/led/cmd",
    "state_topic": "esp32s3/col1/led/mode",
    "options": ["AUTO", "ON", "OFF"],
    "http": "/led"
  },
  "entities": [
    {"key": "temperature", "name": "Θερμοκρασία", "platform": "sensor",
     "topic": "esp32s3/col1/sensors/temperature", "unit": "°C",
     "device_class": "temperature", "state_class": "measurement"},
    {"key": "pressure", "name": "Πίεση", "platform": "sensor",
     "topic": "esp32s3/col1/sensors/pressure", "unit": "hPa",
     "device_class": "atmospheric_pressure", "state_class": "measurement", "scale": 0.01},
    {"key": "presence", "name": "Παρουσία", "platform": "binary_sensor",
     "topic": "esp32s3/col1/sensors/laser_trigger",
     "payload_on": "ON", "payload_off": "OFF", "device_class": "occupancy"},
    {"key": "led_mode", "name": "Λειτουργία λάμπας", "platform": "text",
     "topic": "esp32s3/col1/led/mode"}
  ]
}
```

| Field | Meaning |
|---|---|
| `id` | Unique, stable pole id. Used in entity unique ids. |
| `mac` | WiFi MAC — the device's unique id in Home Assistant. |
| `index` | Position on the street (the firmware's light wave uses it). |
| `auth` | `true` when `POST /led` needs a token. |
| `entities[].platform` | `sensor` (number), `binary_sensor` (on/off) or `text`. |
| `entities[].topic` | MQTT topic with the same value — the integration can subscribe here for instant updates. |
| `entities[].scale` | Multiply the raw value by this (e.g. Pa → hPa). Missing = 1. |
| `entities[].payload_on/off` | Raw values that mean on / off for a `binary_sensor`. |

## `GET /state` — current values and health

```json
{
  "id": "col1",
  "uptime_s": 5231,
  "rssi": -61,
  "ip": "192.168.50.31",
  "heap_free": 182340,
  "heap_max_block": 110580,
  "mqtt": true,
  "values": {
    "temperature": {"value": "21.4", "age_s": 2},
    "pressure": {"value": "101325", "age_s": 9},
    "presence": {"value": "OFF", "age_s": 0},
    "uv": null
  }
}
```

- `value` is the raw payload exactly as published on MQTT (apply `scale`).
- `age_s` = seconds since the pole last published it. Values are re-sent at
  least every 10 s (heartbeat) and immediately when they change.
- `null` = no value yet (sensor missing or not read yet).
- A sensor that is not fitted reports `-1` (e.g. `distance_us` with no echo).
- Values keep updating while the MQTT broker is down (`"mqtt": false`).

## `POST /led` — lamp mode

```
POST /led
Content-Type: application/x-www-form-urlencoded
Authorization: Bearer <KOLONA_API_TOKEN>      (only when "auth": true)

mode=AUTO | ON | OFF
```

| Status | Body |
|---|---|
| 200 | `{"ok": true, "mode": "ON"}` |
| 400 | `{"error": "mode must be AUTO, ON or OFF"}` |
| 401 | `{"error": "unauthorized"}` |

The same command can still be sent over MQTT (`led.command_topic`).

## Security

The API is meant for the local network. Set `KOLONA_API_TOKEN` in
`secrets.h` to protect `POST /led`; `/info` and `/state` stay readable.

## Trying it without hardware

```bash
python3 tools/mock_pole.py --port 8080
curl http://127.0.0.1:8080/info
```
