#pragma once

#include <vector>

// Fenwick Tree / Binary Indexed Tree for point updates + prefix/range sums.
class FenwickTree {
public:
  FenwickTree() = default;
  explicit FenwickTree(int n);

  void reset(int n);

  // Adds delta at index idx (1-based).
  void add(int idx, long long delta);

  // Sets value at idx (1-based) to newValue.
  void set(int idx, long long newValue);

  long long sumPrefix(int idx) const;
  long long sumRange(int l, int r) const;

  int size() const { return n_; }

private:
  int n_ = 0;
  std::vector<long long> bit_;
  std::vector<long long> values_;
};

