# Delhi–Dehradun Expressway Simulation (C++)

CLI-based smart highway simulation inspired by the Delhi–Dehradun Expressway.

## Modules

- **`Graph`** (`include/Graph.hpp`, `src/Graph.cpp`)
  - Adjacency list representation of cities (nodes) and roads (edges).
  - Dijkstra shortest path by **distance/time/toll** using a min-heap.
  - Bellman-Ford negative cycle detection (fuel arbitrage scenario).
  - Kruskal MST using DSU.

- **`Edge`** (`include/Edge.hpp`)
  - Stores `distance`, `time`, `toll` plus a stable `id`.

- **`DSU`** (`include/DSU.hpp`, `src/DSU.cpp`)
  - Disjoint Set Union for Kruskal.

- **`FenwickTree`** (`include/FenwickTree.hpp`, `src/FenwickTree.cpp`)
  - Point update + range sum queries over edge toll values.

- **`BloomFilter`** (`include/BloomFilter.hpp`, `src/BloomFilter.cpp`)
  - Probabilistic vehicle pass tracking.

## Input file

Place `expressway.txt` next to the executable (or run from the project folder). The sample file is included.

## Build and run (Windows, PowerShell)

From `DelhiDehradunExpresswaySimCPP`:

```powershell
cmake -S . -B build
cmake --build build --config Release
Copy-Item .\expressway.txt .\build\Release\expressway.txt -Force
.\build\Release\expressway_sim.exe
```

If you build Debug, copy the file into `build\Debug\`.

## Example interactions

- **Shortest path (distance)** Delhi → Dehradun
- **MST (distance/time/toll)** prints total + edges
- **Toll range query** uses edge ids `[0..M-1]`
- **Update toll** updates Graph + Fenwick tree
- **Bloom filter** checks and records vehicle ids
- **Top-K** prints highest toll roads using max-heap

