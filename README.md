# arcal-busmon

`arcal-busmon` is a lightweight browser monitor for ARCAL traffic. The default
path is now LA-CAL: `arlacal-server` owns DDS access and schema-aware JSON
externalization, while busmon connects over OMS Web Protocol (OWP), subscribes
to all LA-CAL default topics, writes per-topic JSON logs, and streams message
metadata to the browser UI.

![arcal-busmon streaming UCI status messages](images/busmon-ui.png)

## Quick Start

Build the LA-CAL server in the ARCAL repo:

```bash
cd ../arcal
cmake --build build --target arlacal-server
```

Start busmon:

```bash
cd ../arcal-busmon
./busmon-ui.sh
```

The launcher starts `../arcal/build/lacal/arlacal-server`, then starts the
FastAPI busmon UI with `uv`. It sets `CYCLONEDDS_URI` for the LA-CAL server
using ARCAL's bundled loopback Cyclone DDS config:

```bash
../arcal/test/e2e/cyclonedds_localhost.xml
```

This keeps the monitor on the same single-host DDS discovery settings used by
ARCAL's tests and demo publishers.

If an LA-CAL server is already running, point busmon at it instead:

```bash
./busmon-ui.sh --lacal-url ws://127.0.0.1:8766
```

The UI defaults to port `8765` and logs decoded JSON under `/tmp/busmon-out`.
The LA-CAL server defaults to `ws://127.0.0.1:8766`.

## Shape

The bus monitor is intentionally split at the LA-CAL boundary:

- `busmon-ui.sh` starts `arlacal-server` and the Python web app.
- `busmon/owp_client.py` speaks OWP over WebSocket and sends `XSUB *`.
- `busmon/message_store.py` keeps recent metadata and JSON payloads, and writes
  durable per-topic logs.
- `busmon/app.py` serves the browser UI and relays live message batches to
  `ui/static/index.html`.

The only wildcard behavior busmon needs is the LA-CAL extension plan: one
`XSUB` request followed by `XSUBINFO` records from the server, after which
normal `MSG <subscription-id> <json>` traffic carries the payloads.

## Demo Publisher

The ARCAL demo publisher can feed the UI with basic platform/status traffic:

```bash
cd ../arcal
ninja -C build e2e_pub_continuous
export CYCLONEDDS_URI="file://$PWD/test/e2e/cyclonedds_localhost.xml"
./build/test/e2e/e2e_pub_continuous \
  --rate 50 \
  --system-rate 5 \
  --service-rate 10 \
  --subsystem-rate 2
```

That is the same Cyclone DDS config that `../arcal-busmon/busmon-ui.sh` applies
to the LA-CAL server. When running processes manually, use the same
`CYCLONEDDS_URI` value for both:

```bash
cd ../arcal
export CYCLONEDDS_URI="file://$PWD/test/e2e/cyclonedds_localhost.xml"
./build/lacal/arlacal-server --host 127.0.0.1 --port 8766 &
cd ../arcal-busmon
uv run python -m busmon.app --lacal-url ws://127.0.0.1:8766 --log-dir /tmp/busmon-out &
cd ../arcal
./build/test/e2e/e2e_pub_continuous \
  --rate 50 \
  --system-rate 5 \
  --service-rate 10 \
  --subsystem-rate 2
```

That produces `PositionReport`, `SystemStatus`, `ServiceStatus`, and
`SubsystemStatus` messages with `MessageHeader.Mode` set to `SIMULATION`.
