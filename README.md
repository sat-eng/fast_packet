# FastPacket

FastPacket is a small Qt6 GUI tool for testing UDP multicast network throughput and packet loss. It sends UDP packets at a configurable data rate to a multicast (or unicast) destination, and separately receives them on another instance — measuring arrival rate and packet loss in real time with a live scrolling graph.

This is a Qt6/CMake port of an earlier Visual C++ application; only the current codebase is covered here.

## Features

- Configurable destination address, port, packet size, and target data rate (Mbit/s)
- Optional randomized packet payload
- Live measured throughput, packet count, loss count, and sender address
- Scrolling loss-percentage graph, toggleable via the `>>` / `<<` button
- Adjustable OS thread priority for the active send/receive thread
- Auto-populated list of local IPv4 interfaces to bind to

## Architecture

The app is a single top-level `QWidget` (`MainWindow`) that owns two `QThread` subclasses and a custom graph widget. There are five classes, all under `src/`:

| Class | File | Role |
|---|---|---|
| `MainWindow` | `MainWindow.h/.cpp` | Top-level widget; builds the UI, owns the sender/receiver threads and the graph, wires up signals/slots |
| `PacketSender` | `PacketSender.h/.cpp` | `QThread` subclass; tight-loop UDP sender with rate control |
| `PacketReceiver` | `PacketReceiver.h/.cpp` | `QThread` subclass; UDP/multicast receiver with loss detection |
| `LossGraph` | `LossGraph.h/.cpp` | Custom `QWidget`; scrolling `QPainter` graph of loss % over time |

`MainWindow` is mutually exclusive between transmit and listen modes — only one thread runs at a time, started via the **Transmit** or **Listen** button and stopped via **Stop**.

### Threading model

Both `PacketSender` and `PacketReceiver` subclass `QThread` and override `run()`. Shutdown is cooperative via `std::atomic<bool> m_stop`, set by `requestStop()` and polled from inside the loop — there is no forced termination. `PacketReceiver` uses a 100 ms `waitForReadyRead` timeout inside its read loop specifically so it can check `m_stop` even when no data is arriving. Signals emitted from `run()` (which executes on the worker thread) are automatically delivered as queued connections to `MainWindow`'s slots on the main/UI thread — no manual marshalling is needed.

### Sender (`PacketSender`)

- Binds the UDP socket to the selected local interface (or `0.0.0.0` for "any"), sets the outbound multicast interface when a specific one is selected, and disables multicast loopback.
- The first 4 bytes of every packet are a `quint32` sequence number, incremented after each send, starting at 0.
- Inter-packet delay is computed once from `(packetBits) / (dataRateBps)`, then re-measured and adjusted every batch of 50 packets by the ratio `measuredRate / targetRate` — this keeps the achieved rate converging on the target even as scheduling jitter accumulates.
- The delay is enforced with a `std::chrono::steady_clock` busy-wait spin loop rather than a sleep, trading CPU usage for timing precision at high packet rates.
- `dataRateUpdated`/`delayNsUpdated` signals are rate-limited to ~5 Hz so the UI isn't flooded at high send rates.

### Receiver (`PacketReceiver`)

- Binds to `INADDR_ANY` on the configured port and, for a multicast destination, joins the group via `QUdpSocket::joinMulticastGroup` (on a specific interface if one was selected, otherwise the OS default).
- Reads the 4-byte sequence number from each datagram to detect gaps and count lost packets. A sequence number that is lower than or equal to the last one seen is treated as a new sender session and restarts tracking (handles the sender being stopped and restarted).
- Emits `statsUpdated` every 100 packets, or at least every 200 ms even at low packet rates, so the UI never appears to freeze.

### Loss graph (`LossGraph`)

A fixed-size scrolling history (`kMaxSamples = 200`) of per-interval loss percentages, painted directly with `QPainter` in `paintEvent`. `MainWindow` feeds it a new sample from each `statsUpdated` signal, computing the delta packet/loss counts since the previous update.

## Packet format

```
byte 0-3   : quint32 sequence number, native/little-endian, starts at 0, incremented per send
byte 4-N   : payload (zero-filled, or randomized if "Random payload" is checked)
```

The receiver only relies on the first 4 bytes; there is no other framing.

## Build

Requires Qt 6 (`Core`, `Widgets`, `Network`) and CMake 3.20+.

```bash
# macOS (Homebrew Qt6)
cmake -B build -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt6
cmake --build build
```

On Windows, point `CMAKE_PREFIX_PATH` at a vcpkg or official Qt6 install instead. No other dependencies are required.

## Usage

1. Pick a local **Interface** to bind to (or leave "0.0.0.0 (any)").
2. Set the **Destination** address/port and **Packet** size/data rate.
3. Click **Transmit** on one machine and **Listen** on another (or a second instance) pointed at the same destination/port.
4. Watch measured rate, packet count, and loss update live; click **Stop** to end the session.
5. Click `>>` to reveal the scrolling loss graph.

### Testing the receiver without a second FastPacket instance

`tools/fastpacket_sender.py` is a standalone Python script that emits UDP multicast packets in the same format FastPacket's receiver expects (little-endian `quint32` sequence number in the first 4 bytes). It's useful for testing the receiver from a machine that isn't running FastPacket at all — e.g. a Linux box on the same network.

```bash
./tools/fastpacket_sender.py --group 225.10.100.100 --port 105 --rate-mbps 2.0
```

Pass `--loss-rate 0.1` to randomly drop ~10% of packets (the sequence number still advances on a dropped packet, so the receiver's gap detection sees it as loss) — useful for verifying the loss-counting logic against a known rate. Run `./tools/fastpacket_sender.py --help` for the full list of flags.

## License

This project is licensed under the MIT License.

Copyright © 2026 Satish Kunapuli.
See the [LICENSE](LICENSE) file for details.