# Aegis --- TinyML-Assisted Delay-Tolerant Networking with ESP32

## 1. Project Overview

**Aegis** is a proposed intelligent and resilient **Delay-Tolerant
Networking (DTN)** framework designed for resource-constrained embedded
devices.

The project combines:

- **Delay-Tolerant Networking (DTN)**
- **TinyML / Edge AI**
- **ESP32 embedded systems**
- **FreeRTOS**
- **Adaptive routing**
- **Self-healing communication**
- **Priority-aware bundle forwarding**
- **Resource-aware network decisions**
- **Secure communication**

The prototype will use **five ESP32 nodes** to form a small multi-hop
DTN testbed.

The central idea is to make routing decisions more intelligent than a
fixed or purely reactive routing strategy while keeping the complete
system lightweight enough to run on an ESP32.

---

## 2. Motivation

Many conventional communication systems assume that a continuous
end-to-end connection is available:

```text
Source ───────── Network ───────── Destination
```

This assumption does not hold in environments where links can disappear
or become unreliable.

Examples include:

- Disaster-response networks
- Remote IoT deployments
- Mobile ad-hoc networks
- Space and satellite communication
- Mission-critical communication
- Isolated or intermittently connected environments

In these situations, a node may be connected to one device at one moment
and disconnected from it later.

A DTN approach allows data to remain stored at an intermediate node
until a suitable forwarding opportunity becomes available.

Aegis extends this idea by introducing **lightweight AI-assisted
routing** so that a node can select a more suitable next hop based on
changing network and resource conditions.

---

## 3. Core Idea

The project can be summarized as:

> **A five-node ESP32-based DTN system in which TinyML assists next-hop
> selection using network and resource information, while
> Store-Carry-Forward, priority scheduling, and self-healing mechanisms
> improve communication reliability.**

The overall concept is:

```text
                  AEGIS
                    │
       ┌────────────┼────────────┐
       │            │            │
      DTN         TinyML      Embedded
   Networking       AI         Systems
       │            │            │
 Store-Carry-   Adaptive      ESP32
 Forward        Routing       FreeRTOS
       │            │            │
       └────────────┼────────────┘
                    │
          Self-Healing + Security
```

---

# 4. What Is Delay-Tolerant Networking?

**Delay-Tolerant Networking (DTN)** is a networking approach designed
for environments where communication links may be intermittent, delayed,
disrupted, or unavailable.

Unlike conventional networking, DTN does not require a continuous
end-to-end path.

Its fundamental mechanism is:

> **Store → Carry → Forward**

### Store

When a suitable next hop is unavailable, the node stores the data
locally.

### Carry

The node keeps the data while waiting for another communication
opportunity.

### Forward

When a suitable neighbor becomes available, the node forwards the data.

### Example

Suppose:

```text
Node 1 → Node 2 → Node 5
```

If Node 2 temporarily cannot communicate with Node 5:

```text
Node 1 → Node 2 → [Stored]
```

When connectivity returns:

```text
Node 2 → Node 5
```

The bundle can continue toward the destination.

This makes DTN suitable for networks where connectivity cannot be
guaranteed continuously.

---

# 5. Five-Node ESP32 Testbed

The physical prototype will contain **five ESP32 nodes**.

A logical arrangement can be represented as:

```text
                     ┌─────────────┐
                     │   Node 2    │
                     │    Relay    │
                     └──────┬──────┘
                            │
                            │
┌─────────────┐      ┌──────┴──────┐      ┌─────────────┐
│   Node 1    │──────│   Network   │──────│   Node 5    │
│   Source    │      │             │      │ Destination │
└──────┬──────┘      └──────┬──────┘      └─────────────┘
       │                    │
       │             ┌──────┴──────┐
       │             │   Node 4    │
       │             │    Relay    │
       │             └─────────────┘
       │
       └─────────────────────────────
                    Node 3
                   Relay/Alternate
```

The exact physical topology may be changed during testing.

The five nodes provide enough devices to demonstrate:

