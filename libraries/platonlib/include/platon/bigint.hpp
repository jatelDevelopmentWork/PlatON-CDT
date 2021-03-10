#pragma once

#include <string>
#include <cstring>
#include <array>
#include <algorithm>
#include <climits>
#include "chain.hpp"

namespace std
{

  template <size_t Bits, bool Signed>
  class WideInteger
  {
  private:
    template <typename WideInteger1, typename WideInteger2>
    class WideType
    {
      constexpr static bool first_type()
      {
        if constexpr (WideInteger1::bits > WideInteger2::bits)
        {
          return true;
        }
        else if constexpr (WideInteger1::bits == WideInteger2::bits && WideInteger1::signed_info && !WideInteger2::signed_info)
        {
          return true;
        }
        else
        {
          return false;
        }
      }

    public:
      using type = typename std::conditional<first_type(), WideInteger1, WideInteger2>::type;
    };

  public:
    static constexpr size_t bits = Bits;
    static constexpr bool signed_info = Signed;
    static constexpr size_t arr_size = Bits / 8;

  public:
    WideInteger() = default;

    template <size_t Bits2, bool Signed2>
    WideInteger(const WideInteger<Bits2, Signed2> &rhs)
    {
      size_t copy_size = std::min(arr_size, rhs.arr_size);

      // The left and right operands have the same sign
      if (Signed == Signed2)
      {
        negative_ = rhs.negative_;
        memcpy(&arr_[arr_size - copy_size], &rhs.arr_[rhs.arr_size - copy_size],
               copy_size);
      }

      // Unsigned on the left, signed on the right
      if (!Signed && Signed2)
      {
        if (rhs.negative_)
        {
          WideInteger<Bits2, Signed2> complement = (~rhs) + 1;
          memcpy(&arr_[arr_size - copy_size],
                 &complement.arr_[complement.arr_size - copy_size], copy_size);
        }
        else
        {
          memcpy(&arr_[arr_size - copy_size], &rhs.arr_[rhs.arr_size - copy_size],
                 copy_size);
        }
      }

      // Signed on the left, unsigned on the right
      if (Signed && !Signed2)
      {
        memcpy(&arr_[arr_size - copy_size], &rhs.arr_[rhs.arr_size - copy_size],
               copy_size);
      }
    }

    template <typename T,
              class = typename std::enable_if<
                  std::numeric_limits<std::decay_t<T>>::is_integer ||
                  std::numeric_limits<std::decay_t<T>>::is_iec559>::type>
    constexpr WideInteger(const T &num)
    {
      uint128_t temp_value = 0;
      if constexpr (Signed)
      {
        if (num < 0)
        {
          int128_t neg_value = num;
          negative_ = true;
          temp_value = -neg_value;
        }
        else
        {
          temp_value = num;
        }
      }
      else
      {
        temp_value = num;
      }

      for (int i = 1; temp_value != 0; temp_value >>= 8, i++)
      {
        arr_[arr_size - i] = temp_value & 0xff;
      }
    }

    // Type conversion function
    template <typename T,
              class = typename std::enable_if<
                  std::numeric_limits<std::decay_t<T>>::is_integer ||
                  std::numeric_limits<std::decay_t<T>>::is_iec559>::type>
    explicit operator T() const
    {
      uint128_t temp_value = 0;
      size_t copy_size = std::min(arr_size, size_t(16));
      for (int i = arr_size - copy_size; i < arr_size; i++)
      {
        temp_value = temp_value * 256 + arr_[i];
      }

      T result;
      if (*this < WideInteger<Bits, Signed>(0))
      {
        result = -temp_value;
      }
      else
      {
        result = temp_value;
      }

      return result;
    }

    explicit operator bool() const { return 0 != *this; }

    // Convert string to big integer
    WideInteger(const char *str, size_t str_len)
    {
      uint32_t operator_result = string_convert_operator(
          reinterpret_cast<const uint8_t *>(str), str_len, arr_, arr_size);
      negative_ = BigintResultFlag::NEGATIVE & operator_result;
      overflow_ = BigintResultFlag::OVERFLOW & operator_result;
    }

