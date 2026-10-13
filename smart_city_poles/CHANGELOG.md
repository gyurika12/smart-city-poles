# Changelog

## 1.0.0

- Initial release
- Live dashboard (Leaflet map + Chart.js) in the Home Assistant sidebar via ingress
- Automatic installation of the energy-experiment package (helpers, utility_meter
  tariffs, history_stats, template sensors) into `/config/packages/`
- Automatic installation of the "Poles - Full control" automation
  (day/night, pre-light wave, baseline/adaptive scenarios)
- Automatic `packages:` include in `configuration.yaml` (with backup)
- Config validation and reload / restart notification after installation
