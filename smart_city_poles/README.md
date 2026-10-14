# Smart City Poles add-on

Adaptive street lighting for Home Assistant: any number of poles, a
presence-triggered "light wave", day/night control, energy-saving
measurement and a live map dashboard in the sidebar.

Προσαρμοστικός οδικός φωτισμός για το Home Assistant: οσεσδήποτε κολόνες,
«κύμα φωτός» με την ανίχνευση διέλευσης, έλεγχος μέρας/νύχτας, μέτρηση
εξοικονόμησης ενέργειας και ζωντανό dashboard με χάρτη στο sidebar.

## What it does / Τι κάνει

1. **Dashboard (ingress)** — the "Smart Poles" sidebar panel: live map, per-pole
   metrics and 24 h charts, comparison, overview, lamp state, LED-mode control
   and sensor diagnostics. Poles come from the add-on options and their
   sensors are discovered automatically. No user token needed; works without
   internet (except the map tiles).
2. **Lighting automation** — day/night switching, boost on detection, light
   wave to the neighbouring poles, return to base after a hold time, and the
   `adaptive` / `baseline` / `alternate` scenarios. All levels and thresholds
   are options.
3. **Energy package** — `utility_meter` per scenario, hours per scenario and
   the measured saving percentage.
4. **Pole watchdog** — notification when a pole goes offline and when it
   recovers.
5. **Optional InfluxDB / Grafana setup** for long-term storage and reports.

## Installation / Εγκατάσταση

1. Settings → Add-ons → Add-on Store → ⋮ → Repositories → add
   `https://github.com/gyurika12/smart-city-poles`
2. Install **Smart City Poles**.
3. In the **Configuration** tab add your poles (lamp, presence sensor,
   coordinates) and the lux sensor. See the [documentation](DOCS.md).
4. Start the add-on and watch the log. If asked, restart Home Assistant once.
5. Open the **Smart Poles** panel in the sidebar.

## Notes / Σημειώσεις

- The packages are regenerated from the add-on options on every start — do not
  edit the generated files, change the options instead.
- Turning an install option **off** removes the corresponding generated package
  (a restart is then required).
- The MQTT sensors of the poles themselves are defined by your own
  configuration (see `home_assistant/` in the repository).
