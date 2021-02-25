#pragma once
#include <array>
#include <vector>
#include "platon/bigint.hpp"
#include "platon/chain.hpp"

class Mimc {
 private:
  constexpr static int rounds = 91;
  constexpr static char *seed = "mimc";
  const static std::uint256_t Q =
      "21888242871839275222246405745257275088548364400416034343698204186575808495617"_uint256;

 private:
  static std::vector<std::uint256_t> get_constants() {
    std::vector<std::uint256_t> result;
    result.push_back(std::uint256_t(0));
    std::vector<uint8_t> temp(32);
    platon_sha3(reinterpret_cast<const uint8_t *>(seed), strlen(seed), &temp[0],
                temp.size());
    std::uint256_t c;
    c.FromBigEndian(temp);
    for (int i = 1; i < rounds; i++) {
      std::vector<uint8_t> temp(32);
      std::vector<uint8_t> origin_data = c.ToBigEndian();
      platon_sha3(&origin_data[0], origin_data.size(), &temp[0], temp.size());
      c.FromBigEndian(temp);
      std::uint256_t n = c % Q;
      result.push_back(n);
    }

    return result;
  }

  const static std::vector<std::uint256_t> cts = get_constants();

 public:
  void hash(const std::vector<uint8_t> &input, uint8_t *output) {
    constexpr size_t n = 31;
    size_t len = input.size();
    size_t group = len / n;

    std::vector<std::uint256_t> elements;
    for (int i = 0; i < group; i++) {
      std::vector<uint8_t> temp(&input[n * i], &input[n * (i + 1)]);
      std::reverse(temp.begin(), temp.end());
      std::uint256_t one;
      one.FromBigEndian(temp);
      elements.push_back(one);
    }

    if (0 != len % n) {
      std::vector<uint8_t> temp(&input[n * group], &input[len]);
      std::reverse(temp.begin(), temp.end());
      std::uint256_t one;
      one.FromBigEndian(temp);
      elements.push_back(one);
    }

    hash(elements, 0, output);
  }

  void hash(const std::vector<std::uint256_t> &arr, std::uint256_t key,
            uint8_t *result) {
    std::uint256_t r = key;
    for (auto one : arr) {
      r = r + one + mimc7_hash(one, r);
      r = r % Q;
    }

    std::vector<uint8_t> temp = r.ToBigEndian();
    std::copy(temp.begin(), temp.end(), result);
  }

  std::uint256_t mimc7_hash(std::uint256_t in_x, std::uint256_t in_k) {
    std::uint256_t h = 0;
    for (int i = 0; i < rounds; i++) {
      std::uint256_t t;
      if (0 == i) {
        t = in_x + in_k;
      } else {
        t = h + in_k + cts[i];
      }

      std::uint256_t t2 = t * t;
      std::uint256_t t4 = t2 * t2;
      h = t4 * t2 * t;
      h = h % Q;
    }

    return (h + in_k) % Q;
  }
};