    // Conversion between byte streams
    template <typename container>
    WideInteger(const container &bytes, bool big_endian)
    {
      if (big_endian)
      {
        this->FromBigEndian(bytes);
      }
      else
      {
        this->FromLittleEndian(bytes);
      }
    }

    // Conversion between byte streams(little endian)
    template <typename container>
    WideInteger<Bits, Signed> &FromLittleEndian(const container &bytes)
    {
      static_assert(!Signed, "Only unsigned numbers can do this");
      memset(arr_, 0, arr_size);
      auto it = bytes.begin();
      for (int i = arr_size - 1; i >= 0 && it != bytes.end(); ++it, --i)
      {
        arr_[i] = *it;
      }

      return *this;
    }

    template <typename container, typename functor>
    void ToLittleEndian(container &result, const functor &func) const
    {
      static_assert(!Signed, "Only unsigned numbers can do this");
      for (int i = arr_ - 1; i >= 0; --i)
      {
        func(result, arr_[i]);
      }
    }

    // Conversion between byte streams(big endian)
    template <typename container>
    WideInteger<Bits, Signed> &FromBigEndian(const container &bytes)
    {
      static_assert(!Signed, "Only unsigned numbers can do this");
      memset(arr_, 0, arr_size);
      size_t len = bytes.size();
      if (len >= arr_size)
      {
        auto it = bytes.begin();
        if (len - arr_size > 0)
          std::advance(it, len - arr_size);
        for (int i = 0; i < arr_size && it != bytes.end(); ++i, ++it)
        {
          arr_[i] = *it;
        }
      }

      if (len < arr_size)
      {
        auto it = bytes.begin();
        for (int i = arr_size - len; i < arr_size && it != bytes.end(); ++i, ++it)
        {
          arr_[i] = *it;
        }
      }

      return *this;
    }

    template <typename container, typename functor>
    void ToBigEndian(container &result, const functor &func) const
    {
      static_assert(!Signed, "Only unsigned numbers can do this");
      for (int i = 0; i < arr_size; ++i)
      {
        func(result, arr_[i]);
      }
    }

    // Addition operator
    template <size_t Bits2, bool Signed2>
    auto operator+(
        const WideInteger<Bits2, Signed2> &rhs) const -> typename WideType<WideInteger<Bits, Signed>, WideInteger<Bits2, Signed2>>::type
    {
      typename WideType<WideInteger<Bits, Signed>, WideInteger<Bits2, Signed2>>::type result;
      uint32_t operator_result =
          bigint_binary_operator(arr_, negative_, arr_size, &(rhs.arr_[0]), rhs.negative_, rhs.arr_size,
                                 result.arr_, result.arr_size, BinaryOperator::ADD);
      result.negative_ = BigintResultFlag::NEGATIVE & operator_result;
      result.overflow_ = BigintResultFlag::OVERFLOW & operator_result;
      return result;
    }

    template <size_t Bits2, bool Signed2>
    WideInteger<Bits, Signed> &operator+=(const WideInteger<Bits2, Signed2> &rhs)
    {
      uint32_t operator_result =
          bigint_binary_operator(arr_, negative_, arr_size, &(rhs.arr_[0]), rhs.negative_, rhs.arr_size,
                                 arr_, arr_size, BinaryOperator::ADD);
      negative_ = BigintResultFlag::NEGATIVE & operator_result;
      overflow_ = BigintResultFlag::OVERFLOW & operator_result;
      return *this;
    }

    // Subtraction operator
    template <size_t Bits2, bool Signed2>
    auto operator-(
        const WideInteger<Bits2, Signed2> &rhs) const -> typename WideType<WideInteger<Bits, Signed>, WideInteger<Bits2, Signed2>>::type
    {
      typename WideType<WideInteger<Bits, Signed>, WideInteger<Bits2, Signed2>>::type result;
      uint32_t operator_result =
          bigint_binary_operator(arr_, negative_, arr_size, &(rhs.arr_[0]), rhs.negative_, rhs.arr_size,
                                 result.arr_, result.arr_size, BinaryOperator::SUB);
      result.negative_ = BigintResultFlag::NEGATIVE & operator_result;
      result.overflow_ = BigintResultFlag::OVERFLOW & operator_result;
      return result;
    }

