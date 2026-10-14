# Changelog

## 2.0.0

**Any number of poles.** The add-on is no longer tied to the three-pole
prototype.

- New `poles` option: a list of poles (id, name, location, coordinates, lamp,
  presence sensor, optional status entity, sensor match, neighbours, parking
  thresholds). Replaces `light_pole_1..3` / `presence_pole_1..3`; the defaults
  are the original three poles.
- The control automation is generated for any number of poles. Neighbours
  follow the list order or the per-pole `neighbors` option.
- New options for the lighting levels (`brightness_*`), the hold time, the
  day/night lux thresholds and the `scenario` (`adaptive`, `baseline` or the
  `alternate` experiment).
- The dashboard is built from the options: map, overview, comparison and pole
  pages adapt to the configured poles. Sensors are discovered from the
  presence sensor's device and the `entity_match` tokens and classified by
  device class, unit and name. Shows lamp state/brightness and offline poles.
  Optional `demo_poles`.
- The watchdog monitors every configured pole.
- Options are validated before anything is installed (duplicate ids, shared
  lamps/sensors, unknown neighbours, thresholds) with a clear log message and
  notification.
- Helpers moved to their own package (`smart_city_poles_helpers.yaml`), so
  the automation no longer depends on the energy package. Timers are now
  `timer.kolones_<pole id>`.
- `install_energy_package` defaults to off and requires `shelly_energy_entity`.

**Security and reliability**

- Fixed the API proxy path (requests were forwarded to `/core<path>` instead
  of `/core/api/<path>`).
- The proxy only exposes the endpoints the dashboard needs; everything else
  under `/api/` is refused, and nginx only accepts the Supervisor's ingress
  proxy.
- Leaflet and Chart.js are bundled in the image (pinned, checksummed); the
  external web font was removed. The dashboard works without internet except
  for the map tiles.
- CI renders the packages for 3 and 5 poles, checks invalid options are
  rejected, and runs `nginx -t` inside the built image.


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