- Multi-hop communication
- Multiple next-hop choices
- Intermittent links
- Relay selection
- Node failures
- Alternate route selection
- Store-and-forward behavior
- Self-healing
- Resource-aware routing

---

# 6. Node Roles

Node Initial Role Main Responsibility

---

**Node 1** Source Generates data and DTN bundles
**Node 2** Relay Primary forwarding node
**Node 3** Relay Alternate forwarding path
**Node 4** Relay Additional relay/fallback path
**Node 5** Destination Receives and validates bundles

These roles are logical rather than permanent. Depending on the
experiment, relay availability and routing decisions can change
dynamically.

---

# 7. Why ESP32?

The project uses **ESP32** as the embedded platform because it provides
a useful combination of wireless communication capability, processing
resources, low cost, and FreeRTOS support.

Important characteristics for this project include:

- Built-in Wi-Fi
- Microcontroller-class processing
- Low-power operation
- Flash storage
- RAM suitable for lightweight embedded applications
- FreeRTOS support
- Suitable platform for TinyML experiments

The resource limitations of ESP32 are important to the research.

The objective is not simply to run AI on a powerful computer. The
project investigates whether a **lightweight routing model can operate
directly on a resource-constrained embedded node**.

---

# 8. Communication Technology

The initial prototype can use **Wi-Fi communication between ESP32
nodes**.

The communication layer is responsible for:

- Node-to-node communication
- Neighbor discovery
- Data transfer
- Link monitoring
- Bundle forwarding

The project deliberately introduces conditions where links may become
unavailable or unreliable so that the DTN behavior can be evaluated.

---

# 9. Why Five Nodes?

A two-node setup would only demonstrate:

```text
Node A → Node B
```

There is no meaningful routing choice.

With five nodes, the system can have multiple possible paths:

```text
              Node 2
             /      \
Node 1 ─────          ───── Node 5
             \      /
              Node 3
                 |
              Node 4
```

This makes it possible to evaluate:

- Which neighbor should be selected?
- What happens when the preferred relay fails?
- How does the system react to changing link quality?
- Can another route be selected?
- How does TinyML affect routing decisions?

Five nodes therefore provide a practical small-scale testbed for the
proposed approach.

---

# 10. TinyML Component

The main AI component of Aegis is **TinyML**.

TinyML refers to deploying lightweight machine-learning models on
resource-constrained devices such as microcontrollers.

Aegis does not require the ESP32 to train a large model.

Instead, the development flow is:

```text
Network Simulation / ESP32 Measurements
                  │
                  ▼
          Network Dataset
                  │
                  ▼
         Offline Model Training
                  │
                  ▼
       Model Evaluation/Selection
                  │
                  ▼
        Model Optimization
                  │
                  ▼
        TinyML Model Deployment
                  │
                  ▼
               ESP32
                  │
                  ▼
         Real-Time Inference
```

Training is performed offline on a more capable computer.

The ESP32 performs the lightweight inference required during operation.

---

# 11. What Does TinyML Do?

The TinyML model is intended to assist with **next-hop selection**.

When a node has multiple possible neighbors, it can evaluate their
current conditions.

Possible input features include:

- RSSI
- Latency
- Packet loss
- Battery level
- Buffer occupancy
- Trust/reliability score

Conceptually:

```text
RSSI ───────────────┐
Latency ────────────┤
Packet Loss ────────┤
Battery ────────────┤
Buffer ─────────────┤──► TinyML ──► Best Next Hop
Trust ──────────────┘
```

The exact model architecture will be selected after comparing
lightweight candidate models against embedded constraints.

---

# 12. Why Not Use a Large AI Model?

The ESP32 is a resource-constrained device.

A large model can require excessive:

- RAM
- Flash
- CPU time
- Energy

Therefore, Aegis focuses on lightweight machine-learning models.

The model-selection process should consider more than accuracy.

Important criteria include:

Criterion Importance

---

Prediction accuracy Correct routing decisions
Model size Flash usage
RAM usage Runtime memory
Inference time Routing response speed
CPU utilization Embedded processing load
Power consumption Battery efficiency