    template <size_t Bits2, bool Signed2>
    WideInteger<Bits, Signed> &operator-=(const WideInteger<Bits2, Signed2> &rhs)
    {
      uint32_t operator_result =
          bigint_binary_operator(arr_, negative_, arr_size, &(rhs.arr_[0]), rhs.negative_, rhs.arr_size,
                                 arr_, arr_size, BinaryOperator::SUB);
      negative_ = BigintResultFlag::NEGATIVE & operator_result;
      overflow_ = BigintResultFlag::OVERFLOW & operator_result;
      return *this;
    }

    constexpr WideInteger<Bits, Signed> operator-() const
    {
      static_assert(Signed, "Only signed numbers can do this");
      WideInteger<Bits, Signed> result = *this;
      result.Neg();
      return result;
    }

    // Multiplication operator
    template <size_t Bits2, bool Signed2>
    auto operator*(
        const WideInteger<Bits2, Signed2> &rhs) const -> typename WideType<WideInteger<Bits, Signed>, WideInteger<Bits2, Signed2>>::type
    {
      typename WideType<WideInteger<Bits, Signed>, WideInteger<Bits2, Signed2>>::type result;
      uint32_t operator_result =
          bigint_binary_operator(arr_, negative_, arr_size, &(rhs.arr_[0]), rhs.negative_, rhs.arr_size,
                                 result.arr_, result.arr_size, BinaryOperator::MUL);
      result.negative_ = BigintResultFlag::NEGATIVE & operator_result;
      result.overflow_ = BigintResultFlag::OVERFLOW & operator_result;
      return result;
    }

    template <size_t Bits2, bool Signed2>
    WideInteger<Bits, Signed> &operator*=(const WideInteger<Bits2, Signed2> &rhs)
    {
      uint32_t operator_result =
          bigint_binary_operator(arr_, negative_, arr_size, &(rhs.arr_[0]), rhs.negative_, rhs.arr_size,
                                 arr_, arr_size, BinaryOperator::MUL);
      negative_ = BigintResultFlag::NEGATIVE & operator_result;
      overflow_ = BigintResultFlag::OVERFLOW & operator_result;
      return *this;
    }

    // Division operator
    template <size_t Bits2, bool Signed2>
    auto operator/(
        const WideInteger<Bits2, Signed2> &rhs) const -> typename WideType<WideInteger<Bits, Signed>, WideInteger<Bits2, Signed2>>::type
    {
      typename WideType<WideInteger<Bits, Signed>, WideInteger<Bits2, Signed2>>::type result;
      uint32_t operator_result =
          bigint_binary_operator(arr_, negative_, arr_size, &(rhs.arr_[0]), rhs.negative_, rhs.arr_size,
                                 result.arr_, result.arr_size, BinaryOperator::DIV);
      result.negative_ = BigintResultFlag::NEGATIVE & operator_result;
      result.overflow_ = BigintResultFlag::OVERFLOW & operator_result;
      return result;
    }

    template <size_t Bits2, bool Signed2>
    WideInteger<Bits, Signed> &operator/=(const WideInteger<Bits2, Signed2> &rhs)
    {
      uint32_t operator_result =
          bigint_binary_operator(arr_, negative_, arr_size, &(rhs.arr_[0]), rhs.negative_, rhs.arr_size,
                                 arr_, arr_size, BinaryOperator::DIV);
      negative_ = BigintResultFlag::NEGATIVE & operator_result;
      overflow_ = BigintResultFlag::OVERFLOW & operator_result;
      return *this;
    }

