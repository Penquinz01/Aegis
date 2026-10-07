# Aegis v1 Packet Format

All multibyte integer fields are little-endian. Packets are serialized field by field; the implementation never transmits a compiler-dependent C++ struct. The current maximum payload is 200 bytes, keeping the complete packet below the 250-byte ESP-NOW v1 limit.

| Field | Bytes | Meaning |
| --- | ---: | --- |
| Magic | 2 | `AE 61` |
| Version | 1 | `1` |
| Type | 1 | Beacon `1`, bundle `2`, ACK `3` |
| Origin ID | 1 | Original node that created the bundle (or sender for control packets) |
| Last-hop ID | 1 | Node that transmitted this packet directly |
| Destination ID | 1 | Bundle destination; beacon destination is the advertised route target |
| Priority | 1 | Bulk `0`, normal `1`, critical `2` |
| Hop limit | 1 | Remaining forwarding budget |
| Sequence | 2 | Per-node diagnostic/transmission sequence |
| Bundle ID | 8 | Stable ID used for deduplication and acknowledgements |
| TTL seconds | 4 | Remaining bundle lifetime, reduced while queued at each node |
| Visited mask | 4 | Bit `nodeId - 1` is set for nodes already traversed |
| Payload length | 2 | Number of payload bytes |
| Payload | 0–200 | Type-specific content |
| CRC-16/CCITT-FALSE | 2 | Accidental-corruption check over all preceding packet bytes |

## Type-specific payloads

- **Beacon:** three bytes: advertised hop cost (`255` means unknown), queue usage in percent, and free queue slots. A node refreshes its neighbor entry after every valid beacon and expires neighbors after seven seconds.
- **Bundle:** application bytes. A relay stores the packet before returning a hop acknowledgement. Forwarding chooses a live neighbor advertising the same destination with the lowest hop cost, using stronger RSSI to break ties and excluding nodes already in the visited mask.
- **ACK:** no payload. The bundle ID identifies the bundle acknowledged; destination ID is the preceding hop.

The CRC is not a security mechanism. The initial firmware does not configure ESP-NOW peer keys, authenticate beacon identities, encrypt bundles, or persist bundles. Those require a defined provisioning and threat model before production use.
