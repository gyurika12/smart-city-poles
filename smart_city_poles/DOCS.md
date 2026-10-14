# Smart City Poles — Documentation

## How it works

### 1. Dashboard (web UI / ingress)

An nginx server serves the dashboard on the ingress port. It is embedded in
the Home Assistant sidebar as the **Smart Poles** panel: live map, per-pole
metrics and 24 h charts, comparison between poles, overview table, lamp
state and LED-mode control, and a sensor diagnostics dialog.

- **Configured from the add-on options.** On every start the add-on writes
  `config.json` (poles, coordinates, colours) next to the dashboard. No code
  changes are needed to add a pole.
- **Sensors are discovered automatically.** For each pole the dashboard takes
  every entity of the *device* its presence sensor belongs to, plus every
  entity whose id contains one of the pole's `entity_match` tokens as a whole
  word (`col1` matches `sensor.col1_temperature`, not `sensor.col10_…`). Each
  entity is classified by its `device_class`, unit and name (temperature,
  humidity, CO₂, lux, distance, …). Anything that cannot be classified is
  still shown as an extra card. Open **🔍 Έλεγχος αισθητήρων** to see exactly
  which entity was mapped to what.
- **No token in the browser.** nginx forwards API calls to Home Assistant
  with the Supervisor token, but only these endpoints are exposed:
  `GET /api/states`, `GET /api/history/period/…`, `POST /api/template`, and
  the services `select.select_option`, `switch.turn_on/off`,
  `light.turn_on/off`. Everything else under `/api/` returns 403. nginx only
  accepts connections from the Supervisor's ingress proxy.
- **Works offline.** Leaflet and Chart.js are bundled in the image (pinned
  versions, verified checksums). Only the map background tiles need internet.

### 2. Lighting automation

The add-on renders the **"Κολόνες - Πλήρης έλεγχος"** automation for any
number of poles:

| Situation | Lamp level (option) |
| --- | --- |
| Day (`lux_sensor` > `lux_day_above` for 10 s) | off |
| Night, nobody around (`lux_sensor` < `lux_night_below` for 10 s) | `brightness_base` |
| A neighbouring pole detected someone (the wave) | `brightness_wave` |
| This pole detected someone | `brightness_boost` |
| Baseline scenario, all night | `brightness_baseline` |

After a detection the pole and its neighbours return to the base level
after `hold_seconds`. **Neighbours** are the previous and next pole in the
`poles` list (street order). For other layouts (junctions, a loop) set
`neighbors` on a pole: a comma-separated list of pole ids. It is
one-directional — set it on both poles if both should light each other.

`scenario`:

- `adaptive` — normal operation (dimmed base + boost + wave).
- `baseline` — full brightness all night (conventional lighting).
- `alternate` — baseline and adaptive on alternating nights (even/odd day of
  the year), to measure the saving on the same street. The scenario is never
  changed in the middle of a night, even after a restart.

### 3. Pole watchdog

Polls one "alive" entity per pole every `watchdog_poll_seconds`: the pole's
`status_entity`, or its presence sensor if none is set. When a pole becomes
`unavailable`/`unknown` you get a persistent notification, and another when
it recovers.

### 4. Generated Home Assistant packages

On start the add-on writes these files to `/config/packages/` (and rewrites
them only when their content changes):

| File | Written when | Contents |
| --- | --- | --- |
| `smart_city_poles_helpers.yaml` | automation or energy enabled | `input_select.kolones_mode`, `counter.kolones_passes_night`, `input_number.kolones_passes_last_*`, one `timer.kolones_<pole id>` per pole |
| `smart_city_poles_automation.yaml` | `install_automation` | the control automation |
| `smart_city_poles_energy.yaml` | `install_energy_package` + `shelly_energy_entity` | `utility_meter` with baseline/adaptive tariffs, `history_stats` hours, template sensors (average W per scenario, saving %, average brightness) |
| `smart_city_poles_influxdb.yaml` | `install_influxdb` | `influxdb` integration block (needs the InfluxDB add-on and `influxdb_password` in `secrets.yaml`, plus a full restart) |

With `install_grafana` the add-on also writes a **Grafana provisioning kit**
to `/share/grafana-provisioning/`: an InfluxDB datasource (`influxdb-kolones`)
and the poles dashboard. Add `GF_PATHS_PROVISIONING=/share/grafana-provisioning`
as an `env_vars` entry in the Grafana add-on configuration and restart Grafana
(you get a notification with these steps).

If `configuration.yaml` does not load the `packages` directory yet, the add-on
adds `packages: !include_dir_named packages` under `homeassistant:` — after
creating a timestamped backup next to it — and asks for a **full restart**.
Otherwise the configuration is validated and reloaded automatically.

## Configuration

Every option is described in the Configuration tab (EN/EL). Example of a
pole entry:

```yaml
poles:
  - id: col1                      # a-z, 0-9, _ (used in timer / notification ids)
    name: Pole 1
    location: Main Square         # optional, shown on the map
    latitude: 38.2472
    longitude: 21.7335
    light: light.pole_1_lamp      # the lamp to control
    presence: binary_sensor.pole_1_presence
    status_entity: select.col1_led_mode   # optional, goes unavailable when offline
    entity_match: col1, kolona_1  # optional, helps the dashboard find sensors
    neighbors: col2, col5         # optional, overrides the list order
    parking_occupied_cm: 20       # optional, parking spot indicator from the
    parking_free_cm: 23.5         #   ultrasonic distance (with hysteresis)
```

Invalid options (duplicate ids, a lamp or presence sensor used by two poles,
unknown neighbours, a non-`light.*` lamp, night threshold above the day
threshold) are reported in the add-on log and as a notification, and
nothing is installed until they are fixed.

## Upgrading from 1.x

- The per-pole options `light_pole_1..3` / `presence_pole_1..3` are replaced
  by the `poles` list. Its defaults match the original three-pole prototype.
- The helpers moved from the energy package to
  `smart_city_poles_helpers.yaml` (same entity ids). The return timers are now
  `timer.kolones_<pole id>` instead of `timer.kolona1-3`.
- `install_energy_package` now defaults to off and needs
  `shelly_energy_entity` to be set.

## Uninstalling

Generated packages are removed when the corresponding option is turned off
and the add-on restarts (a Home Assistant restart is then needed). The add-on
never touches anything else in `/config`.