    // Modulo operator
    template <size_t Bits2, bool Signed2>
    auto operator%(
        const WideInteger<Bits2, Signed2> &rhs) const -> typename WideType<WideInteger<Bits, Signed>, WideInteger<Bits2, Signed2>>::type
    {
      typename WideType<WideInteger<Bits, Signed>, WideInteger<Bits2, Signed2>>::type result;
      uint32_t operator_result =
          bigint_binary_operator(arr_, negative_, arr_size, &(rhs.arr_[0]), rhs.negative_, rhs.arr_size,
                                 result.arr_, result.arr_size, BinaryOperator::MOD);
      result.negative_ = BigintResultFlag::NEGATIVE & operator_result;
      result.overflow_ = BigintResultFlag::OVERFLOW & operator_result;
      return result;
    }

    template <size_t Bits2, bool Signed2>
    WideInteger<Bits, Signed> &operator%=(const WideInteger<Bits2, Signed2> &rhs)
    {
      uint32_t operator_result =
          bigint_binary_operator(arr_, negative_, arr_size, &(rhs.arr_[0]), rhs.negative_, rhs.arr_size,
                                 arr_, arr_size, BinaryOperator::MOD);
      negative_ = BigintResultFlag::NEGATIVE & operator_result;
      overflow_ = BigintResultFlag::OVERFLOW & operator_result;
      return *this;
    }

    // And operator
    template <size_t Bits2, bool Signed2>
    auto operator&(
        const WideInteger<Bits2, Signed2> &rhs) const -> typename WideType<WideInteger<Bits, Signed>, WideInteger<Bits2, Signed2>>::type
    {
      typename WideType<WideInteger<Bits, Signed>, WideInteger<Bits2, Signed2>>::type result;
      uint32_t operator_result =
          bigint_binary_operator(arr_, negative_, arr_size, &(rhs.arr_[0]), rhs.negative_, rhs.arr_size,
                                 result.arr_, result.arr_size, BinaryOperator::AND);
      result.negative_ = BigintResultFlag::NEGATIVE & operator_result;
      result.overflow_ = BigintResultFlag::OVERFLOW & operator_result;
      return result;
    }

    template <size_t Bits2, bool Signed2>
    WideInteger<Bits, Signed> &operator&=(
        const WideInteger<Bits2, Signed2> &rhs) noexcept
    {
      uint32_t operator_result =
          bigint_binary_operator(arr_, negative_, arr_size, &(rhs.arr_[0]), rhs.negative_, rhs.arr_size,
                                 arr_, arr_size, BinaryOperator::AND);
      negative_ = BigintResultFlag::NEGATIVE & operator_result;
      overflow_ = BigintResultFlag::OVERFLOW & operator_result;
      return *this;
    }

    // OR operator
    template <size_t Bits2, bool Signed2>
    auto operator|(
        const WideInteger<Bits2, Signed2> &rhs) const -> typename WideType<WideInteger<Bits, Signed>, WideInteger<Bits2, Signed2>>::type
    {
      typename WideType<WideInteger<Bits, Signed>, WideInteger<Bits2, Signed2>>::type result;
      uint32_t operator_result =
          bigint_binary_operator(arr_, negative_, arr_size, &(rhs.arr_[0]), rhs.negative_, rhs.arr_size,
                                 result.arr_, result.arr_size, BinaryOperator::OR);
      result.negative_ = BigintResultFlag::NEGATIVE & operator_result;
      result.overflow_ = BigintResultFlag::OVERFLOW & operator_result;
      return result;
    }

    template <size_t Bits2, bool Signed2>
    WideInteger<Bits, Signed> &operator|=(const WideInteger<Bits2, Signed2> &rhs)
    {
      uint32_t operator_result =
          bigint_binary_operator(arr_, negative_, arr_size, &(rhs.arr_[0]), rhs.negative_, rhs.arr_size,
                                 arr_, arr_size, BinaryOperator::OR);
      negative_ = BigintResultFlag::NEGATIVE & operator_result;
      overflow_ = BigintResultFlag::OVERFLOW & operator_result;
      return *this;
    }

