#include "DSU.hpp"

#include <numeric>

DSU::DSU(int n) { reset(n); }

void DSU::reset(int n) {
  parent_.resize(n);
  size_.assign(n, 1);
  std::iota(parent_.begin(), parent_.end(), 0);
}

int DSU::find(int x) {
  if (parent_[x] == x) return x;
  parent_[x] = find(parent_[x]);
  return parent_[x];
}

bool DSU::unite(int a, int b) {
  a = find(a);
  b = find(b);
  if (a == b) return false;
  if (size_[a] < size_[b]) std::swap(a, b);
  parent_[b] = a;
  size_[a] += size_[b];
  return true;
}

