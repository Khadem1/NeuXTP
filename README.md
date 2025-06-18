# NeuXTP
##  **NeuXTP** (Neuro-Accelerated eXtreme Transport Protocol)

###  Hybrid Concept:

A high-performance, AI-augmented transport protocol that maintains XTP’s line-rate throughput but adds dynamic decision-making from AI models embedded in SmartNIC or software fast-path.

###  **Implementation Plan**

#### 1. **Foundation: XTP (High PPS Baseline)**

- DPDK-based L2/L3 fast-path protocol
- Lightweight custom header (already done)
- Zero-copy buffers, NUMA-aware TX queues

#### 2. **Augment: NeuNIC Intelligence**

- Add a simple AI/ML classifier (Python/TFLite) to:
  - Prioritize packets (e.g., based on payload content or QoS tag)
  - Decide compression (Zstd on text/video, drop junk)
  - Flag anomalies (e.g., sudden flood of certain patterns)
- Embed logic in user-space for now (later move to NIC or FPGA)

#### 3. **Runtime Feedback Loop**

- Use shared memory ring for real-time AI hints to DPDK loop
- Sampled packets → AI engine → tag/predict → DPDK fast-path adjusts

```c
if (ai_predict_priority(pkt) > THRESH) {
    route_to_high_prio_queue(pkt);
} else {
    drop_or_defer(pkt);
}
```

#### 4. **Performance Monitoring**

- Enhanced logging of:
  - PPS
  - Avg CPU utilization per core
  - Latency histogram
  - AI inference count / ms

#### 5. **Table Output + CLI Interface**

```text
Protocol   Cores   Payload   Avg PPS   CPU%   Latency   AI-Hits/s
NeuXTP       2      64B      6.0M      75%     2.4µs        3000
```
###  Minimal AI Module (PoC)

```python
# ai_predictor.py
def ai_predict_priority(payload):
    if b'urgent' in payload:
        return 0.95
    elif b'video' in payload:
        return 0.7
    else:
        return 0.3
```

Called in DPDK loop via Python-C binding or subprocess for simplicity in PoC.

### Output Example

| Protocol   | Cores | Payload | Avg PPS | CPU % | Latency | AI Hits/s |
| ---------- | ----- | ------- | ------- | ----- | ------- | --------- |
| **XTP**    | 1     | 64B     | 6.1M    | 80%   | 2.3µs   | N/A       |
| **NeuXTP** | 2     | 64B     | 5.8M    | 78%   | 2.4µs   | 3,100     |
| **UDP**    | 1     | 64B     | 5.7M    | 78%   | 2.5µs   | N/A       |
| **TCP**    | 1     | 64B     | 5.6M    | 90%   | 3.1µs   | N/A       |

---

###  Next Steps

1. Reuse XTP DPDK benchmarking
2. Plug in Python-based AI predictor
3. Add AI hooks into DPDK TX/RX loop
4. Benchmark NeuXTP vs. XTP/TCP/UDP

---

##  How NeuXTP Helps AI Workloads (No RDMA Needed)

### NeuXTP Fusion:

- Custom transport protocol (XTP)
- DPDK acceleration
- AI logic (SmartNIC/CPU-assisted)

###  AI Workload Benefits

| AI Workload                          | NeuXTP Benefit                               |
| ----------------------------------- | -------------------------------------------- |
| Distributed AI training             | Low latency + packet prioritization          |
| Model inference serving             | Predictable, low-latency                     |
| Real-time data (camera/LiDAR)       | Fast scheduling, jitter minimization         |
| Edge AI coordination                | Mesh-aware, latency-sensitive routing        |
| Telemetry/gradient prioritization   | AI-annotated data gets higher priority       |


###  AI-Aware Packet Prioritization

NeuXTP can tag:

- `XTP-AI-Priority`
- `Model-ID` / `Stream-ID`
- `Latency-SLA`

Use cases:

- Drop stale packets (old video frames)
- Prioritize model-critical traffic
- Segregate background traffic from real-time inference

###  Fine-Grained Feedback Loop

XTP's custom design enables:

- AI-specific control messages
- Per-packet tensor/context metadata
- In-network prefiltering/compression

###  Mesh-Optimized for AI Clusters

- Deterministic multi-NIC routing
- AI-cluster-aware sharding
- Integration with inference orchestrators

###  Programmable AI Hooks

- Embed AI scoring in header
- `neux_route()` to AI-route traffic
- SLA/load-based dynamic weighting

```c
struct xtp_hdr {
    uint8_t priority_level;
    uint8_t task_class;
    uint32_t flow_id;
    uint8_t ai_sla_hint;
};
```

```c
if (xtp_hdr->priority_level > 5)
    forward_to_fast_queue();
else
    queue_for_background();
```
##  NeuXTP Header Format

```c
struct neuxtp_hdr {
    uint8_t version;
    uint8_t flags;
    uint16_t length;

    uint16_t session_id;
    uint8_t ai_tag;
    uint8_t priority;

    uint32_t checksum;
} __attribute__((__packed__));
```

Supports:

