# Smart City Poles — Documentation

## How it works

The add-on has two jobs:

### 1. Dashboard (web UI / ingress)

An nginx server serves the single-file dashboard on the ingress port. It is
embedded in the Home Assistant sidebar as the **Smart Poles** panel.

The nginx server also **proxies `/api/*` to the Home Assistant core** and
injects the Supervisor token server-side. When the dashboard detects it is
served through ingress, it routes its API calls through the add-on — so it
works for any logged-in user, without "Keep me logged in" or any token
configuration. (WebSocket push is disabled in ingress mode; the page falls
back to 10 s polling. When served from `/local/` the original behaviour —
user token + WebSocket — is unchanged.)

### 2. Pole watchdog

A small service polls one "alive" entity per pole (`select.colX_led_mode`)
every `watchdog_poll_seconds`. When a pole becomes `unavailable`/`unknown`
you get a persistent notification, and another when it recovers. Disable it
with the `watchdog` option.

### 3. Automatic configuration installation

On start, the add-on renders two Home Assistant **packages** from its options
and writes them to `/config/packages/`:

| File | Contents |
| --- | --- |
| `smart_city_poles_energy.yaml` | `input_select.kolones_mode`, `counter.kolones_passes_night`, `input_number.kolones_passes_last_*`, `timer.kolona1-3`, `utility_meter` (baseline/adaptive tariffs), `history_stats` hours, template sensors (avg W, saving %, avg brightness) |
| `smart_city_poles_automation.yaml` | The "Κολόνες - Πλήρης έλεγχος" automation (day/night switching, 88 % pre-light wave, 60 s return-to-base timers, scenario alternation) |
| `smart_city_poles_influxdb.yaml` (optional) | The `influxdb` integration block for long-term storage (needs the InfluxDB add-on and `influxdb_password` in `secrets.yaml`, plus a full restart) |

With `install_grafana` the add-on also writes a **Grafana provisioning kit** to
`/share/grafana-provisioning/`: a ready InfluxDB datasource
(`influxdb-kolones`) and the 29-panel "Έξυπνες Κολώνες — Πάτρα" dashboard.
Then add `GF_PATHS_PROVISIONING=/share/grafana-provisioning` as an `env_vars`
entry in the Grafana add-on configuration and restart Grafana (you get a
notification with these steps).

If `configuration.yaml` does not load the `packages` directory yet, the add-on
adds `packages: !include_dir_named packages` under the `homeassistant:` key —
after creating a timestamped backup next to it. In that case a **full restart**
of Home Assistant is required (you get a notification). Otherwise the
configuration is validated and reloaded automatically.

## Options

See the Configuration tab — every option is described there (EN/EL). The most
important ones:

- `shelly_energy_entity`: the energy (kWh) sensor of the Shelly EM channel that
  feeds the lamps. Find it by turning all three lamps to full brightness and
  watching which `*_channel_X_power` rises.
- `experiment_start_date`: when the baseline/adaptive experiment started
  (must not be in the future).
- `light_pole_1..3`, `presence_pole_1..3`, `lux_sensor`: the entity IDs the
  automation should use.

## Uninstalling

Generated packages are removed when the corresponding install option is turned
off and the add-on restarts (a Home Assistant restart is then needed). The
add-on never touches anything else in `/config`.
