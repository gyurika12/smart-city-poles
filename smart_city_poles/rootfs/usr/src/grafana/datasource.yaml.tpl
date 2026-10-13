# InfluxDB datasource for the Smart City Poles Grafana dashboard.
# Rendered by the add-on (user/password come from the add-on options).
apiVersion: 1

datasources:
  - name: InfluxDB-Kolones
    uid: influxdb-kolones
    type: influxdb
    access: proxy
    url: http://a0d7b954-influxdb:8086
    user: __INFLUX_USER__
    password: __INFLUX_PASS__
    database: kolones
    isDefault: true
    editable: true
    jsonData:
      httpMode: GET
