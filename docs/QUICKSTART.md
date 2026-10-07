# Aegis ESP32 Quick Start

## Hardware and radio settings

- Use five ESP32 boards supported by the `esp32doit-devkit-v1` PlatformIO board definition.
- All nodes use ESP-NOW over the 2.4 GHz station interface on Wi-Fi channel 6. Keep the nodes on the same channel and close any nearby Wi-Fi access point operating on channel 6 during controlled experiments.
- Node IDs are assigned by the PlatformIO environment: `node1` through `node5`. Node 5 advertises itself as the initial destination.
- ESP-NOW discovers radio peers automatically. A neighbor must be in radio range to appear in the status output.

## Build and flash

Install PlatformIO Core 6.2.0 or newer, or the PlatformIO IDE extension with a matching Core. The project pins the pioarduino platform package in `platformio.ini` to provide Arduino-ESP32 3.x and ESP-IDF 5.x while retaining the Arduino framework. `scripts/xtensa_toolchain.py` adds the package's nested compiler directory to the build path.

Build one node image:

```sh
pio run -e node1
```

Flash each board with the corresponding environment, substituting the actual serial port:

```sh
pio run -e node1 -t upload --upload-port COM5
pio run -e node2 -t upload --upload-port COM6
pio run -e node3 -t upload --upload-port COM7
pio run -e node4 -t upload --upload-port COM8
pio run -e node5 -t upload --upload-port COM9
```

Open serial monitors at 115200 baud. The node prints its ID and station MAC at boot, then emits machine-readable `event=...` records. Node IDs are compile-time settings; do not flash the same environment to multiple boards.

## Serial commands

```text
help
status
send <node 1..5> <critical|normal|bulk> <message>
```

Example from Node 1:

```text
send 5 critical evacuation-zone-east
```

The message is held in the bounded RAM queue until the next hop acknowledges it. A relay acknowledges only after it has accepted the bundle into its own queue. Node 5 prints the delivery record and acknowledges the final hop. If no route is available, the bundle remains queued until a neighbor advertises a route, the bundle expires, or power is lost.

## Suggested first run

1. Flash each board with a unique node environment and open all five serial monitors.
2. Wait for `event=neighbor` records and check that the topology gives Node 1 a route to Node 5.
3. Send a `normal` bundle from Node 1 to Node 5 and follow its `bundle_forwarded`, `bundle_buffered`, and `bundle_delivered` records.
4. Power off a relay and observe neighbor timeout and route changes. Power it back on and wait for beacons before sending again.
5. Use `status` to capture queue, route cost, RSSI, retry, delivery, drop, malformed-frame, and free-heap counters.

This is the first firmware milestone, not yet a validated experiment. The queue is RAM-only and resets lose pending bundles. The current route metric is hop count with RSSI as a tie-breaker. The CRC detects accidental corruption; it does not authenticate senders or encrypt data. Record board revisions, firmware/platform versions, topology, test duration, and repeats before reporting research results.
