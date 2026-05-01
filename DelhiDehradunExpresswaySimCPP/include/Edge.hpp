#pragma once

#include <string>

struct Edge {
  int from = -1;
  int to = -1;
  long long distance = 0;
  long long time = 0;
  long long toll = 0;

  // Stable id for toll-management (Fenwick) and top-K queries.
  int id = -1;

  // Display-friendly names (optional; Graph owns authoritative names).
  std::string fromName;
  std::string toName;
};

