#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Simple Bloom filter for probabilistic membership queries.
// False positives possible; false negatives not possible (for inserted items).
class BloomFilter {
public:
  BloomFilter() = default;
  BloomFilter(std::size_t bitCount, std::size_t hashCount);

  void reset(std::size_t bitCount, std::size_t hashCount);

  void add(const std::string& item);
  bool possiblyContains(const std::string& item) const;

  std::size_t bitCount() const { return bitCount_; }
  std::size_t hashCount() const { return hashCount_; }

private:
  std::size_t bitCount_ = 0;
  std::size_t hashCount_ = 0;
  std::vector<std::uint8_t> bits_; // packed bits

  void setBit(std::size_t idx);
  bool getBit(std::size_t idx) const;

  static std::uint64_t fnv1a64(const std::string& s);
  static std::uint64_t splitmix64(std::uint64_t x);
};

