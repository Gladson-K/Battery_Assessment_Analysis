FROM python:3.11-slim AS runtime

ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1 \
    PIP_NO_CACHE_DIR=1

WORKDIR /app
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY protocol.py serial_gateway.py ingest.py api.py simulator.py ./
COPY devices.yml /app/devices.yml

RUN useradd --create-home --uid 10001 gateway && mkdir -p /data && chown -R gateway:gateway /data /app
USER gateway

CMD ["python", "serial_gateway.py"]
