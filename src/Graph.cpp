#include "Graph.hpp"

#include "DSU.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <functional>
#include <limits>
#include <queue>
#include <sstream>
#include <stdexcept>

static bool isWhitespaceOnly(const std::string& s) {
  for (char c : s) {
    if (!std::isspace(static_cast<unsigned char>(c))) return false;
  }
  return true;
}

std::optional<int> Graph::getCityIndex(const std::string& name) const {
  auto it = nameToIndex_.find(name);
  if (it == nameToIndex_.end()) return std::nullopt;
  return it->second;
}

std::string Graph::getCityName(int idx) const {
  if (idx < 0 || idx >= static_cast<int>(cityNames_.size())) return "<invalid>";
  return cityNames_[idx];
}

long long Graph::metricCost(const Edge& e, CostMetric metric) {
  switch (metric) {
    case CostMetric::Distance: return e.distance;
    case CostMetric::Time: return e.time;
    case CostMetric::Toll: return e.toll;
  }
  return e.distance;
}

bool Graph::loadFromFile(const std::string& filePath, std::string& error) {
  error.clear();
  std::ifstream in(filePath);
  if (!in) {
    error = "Failed to open file: " + filePath;
    return false;
  }

  int n = 0, m = 0;
  if (!(in >> n >> m)) {
    error = "Invalid first line. Expected: <nodes> <edges>";
    return false;
  }
  if (n <= 0 || m < 0) {
    error = "Invalid graph size.";
    return false;
  }

  n_ = n;
  m_ = m;
  cityNames_.assign(n_, "");
  nameToIndex_.clear();
  edges_.clear();
  adj_.assign(n_, {});

  // Next n lines: cityName index
  for (int i = 0; i < n_; ++i) {
    std::string city;
    int idx = -1;
    if (!(in >> city >> idx)) {
      error = "Invalid city line at position " + std::to_string(i + 1) +
              ". Expected: <CityName> <Index>";
      return false;
    }
    if (idx < 0 || idx >= n_) {
      error = "City index out of range for city: " + city;
      return false;
    }
    if (!cityNames_[idx].empty()) {
      error = "Duplicate city index: " + std::to_string(idx);
      return false;
    }
    if (nameToIndex_.count(city)) {
      error = "Duplicate city name: " + city;
      return false;
    }
    cityNames_[idx] = city;
    nameToIndex_[city] = idx;
  }

  for (int i = 0; i < n_; ++i) {
    if (cityNames_[i].empty()) {
      error = "Missing city definition for index: " + std::to_string(i);
      return false;
    }
  }

  // Remaining lines: uName vName distance time toll
  std::string uName, vName;
  long long dist = 0, tm = 0, toll = 0;
  int readEdges = 0;

  while (readEdges < m_ && (in >> uName >> vName >> dist >> tm >> toll)) {
    auto uOpt = getCityIndex(uName);
    auto vOpt = getCityIndex(vName);
    if (!uOpt || !vOpt) {
      error = "Unknown city name in edge: " + uName + " " + vName;
      return false;
    }
    int u = *uOpt;
    int v = *vOpt;
    if (u == v) {
      error = "Self-loop edge not allowed: " + uName;
      return false;
    }

    Edge e;
    e.from = u;
    e.to = v;
    e.distance = dist;
    e.time = tm;
    e.toll = toll;
    e.id = readEdges; // stable id (0..m-1)
    e.fromName = uName;
    e.toName = vName;

    edges_.push_back(e);

    // undirected adjacency: push both directions with same id
    Edge e1 = e;
    Edge e2 = e;
    std::swap(e2.from, e2.to);
    std::swap(e2.fromName, e2.toName);

    adj_[e1.from].push_back(e1);
    adj_[e2.from].push_back(e2);

    ++readEdges;
  }

  if (readEdges != m_) {
    error = "Expected " + std::to_string(m_) + " edges, but read " + std::to_string(readEdges) +
            ". Check file formatting.";
    return false;
  }

  return true;
}

Graph::PathResult Graph::dijkstra(int src, int dst, CostMetric metric) const {
  PathResult result;
  if (src < 0 || src >= n_ || dst < 0 || dst >= n_) return result;

  const long long INF = std::numeric_limits<long long>::max() / 4;
  std::vector<long long> dist(n_, INF);
  std::vector<int> parent(n_, -1);

  using State = std::pair<long long, int>; // (cost, node)
  std::priority_queue<State, std::vector<State>, std::greater<State>> pq;

  dist[src] = 0;
  pq.push({0, src});

  while (!pq.empty()) {
    auto [d, u] = pq.top();
    pq.pop();
    if (d != dist[u]) continue;
    if (u == dst) break;

    for (const auto& e : adj_[u]) {
      int v = e.to;
      long long w = metricCost(e, metric);
      if (w < 0) continue; // Dijkstra assumes non-negative weights
      if (dist[u] + w < dist[v]) {
        dist[v] = dist[u] + w;
        parent[v] = u;
        pq.push({dist[v], v});
      }
    }
  }

  if (dist[dst] == INF) return result;
  result.reachable = true;
  result.totalCost = dist[dst];

  // Reconstruct path
  std::vector<int> path;
  for (int cur = dst; cur != -1; cur = parent[cur]) path.push_back(cur);
  std::reverse(path.begin(), path.end());
  result.path = std::move(path);
  return result;
}

