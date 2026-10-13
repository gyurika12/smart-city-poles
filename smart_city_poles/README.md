# Smart City Poles add-on

Adaptive street-light poles (ESP32-S3 + MQTT) — live dashboard in the Home
Assistant sidebar and one-click installation of the control automation and the
energy-saving experiment.

Ζωντανό dashboard στο sidebar του Home Assistant και αυτόματη εγκατάσταση του
αυτοματισμού ελέγχου και του πακέτου πειράματος ενέργειας για τις έξυπνες
κολόνες φωτισμού.

## What it does / Τι κάνει

1. **Dashboard (ingress):** serves the custom poles dashboard
   (`home_assistant/www/dashboard_kolones.html`) at the "Smart Poles" sidebar
   panel — live map, per-pole metrics, 24 h history charts, LED controls.
2. **Energy package:** writes helpers (`input_select.kolones_mode`,
   `counter.kolones_passes_night`, `input_number.*`, `timer.kolona1-3`),
   `utility_meter` tariffs (baseline/adaptive), `history_stats` hours and
   template sensors (`Kolones Saving Pct` etc.) to
   `/config/packages/smart_city_poles_energy.yaml`.
3. **Automation:** writes the "Κολόνες - Πλήρης έλεγχος" automation to
   `/config/packages/smart_city_poles_automation.yaml`.
4. Makes sure `configuration.yaml` loads the `packages` directory
   (backs it up first), validates the configuration and reloads — or tells you
   in the notifications if a full restart is needed.

## Installation / Εγκατάσταση

1. Settings → Add-ons → Add-on Store → ⋮ → Repositories → add:
   `https://github.com/gyurika12/smart-city-poles`
2. Install **Smart City Poles**.
3. In the **Configuration** tab set your entity IDs (lights, Shelly EM energy
   sensor, lux sensor, presence sensors) and the experiment start date.
4. Start the add-on and watch the log. If asked, restart Home Assistant once.
5. Open the **Smart Poles** panel in the sidebar.

## Notes / Σημειώσεις

- The packages are regenerated from the add-on options on every start — do not
  edit the generated files, change the options instead.
- Turning an install option **off** removes the corresponding generated package
  (a restart is then required).
- The MQTT sensors of the poles themselves are still defined by your own
  configuration (see `home_assistant/` in the repository).
