# NeuXTP
##  **NeuXTP** (Neuro-Accelerated eXtreme Transport Protocol)

###  Hybrid Concept:

> A high-performance, AI-augmented transport protocol that maintains XTP’s line-rate throughput but adds dynamic decision-making from AI models embedded in SmartNIC or software fast-path.

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



## License

This project is licensed under the GNU Affero General Public License v3.0.  
See the [LICENSE](./LICENSE) file for details.
