#undef NDEBUG
#include "platon/platon.hpp"
#include "platon/mimc.hpp"
#include "platon/bigint.hpp"
#include "unit_test.hpp"

TEST_CASE(mimc, hash) {
    std::vector<uint8_t> result(32);
    const char* data = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";
    std::vector<uint8_t> input(reinterpret_cast<const uint8_t *>(data), reinterpret_cast<const uint8_t *>(data) + strlen(data));

    const char* fn = "mimc hash";
    platon_debug_gas(__LINE__, fn, strlen(fn));
    Mimc::hash_bytes(input, &result[0]);
    platon_debug_gas(__LINE__, fn, strlen(fn));

    std::uint256_t bigint_hash;
    bigint_hash.FromBigEndian(result);
    printf("%s\t\n", std::to_string(bigint_hash).c_str());

    // Mimc::hash(std::vector<std::uint256_t>{12, 45}, 0, &result[0]);
    // bigint_hash.FromBigEndian(result);
    // printf("%s\t\n", std::to_string(bigint_hash).c_str());
}

TEST_CASE(mimc, initial){
  // printf("%s\t\n", std::to_string(Mimc::Q).c_str());
  // for(auto one : Mimc::cts){
  //   printf("%s\t\n", std::to_string(one).c_str());
  // }

  std::uint256_t jatel = Mimc::mimc7_hash(1, 2);
  printf("%s\t\n", std::to_string(jatel).c_str());

  std::uint256_t a = 6;
  std::uint512_t b = 10;
  std::uint512_t c = b % a;
  printf("%s\t\n", std::to_string(c).c_str());
}

UNITTEST_MAIN() {
  // RUN_TEST(mimc, hash);
  RUN_TEST(mimc, initial);
}