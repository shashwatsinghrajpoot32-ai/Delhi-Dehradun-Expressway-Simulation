#include "BloomFilter.hpp"
#include "FenwickTree.hpp"
#include "Graph.hpp"
#include "httplib.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <queue>
#include <sstream>
#include <string>
#include <vector>

static std::string readFile(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return {};
  std::ostringstream oss;
  oss << in.rdbuf();
  return oss.str();
}

static std::string jsonEscape(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (char c : s) {
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '"': out += "\\\""; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) out += '?';
        else out += c;
    }
  }
  return out;
}

static bool parseJsonInt(const std::string& body, const std::string& key, long long& out) {
  // Very small JSON parser for { "key": 123 } style payloads.
  auto pos = body.find("\"" + key + "\"");
  if (pos == std::string::npos) return false;
  pos = body.find(':', pos);
  if (pos == std::string::npos) return false;
  ++pos;
  while (pos < body.size() && std::isspace(static_cast<unsigned char>(body[pos]))) ++pos;
  bool neg = false;
  if (pos < body.size() && body[pos] == '-') { neg = true; ++pos; }
  if (pos >= body.size() || !std::isdigit(static_cast<unsigned char>(body[pos]))) return false;
  long long val = 0;
  while (pos < body.size() && std::isdigit(static_cast<unsigned char>(body[pos]))) {
    val = val * 10 + (body[pos] - '0');
    ++pos;
  }
  out = neg ? -val : val;
  return true;
}

static bool parseJsonString(const std::string& body, const std::string& key, std::string& out) {
  auto pos = body.find("\"" + key + "\"");
  if (pos == std::string::npos) return false;
  pos = body.find(':', pos);
  if (pos == std::string::npos) return false;
  pos = body.find('"', pos);
  if (pos == std::string::npos) return false;
  ++pos;
  std::string val;
  while (pos < body.size()) {
    char c = body[pos++];
    if (c == '"') break;
    if (c == '\\' && pos < body.size()) {
      char n = body[pos++];
      if (n == 'n') val.push_back('\n');
      else if (n == 'r') val.push_back('\r');
      else if (n == 't') val.push_back('\t');
      else val.push_back(n);
    } else {
      val.push_back(c);
    }
  }
  out = val;
  return true;
}

static void setJson(httplib::Response& res, const std::string& body) {
  res.set_content(body, "application/json; charset=utf-8");
  res.headers["Access-Control-Allow-Origin"] = "*";
}