    // XOR operator
    template <size_t Bits2, bool Signed2>
    auto operator^(
        const WideInteger<Bits2, Signed2> &rhs) const -> typename WideType<WideInteger<Bits, Signed>, WideInteger<Bits2, Signed2>>::type
    {
      typename WideType<WideInteger<Bits, Signed>, WideInteger<Bits2, Signed2>>::type result;
      uint32_t operator_result =
          bigint_binary_operator(arr_, negative_, arr_size, &(rhs.arr_[0]), rhs.negative_, rhs.arr_size,
                                 result.arr_, result.arr_size, BinaryOperator::XOR);
      result.negative_ = BigintResultFlag::NEGATIVE & operator_result;
      result.overflow_ = BigintResultFlag::OVERFLOW & operator_result;
      return result;
    }

    template <size_t Bits2, bool Signed2>
    WideInteger<Bits, Signed> &operator^=(const WideInteger<Bits2, Signed2> &rhs)
    {
      uint32_t operator_result =
          bigint_binary_operator(arr_, negative_, arr_size, &(rhs.arr_[0]), rhs.negative_, rhs.arr_size,
                                 arr_, arr_size, BinaryOperator::XOR);
      negative_ = BigintResultFlag::NEGATIVE & operator_result;
      overflow_ = BigintResultFlag::OVERFLOW & operator_result;
      return *this;
    }

    // Negation operator
    WideInteger<Bits, Signed> operator~() const
    {
      WideInteger<Bits, Signed> result;
      if constexpr (Signed)
      {
        WideInteger<Bits, Signed> temp = -1;
        result = *this | temp;
      }
      else
      {
        WideInteger<Bits, Signed> temp;
        temp.Fill();
        result = *this | temp;
      }
      return result;
    }

    // Parity operator
    template <size_t Bits2, bool Signed2>
    bool operator==(const WideInteger<Bits2, Signed2> &rhs) const
    {
      int result =
          bigint_cmp(arr_, negative_, arr_size, &(rhs.arr_[0]), rhs.negative_, rhs.arr_size);
      return 0 == result;
    }

    template <size_t Bits2, bool Signed2>
    bool operator!=(const WideInteger<Bits2, Signed2> &rhs) const
    {
      return !(*this == rhs);
    }

    // Comparison operator
    template <size_t Bits2, bool Signed2>
    bool operator<(const WideInteger<Bits2, Signed2> &rhs) const
    {
      int result =
          bigint_cmp(arr_, negative_, arr_size, &(rhs.arr_[0]), rhs.negative_, rhs.arr_size);
      return -1 == result;
    }

    template <size_t Bits2, bool Signed2>
    bool operator>(const WideInteger<Bits2, Signed2> &rhs) const
    {
      return !(*this == rhs) && !(*this < rhs);
    }

    template <size_t Bits2, bool Signed2>
    bool operator>=(const WideInteger<Bits2, Signed2> &rhs) const
    {
      return *this == rhs || *this > rhs;
    }

    template <size_t Bits2, bool Signed2>
    bool operator<=(const WideInteger<Bits2, Signed2> &rhs) const
    {
      return *this == rhs || *this < rhs;
    }

    // Increment operator
    WideInteger<Bits, Signed> &operator++()
    {
      *this += WideInteger<Bits, Signed>(1);
      return *this;
    };

    WideInteger<Bits, Signed> &operator++(int)
    {
      WideInteger<Bits, Signed> result = *this;
      *this += WideInteger<Bits, Signed>(1);
      return result;
    };

    // Decrement operator
    WideInteger<Bits, Signed> &operator--()
    {
      *this -= WideInteger<Bits, Signed>(1);
      return *this;
    };

    WideInteger<Bits, Signed> &operator--(int)
    {
      WideInteger<Bits, Signed> result = *this;
      *this -= WideInteger<Bits, Signed>(1);
      return result;
    };

    // Left shift operator
    WideInteger<Bits, Signed> operator<<(uint32_t n) const
    {
      WideInteger<Bits, Signed> result;
      uint32_t operator_result = bigint_sh(arr_, negative_, arr_size, &(result.arr_[0]),
                                           arr_size, n, ShiftDirection::LEFT);
      result.negative_ = BigintResultFlag::NEGATIVE & operator_result;
      result.overflow_ = BigintResultFlag::OVERFLOW & operator_result;
      return result;
    }