A slightly smaller model with adequate accuracy may be more suitable
than a larger model with marginally higher accuracy.

---

# 13. Possible Routing Features

## RSSI

Received Signal Strength Indicator represents the strength of the
received wireless signal.

A stronger signal can indicate a better link, but RSSI alone is not
enough to determine the best route.

## Latency

Measures communication delay.

Lower latency can be useful for time-sensitive bundles.

## Packet Loss

Indicates link reliability.

A link with high packet loss may be avoided even if its signal strength
is good.

## Battery Level

A relay with critically low battery may not be an appropriate forwarding
node.

## Buffer Occupancy

A node with an almost-full buffer may be a poor forwarding candidate.

## Trust / Reliability

A trust score can represent the observed reliability of a node and can
be used as an additional routing factor.

---

# 14. Adaptive Routing

Traditional routing may use fixed rules.

For example:

```text
Choose the first available neighbor
```

or:

```text
Choose the neighbor with the strongest signal
```

Aegis aims to consider several parameters together.

Example:

Parameter Node 2 Node 3 Node 4

---

RSSI -45 dBm -60 dBm -52 dBm
Latency 12 ms 25 ms 16 ms
Battery 85% 40% 90%
Buffer Usage 25% 75% 35%
Trust High Medium High

Node 2 may be selected even if Node 4 has higher battery, or Node 4 may
be selected if the combined conditions make it more suitable.

The purpose of TinyML is to learn this multi-parameter relationship
rather than relying on one fixed metric.

---

# 15. Neighbor Discovery

Each ESP32 needs to maintain information about neighboring nodes.

A heartbeat mechanism can be used.

A node periodically broadcasts a small heartbeat message.

Example:

```text
Node ID      : 2
Timestamp    : 10234
RSSI         : -48 dBm
Battery      : 84%
Buffer       : 30%
```

The receiving node updates its neighbor table.

A simplified neighbor table may look like:

Node ID RSSI Battery Buffer Last Seen Status

---

2 -48 84% 30% Recent Active
3 -62 42% 70% Recent Active
4 -55 90% 35% Old Inactive

If a node does not respond for a configured timeout period, it can be
marked unavailable.

---

# 16. Self-Healing Communication

Self-healing is the ability of the network to recover from a failed or
unavailable node without manual route configuration.

Example:

### Normal operation

```text
Node 1 → Node 2 → Node 5
```

### Node 2 fails

```text
Node 1 → X Node 2

Node 3 → Node 4 → Node 5
```

The system performs:

```text
Heartbeat timeout
        ↓
Detect failed node
        ↓
Remove failed node from candidate list
        ↓
Check available neighbors
        ↓
Collect current network parameters
        ↓
Run routing decision
        ↓
Select alternate path
        ↓
Resume forwarding
```

This combines DTN's resilience with adaptive routing.

---

# 17. Store-and-Forward Buffer

If no suitable next hop is currently available, the bundle is not
immediately discarded.

It is stored locally.

```text
Bundle generated
       ↓
No suitable link
       ↓
Local DTN buffer
       ↓
Wait for connectivity
       ↓
Neighbor becomes available
       ↓
Routing decision
       ↓
Forward bundle
```

This is one of the fundamental differences between DTN and ordinary
end-to-end communication.

---

# 18. Bundle Concept

DTN data can be treated as a **bundle** rather than a normal transient
packet.

A bundle can conceptually contain:

```text
Bundle ID
Source
Destination
Creation Timestamp
Priority
Payload
Security/Integrity Information
```

The bundle manager is responsible for maintaining these objects while
they are stored and forwarded.

---

# 19. Priority-Aware Bundle Scheduling

Not every bundle has equal importance.

Aegis can categorize bundles into priority levels.

### Critical

Examples:

- Emergency alerts
- Safety notifications
- Critical fault messages

### Normal

Examples:

- Routine sensor measurements
- Periodic telemetry

### Bulk / Low Priority

Examples:

- Logs
- Non-urgent files
- Background data

