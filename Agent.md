# Aegis Agent Guidance

## Project context

- This is ESP32 firmware built with PlatformIO and the Arduino framework. The configured environment is `esp32doit-devkit-v1` in `platformio.ini`.
- Read `Readme.md` for the intended research prototype and `PLAN.md` for staged implementation priorities.
- Treat README components and expected results as goals, not implemented behavior, until the source and repeatable experiments confirm them.
- Preserve the PlatformIO Arduino framework and configured board unless the user requests a change or the hardware requirements make one necessary.

## C++ and Arduino conventions

- Use modern C++ supported by the pinned PlatformIO toolchain, while keeping code compatible with the selected Arduino-ESP32 core.
- Use four spaces for indentation, one statement per line, and braces on the same line as declarations/control statements. Keep lines readable and avoid clever macros.
- Use `PascalCase` for types, `camelCase` for functions and variables, and `UPPER_SNAKE_CASE` only for compile-time constants/macros where appropriate. Follow an established local convention when one exists.
- Prefer `constexpr` or typed constants to preprocessor constants. Keep macros for conditional compilation and unavoidable platform integration.
- Include only the headers needed by a translation unit. Use `#pragma once` for project headers unless existing code establishes another convention.
- Declare interfaces in headers and define non-trivial implementation in `.cpp` files. Keep headers self-contained and avoid importing Arduino headers transitively when not required.
- Mark functions `const`, `noexcept`, or `[[nodiscard]]` when the contract supports it. Use `enum class` for closed sets and fixed-width integer types for protocol fields.
- Avoid hidden global mutable state. Encapsulate state in small components and make ownership/lifetime clear.
- Prefer fixed-capacity or bounded containers for data on constrained nodes. Avoid unbounded allocation and repeated heap allocation in long-running paths; document any intentional dynamic allocation.
- Use RAII where practical, but do not introduce abstractions that obscure hardware ownership or resource limits.

## Embedded and concurrency rules

- Keep callbacks (including Wi-Fi/ESP-NOW callbacks and ISRs) short: validate/copy minimal data, enqueue work, and return. Do not perform blocking I/O, lengthy parsing, or inference inside a callback.
- Use FreeRTOS tasks, queues, or synchronization primitives only where concurrency is needed. Define task stack sizes, priorities, queue capacities, and ownership explicitly.
- Do not call blocking `delay()` in code responsible for communication progress. Prefer elapsed-time scheduling with `millis()` or a task delay when blocking that task is intentional.
- Protect shared state with the narrowest suitable synchronization. Do not hold locks while performing network, filesystem, serial, or other slow operations.
- Check return values from hardware, storage, and networking APIs. Make errors visible through diagnostics and define whether the operation retries, drops, or falls back.
- Bound CPU work per loop/task iteration so message bursts cannot indefinitely starve housekeeping, routing, or watchdog progress.
- Keep hardware-specific code behind small interfaces where this improves portability or enables simulation; avoid a generic framework without a concrete need.

## Protocol, storage, and routing

- Make on-air formats explicit, bounded, and versioned. Do not transmit raw compiler-dependent structs as a wire format; serialize fields with defined widths and byte order.
- Validate frame size, field ranges, protocol version, destination, and integrity before accepting data. Treat all received data as untrusted.
- Give bundles stable IDs and define duplicate handling, hop limits, expiry, acknowledgement semantics, retry limits, and queue-full behavior.
- Define maximum payload and storage capacity centrally. Check arithmetic for overflow before allocating or copying based on received lengths.
- Keep transport, bundle lifecycle, queue/storage, and routing logic separated behind narrow interfaces.
- Implement a deterministic, measurable routing baseline before TinyML. Provide safe fallback behavior for missing/stale features or inference failures.
- Do not describe a checksum as security. Select encryption/authentication only after documenting the threat model, key provisioning, replay protection, and key lifecycle.

## Dependencies and configuration

- Keep `platformio.ini` reproducible; pin platform/framework/library versions when the project is ready to freeze them.
- Before adding a dependency, confirm it supports the selected PlatformIO platform, Arduino framework, and target board. Record why it is needed.
- Keep secrets and per-device credentials out of source control. Do not add default production keys or print secrets in logs.
- Put node identity and experiment parameters in a clear configuration mechanism rather than scattering node-specific constants through the implementation.

## Diagnostics and documentation

- Use consistent, parseable diagnostic fields for node ID, event, bundle ID, and relevant counters. Avoid excessive logs in timing-sensitive paths.
- Document units, valid ranges, update rates, and stale-data behavior for routing features such as RSSI, latency, packet loss, battery, buffer occupancy, and trust.
- Document topology, board/core versions, upload settings, experiment procedure, and known limitations so results can be reproduced.
- Keep comments focused on reasoning, constraints, or non-obvious behavior; do not narrate code that is already clear.
- Report measured results as measurements and distinguish them from design goals or hypotheses.

## Build and verification

- Use the configured PlatformIO environment, for example `pio run -e esp32doit-devkit-v1`.
- For firmware changes, build the affected environment and summarize the result. Hardware behavior must be identified as unverified when it was not exercised on devices.
- Do not add or run tests unless the user asks for testing/verification; when asked, prioritize focused checks relevant to the change.
- Keep changes scoped and avoid unrelated cleanup while implementing a feature.
