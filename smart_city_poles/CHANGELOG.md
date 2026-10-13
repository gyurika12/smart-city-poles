# Changelog

## 1.1.0

- **No token needed any more**: the dashboard now talks to the Home Assistant
  API through the add-on's nginx proxy (the Supervisor token is injected
  server-side). Works via ingress without "Keep me logged in".
- **Pole watchdog**: new service that monitors the three poles and sends a
  persistent notification when one goes offline and when it recovers
  (`watchdog`, `watchdog_poll_seconds` options).
- **Optional InfluxDB package** (`install_influxdb`): long-term storage config
  for Grafana, installed into `/config/packages/`.
- **Optional Grafana provisioning** (`install_grafana`): ready datasource +
  the 29-panel "Έξυπνες Κολώνες — Πάτρα" dashboard written to
  `/share/grafana-provisioning/`.
- Packages are now only rewritten (and HA reloaded) when their content
  actually changes — no more repeat notifications on every add-on start.

## 1.0.0

- Initial release
- Live dashboard (Leaflet map + Chart.js) in the Home Assistant sidebar via ingress
- Automatic installation of the energy-experiment package (helpers, utility_meter
  tariffs, history_stats, template sensors) into `/config/packages/`
- Automatic installation of the "Poles - Full control" automation
  (day/night, pre-light wave, baseline/adaptive scenarios)
- Automatic `packages:` include in `configuration.yaml` (with backup)
- Config validation and reload / restart notification after installation