The queue can prioritize:

```text
Critical
   ↓
Normal
   ↓
Bulk
```

This becomes especially useful when the communication window is short.

---

# 20. Buffer Management

Because DTN nodes may remain disconnected for some time, bundles can
accumulate.

Each node therefore needs to monitor its available storage.

Important parameters include:

- Buffer capacity
- Current buffer usage
- Number of queued bundles
- Bundle priority
- Available storage

A node with 95% buffer usage may be a poor relay even if it has an
excellent wireless link.

This is another reason buffer occupancy can be provided to the TinyML
routing model.

---

### Portability: Arduino Now, ESP-IDF Later

The core project logic is intentionally separated from Arduino-specific APIs.

Portable modules will contain:

- `dtn/` — bundle handling, queues, ACKs, deduplication and replication
- `routing/` — candidate filtering and routing policies
- `neighbor/` — heartbeat processing, metrics and self-healing logic
- `ml/` — model interface and inference wrapper

Hardware-dependent code will be isolated behind:

- `transport/` — ESP-NOW and link/disruption control
- `hal/` — timing, ADC battery measurement, sleep and logging
- `storage/` — LittleFS persistence
- `security/` — mbedTLS integration

The goal is to keep the core C++ modules free from direct `Arduino.h` dependencies. This allows the same routing and DTN logic to be unit-tested on a PC and makes a future migration to ESP-IDF significantly easier.

# 21. FreeRTOS

ESP32 supports FreeRTOS, which can be used to divide the node firmware
into separate tasks.

A possible task structure is:

```text
ESP32
 │
 ├── Neighbor Discovery Task
 │
 ├── Network Monitoring Task
 │
 ├── TinyML Inference Task
 │
 ├── Bundle Management Task
 │
 ├── Communication Task
 │
 ├── Security Task
 │
 └── Logging / Monitoring Task
```

The final task structure will depend on resource measurements and
implementation requirements.

FreeRTOS provides:

- Task scheduling
- Priorities
- Inter-task communication
- Timing mechanisms
- Synchronization

This allows communication, monitoring, routing and other operations to
coexist on the ESP32.

---

# 22. Security Layer

Aegis also considers secure communication between nodes.

The proposed architecture includes encryption such as **AES-256** for
protecting data.

Conceptually:

```text
Application Data
       ↓
Create Bundle
       ↓
Encrypt
       ↓
Store / Forward
       ↓
Destination
       ↓
Decrypt / Verify
```

Security must be implemented with consideration for:

- CPU overhead
- Memory usage
- Key management
- Power consumption
- Communication overhead

The final cryptographic configuration will be validated during
implementation.

---

# 23. Complete Node Architecture

Each of the five ESP32 nodes can contain the following logical
components:

```text
┌───────────────────────────────────────┐
│              ESP32 NODE               │
│                                       │
│             FreeRTOS                  │
│                                       │
│ ┌───────────────────────────────────┐ │
│ │ Neighbor Discovery                │ │
│ └───────────────────────────────────┘ │
│                  │                    │
│ ┌───────────────────────────────────┐ │
│ │ Network Metrics                   │ │
│ │ RSSI / Latency / Loss / Battery   │ │
│ │ Buffer / Trust                    │ │
│ └───────────────────────────────────┘ │
│                  │                    │
│ ┌───────────────────────────────────┐ │
│ │ TinyML Routing Engine             │ │
│ └───────────────────────────────────┘ │
│                  │                    │
│ ┌───────────────────────────────────┐ │
│ │ DTN Bundle Manager                │ │
│ └───────────────────────────────────┘ │
│                  │                    │
│ ┌───────────────────────────────────┐ │
│ │ Priority Queue / Buffer           │ │
│ └───────────────────────────────────┘ │
│                  │                    │
│ ┌───────────────────────────────────┐ │
│ │ Security Layer                   │ │
│ └───────────────────────────────────┘ │
│                                       │
└───────────────────┬───────────────────┘
                    │
                 Wi-Fi
                    │
             Other ESP32 Nodes
```

