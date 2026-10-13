# Smart City Poles — Documentation

## How it works

The add-on has two jobs:

### 1. Dashboard (web UI / ingress)

An nginx server serves the single-file dashboard on the ingress port. It is
embedded in the Home Assistant sidebar as the **Smart Poles** panel. The page
talks to the Home Assistant API using your current login (same origin), so no
token configuration is needed.

### 2. Automatic configuration installation

On start, the add-on renders two Home Assistant **packages** from its options
and writes them to `/config/packages/`:

| File | Contents |
| --- | --- |
| `smart_city_poles_energy.yaml` | `input_select.kolones_mode`, `counter.kolones_passes_night`, `input_number.kolones_passes_last_*`, `timer.kolona1-3`, `utility_meter` (baseline/adaptive tariffs), `history_stats` hours, template sensors (avg W, saving %, avg brightness) |
| `smart_city_poles_automation.yaml` | The "Κολόνες - Πλήρης έλεγχος" automation (day/night switching, 88 % pre-light wave, 60 s return-to-base timers, scenario alternation) |

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
