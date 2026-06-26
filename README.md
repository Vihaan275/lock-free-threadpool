# Lock-Free Work-Stealing Thread Pool

A thread pool in C++ built around per-thread task queues and lock-free work stealing. Threads run tasks from their own queue and steal from others when idle, keeping cores busy without a central bottleneck.

## Architecture

Each thread owns a fixed-size ring buffer queue (capacity 1024). Task submission uses round-robin distribution across queues. When a thread exhausts its own queue, it walks the other queues in order and steals available tasks via CAS on the head index. Threads that find no work anywhere block on a condition variable and wake on the next `add_task` call.

```
submit ──round-robin──► [Q0] [Q1] [Q2] [Q3]
                          ▲         │
                          └─steal───┘
```

Queue operations:
- **Push (tail):** owner writes the slot, then bumps tail with `fetch_add(release)` 
- **Pop (head):** stealers CAS the head index to claim a slot before moving from it, avoiding double-execution.
- **Idle:** threads park on a condition variable; `add_task` calls `notify_all` to wake them.

## Memory Ordering

The queue uses acquire/release semantics throughout rather than `seq_cst`, which avoids unnecessary full memory barriers on x86 and is correctness-critical on weakly ordered architectures (ARM, POWER):

- tail store: `release`: slot write happens-before index bump
- head CAS: `acq_rel`: stealer sees the full slot before claiming it
- stop flag: `release` on write, `acquire` on read

## Bugs Found and Fixed

**Slot-write race:** the initial implementation incremented `tail` before the slot write was guaranteed visible to other threads. On TSO (x86) this rarely manifests, but on weakly ordered hardware a stealer could CAS the head, win the slot, and read an uninitialized `std::function`, producing `bad_function_call`. Fixed with a release fence between the slot store and the `fetch_add`.

**Constructor ordering race:** threads were spawned in the same loop iteration that emplaced their queue. Thread 0 could begin stealing and index into `queues[1]` before queue 1 existed, triggering an out-of-bounds assert. Fixed by splitting construction into two passes: populate all queues, then spawn all threads.

**Shutdown race:** the original `kill_tp` gated `join()` on queue emptiness, which could cause threads that had already exited (or were stealing from other queues) to never get joined. Fixed by setting `stop`, notifying all threads, then joining sequentially.


## Benchmark

1000 tasks, each incrementing a counter to 10,000. Measured wall-clock time via `std::chrono`. Run on Intel I7.

| Threads | Time (s)  | Speedup vs 1 thread |
|---------|-----------|---------------------|
| 1       | 0.012609  | 1.00×               |
| 2       | 0.008565  | 1.47×               |
| 4       | 0.004568  | 2.76×               |
| 6       | 0.003873  | 3.26×               |
| 8       | 0.002528  | 4.99×               |

Profiled with `perf stat` to confirm work was distributed across cores and no single queue became a bottleneck under load.

## Building

```bash
# Standard
g++ -std=c++20 -O2 -pthread atomic_thread_pool.cpp -o threadpool
```