int main() {
  Graph g;
  std::string error;
  if (!g.loadFromFile("expressway.txt", error)) {
    if (!g.loadFromFile("../expressway.txt", error)) {
      std::cerr << "Failed to load expressway graph. " << error << "\n";
      return 1;
    }
  }

  FenwickTree fenwick(g.edgeCount());
  for (const auto& e : g.edges()) fenwick.add(e.id + 1, e.toll);

  BloomFilter bloom(1 << 18, 7);

  const std::string indexHtml = readFile("frontend/index.html");
  const std::string appJs = readFile("frontend/app.js");
  const std::string stylesCss = readFile("frontend/styles.css");

  httplib::Server svr;

  svr.Get(R"(^/$)", [&](const httplib::Request&, httplib::Response& res) {
    if (indexHtml.empty()) {
      res.status = 500;
      res.set_content("Missing frontend/index.html", "text/plain");
      return;
    }
    res.set_content(indexHtml, "text/html; charset=utf-8");
  });

  svr.Get(R"(^/app\.js$)", [&](const httplib::Request&, httplib::Response& res) {
    if (appJs.empty()) {
      res.status = 500;
      res.set_content("Missing frontend/app.js", "text/plain");
      return;
    }
    res.set_content(appJs, "application/javascript; charset=utf-8");
  });

  svr.Get(R"(^/styles\.css$)", [&](const httplib::Request&, httplib::Response& res) {
    if (stylesCss.empty()) {
      res.status = 500;
      res.set_content("Missing frontend/styles.css", "text/plain");
      return;
    }
    res.set_content(stylesCss, "text/css; charset=utf-8");
  });

  svr.Get(R"(^/api/cities$)", [&](const httplib::Request&, httplib::Response& res) {
    std::ostringstream oss;
    oss << "{\"ok\":true,\"cities\":[";
    for (int i = 0; i < g.nodeCount(); ++i) {
      if (i) oss << ",";
      oss << "\"" << jsonEscape(g.getCityName(i)) << "\"";
    }
    oss << "]}";
    setJson(res, oss.str());
  });

  svr.Post(R"(^/api/dijkstra$)", [&](const httplib::Request& req, httplib::Response& res) {
    long long metric = 1;
    std::string src, dst;
    if (!parseJsonInt(req.body, "metric", metric) || !parseJsonString(req.body, "src", src) ||
        !parseJsonString(req.body, "dst", dst)) {
      res.status = 400;
      setJson(res, "{\"ok\":false,\"error\":\"Invalid JSON payload\"}");
      return;
    }
    auto s = g.getCityIndex(src);
    auto t = g.getCityIndex(dst);
    if (!s || !t) {
      res.status = 400;
      setJson(res, "{\"ok\":false,\"error\":\"Unknown city\"}");
      return;
    }
    auto m = static_cast<Graph::CostMetric>(static_cast<int>(metric));
    auto r = g.dijkstra(*s, *t, m);
    if (!r.reachable) {
      setJson(res, "{\"ok\":true,\"path\":\"<unreachable>\",\"total\":0}");
      return;
    }
    std::ostringstream oss;
    oss << "{\"ok\":true,\"path\":\"" << jsonEscape(g.formatPath(r.path)) << "\",\"total\":" << r.totalCost << "}";
    setJson(res, oss.str());
  });

  svr.Post(R"(^/api/mst$)", [&](const httplib::Request& req, httplib::Response& res) {
    long long metric = 1;
    if (!parseJsonInt(req.body, "metric", metric)) {
      res.status = 400;
      setJson(res, "{\"ok\":false,\"error\":\"Invalid JSON payload\"}");
      return;
    }
    auto m = static_cast<Graph::CostMetric>(static_cast<int>(metric));
    auto mst = g.kruskalMST(m);
    if (!mst.possible) {
      setJson(res, "{\"ok\":false,\"error\":\"MST not possible (disconnected graph)\"}");
      return;
    }
    std::ostringstream oss;
    oss << "{\"ok\":true,\"total\":" << mst.totalWeight << ",\"edges\":[";
    for (std::size_t i = 0; i < mst.edges.size(); ++i) {
      if (i) oss << ",";
      oss << "\"" << jsonEscape(g.formatEdge(mst.edges[i])) << "\"";
    }
    oss << "]}";
    setJson(res, oss.str());
  });

  svr.Get(R"(^/api/toll/range$)", [&](const httplib::Request& req, httplib::Response& res) {
    int l = 0, r = 0;
    try {
      auto itL = req.params.find("l");
      auto itR = req.params.find("r");
      if (itL == req.params.end() || itR == req.params.end()) throw std::runtime_error("missing");
      l = std::stoi(itL->second);
      r = std::stoi(itR->second);
    } catch (...) {
      res.status = 400;
      setJson(res, "{\"ok\":false,\"error\":\"Missing/invalid l,r\"}");
      return;
    }
    long long sum = fenwick.sumRange(l + 1, r + 1);
    std::ostringstream oss;
    oss << "{\"ok\":true,\"l\":" << std::min(l, r) << ",\"r\":" << std::max(l, r) << ",\"sum\":" << sum << "}";
    setJson(res, oss.str());
  });

  svr.Post(R"(^/api/toll/update$)", [&](const httplib::Request& req, httplib::Response& res) {
    long long edgeId = -1, newToll = 0;
    if (!parseJsonInt(req.body, "edgeId", edgeId) || !parseJsonInt(req.body, "newToll", newToll)) {
      res.status = 400;
      setJson(res, "{\"ok\":false,\"error\":\"Invalid JSON payload\"}");
      return;
    }
    if (edgeId < 0 || edgeId >= g.edgeCount()) {
      res.status = 400;
      setJson(res, "{\"ok\":false,\"error\":\"Invalid edgeId\"}");
      return;
    }
    g.updateTollByEdgeId(static_cast<int>(edgeId), newToll);
    fenwick.set(static_cast<int>(edgeId) + 1, newToll);
    std::ostringstream oss;
    oss << "{\"ok\":true,\"edge\":\"" << jsonEscape(g.formatEdge(g.edges()[static_cast<int>(edgeId)])) << "\"}";
    setJson(res, oss.str());
  });

  svr.Post(R"(^/api/bloom$)", [&](const httplib::Request& req, httplib::Response& res) {
    std::string vehicleId;
    if (!parseJsonString(req.body, "vehicleId", vehicleId) || vehicleId.empty()) {
      res.status = 400;
      setJson(res, "{\"ok\":false,\"error\":\"Invalid vehicleId\"}");
      return;
    }
    bool before = bloom.possiblyContains(vehicleId);
    bloom.add(vehicleId);
    bool after = bloom.possiblyContains(vehicleId);
    std::ostringstream oss;
    oss << "{\"ok\":true,\"before\":" << (before ? "true" : "false") << ",\"after\":" << (after ? "true" : "false")
        << "}";
    setJson(res, oss.str());
  });

  svr.Post(R"(^/api/topk$)", [&](const httplib::Request& req, httplib::Response& res) {
    long long k = 3;
    if (!parseJsonInt(req.body, "k", k) || k <= 0) {
      res.status = 400;
      setJson(res, "{\"ok\":false,\"error\":\"Invalid k\"}");
      return;
    }
    struct Item {
      long long toll;
      int edgeId;
      bool operator<(const Item& other) const { return toll < other.toll; }
    };
    std::priority_queue<Item> pq;
    for (const auto& e : g.edges()) pq.push({e.toll, e.id});
    std::ostringstream oss;
    oss << "{\"ok\":true,\"items\":[";
    for (long long i = 0; i < k && !pq.empty(); ++i) {
      auto it = pq.top();
      pq.pop();
      if (i) oss << ",";
      oss << "\"" << jsonEscape(g.formatEdge(g.edges()[it.edgeId])) << "\"";
    }
    oss << "]}";
    setJson(res, oss.str());
  });

  svr.Post(R"(^/api/bellman$)", [&](const httplib::Request& req, httplib::Response& res) {
    long long fuelSavings = 0;
    if (!parseJsonInt(req.body, "fuelSavings", fuelSavings)) {
      res.status = 400;
      setJson(res, "{\"ok\":false,\"error\":\"Invalid fuelSavings\"}");
      return;
    }
    std::vector<long long> w(g.edgeCount(), 0);
    for (const auto& e : g.edges()) w[e.id] = e.toll - fuelSavings;
    std::string note;
    g.bellmanFordDetectNegativeCycle(w, note);
    std::ostringstream oss;
    oss << "{\"ok\":true,\"note\":\"" << jsonEscape(note) << "\"}";
    setJson(res, oss.str());
  });

  int port = 8080;
  if (const char* p = std::getenv("PORT")) {
    try {
      port = std::stoi(p);
    } catch (...) {
      port = 8080;
    }
  }

  std::cout << "Server running on port " << port << "\n";
  std::cout << "Press Ctrl+C to stop.\n";
  if (!svr.listen("0.0.0.0", port)) {
    std::cerr << "Failed to start server on port " << port << ".\n";
    return 1;
  }
  return 0;
}

