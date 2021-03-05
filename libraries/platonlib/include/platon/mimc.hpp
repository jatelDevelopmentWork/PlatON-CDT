#pragma once
#include <array>
#include <vector>
#include <deque>
#include <cstring>
#include "platon/chain.hpp"
#include "platon/bigint.hpp"

class Mimc
{
private:
  constexpr static int rounds = 91;
  static const char *const seed;
  const static std::uint256_t Q;
  const static std::vector<std::uint256_t> cts;

public:
  static std::vector<std::uint256_t> get_constants()
  {
    std::vector<std::uint256_t> result;
    result.push_back(std::uint256_t(0));
    std::uint256_t c;
    std::vector<uint8_t> temp(32);
    platon_sha3(reinterpret_cast<const uint8_t *>(seed), strlen(seed), &temp[0], 32);
    c.FromBigEndian(temp);
    auto func = [](std::vector<uint8_t> &result, uint8_t one) {
      result.push_back(one);
    };
    for (int i = 1; i < rounds; i++)
    {
      std::vector<uint8_t> origin_data;
      c.ToBigEndian(origin_data, func);
      platon_sha3(&origin_data[0], origin_data.size(), &temp[0], 32);
      c.FromBigEndian(temp);
      std::uint256_t n = c % Q;
      result.push_back(n);
    }

    return result;
  }

  static void hash(const std::vector<std::uint256_t> &arr, std::uint256_t key,
                   uint8_t *result)
  {
    std::uint256_t r = key;
    for (auto one : arr)
    {
      r = r + one + mimc7_hash(one, r);
      r = r % Q;
    }

    auto func = [](std::vector<uint8_t> &result, uint8_t one) { result.push_back(one); };
    std::vector<uint8_t> temp;
    r.ToBigEndian(temp, func);
    std::copy(temp.begin(), temp.end(), result);
  }

  static std::uint256_t mimc7_hash(std::uint256_t in_x, std::uint256_t in_k)
  {
    std::uint256_t h = 0;
    for (int i = 0; i < rounds; i++)
    {
      std::uint256_t t;
      if (0 == i)
      {
        t = in_x + in_k;
      }
      else
      {
        t = h + in_k + cts[i];
      }

      std::uint256_t t2 = t * t;
      std::uint256_t t4 = t2 * t2;
      h = t4 * t2 * t;
      h = h % Q;
    }

    return (h + in_k) % Q;
  }

  static void hash_bytes(const std::vector<uint8_t> &input, uint8_t *output)
  {
    constexpr size_t n = 31;
    size_t len = input.size();
    size_t group = len / n;

    std::vector<std::uint256_t> elements;
    for (int i = 0; i < group; i++)
    {
      std::vector<uint8_t> temp(&input[n * i], &input[n * (i + 1)]);
      std::uint256_t one(temp, false);
      elements.push_back(one);
    }

    if (0 != len % n)
    {
      std::vector<uint8_t> temp(&input[n * group], &input[len]);
      std::uint256_t one(temp, false);
      elements.push_back(one);
    }

    hash(elements, 0, output);
  }
};
