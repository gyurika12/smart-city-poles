"""Validate the output of smart-poles-render (run by CI and locally).

Usage: python3 tests/addon/check_render.py OUT_DIR OPTIONS_JSON
"""
import json
import re
import sys

import yaml


class Loader(yaml.SafeLoader):
    pass


Loader.add_multi_constructor("!", lambda loader, suffix, node: None)

out, opts_path = sys.argv[1], sys.argv[2]
opts = json.load(open(opts_path))
poles = opts["poles"]


def load(name):
    text = open(f"{out}/{name}").read()
    leftover = re.findall(r"__[A-Z0-9_]+__", text)
    assert not leftover, f"{name}: unresolved placeholders {leftover}"
    return yaml.load(text, Loader=Loader)


helpers = load("smart_city_poles_helpers.yaml")
assert sorted(helpers["timer"]) == sorted(f"kolones_{p['id']}" for p in poles)
print("helpers OK:", len(helpers["timer"]), "timers")

auto = load("smart_city_poles_automation.yaml")["automation"][0]
v = auto["variables"]
assert v["oles"] == [p["light"] for p in poles]
assert set(v["poles"]) == {p["presence"] for p in poles}
assert len(auto["actions"][0]["choose"]) == 4, "automation branches changed?"
trig = {t["id"]: t for t in auto["triggers"]}
assert trig["presence"]["entity_id"] == [p["presence"] for p in poles]
assert trig["timer"]["entity_id"] == [f"timer.kolones_{p['id']}" for p in poles]
for p in poles:
    entry = v["poles"][p["presence"]]
    assert entry["light"] == p["light"]
    assert len(entry["neighbor_lights"]) == len(entry["neighbor_timers"])
    assert p["light"] not in entry["neighbor_lights"], "a pole cannot be its own neighbour"
print("automation OK:", len(poles), "poles, scenario", v["scenario_cfg"])

if opts.get("install_energy_package") and opts.get("shelly_energy_entity"):
    energy = load("smart_city_poles_energy.yaml")
    assert energy["utility_meter"]["kolones_energy"]["source"] == opts["shelly_energy_entity"]
    print("energy OK")

if opts.get("install_influxdb"):
    assert load("smart_city_poles_influxdb.yaml")["influxdb"]["database"] == "kolones"
    print("influxdb OK")

dash = json.load(open(f"{out}/config.json"))
assert [p["id"] for p in dash["poles"]] == [p["id"] for p in poles]
assert "password" not in json.dumps(dash), "secrets must not reach the dashboard config"
print("dashboard config OK")
