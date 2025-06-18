# NeuXTP
##  **NeuXTP** (Neuro-Accelerated eXtreme Transport Protocol)

###  Hybrid Concept:

 A high-performance, AI-augmented transport protocol that maintains XTP’s line-rate throughput but adds dynamic decision-making from AI models embedded in SmartNIC or software fast-path.

###  **Implementation Plan**

#### 1. **Foundation: XTP (High PPS Baseline)**

* DPDK-based L2/L3 fast-path protocol
* Lightweight custom header (already done)
* Zero-copy buffers, NUMA-aware TX queues

#### 2. **Augment: NeuNIC Intelligence**

* Add a simple AI/ML classifier (Python/TFLite) to:

  * Prioritize packets (e.g., based on payload content or QoS tag)
  * Decide compression (Zstd on text/video, drop junk)
  * Flag anomalies (e.g., sudden flood of certain patterns)
* Embed logic in user-space for now (later move to NIC or FPGA)

#### 3. **Runtime Feedback Loop**

* Use shared memory ring for real-time AI hints to DPDK loop
* Sampled packets → AI engine → tag/predict → DPDK fast-path adjusts
* Example:

  ```c
  if (ai_predict_priority(pkt) > THRESH) {
      route_to_high_prio_queue(pkt);
  } else {
      drop_or_defer(pkt);
  }
  ```

#### 4. **Performance Monitoring**

* Enhanced logging of:

  * PPS
  * Avg CPU utilization per core
  * Latency histogram
  * AI inference count / ms

#### 5. **Table Output + CLI Interface**

* Display live stats:

  ```
  Protocol   Cores   Payload   Avg PPS   CPU%   Latency   AI-Hits/s
  NeuXTP       2      64B      6.0M      75%     2.4µs        3000
  ```

### Minimal AI Module (PoC)

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

###  Output Example

| Protocol   | Cores | Payload | Avg PPS | CPU % | Latency | AI Hits/s |
| ---------- | ----- | ------- | ------- | ----- | ------- | --------- |
| **XTP**    | 1     | 64B     | 6.1M    | 80%   | 2.3µs   | N/A       |
| **NeuXTP** | 2     | 64B     | 5.8M    | 78%   | 2.4µs   | 3,100     |
| **UDP**    | 1     | 64B     | 5.7M    | 78%   | 2.5µs   | N/A       |
| **TCP**    | 1     | 64B     | 5.6M    | 90%   | 3.1µs   | N/A       |

### Next Steps:

1.  **Already have XTP DPDK benchmarking — reuse it.**
2.  **Plug in Python-based AI predictor** (can use your own logic).
3.  **Add AI hooks into DPDK TX/RX loop** for dynamic behavior.
4.  **Benchmark NeuXTP vs. XTP/TCP/UDP.**

## How **NeuXTP** Can Help AI Workloads (Without RDMA)

NeuXTP is a concept that fuses:

* A **custom transport protocol (XTP)** with
* **DPDK acceleration**, and
* Optional **AI-augmented logic**, often running on SmartNICs or CPU cores.

###  Key AI Use Cases Where NeuXTP Helps

| AI Workload Type                                  | NeuXTP Advantage                              |
| ------------------------------------------------- | --------------------------------------------- |
| **Distributed AI training** (multi-node)          | Lower latency + smarter packet prioritization |
| **Model inference serving**                       | Predictable low-latency delivery              |
| **Real-time data ingestion** (e.g., camera/LiDAR) | Fast packet scheduling, minimal jitter        |
| **Edge AI coordination** (robotics, drones)       | Mesh-aware, time-sensitive packet handling    |
| **Telemetry/gradient prioritization**             | AI-annotated data gets higher priority        |


###  1. AI-Aware Packet Prioritization

NeuXTP can tag packets with fields like:

* `XTP-AI-Priority`
* `Model-ID` or `Stream-ID`
* `Latency-SLA`

Then, **in-software** (via DPDK or eBPF) or **in-hardware** (SmartNIC queues), the NIC can:

* Drop stale data (e.g., old frames)
* Boost model-critical packets (e.g., token-level prioritization in LLMs)
* Isolate background sync from real-time prediction


###  2. Fine-Grained Feedback Loop

Because XTP is **custom-defined**, we can:

* Include **AI-specific control messages** (e.g., load imbalance feedback)
* Insert per-packet **tensor or context metadata**
* Allow **in-network prefiltering** or compression decisions (e.g., lossless for gradient data, lossy for activation previews)

---

###  3. Mesh-Optimized for AI Clusters

NeuXTP can be designed to support:

* Deterministic flows across **multi-NIC mesh fabrics**
* Explicit support for AI clusters (GPU-to-GPU, TPU-to-DPU links)
* Seamless integration with **inference orchestrators** or **model sharding logic**

---

###  4. Programmable Hooks for AI Logic

We can:

