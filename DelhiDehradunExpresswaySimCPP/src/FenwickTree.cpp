#include "FenwickTree.hpp"

#include <algorithm>
#include <stdexcept>

FenwickTree::FenwickTree(int n) { reset(n); }

void FenwickTree::reset(int n) {
  if (n < 0) throw std::invalid_argument("FenwickTree size cannot be negative");
  n_ = n;
  bit_.assign(n_ + 1, 0);
  values_.assign(n_ + 1, 0);
}

void FenwickTree::add(int idx, long long delta) {
  if (idx <= 0 || idx > n_) throw std::out_of_range("FenwickTree index out of range");
  values_[idx] += delta;
  for (int i = idx; i <= n_; i += i & -i) bit_[i] += delta;
}

void FenwickTree::set(int idx, long long newValue) {
  if (idx <= 0 || idx > n_) throw std::out_of_range("FenwickTree index out of range");
  long long delta = newValue - values_[idx];
  add(idx, delta);
}

long long FenwickTree::sumPrefix(int idx) const {
  idx = std::clamp(idx, 0, n_);
  long long res = 0;
  for (int i = idx; i > 0; i -= i & -i) res += bit_[i];
  return res;
}

long long FenwickTree::sumRange(int l, int r) const {
  if (l > r) std::swap(l, r);
  l = std::clamp(l, 1, n_);
  r = std::clamp(r, 1, n_);
  if (n_ == 0) return 0;
  if (l > r) return 0;
  return sumPrefix(r) - sumPrefix(l - 1);
}