---

# 24. End-to-End Workflow

The complete operation can be summarized as:

```text
1. Data is generated
        ↓
2. DTN bundle is created
        ↓
3. Bundle priority is assigned
        ↓
4. Bundle is secured
        ↓
5. Neighbor discovery runs
        ↓
6. Network parameters are collected
        ↓
7. TinyML evaluates candidate next hops
        ↓
8. Best available next hop is selected
        ↓
9. Bundle is forwarded
        ↓
10. If no link is available, bundle is stored
        ↓
11. If a relay fails, self-healing is triggered
        ↓
12. Alternate next hop is selected
        ↓
13. Bundle eventually reaches Node 5
        ↓
14. Delivery is recorded
```

---

# 25. Example Five-Node Scenario

Assume:

```text
Node 1 = Source
Node 2 = Relay A
Node 3 = Relay B
Node 4 = Relay C
Node 5 = Destination
```

Node 1 generates:

```text
Emergency Alert
Priority = Critical
```

Node 1 discovers Nodes 2 and 3.

It receives:

```text
Node 2:
RSSI = -45 dBm
Battery = 88%
Buffer = 25%
Trust = 0.95

Node 3:
RSSI = -52 dBm
Battery = 40%
Buffer = 75%
Trust = 0.82
```

The TinyML model evaluates the conditions and selects Node 2.

```text
Node 1 → Node 2
```

Node 2 forwards toward Node 5.

If Node 2 fails:

```text
Node 1 → X Node 2
```

The heartbeat timeout detects the failure.

Node 1 then considers Node 3 and Node 4.

The routing mechanism selects an alternative.

For example:

```text
Node 1 → Node 3 → Node 4 → Node 5
```

The bundle remains available throughout the disruption because of DTN's
store-and-forward mechanism.

---

# 26. Simulation and Hardware Development

The project can follow a two-stage approach.

## Stage 1 --- Simulation

The five-node network is modeled before full hardware integration.

Simulation can be used to test:

- Link failures
- Packet loss
- Variable latency
- Different node conditions
- Routing decisions
- Bundle delivery
- Buffer behavior
- Self-healing

## Stage 2 --- ESP32 Testbed

The algorithms are then deployed on five physical ESP32 nodes.

The physical testbed allows measurement of:

- Real RSSI
- Real latency
- Real packet loss
- RAM usage
- Flash usage
- CPU utilization
- TinyML inference time
- Power consumption
- Recovery time

The exact simulator and hardware measurement setup can be selected
during implementation.

---

# 27. TinyML Dataset

The project does not require an image or speech dataset.

The model requires **network telemetry**.

A sample record could contain:

```text
RSSI
Latency
Packet Loss
Battery
Buffer Occupancy
Trust Score
Candidate Node ID
Routing Label
```

Example:

    RSSI   Latency   Packet Loss   Battery   Buffer   Trust Label

---

     -45        10            1%       90%      20%    0.95 Node 2
     -61        23            4%       72%      45%    0.88 Node 3
     -54        17            2%       86%      30%    0.94 Node 4

The initial dataset can be produced using simulation and later enhanced
using measurements from the five ESP32 nodes.

---

# 28. Model Training Pipeline

The model-development pipeline is:

```text
             DATA GENERATION
                    │
        ┌───────────┴───────────┐
        │                       │
   Simulation              ESP32 Data
        │                       │
        └───────────┬───────────┘
                    ▼
              Dataset
                    │
                    ▼
             Preprocessing
                    │
                    ▼
              Model Training
                    │
                    ▼
             Model Evaluation
                    │
                    ▼
          Lightweight Model
                    │
                    ▼
       Quantization / Optimization
                    │
                    ▼
          TensorFlow Lite Micro
              / Embedded Model
                    │
                    ▼
                 ESP32
```

The final model should be selected based on both predictive performance
and embedded resource requirements.

---

# 29. Evaluation Metrics

The project should evaluate both **network performance** and **embedded
performance**.

## Network Metrics

### Packet Delivery Ratio

