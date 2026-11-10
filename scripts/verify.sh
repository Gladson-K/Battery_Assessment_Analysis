#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

if [[ -n "${PIO:-}" ]]; then
    PIO_BIN="$PIO"
elif command -v pio >/dev/null 2>&1; then
    PIO_BIN="$(command -v pio)"
elif [[ -x /tmp/opencode/pio-venv/bin/pio ]]; then
    PIO_BIN=/tmp/opencode/pio-venv/bin/pio
else
    echo "error: PlatformIO not found; set PIO=/path/to/pio" >&2
    exit 1
fi

ENVS=(
    blackpill_f411ce_bench
    blackpill_f411ce_ev_pack
    blackpill_f411ce_server_ups
    blackpill_f411ce_solar_cabinet
    blackpill_f411ce_cold_chain
)

echo "== host tests =="
make -C firmware/tests clean all test

echo "== firmware builds =="
for env in "${ENVS[@]}"; do
    echo "-- $env"
    "$PIO_BIN" run -d firmware -e "$env"
done

echo "== NAS protocol tests =="
python3 -m unittest discover -s nas/tests -v

echo "== ML NAS tests =="
python3 -m unittest discover -s model/nas -p 'test_*.py' -v

echo "== Python syntax =="
python3 -m py_compile nas/gateway/*.py nas/tests/*.py model/nas/*.py model/train_v2.py

echo "== Compose configuration =="
if command -v docker-compose >/dev/null 2>&1; then
    (
        cd nas
        set -a
        # shellcheck disable=SC1091
        . ./.env.example
        set +a
        docker-compose config >/dev/null
    )
elif command -v docker >/dev/null 2>&1 && docker compose version >/dev/null 2>&1; then
    docker compose -f nas/docker-compose.yml --env-file nas/.env.example config >/dev/null
else
    echo "warning: docker compose not available; skipped configuration check" >&2
fi

if [[ "${VERIFY_DOCKER:-0}" == "1" ]]; then
    echo "== Gateway image build =="
    docker build -t battery-gateway:test nas/gateway
fi

echo "verify: all requested checks passed"
