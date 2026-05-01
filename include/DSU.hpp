#pragma once

#include <vector>

class DSU {
public:
  explicit DSU(int n = 0);

  void reset(int n);
  int find(int x);
  bool unite(int a, int b);

private:
  std::vector<int> parent_;
  std::vector<int> size_;
};

