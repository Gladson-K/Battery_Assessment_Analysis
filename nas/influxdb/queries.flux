// Useful Flux queries for the battery telemetry bucket.
// Replace "battery" with INFLUX_BUCKET if you changed it.

// 1. Latest value for every field of one device.
from(bucket: "battery")
  |> range(start: -24h)
  |> filter(fn: (r) => r._measurement == "battery_telemetry" and r.device_id == "bench-01")
  |> last()

// 2. Temperature channels over the last 6 hours.
from(bucket: "battery")
  |> range(start: -6h)
  |> filter(fn: (r) => r._measurement == "battery_telemetry" and r.device_id == "bench-01" and r._field =~ /^t[0-9]+$/)
  |> aggregateWindow(every: 1m, fn: mean, createEmpty: false)

// 3. Risk score and state.
from(bucket: "battery")
  |> range(start: -6h)
  |> filter(fn: (r) => r._measurement == "battery_telemetry" and r.device_id == "bench-01" and r._field == "risk")
  |> aggregateWindow(every: 1m, fn: mean, createEmpty: false)

// 4. High-risk events.
from(bucket: "battery")
  |> range(start: -24h)
  |> filter(fn: (r) => r._measurement == "battery_telemetry" and r._field == "risk" and r._value >= 70.0)
  |> sort(columns: ["_time"], desc: true)

// 5. Device inventory from status messages.
from(bucket: "battery")
  |> range(start: -7d)
  |> filter(fn: (r) => r._measurement == "battery_status" and r._field == "online")
  |> last()

// 6. Data continuity: count points per hour.
from(bucket: "battery")
  |> range(start: -24h)
  |> filter(fn: (r) => r._measurement == "battery_telemetry" and r._field == "risk")
  |> aggregateWindow(every: 1h, fn: count, createEmpty: true)

// 7. Optional downsampling task (create in the InfluxDB UI or CLI).
// option task = {name: "battery-hourly-rollup", every: 1h}
// from(bucket: "battery")
//   |> range(start: -task.every)
//   |> filter(fn: (r) => r._measurement == "battery_telemetry")
//   |> aggregateWindow(every: 1h, fn: mean, createEmpty: false)
//   |> set(key: "_measurement", value: "battery_telemetry_hourly")
//   |> to(bucket: "battery")

// 8. Optional alert task: write a row when risk crosses the warning threshold.
// option task = {name: "battery-alert-events", every: 1m}
// from(bucket: "battery")
//   |> range(start: -2m)
//   |> filter(fn: (r) => r._measurement == "battery_telemetry" and r._field == "risk" and r._value >= 70.0)
//   |> set(key: "_measurement", value: "battery_alerts")
//   |> set(key: "_field", value: "risk")
//   |> to(bucket: "battery")
