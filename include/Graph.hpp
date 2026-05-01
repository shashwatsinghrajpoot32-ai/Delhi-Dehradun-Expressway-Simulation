#pragma once

#include "Edge.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class Graph {
public:
  enum class CostMetric { Distance = 1, Time = 2, Toll = 3 };

  struct PathResult {
    bool reachable = false;
    std::vector<int> path; // node indices
    long long totalCost = 0;
  };

  struct MSTResult {
    bool possible = false;
    long long totalWeight = 0;
    std::vector<Edge> edges;
  };

  Graph() = default;

  bool loadFromFile(const std::string& filePath, std::string& error);

  int nodeCount() const { return n_; }
  int edgeCount() const { return static_cast<int>(edges_.size()); }

  const std::vector<std::string>& cityNames() const { return cityNames_; }
  const std::unordered_map<std::string, int>& cityIndexByName() const { return nameToIndex_; }

  std::optional<int> getCityIndex(const std::string& name) const;
  std::string getCityName(int idx) const;

  const std::vector<Edge>& edges() const { return edges_; }
  const std::vector<std::vector<Edge>>& adjacency() const { return adj_; }

  // Dijkstra shortest path based on metric.
  PathResult dijkstra(int src, int dst, CostMetric metric) const;

  // Bellman-Ford negative cycle detection on an arbitrary edge-weight view.
  // Used to simulate "fuel arbitrage" by allowing negative weights.
  bool bellmanFordDetectNegativeCycle(const std::vector<long long>& customEdgeWeights,
                                      std::string& cycleNote) const;

  // Kruskal MST based on a metric.
  MSTResult kruskalMST(CostMetric metric) const;

  // Toll edge updates (by edge id).
  bool updateTollByEdgeId(int edgeId, long long newToll);

  // Helpers for displaying paths and edges.
  std::string formatPath(const std::vector<int>& path) const;
  std::string formatEdge(const Edge& e) const;

private:
  int n_ = 0;
  int m_ = 0;

  std::vector<std::string> cityNames_;
  std::unordered_map<std::string, int> nameToIndex_;

  // Edge list (undirected roads represented once here).
  std::vector<Edge> edges_;

  // Adjacency list includes both directions for undirected roads.
  std::vector<std::vector<Edge>> adj_;

  static long long metricCost(const Edge& e, CostMetric metric);
};