bool Graph::bellmanFordDetectNegativeCycle(const std::vector<long long>& customEdgeWeights,
                                           std::string& cycleNote) const {
  cycleNote.clear();
  if (static_cast<int>(customEdgeWeights.size()) != m_) {
    cycleNote = "Invalid weights array size. Expected " + std::to_string(m_);
    return false;
  }

  // Classic negative cycle detection: initialize distances to 0 for all nodes
  // so that cycles in any component are discoverable.
  const long long INF = std::numeric_limits<long long>::max() / 4;
  std::vector<long long> dist(n_, 0);
  std::vector<int> parent(n_, -1);
  int updatedVertex = -1;

  // Relax edges V times; if there is an update on Vth iteration -> negative cycle.
  for (int i = 0; i < n_; ++i) {
    updatedVertex = -1;
    for (int ei = 0; ei < m_; ++ei) {
      const auto& e = edges_[ei];
      long long w = customEdgeWeights[ei];
      int u = e.from;
      int v = e.to;

      // treat as directed both ways for undirected graph by relaxing u->v and v->u
      if (dist[u] + w < dist[v]) {
        dist[v] = std::max(-INF, dist[u] + w);
        parent[v] = u;
        updatedVertex = v;
      }
      if (dist[v] + w < dist[u]) {
        dist[u] = std::max(-INF, dist[v] + w);
        parent[u] = v;
        updatedVertex = u;
      }
    }
  }

  if (updatedVertex == -1) {
    cycleNote = "No negative cycle detected.";
    return false;
  }

  // Try to retrieve a cycle vertex by walking parents n times.
  int y = updatedVertex;
  for (int i = 0; i < n_; ++i) y = parent[y] == -1 ? y : parent[y];

  // Now reconstruct cycle (best-effort).
  std::vector<int> cycle;
  int cur = y;
  for (int i = 0; i < n_ + 5; ++i) {
    cycle.push_back(cur);
    cur = parent[cur];
    if (cur == -1) break;
    if (cur == y) {
      cycle.push_back(cur);
      break;
    }
  }

  if (cycle.size() >= 2 && cycle.front() == cycle.back()) {
    std::reverse(cycle.begin(), cycle.end());
    cycleNote = "Negative cycle found (fuel arbitrage possible): " + formatPath(cycle);
  } else {
    cycleNote = "Negative cycle found (cycle reconstruction partial).";
  }
  return true;
}

Graph::MSTResult Graph::kruskalMST(CostMetric metric) const {
  MSTResult res;
  if (n_ == 0) return res;

  std::vector<Edge> sorted = edges_;
  std::sort(sorted.begin(), sorted.end(), [&](const Edge& a, const Edge& b) {
    return metricCost(a, metric) < metricCost(b, metric);
  });

  DSU dsu(n_);
  long long total = 0;
  std::vector<Edge> chosen;
  chosen.reserve(n_ - 1);

  for (const auto& e : sorted) {
    if (dsu.unite(e.from, e.to)) {
      chosen.push_back(e);
      total += metricCost(e, metric);
      if (static_cast<int>(chosen.size()) == n_ - 1) break;
    }
  }

  if (static_cast<int>(chosen.size()) != n_ - 1) {
    res.possible = false;
    return res;
  }

  res.possible = true;
  res.totalWeight = total;
  res.edges = std::move(chosen);
  return res;
}

bool Graph::updateTollByEdgeId(int edgeId, long long newToll) {
  if (edgeId < 0 || edgeId >= m_) return false;
  edges_[edgeId].toll = newToll;

  // Update adjacency copies (both directions).
  for (auto& e : adj_[edges_[edgeId].from]) {
    if (e.id == edgeId) e.toll = newToll;
  }
  for (auto& e : adj_[edges_[edgeId].to]) {
    if (e.id == edgeId) e.toll = newToll;
  }
  return true;
}

std::string Graph::formatPath(const std::vector<int>& path) const {
  if (path.empty()) return "<empty>";
  std::ostringstream oss;
  for (std::size_t i = 0; i < path.size(); ++i) {
    if (i) oss << " \xE2\x86\x92 "; // Unicode arrow →
    oss << getCityName(path[i]);
  }
  return oss.str();
}

std::string Graph::formatEdge(const Edge& e) const {
  std::ostringstream oss;
  oss << getCityName(e.from) << " \xE2\x86\x94 " << getCityName(e.to)
      << "  (dist=" << e.distance << ", time=" << e.time << ", toll=" << e.toll << ", id=" << e.id
      << ")";
  return oss.str();
}