    WideInteger<Bits, Signed> &operator<<=(uint32_t n)
    {
      uint32_t operator_result =
          bigint_sh(arr_, negative_, arr_size, arr_, arr_size, n, ShiftDirection::LEFT);
      negative_ = BigintResultFlag::NEGATIVE & operator_result;
      overflow_ = BigintResultFlag::OVERFLOW & operator_result;
      return *this;
    }

    // Right shift operator
    WideInteger<Bits, Signed> operator>>(uint32_t n) const
    {
      WideInteger<Bits, Signed> result;
      uint32_t operator_result = bigint_sh(arr_, negative_, arr_size, &(result.arr_[0]),
                                           arr_size, n, ShiftDirection::RIGHT);
      result.negative_ = BigintResultFlag::NEGATIVE & operator_result;
      result.overflow_ = BigintResultFlag::OVERFLOW & operator_result;
      return result;
    }

    WideInteger<Bits, Signed> &operator>>=(uint32_t n)
    {
      uint32_t operator_result =
          bigint_sh(arr_, negative_, arr_size, arr_, arr_size, n, ShiftDirection::RIGHT);
      negative_ = BigintResultFlag::NEGATIVE & operator_result;
      overflow_ = BigintResultFlag::OVERFLOW & operator_result;
      return *this;
    }

    // zk algorithm custom operator
    template <size_t Bits2, bool Signed2, size_t Bits3, bool Signed3>
    friend auto Exp(const WideInteger<Bits, Signed> &left,
                    const WideInteger<Bits2, Signed2> &right,
                    const WideInteger<Bits3, Signed3> &mod) ->
        typename WideType<typename WideType<WideInteger<Bits, Signed>, WideInteger<Bits2, Signed2>>::type, WideInteger<Bits3, Signed3>>::type
    {
      typename WideType<typename WideType<WideInteger<Bits, Signed>, WideInteger<Bits2, Signed2>>::type, WideInteger<Bits3, Signed3>>::type result;
      uint32_t operator_result = bigint_exp_mod(
          &(left.arr_[0]), left.negative_, left.arr_size, &(right.arr_[0]), right.negative_, right.arr_size,
          &(mod.arr_[0]), mod.negative_, mod.arr_size, result.arr_, result.arr_size);
      result.negative_ = BigintResultFlag::NEGATIVE & operator_result;
      result.overflow_ = BigintResultFlag::OVERFLOW & operator_result;
      return result;
    }

    bool Overflow() { return overflow_; }

    uint8_t *Value() { return arr_; }

  private:
    void Neg() { negative_ = !negative_; }

    void Fill()
    {
      for (int i = 0; i < arr_size; i++)
      {
        arr_[i] = 0xff;
      }
    }

  public:
    bool negative_ = false;
    bool overflow_ = false;
    uint8_t arr_[arr_size] = {};
  };

  // Type instance
  template <size_t Bits>
  using WideInt = WideInteger<Bits, true>;

  template <size_t Bits>
  using WideUint = WideInteger<Bits, false>;

  using int256_t = WideInt<256>;
  using uint256_t = WideUint<256>;

  using int512_t = WideInt<512>;
  using uint512_t = WideUint<512>;

  template <size_t Bits, bool Signed>
  string to_string(const WideInteger<Bits, Signed> &value)
  {
    const char *charmap = "0123456789";
    string result;
    WideInteger<Bits, Signed> helper = value;
    if (helper < WideInteger<Bits, Signed>(0))
      helper = WideInteger<Bits, Signed>(0) - helper;
    do
    {
      result += charmap[int(helper % WideInteger<Bits, Signed>(10))];
      helper /= WideInteger<Bits, Signed>(10);
    } while (helper != WideInteger<Bits, Signed>(0));
    reverse(result.begin(), result.end());

    if (value < WideInteger<Bits, Signed>(0))
      result = "-" + result;
    return result;
  }

} // namespace std

std::int256_t operator"" _int256(const char *str_value, size_t n);
std::int512_t operator"" _int512(const char *str_value, size_t n);
std::uint256_t operator"" _uint256(const char *str_value, size_t n);
std::uint512_t operator"" _uint512(const char *str_value, size_t n);
