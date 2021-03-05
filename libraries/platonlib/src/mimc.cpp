#include "platon/mimc.hpp"

std::int256_t operator"" _int256(const char *str_value, size_t n) {
  std::int256_t result(str_value, n);
  return result;
}

std::int512_t operator"" _int512(const char *str_value, size_t n) {
  std::int512_t result(str_value, n);
  return result;
}

std::uint256_t operator"" _uint256(const char *str_value, size_t n) {
  std::uint256_t result(str_value, n);
  return result;
}

std::uint512_t operator"" _uint512(const char *str_value, size_t n) {
  std::uint512_t result(str_value, n);
  return result;
}

const char * const Mimc::seed = "mimc";
const std::uint256_t Mimc::Q = "21888242871839275222246405745257275088548364400416034343698204186575808495617"_uint256;
const std::vector<std::uint256_t> Mimc::cts = Mimc::get_constants();