Percentage of generated bundles successfully delivered.

### End-to-End Delay

Time from bundle generation to delivery.

### Packet/Bundle Loss

Number or percentage of bundles that fail to reach the destination.

### Routing Overhead

Additional communication required for routing and control.

### Recovery Time

Time required to recover after a relay failure.

### Buffer Utilization

How efficiently node storage is used.

---

## Embedded Metrics

### RAM Usage

Memory required during operation.

### Flash Usage

Size of firmware and TinyML model.

### Inference Time

Time required to make a routing prediction.

### CPU Utilization

Processing load generated by the system.

### Power Consumption

Energy used during communication, monitoring and inference.

---

# 30. Baseline Comparison

To evaluate whether the proposed approach is useful, it should be
compared with a simpler routing strategy.

### Baseline

A fixed or rule-based routing strategy.

### Proposed

TinyML-assisted adaptive routing.

The comparison can include:

Metric Baseline TinyML-Based

---

Delivery Ratio Measure Measure
Delay Measure Measure
Packet Loss Measure Measure
Recovery Time Measure Measure
Buffer Usage Measure Measure
Routing Overhead Measure Measure
RAM Usage Measure Measure
Inference Time N/A Measure
Power Consumption Measure Measure

The final results should be based on actual experiments rather than
assumed improvements.

---

# 31. Research Gap

The individual technologies used in Aegis are established:

- DTN is established for disruption-tolerant communication.
- TinyML enables machine learning on resource-constrained devices.
- ESP32 provides an embedded wireless platform.
- Self-healing techniques can provide network recovery.

The project focuses on the **integration of these concepts**.

The research direction is:

> **Developing and evaluating lightweight AI-assisted DTN routing on
> resource-constrained ESP32 nodes while considering dynamic link
> quality, energy, buffer availability, reliability and autonomous
> recovery.**

The important engineering question is whether this integrated approach
provides useful routing improvements without creating excessive embedded
resource overhead.

---

# 32. Proposed Project Architecture

The complete five-node system can be represented as:

```text
                        ┌──────────────┐
                        │   Node 2     │
                        │    ESP32     │
                        │    Relay     │
                        └──────┬───────┘
                               │
                               │
┌──────────────┐        ┌──────┴───────┐        ┌──────────────┐
│    Node 1    │        │   Dynamic    │        │    Node 5    │
│    ESP32     │◄──────►│   Network    │◄──────►│    ESP32     │
│    Source    │        │              │        │ Destination  │
└──────┬───────┘        └──────┬───────┘        └──────────────┘
       │                        │
       │                 ┌──────┴───────┐
       │                 │    Node 4    │
       │                 │     ESP32    │
       │                 │     Relay    │
       │                 └──────────────┘
       │
       │
┌──────┴───────┐
│    Node 3    │
│    ESP32     │
│    Relay     │
└──────────────┘

Each node:
ESP32 + FreeRTOS + DTN Bundle Manager
       + Neighbor Discovery
       + TinyML Routing
       + Buffer/Priority Manager
       + Security
```

---

# 33. Main Components

---

Component Purpose

---

**ESP32** Embedded hardware platform

**Wi-Fi** Initial wireless communication
medium

**FreeRTOS** Task scheduling and concurrency

**DTN Bundle Manager** Store, carry and forward data

**Neighbor Discovery** Detect available nodes

**TinyML Engine** Adaptive routing decision

**Buffer Manager** Store pending bundles

**Priority Scheduler** Forward important data first

**Self-Healing Module** Recover from node/link failure

**Security Layer** Protect transmitted/stored data

**Simulation Environment** Validate algorithms
before/alongside hardware

---

---

# 34. Project Development Flow

```text
Problem Definition
       ↓
Literature Survey
       ↓
Research Gap Identification
       ↓
System Architecture
       ↓
DTN Routing Design
       ↓
Five-Node Simulation
       ↓
Dataset Generation
       ↓
TinyML Model Selection
       ↓
Model Training
       ↓
Model Optimization
       ↓
ESP32 + FreeRTOS Implementation
       ↓
Five-Node Hardware Integration
       ↓
Self-Healing Testing
       ↓
Performance Evaluation
       ↓
Optimization
       ↓
Final Demonstration
```

