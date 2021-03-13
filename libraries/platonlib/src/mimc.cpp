#include "platon/mimc.hpp"

const char * const Mimc::seed = "mimc";
const std::uint256_t Mimc::Q = "21888242871839275222246405745257275088548364400416034343698204186575808495617"_uint256;
const std::vector<std::uint256_t> Mimc::cts = Mimc::get_constants();