# -*- coding: utf-8 -*-
"""Generates the Grafana dashboard  grafana/dashboards/kolones-patras.json
   Παράγει το dashboard του Grafana για τις τρεις κολόνες.

   Run / Τρέξε:   python3 grafana/build_dashboard.py

   Data source: InfluxDB 1.x, filled by the Home Assistant `influxdb` integration
   (measurement = unit of measurement, tag entity_id = HA entity without domain).
"""
import json, os

DS = {"type": "influxdb", "uid": "${DS_INFLUXDB}"}
COLOR = {1: "#4f98a3", 2: "#e8af34", 3: "#6daa45"}

# metric -> (title, grafana unit, influx measurement)
METRIC = {
    "temp":  ("Θερμοκρασία",       "celsius",     "°C"),
    "hum":   ("Υγρασία",           "percent",     "%"),
    "press": ("Πίεση",             "pressurehpa", "hPa"),
    "ldr":   ("Φωτεινότητα (LDR)", "short",       "state"),
    "eco2":  ("CO₂ (ppm)",         "short",       "ppm"),
    "tvoc":  ("TVOC (ppb)",        "short",       "ppb"),
}
# HA entity regex per pole and metric
ENTITY = {
    1: {"temp": "/^col1_temperature$/", "hum": "/^col1_humidity$/", "press": "/^col1_pressure$/",
        "ldr": "/^col1_ldr$/"},
    2: {"temp": "/^col2_temperature$/", "hum": "/^col2_humidity$/", "press": "/^col2_pressure$/",
        "ldr": "/^col2_ldr$/", "eco2": "/^col2_eco2$/", "tvoc": "/^col2_tvoc$/"},
    3: {"temp": "/^col3_temperature$/", "hum": "/^col3_humidity$/", "press": "/^col3_pressure$/",
        "ldr": "/^col3_ldr$/", "eco2": "/kolona_3_kolona_3_eco2/", "tvoc": "/kolona_3_kolona_3_tvoc/"},
}
# gas sensor differs per pole: (unit, measurement, regex)
GAS = {1: ("ohm", "Ω", "/kolona_1_kolona_1_aerio_bme/"),
       2: ("short", "state", "/^col2_gas$/"),
       3: ("short", "state", "/kolona_3_kolona_3_aerio/")}
# panel order inside each pole's row
POLE_PANELS = {1: ["temp", "hum", "press", "gas", "ldr"],
               2: ["temp", "hum", "press", "eco2", "tvoc", "gas", "ldr"],
               3: ["temp", "hum", "press", "eco2", "tvoc", "gas", "ldr"]}


def target(pole, ref, measurement, regex):
    return {"alias": f"Κολώνα {pole}", "datasource": DS,
            "query": f'SELECT mean("value") FROM "{measurement}" WHERE ("entity_id" =~ {regex}) '
                     f'AND $timeFilter GROUP BY time($__interval) fill(previous)',
            "rawQuery": True, "refId": ref, "resultFormat": "time_series"}


def timeseries(pid, x, y, title, unit, series):
    """series = [(pole, measurement, regex), ...]"""
    return {
        "datasource": DS,
        "fieldConfig": {
            "defaults": {
                "color": {"mode": "palette-classic"},
                "custom": {"drawStyle": "line", "fillOpacity": 16, "gradientMode": "opacity",
                           "lineInterpolation": "smooth", "lineWidth": 2, "pointSize": 5,
                           "showPoints": "never", "spanNulls": True},
                "unit": unit},
            "overrides": [{"matcher": {"id": "byName", "options": f"Κολώνα {p}"},
                           "properties": [{"id": "color", "value": {"fixedColor": COLOR[p], "mode": "fixed"}}]}
                          for p, _, _ in series]},
        "gridPos": {"h": 8, "w": 12, "x": x, "y": y},
        "id": pid,
        "options": {"legend": {"calcs": ["lastNotNull", "min", "max", "mean"], "displayMode": "table",
                               "placement": "bottom", "showLegend": True},
                    "tooltip": {"mode": "multi", "sort": "desc"}},
        "targets": [target(p, "ABC"[i], m, r) for i, (p, m, r) in enumerate(series)],
        "title": title, "type": "timeseries"}


def row(pid, y, title):
    return {"collapsed": False, "gridPos": {"h": 1, "w": 24, "x": 0, "y": y}, "id": pid,
            "panels": [], "title": title, "type": "row"}


def build():
    panels, pid, y = [], 1, 0

    def grid(items, y0):
        nonlocal pid
        for i, (title, unit, series) in enumerate(items):
            pid += 1
            panels.append(timeseries(pid, 12 * (i % 2), y0 + 8 * (i // 2), title, unit, series))
        return y0 + 8 * ((len(items) + 1) // 2)

    # 1. Comparison row - all three poles on the same chart
    panels.append(row(pid, y, "⟷  Συγκριτικά — και οι 3 κολώνες"))
    items = []
    for key in ["temp", "hum", "press", "ldr", "eco2", "tvoc"]:
        title, unit, meas = METRIC[key]
        items.append((title, unit, [(p, meas, ENTITY[p][key]) for p in (1, 2, 3) if key in ENTITY[p]]))
    y = grid(items, y + 1)

    # 2. One row per pole
    for pole in (1, 2, 3):
        pid += 1
        panels.append(row(pid, y, f"▊  Κολώνα {pole}"))
        items = []
        for key in POLE_PANELS[pole]:
            if key == "gas":
                unit, meas, regex = GAS[pole]
                items.append((f"Κολώνα {pole} — Αέριο", unit, [(pole, meas, regex)]))
            else:
                title, unit, meas = METRIC[key]
                items.append((f"Κολώνα {pole} — {title}", unit, [(pole, meas, ENTITY[pole][key])]))
        y = grid(items, y + 1)

    return {"annotations": {"list": []}, "editable": True, "panels": panels, "refresh": "30s",
            "schemaVersion": 39, "style": "dark", "tags": ["kolones", "microlab"],
            "templating": {"list": []}, "time": {"from": "now-24h", "to": "now"},
            "timezone": "browser", "title": "Έξυπνες Κολώνες — Πάτρα", "uid": "kolones-patras",
            "version": 1}


if __name__ == "__main__":
    dash = build()
    # "__inputs" lets Grafana ask for the InfluxDB data source on import
    export = {"__inputs": [{"name": "DS_INFLUXDB", "label": "InfluxDB", "type": "datasource",
                            "pluginId": "influxdb", "pluginName": "InfluxDB"}], **dash}
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "dashboards", "kolones-patras.json")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w", encoding="utf-8") as f:
        json.dump(export, f, ensure_ascii=False, indent=2)
    print("wrote", out, "-", len(dash["panels"]), "panels")