- AI prioritization
- Queueing logic
- SmartNIC/FPGA offload


##  AI Benchmarking Flow

| Mode        | Description                     | Payload    | Examples             |
| ----------- | ------------------------------- | ---------- | -------------------- |
| **Batch**   | Dense tensors                   | 512B–8KB   | Transformer inputs   |
| **Real-Time** | Token-by-token inference       | 16B–256B   | Chat streaming       |
| **Control** | RPC/scheduling                  | <128B      | Prompt management    |


##  NeuXTP Mesh Simulation

### Topology:

```
 +---------+       +---------+
 | XPU-0   | <-->  | XPU-1   |
 +---------+       +---------+
      |                 |
      v                 v
 +---------+       +---------+
 | XPU-2   | <-->  | XPU-3   |
 +---------+       +---------+
```

- Each core/NIC = mesh node
- AI tags/priorities influence routing
- Simulates smart cluster fabric

#### Results
```
 ./neuxtp_mesh_sim -l 0-3 -a 0000:03:00.0 -a 0000:03:00.1 --huge-dir=/mnt/huge
EAL: Detected CPU lcores: 24
EAL: Detected NUMA nodes: 2
EAL: Detected shared linkage of DPDK
EAL: Multi-process socket /var/run/dpdk/rte/mp_socket
EAL: Selected IOVA mode 'VA'
EAL: 1024 hugepages of size 2097152 reserved, but no mounted hugetlbfs found for that size
EAL: VFIO support initialized
EAL: Probe PCI driver: mlx5_pci (15b3:101d) device: 0000:03:00.0 (socket 0)
EAL: Probe PCI driver: mlx5_pci (15b3:101d) device: 0000:03:00.1 (socket 0)
TELEMETRY: No legacy callbacks, legacy socket not created
Initialized port 0
Initialized port 1
[Core 1] Sending NeuXTP packets on port 0 queue 0
[Core 2] Sending NeuXTP packets on port 1 queue 1
[Core 3] Sending NeuXTP packets on port 0 queue 2

==== Packet Stats (every 2s) ====
Core 1: Port 0 | TX: 0 | RX: 0
Core 2: Port 1 | TX: 0 | RX: 0
Core 3: Port 0 | TX: 0 | RX: 0

==== Packet Stats (every 2s) ====
Core 1: Port 0 | TX: 0 | RX: 0
Core 2: Port 1 | TX: 0 | RX: 0
Core 3: Port 0 | TX: 0 | RX: 0
[RX] Core 1 | Port 0 | Queue 0 | Packets: 1
[BUILD] Port 0 | AI Tag: 3 | Priority: 4 | Session: 1761554974
[TX] Core 3 | Port 0 | Queue 2
[RX] Core 1 | Port 0 | Queue 0 | Packets: 1
[BUILD] Port 1 | AI Tag: 2 | Priority: 5 | Session: 1762493504
[TX] Core 2 | Port 1 | Queue 1
[RX] Core 1 | Port 0 | Queue 0 | Packets: 1
[BUILD] Port 1 | AI Tag: 2 | Priority: 5 | Session: 1763467882
[TX] Core 2 | Port 1 | Queue 1
[RX] Core 1 | Port 0 | Queue 0 | Packets: 1
[BUILD] Port 1 | AI Tag: 2 | Priority: 5 | Session: 1764175387
[TX] Core 2 | Port 1 | Queue 1
[RX] Core 1 | Port 0 | Queue 0 | Packets: 1
[BUILD] Port 1 | AI Tag: 2 | Priority: 5 | Session: 1765343774
[TX] Core 2 | Port 1 | Queue 1
[RX] Core 1 | Port 0 | Queue 0 | Packets: 1
[BUILD] Port 0 | AI Tag: 3 | Priority: 4 | Session: 1766036855
[TX] Core 3 | Port 0 | Queue 2
[RX] Core 1 | Port 0 | Queue 0 | Packets: 1
[BUILD] Port 1 | AI Tag: 2 | Priority: 5 | Session: 1766972809
[TX] Core 2 | Port 1 | Queue 1
[RX] Core 1 | Port 0 | Queue 0 | Packets: 1
[BUILD] Port 0 | AI Tag: 3 | Priority: 4 | Session: 1767899349
[TX] Core 3 | Port 0 | Queue 2
[RX] Core 1 | Port 0 | Queue 0 | Packets: 1
[BUILD] Port 1 | AI Tag: 2 | Priority: 5 | Session: 1769062844
[TX] Core 2 | Port 1 | Queue 1
[RX] Core 1 | Port 0 | Queue 0 | Packets: 1
[BUILD] Port 0 | AI Tag: 3 | Priority: 4 | Session: 1769989521
[TX] Core 3 | Port 0 | Queue 2

==== Packet Stats (every 2s) ====
Core 1: Port 0 | TX: 0 | RX: 10
Core 2: Port 1 | TX: 6 | RX: 0
Core 3: Port 0 | TX: 4 | RX: 0


```
## 📄 License

GNU Affero General Public License v3.0
See [LICENSE](./LICENSE) for full terms.
