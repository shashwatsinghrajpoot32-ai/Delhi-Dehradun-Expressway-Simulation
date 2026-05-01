#include "BloomFilter.hpp"
#include "FenwickTree.hpp"
#include "Graph.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <limits>
#include <queue>
#include <string>
#include <vector>

static void printMenu() {
  std::cout << "\n=== Delhi–Dehradun Expressway Simulation (CLI) ===\n";
  std::cout << "5.  Find Shortest Path (Dijkstra)\n";
  std::cout << "6.  Detect Negative Cycle (Bellman-Ford)\n";
  std::cout << "7.  Show Minimum Spanning Tree (Kruskal)\n";
  std::cout << "8.  Toll Range Query (Fenwick Tree)\n";
  std::cout << "9.  Update Toll (Fenwick Tree + Graph)\n";
  std::cout << "10. Check Vehicle (Bloom Filter)\n";
  std::cout << "11. Top-K Toll Roads (Max Heap)\n";
  std::cout << "12. Exit\n";
  std::cout << "Choose option: ";
}

static std::string readTokenLineTrimmed() {
  std::string s;
  std::getline(std::cin, s);
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
  std::size_t i = 0;
  while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
  return s.substr(i);
}

static int readInt(const std::string& prompt) {
  while (true) {
    std::cout << prompt;
    long long x;
    if (std::cin >> x) {
      std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
      return static_cast<int>(x);
    }
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cout << "Invalid number. Try again.\n";
  }
}

static long long readLL(const std::string& prompt) {
  while (true) {
    std::cout << prompt;
    long long x;
    if (std::cin >> x) {
      std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
      return x;
    }
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cout << "Invalid number. Try again.\n";
  }
}

static std::string readCityName(const std::string& prompt) {
  std::cout << prompt;
  return readTokenLineTrimmed();
}