---

# 35. Expected Demonstration

A complete demonstration can show the following sequence:

1.  Five ESP32 nodes are powered on.
2.  Nodes discover their neighbors.
3.  Node 1 generates a bundle.
4.  The bundle receives a priority.
5.  Network conditions are collected.
6.  TinyML assists in selecting a next hop.
7.  The bundle is forwarded through relay nodes.
8.  One relay is intentionally made unavailable.
9.  The failure is detected using heartbeat information.
10. The system selects an alternate route.
11. The stored bundle continues forwarding.
12. Node 5 receives the bundle.
13. Delivery and recovery statistics are displayed.

This demonstrates the main project concepts in a single experiment.

---

# 36. What the Project Is and Is Not

## The project is

- A five-node embedded DTN prototype
- ESP32-based
- TinyML-assisted
- Resource-aware
- Designed for intermittent connectivity
- Focused on adaptive routing
- Designed to demonstrate self-healing
- Evaluated through simulation and hardware experiments

## The project is not

- A replacement for the Internet
- A large-scale production DTN deployment
- A cloud-based AI system
- A large neural-network platform
- A claim that every DTN problem is solved

The focus is a **research prototype and experimental framework**.

---

# 37. Key Research Contributions Intended

The project aims to demonstrate:

1.  **A five-node ESP32 DTN testbed** for intermittent communication.
2.  **TinyML-assisted next-hop selection** using multiple
    network/resource features.
3.  **Self-healing route recovery** after relay failure.
4.  **Priority-aware DTN bundle forwarding**.
5.  **Resource-aware routing** considering battery and buffer
    conditions.
6.  **Embedded implementation using ESP32 and FreeRTOS**.
7.  **Simulation-to-hardware evaluation** of the proposed approach.
8.  **Measurement of network and embedded resource performance**.

These should be treated as project objectives and intended contributions
until validated experimentally.

---

# 38A. Development Toolchain Summary

The intended development environment is:

```text
PlatformIO
   │
   ├── Arduino-ESP32 core 3.x
   │       └── ESP-IDF 5.x based
   │
   ├── FreeRTOS
   ├── ESP-NOW
   ├── mbedTLS AES-GCM
   ├── LittleFS
   └── emlearn
          │
          ▼
       Base ESP32
          │
          ▼
      ESP32-S3 later

Python + SimPy
   │
   ├── DTN simulation
   ├── baseline evaluation
   ├── dataset generation
   └── model evaluation
```

The repository should pin the exact PlatformIO platform/core versions and record them in `docs/` so that the firmware can be reproduced later.

---

# 38. Final Summary

Aegis brings together **DTN, TinyML and embedded systems** into a
five-node ESP32 prototype.

The DTN layer allows bundles to survive intermittent connectivity using
**Store-Carry-Forward**.

The TinyML layer assists the node in choosing a suitable next hop using
parameters such as:

- RSSI
- Latency
- Packet loss
- Battery level
- Buffer occupancy
- Trust/reliability

The FreeRTOS layer organizes the embedded tasks.

The self-healing mechanism detects unavailable relays and enables route
recovery.

The priority mechanism ensures that important bundles receive
preferential forwarding.

The buffer manager stores data during disconnections.

The security layer protects data during storage and communication.

The five-node testbed provides a practical environment for testing
multi-hop routing, link disruption, alternate paths, and resource-aware
decision making.

The overall objective is to investigate whether **lightweight
AI-assisted routing can improve DTN behavior while remaining practical
on resource-constrained ESP32 hardware**.

---

## 39. One-Line Project Definition

> **Aegis is a five-node ESP32-based intelligent DTN framework that uses
> TinyML-assisted, resource-aware next-hop selection together with
> Store-Carry-Forward, priority scheduling and self-healing to support
> reliable communication over intermittent networks.**
