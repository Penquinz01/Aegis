# Aegis ESP32 Quick Start

## Hardware and radio settings

- Use up to five ESP32 boards supported by the `esp32doit-devkit-v1` PlatformIO board definition. A smaller subset is enough for a smoke test.
- All nodes use ESP-NOW over the 2.4 GHz station interface on Wi-Fi channel 6. Keep the nodes on the same channel and close any nearby Wi-Fi access point operating on channel 6 during controlled experiments.
- Node IDs are assigned by the PlatformIO environment: `node1` through `node5`. Every node advertises route costs to all five node IDs, so any powered node can be a destination. Node 5 remains the example destination in the commands below.
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
model on|off
sim rssi <peer 1..5> <-110..-20|auto>
```

Example from Node 1:

```text
send 5 critical evacuation-zone-east
```

Node 5 is only an example destination. For a three-node chain such as Node 1 ↔ Node 2 ↔ Node 3, Node 1 can send through Node 2 to Node 3:

```text
send 3 normal hello-through-relay
```

The message is held in the bounded RAM queue until the next hop acknowledges it. A relay acknowledges only after it has accepted the bundle into its own queue. The destination node prints the delivery record and acknowledges the final hop. If no route is available, the bundle remains queued until a neighbor advertises a route, the bundle expires, or power is lost.

The routing policy chooses the strongest-RSSI eligible neighbor first. The `tiny_rssi_first` mode uses route cost and buffer occupancy only to break exact RSSI ties; `model off` uses route cost and node ID for ties. The current integer tie-breaker is hand-set, not trained from data. At the source, the destination itself is excluded for the first send so a relay is required. This needs at least one reachable relay with a route to the destination; if none is available, the bundle waits instead of taking a direct shortcut. `model on` is the default.

To emulate a weak link from this node to a discovered peer, run `sim rssi 2 -95` on that node's serial console. The node substitutes that RSSI in its local routing table while still reporting actual packet reception and radio behavior. Use `sim rssi 2 auto` to return to measured RSSI. This does not create a radio outage or packet loss; it isolates the routing response to a low-signal feature. Use real attenuation or distance changes to measure physical link degradation.

## Live dashboard

Open `dashboard/index.html` from a local web server in Chrome or Edge, for example:

```sh
python -m http.server 8000
```

Then visit `http://localhost:8000/dashboard/`. Close any other serial monitor using a board, select **Connect ESP32 serial**, and choose its USB serial port. Repeat for each board. The dashboard shows serial-connected nodes and radio neighbors; **Next-hop trace** groups forwarding events by bundle ID and displays the observed path. Use **Send a bundle** to choose a connected source node, destination, priority, and message. Browser Web Serial requires a supported browser and a secure context such as localhost.

## Suggested first run

1. Flash each available board with a unique node environment and connect each serial stream. Node IDs can be any unique values from 1 through 5.
2. Wait for `event=neighbor` records. Each peer should report advertisements for destinations 1 through 5; the `status` line includes a `routes=` field with the current route cost to every node.
3. Send a `normal` bundle from Node 1 to any powered-on node and follow its `bundle_forwarded`, `bundle_buffered`, and `bundle_delivered` records.
4. Power off a relay and observe neighbor timeout and route changes. Power it back on and wait for beacons before sending again.
5. Use `status` to capture queue, route cost, RSSI, retry, delivery, drop, malformed-frame, and free-heap counters.

This is the first firmware milestone, not yet a validated experiment. The queue is RAM-only and resets lose pending bundles. The current forwarding policy prioritizes strongest RSSI among eligible neighbors; route cost and buffer are tie-break features. The CRC detects accidental corruption; it does not authenticate senders or encrypt data. Record board revisions, firmware/platform versions, topology, test duration, and repeats before reporting research results.
