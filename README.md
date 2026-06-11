# BidMatrix — Dynamic Real-Time Ad Bidding Matrix

> **ITM Skills University | Case Study #88**
> A high-performance Demand-Side Platform (DSP) simulation built entirely in C++, modelled after real-world ad exchanges like **Google DV360** and **The Trade Desk**.

---

## 📌 Problem Statement

BidMatrix is an online advertising exchange that auctions display banner spaces in real time, manages advertiser budgets, and targets ads to specific customer demographics. The legacy system faced critical bottlenecks:

| Problem | Impact |
|---|---|
| Slow banner-space inventory lookups | Missed bid windows |
| Budget changes cannot be tracked or reversed | Risk of overspending |
| Live bids processed out of order | Unfair auctions & race conditions |
| No fast user demographic verification | Poor ad targeting accuracy |
| No campaign revenue ranking | Sub-optimal ad selection |
| No publisher marketplace map | Cannot optimize ad delivery routes |
| No shortest latency path solver | Violates `<100 ms` SLA |
| No CDN edge-server storage management | Creative cache fragmentation |

---

## 🚀 Features & Data Structures

| # | Feature | Data Structure | Complexity | Real-World Mapping |
|---|---|---|---|---|
| 1 | **Inventory Listing** | Custom Chained Hash Map | O(1) avg | Active display inventory database |
| 2 | **Update History / Rollback** | Custom Stack (LIFO) | O(1) | Budget allocation ledger & safety rollback |
| 3 | **Auction Loop** | Custom Linked FIFO Queue | O(1) | Real-time bidding (RTB) TCP event stream |
| 4 | **Attribute Token** | Custom Trie | O(L) | Fast demographic profile verification |
| 5 | **Campaign Sorter** | Custom Max-Heap | O(log N) | Campaign yield ranker (eCPM bidding logic) |
| 6 | **Marketplace Network** | Graph (Adjacency List) | O(V + E) | Exchange/SSP publisher partner map |
| 7 | **Route Path (Latency)** | Dijkstra's Algorithm | O(E log V) | Smart routing & ad delivery optimisation |
| 8 | **Block Allocator** | Free List (First-Fit) | O(N) | Edge server (CDN) creative asset storage |

---

## 🏗️ Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                   BidMatrix DSP Engine                       │
│                                                             │
│  ┌──────────────┐   ┌──────────────┐   ┌───────────────┐   │
│  │ Inventory    │   │ Budget       │   │ Auction Loop  │   │
│  │ Hash Map     │   │ Stack        │   │ FIFO Queue    │   │
│  │ O(1) lookup  │   │ O(1) rollback│   │ O(1) enqueue  │   │
│  └──────────────┘   └──────────────┘   └───────────────┘   │
│                                                             │
│  ┌──────────────┐   ┌──────────────┐   ┌───────────────┐   │
│  │ Demographic  │   │ Campaign     │   │ Marketplace   │   │
│  │ Trie O(L)    │   │ Max-Heap     │   │ Graph + Dijkstra│  │
│  │              │   │ O(log N)     │   │ O(E log V)    │   │
│  └──────────────┘   └──────────────┘   └───────────────┘   │
│                                                             │
│  ┌──────────────────────────────────────────────────────┐   │
│  │          Edge Server Block Allocator (Free List)     │   │
│  └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

---

## 📋 System Menu

When run, the program presents an interactive console menu:

```
=============================================================================
                   DYNAMIC REAL-TIME AD BIDDING MATRIX
                         ITM SKILLS UNIVERSITY
=============================================================================
 1. Manage Display Inventory Listing [Hash Table]
 2. Log Budget Adjustments & Run Rollback [Stack]
 3. Stream Live Bids into Auction Loop [FIFO Queue]
 4. Target Demographics Token Manager [Trie]
 5. Campaign Sorter & Yield Ranker [Max-Heap]
 6. Visualize SSP/Publisher Marketplace [Graph]
 7. Route Delivery Shortest Latency Path [Dijkstra]
 8. Server Storage Block Allocator [Free List]
 9. Run Interactive Unified E2E Auction Simulation
10. View Architectural DSA Justification Report
11. Exit System
```

---

## 🛠️ How to Build & Run

### Prerequisites
- C++17 compatible compiler (`g++`, `clang++`, or MSVC)

### Compile
```bash
g++ -std=c++17 -O2 -o bidmatrix main.cpp
```

### Run
```bash
./bidmatrix
```

---

## 📖 DSA Design Justifications

### 1. Hash Table — Inventory Listing
The custom hash map (djb2 hash, chaining collision resolution) delivers **O(1) average** banner-space lookups. In production DSPs, millions of bid requests per second query inventory availability — linear scans would be catastrophically slow.

### 2. Stack — Budget Update History
**LIFO** ordering means the most-recent budget change is always on top, enabling instant **O(1) rollback** to the previous safe state. This mirrors DV360's campaign budget reversion flow used when algorithmic pacing overshoots the daily cap.

### 3. FIFO Queue — Auction Loop
Ad exchanges receive thousands of bid events per second via WebSocket/TCP streams. A **linked FIFO queue** guarantees strict chronological processing, preventing advertiser starvation and ensuring regulatory auction fairness.

### 4. Trie — Attribute Token / Demographic Verification
A Trie stores demographic segment keys (e.g. `dem_age_18_24`, `geo_us_ny`) with **O(L)** verification time where L is key length — constant regardless of database size. This mirrors The Trade Desk's audience segment token matching at the bid-decision layer.

### 5. Max-Heap — Campaign Yield Sorter
The custom max-heap stores campaigns ranked by **eCPM = CTR × Bid × 1000**. It provides **O(1)** access to the highest-value campaign and **O(log N)** insertion — critical for real-time ad selection under strict latency budgets.

### 6. Graph + Dijkstra — Marketplace Network & Route Path
Publisher SSPs and edge nodes are modelled as a weighted directed graph. **Dijkstra's algorithm** finds the minimum-latency path in **O(E log V)**, ensuring creative payloads meet the industry-standard `<100 ms` delivery SLA.

### 7. Free List Block Allocator — Edge Server Storage
A linked-list free-list with **First-Fit allocation** and **adjacent-block coalescing** on deallocation simulates CDN cache management on edge servers. Coalescing prevents memory fragmentation as creatives are evicted and replaced.

---

## 📁 Project Structure

```
BidMatrix/
├── main.cpp          # Full C++ implementation (1083 lines)
├── README.md         # This file
└── LICENSE           # MIT License
```

---

## 🌐 Real-World Platform Analogies

| Component | Google DV360 | The Trade Desk |
|---|---|---|
| Inventory Hash Map | Inventory Availability Cache | Supply-side inventory cache |
| Budget Stack | Daily budget pacing rollback | Spend control reversion |
| Auction FIFO Queue | OpenRTB bid stream processor | Kokai bid stream handler |
| Demographic Trie | Audience segment token match | UID2 attribute resolution |
| Campaign Max-Heap | eCPM yield optimizer | Koa AI bid optimisation |
| Graph + Dijkstra | Programmatic ad route optimizer | Global edge routing |
| Block Allocator | GCP CDN creative cache | Edge creative storage |

---

## 📜 License

This project is licensed under the **MIT License** — see [LICENSE](LICENSE) for details.

---

## 👤 Author

**Saiyash Poojari**
ITM Skills University — DSA Case Study #88
