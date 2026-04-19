# arcal-busmon

`arcal-busmon` is a lightweight DDS bus monitor for ARCAL's opaque UCI payload
topics. It discovers live ARCAL topics, decodes payloads through the generated
CDR and JSON externalizers, writes per-topic JSON logs, and can stream message
metadata to the browser UI.

![arcal-busmon streaming UCI status messages](images/busmon-ui.png)

## Quick Start

Build the monitor:

```bash
cmake --build build
```

Start the web UI:

```bash
./busmon-ui.sh
```

For stream-level diagnostics:

```bash
./busmon-ui.sh --debug-stream
```

The UI defaults to port `8765` and logs decoded JSON under `/tmp/busmon-out`.

## Demo Publisher

The ARCAL demo publisher can feed the UI with basic platform/status traffic:

```bash
cd ../arcal
ninja -C build e2e_pub_continuous
CYCLONEDDS_URI="file://$PWD/test/e2e/cyclonedds_localhost.xml" \
  ./build/test/e2e/e2e_pub_continuous \
    --rate 50 \
    --system-rate 5 \
    --service-rate 10 \
    --subsystem-rate 2
```

That produces `PositionReport`, `SystemStatus`, `ServiceStatus`, and
`SubsystemStatus` messages with `MessageHeader.Mode` set to `SIMULATION`.
