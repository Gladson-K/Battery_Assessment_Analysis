#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

if [ ! -f .env ]; then
  cp .env.example .env
  chmod 600 .env
  echo "created .env from .env.example; edit the secrets before production use"
fi

set -a
# shellcheck disable=SC1091
source .env
set +a

mkdir -p data/gateway
chmod 700 data/gateway

template="grafana/templates/influxdb.yml.template"
target="grafana/provisioning/datasources/influxdb.yml"
if command -v envsubst >/dev/null 2>&1; then
  envsubst '${INFLUX_ORG} ${INFLUX_BUCKET} ${INFLUX_TOKEN}' < "$template" > "$target"
else
  sed -e "s|\${INFLUX_ORG}|${INFLUX_ORG}|g" \
      -e "s|\${INFLUX_BUCKET}|${INFLUX_BUCKET}|g" \
      -e "s|\${INFLUX_TOKEN}|${INFLUX_TOKEN}|g" \
      "$template" > "$target"
fi
chmod 600 "$target"

if docker compose version >/dev/null 2>&1; then
  compose=(docker compose)
else
  compose=(docker-compose)
fi

profile=()
args=()
while [ "$#" -gt 0 ]; do
  case "$1" in
    --profile)
      profile=(--profile "${2:?--profile requires a value}")
      shift 2
      ;;
    --profile=*)
      profile=(--profile "${1#*=}")
      shift
      ;;
    *)
      args+=("$1")
      shift
      ;;
  esac
done

"${compose[@]}" "${profile[@]}" up -d --build "${args[@]}"

cat <<EOF
Battery platform starting:
  Grafana    http://<nas-ip>:3000
  InfluxDB   http://<nas-ip>:8086
  API        http://<nas-ip>:8088/docs
  MQTT       <nas-ip>:1883 (1883/tcp, 9001/ws)

Demo without hardware:
  ./deploy.sh --profile demo
EOF