static Graph::CostMetric readMetric() {
  while (true) {
    std::cout << "Choose metric: 1=Distance, 2=Time, 3=Toll : ";
    int x;
    if (std::cin >> x) {
      std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
      if (x == 1) return Graph::CostMetric::Distance;
      if (x == 2) return Graph::CostMetric::Time;
      if (x == 3) return Graph::CostMetric::Toll;
    } else {
      std::cin.clear();
      std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
    std::cout << "Invalid metric. Try again.\n";
  }
}

int main() {
  Graph g;
  std::string error;

  // Expecting expressway.txt in working directory; allow fallback to project-relative.
  if (!g.loadFromFile("expressway.txt", error)) {
    if (!g.loadFromFile("../expressway.txt", error)) {
      std::cerr << "Failed to load expressway graph. " << error << "\n";
      return 1;
    }
  }

  // Fenwick tree over edge tolls (1-based indices map to edgeId+1).
  FenwickTree fenwick(g.edgeCount());
  for (const auto& e : g.edges()) {
    fenwick.add(e.id + 1, e.toll);
  }

  // Bloom filter for vehicle id tracking (toll plaza pass simulation).
  BloomFilter bloom(1 << 18, 7); // 262k bits, 7 hashes

  std::cout << "Loaded cities (" << g.nodeCount() << "): ";
  for (int i = 0; i < g.nodeCount(); ++i) {
    if (i) std::cout << ", ";
    std::cout << g.getCityName(i);
  }
  std::cout << "\nLoaded roads (" << g.edgeCount() << "). Edge IDs are 0.." << (g.edgeCount() - 1)
            << "\n";

  while (true) {
    printMenu();
    int choice;
    if (!(std::cin >> choice)) {
      std::cin.clear();
      std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
      std::cout << "Invalid option.\n";
      continue;
    }
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    if (choice == 12) {
      std::cout << "Exiting. Bye.\n";
      break;
    }

    switch (choice) {
      case 5: {
        auto metric = readMetric();
        std::string srcName = readCityName("Source city: ");
        std::string dstName = readCityName("Destination city: ");
        auto s = g.getCityIndex(srcName);
        auto t = g.getCityIndex(dstName);
        if (!s || !t) {
          std::cout << "Unknown city name.\n";
          break;
        }
        auto res = g.dijkstra(*s, *t, metric);
        if (!res.reachable) {
          std::cout << "No path found.\n";
          break;
        }
        std::cout << "Path: " << g.formatPath(res.path) << "\n";
        std::cout << "Total cost: " << res.totalCost << "\n";
        break;
      }

      case 6: {
        // Fuel arbitrage simulation:
        // Interpret each edge's "profit" as: fuelSavings - toll, allowing negative weights.
        // We'll ask user for a uniform fuelSavings value and compute weight = toll - fuelSavings.
        long long fuelSavings = readLL("Enter hypothetical fuel savings per road (can be > toll): ");
        std::vector<long long> w(g.edgeCount(), 0);
        for (const auto& e : g.edges()) {
          // If fuelSavings > toll, weight becomes negative => arbitrage potential.
          w[e.id] = e.toll - fuelSavings;
        }
        std::string note;
        bool hasNeg = g.bellmanFordDetectNegativeCycle(w, note);
        std::cout << note << "\n";
        if (!hasNeg) {
          std::cout << "Interpretation: No arbitrage loop under current savings.\n";
        } else {
          std::cout << "Interpretation: A loop can reduce total cost indefinitely.\n";
        }
        break;
      }

      case 7: {
        auto metric = readMetric();
        auto mst = g.kruskalMST(metric);
        if (!mst.possible) {
          std::cout << "MST not possible (graph may be disconnected).\n";
          break;
        }
        std::cout << "MST total weight: " << mst.totalWeight << "\n";
        std::cout << "Edges in MST:\n";
        for (const auto& e : mst.edges) std::cout << "  - " << g.formatEdge(e) << "\n";
        break;
      }

      case 8: {
        std::cout << "Toll range query uses edge IDs 0.." << (g.edgeCount() - 1) << "\n";
        int l = readInt("Enter L (edge id): ");
        int r = readInt("Enter R (edge id): ");
        long long sum = fenwick.sumRange(l + 1, r + 1);
        std::cout << "Total toll sum for edges [" << std::min(l, r) << ".." << std::max(l, r)
                  << "] = " << sum << "\n";
        break;
      }

      case 9: {
        std::cout << "Update toll by edge ID.\n";
        int id = readInt("Edge id: ");
        long long newToll = readLL("New toll value: ");
        if (id < 0 || id >= g.edgeCount()) {
          std::cout << "Invalid edge id.\n";
          break;
        }
        // Update both Graph and Fenwick.
        if (!g.updateTollByEdgeId(id, newToll)) {
          std::cout << "Failed to update toll.\n";
          break;
        }
        fenwick.set(id + 1, newToll);
        std::cout << "Updated. Edge now: " << g.formatEdge(g.edges()[id]) << "\n";
        break;
      }

      case 10: {
        std::string vehicleId = readCityName("Enter vehicle id (string): ");
        std::cout << "Check before add => " << (bloom.possiblyContains(vehicleId) ? "true" : "false")
                  << "\n";
        bloom.add(vehicleId);
        std::cout << "Recorded pass.\n";
        std::cout << "Check after add  => " << (bloom.possiblyContains(vehicleId) ? "true" : "false")
                  << "\n";
        std::cout << "(Note: Bloom filter can have false positives.)\n";
        break;
      }

      case 11: {
        int k = readInt("Enter K: ");
        if (k <= 0) {
          std::cout << "K must be > 0.\n";
          break;
        }
        struct Item {
          long long toll;
          int edgeId;
          bool operator<(const Item& other) const { return toll < other.toll; } // max-heap
        };
        std::priority_queue<Item> pq;
        for (const auto& e : g.edges()) pq.push({e.toll, e.id});
        std::cout << "Top-" << k << " toll roads:\n";
        for (int i = 0; i < k && !pq.empty(); ++i) {
          auto it = pq.top();
          pq.pop();
          const auto& e = g.edges()[it.edgeId];
          std::cout << "  " << (i + 1) << ") " << g.formatEdge(e) << "\n";
        }
        break;
      }

      default:
        std::cout << "Unknown option.\n";
        break;
    }
  }

  return 0;
}

