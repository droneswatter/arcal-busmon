# arcal-busmon

`arcal-busmon` is a lightweight browser monitor for ARCAL traffic. The busmon
UI is a static single-page app: `arlacal-server` owns DDS access and
schema-aware JSON externalization, and the browser connects directly to it over
OMS Web Protocol (OWP).

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

The launcher starts `../arcal/build/lacal/arlacal-server` and prints a
`file://...` URL for the static busmon page. It sets `CYCLONEDDS_URI` for the
LA-CAL server using ARCAL's bundled loopback Cyclone DDS config:

```bash
../arcal/test/e2e/cyclonedds_localhost.xml
```

This keeps the monitor on the same single-host DDS discovery settings used by
ARCAL's tests and demo publishers.

By default, the launcher assumes `arcal-busmon` and `arcal` are sibling
checkouts. If ARCAL lives somewhere else, set `ARCAL_DIR`:

```bash
ARCAL_DIR=/path/to/arcal ./busmon-ui.sh
```

`ARCAL_BUILD_DIR` and `CYCLONEDDS_XML` can also be set when the build
directory or Cyclone DDS config are not under the default ARCAL tree.

If an LA-CAL server is already running, point busmon at it instead:

```bash
./busmon-ui.sh --lacal-url ws://127.0.0.1:8766
```

The LA-CAL server defaults to `ws://127.0.0.1:8766`. The static page remembers
the last WebSocket URL in browser storage, and the address can be changed from
the UI after load.

When `arlacal-server` must accept connections from outside the local machine,
adjust the bind address:

```bash
./busmon-ui.sh --host 0.0.0.0
```

Use `127.0.0.1` for local-only access, `0.0.0.0` for all local interfaces, or
an assigned interface IP to listen on just that interface.

## Shape

The bus monitor is intentionally simple:

- `busmon-ui.sh` optionally starts `arlacal-server` and prints the static page
  URL.
- `ui/static/index.html` speaks OWP over WebSocket and sends `XSUB busmon *`.
- Message metadata and recent payloads are retained in browser memory only.
  The UI shows how many payloads or metadata records have been evicted by the
  selected retention preset.

The only wildcard behavior busmon needs is the LA-CAL extension plan: one
`XSUB` request followed by `XSUBINFO` records from the server, after which
normal `MSG <subscription-id> <json>` traffic carries the payloads.

Busmon currently ships three in-memory retention presets:

- `Light` - small rolling history, suited to constrained machines
- `Normal` - the default day-to-day setting
- `Deep` - larger rolling history for heavier inspection sessions

Older messages remain visible in the list until metadata is evicted. If a
payload has been dropped from memory, selecting that row reports that the
payload is no longer retained locally.

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

That is the same Cyclone DDS config that `../arcal-busmon/busmon-ui.sh`
applies to the LA-CAL server. When running processes manually, use the same
`CYCLONEDDS_URI` value for both:

```bash
cd ../arcal
export CYCLONEDDS_URI="file://$PWD/test/e2e/cyclonedds_localhost.xml"
./build/lacal/arlacal-server --host 127.0.0.1 --port 8766 &
cd ../arcal-busmon
xdg-open "file://$PWD/ui/static/index.html?ws=ws://127.0.0.1:8766"
cd ../arcal
./build/test/e2e/e2e_pub_continuous \
  --rate 50 \
  --system-rate 5 \
  --service-rate 10 \
  --subsystem-rate 2
```

That produces `PositionReport`, `SystemStatus`, `ServiceStatus`, and
`SubsystemStatus` messages with `MessageHeader.Mode` set to `SIMULATION`.
