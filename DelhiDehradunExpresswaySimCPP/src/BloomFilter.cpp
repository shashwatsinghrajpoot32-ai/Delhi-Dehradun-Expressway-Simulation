#include "BloomFilter.hpp"

#include <algorithm>
#include <stdexcept>

BloomFilter::BloomFilter(std::size_t bitCount, std::size_t hashCount) {
  reset(bitCount, hashCount);
}

void BloomFilter::reset(std::size_t bitCount, std::size_t hashCount) {
  if (bitCount == 0) throw std::invalid_argument("BloomFilter bitCount must be > 0");
  if (hashCount == 0) throw std::invalid_argument("BloomFilter hashCount must be > 0");
  bitCount_ = bitCount;
  hashCount_ = hashCount;
  std::size_t byteCount = (bitCount_ + 7) / 8;
  bits_.assign(byteCount, 0);
}

void BloomFilter::setBit(std::size_t idx) {
  idx %= bitCount_;
  bits_[idx / 8] |= static_cast<std::uint8_t>(1u << (idx % 8));
}

bool BloomFilter::getBit(std::size_t idx) const {
  idx %= bitCount_;
  return (bits_[idx / 8] & static_cast<std::uint8_t>(1u << (idx % 8))) != 0;
}

std::uint64_t BloomFilter::fnv1a64(const std::string& s) {
  std::uint64_t hash = 1469598103934665603ull;
  for (unsigned char c : s) {
    hash ^= static_cast<std::uint64_t>(c);
    hash *= 1099511628211ull;
  }
  return hash;
}

std::uint64_t BloomFilter::splitmix64(std::uint64_t x) {
  x += 0x9e3779b97f4a7c15ull;
  x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
  x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
  return x ^ (x >> 31);
}

void BloomFilter::add(const std::string& item) {
  if (bitCount_ == 0 || hashCount_ == 0) throw std::logic_error("BloomFilter not initialized");
  std::uint64_t h1 = fnv1a64(item);
  std::uint64_t h2 = splitmix64(h1) | 1ull; // ensure non-zero odd step

  for (std::size_t i = 0; i < hashCount_; ++i) {
    std::uint64_t combined = h1 + i * h2;
    setBit(static_cast<std::size_t>(combined % bitCount_));
  }
}

bool BloomFilter::possiblyContains(const std::string& item) const {
  if (bitCount_ == 0 || hashCount_ == 0) throw std::logic_error("BloomFilter not initialized");
  std::uint64_t h1 = fnv1a64(item);
  std::uint64_t h2 = splitmix64(h1) | 1ull;

  for (std::size_t i = 0; i < hashCount_; ++i) {
    std::uint64_t combined = h1 + i * h2;
    if (!getBit(static_cast<std::size_t>(combined % bitCount_))) return false;
  }
  return true;
}