* Embed AI/ML scoring in the packet headers
* Run a `neux_route()` function that routes traffic using an AI policy
* Dynamically reweight traffic based on load, SLAs, or model stages


###  Example:

**Edge AI Camera → Central GPU Cluster**

If someone wants to:

* Prioritize `frame_type="face_detected"` over normal frames
* Drop low-SNR frames on congestion
* Tag certain packets for GPU-A ID vs GPU-B

With NeuXTP:

```c
struct xtp_hdr {
    uint8_t priority_level;  // AI-assigned
    uint8_t task_class;      // e.g., object_detect=1, LLM_token=2
    uint32_t flow_id;
    uint8_t ai_sla_hint;     // 0 = low latency, 1 = high throughput
};
```

In DPDK RX loop:

```c
if (xtp_hdr->priority_level > 5)
    forward_to_fast_queue();
else
    queue_for_background();
```

##  Summary: NeuXTP Benefits to AI Workloads

| Feature                       | Benefit to AI Workloads                                |
| ----------------------------- | ------------------------------------------------------ |
| Custom transport header       | Application-specific intelligence (model ID, priority) |
| DPDK + XTP stack              | Lower latency, flexible routing                        |
| AI-injected control fields    | Smarter resource usage                                 |
| Flow separation + SLA tagging | Better inference + training coexistence                |
| Programmability               | Future-proof for emerging AI use cases                 |


### 1. `xtp_hdr` or `neuxtp_hdr` Design

We'll enhance the XTP header with optional AI-aware metadata fields. Here's a sample `neuxtp_hdr`:

```c
struct neuxtp_hdr {
    uint8_t version;            // Protocol version
    uint8_t flags;              // Control bits
    uint16_t length;            // Payload length

    uint16_t session_id;        // Optional AI-session tracking
    uint8_t ai_tag;             // Classification label (e.g., image, speech, text)
    uint8_t priority;           // Priority tag for scheduling/offload

    uint32_t checksum;          // Optional integrity check
} __attribute__((__packed__));
```

These fields can be used for:

* Smart prioritization
* Queue placement
* Hardware offload hints for AI jobs (e.g., inference, vector streaming)

###  2. AI Benchmarking Flow (Tailored for AI-serving)

Here’s a benchmarking setup suitable for AI-serving traffic:

| Mode          | Description                       | Payload Type | Examples               |
| ------------- | --------------------------------- | ------------ | ---------------------- |
| **Batch**     | Streams of dense tensors          | `512B - 8KB` | Transformer inputs     |
| **Real-Time** | Token-by-token latency-sensitive  | `16B - 256B` | Chat/stream inference  |
| **Control**   | Low-bandwidth RPC-style signaling | `<128B`      | Scheduling/prompt mgmt |

Each of these traffic modes can be scripted using DPDK generators and NeuXTP headers.


###  3. Mesh Flow Control Diagram (for GPU/AI clusters)

```
 +------------+        +------------+        +------------+
 |  NeuNIC 0  | <----> |  NeuNIC 1  | <----> |  NeuNIC 2  |
 | (GPU Node) |        | (GPU Node) |        | (GPU Node) |
 +------------+        +------------+        +------------+
       ↑                     ↑                     ↑
       |                     |                     |
   [Mesh Scheduler/Router with NeuXTP Metadata Awareness]
       |                     |                     |
  Inference Queue      Training Queue         Token Streaming
```

* Each NeuNIC has its own NeuXTP packet classification logic.
* AI tags (`ai_tag`, `priority`) help with mesh routing and latency optimization.

#### integration Notes:

    * session_id lets you isolate inference streams (e.g., for streaming LLMs).
    * ai_tag enables the NIC, switch, or software logic to prioritize specific AI content.
    * priority could be used in:
        ** Lcore scheduling
        ** Queue assignment
        ** Offload hints for AI/NPU/FPGA acceleration 
    * flags is reserved for ACKs, retransmission logic, or even flow control extensions.


##  NeuXTP Mesh Simulation Across Cores/NICs

###  Architecture Overview

Each logical core (lcore) or NIC port simulates a node in a mesh. NeuXTP packets with different AI tags are routed across these "nodes".

```
                +---------+         +---------+
   Core 0  ---> | XPU-0   | <-----> | XPU-1   | <--- Core 1
                +---------+         +---------+
                     |                   |
                     v                   v
                +---------+         +---------+
   Core 2  ---> | XPU-2   | <-----> | XPU-3   | <--- Core 3
                +---------+         +---------+
```

####  What This Simulates

* Multiple **cores/NICs** acting as mesh nodes
* Each core/NIC transmits NeuXTP packets with different AI tags and priorities
* Simulates **intelligent routing/load distribution** in a NeuXTP network



## License

This project is licensed under the GNU Affero General Public License v3.0.  
See the [LICENSE](./LICENSE) file for details.
