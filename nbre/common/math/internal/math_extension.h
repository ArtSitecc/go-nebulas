// Copyright (C) 2018 go-nebulas authors
//
// This file is part of the go-nebulas library.
//
// the go-nebulas library is free software: you can redistribute it and/or
// modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// the go-nebulas library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with the go-nebulas library.  If not, see
// <http://www.gnu.org/licenses/>.
//
#pragma once
#include "common/math/internal/math_template.h"

namespace neb {
namespace math {
template <typename T> T exp(const T &x) {
  T one = softfloat_cast<uint32_t, typename T::value_type>(1);
  T ret = one;

  T i = one;
  T prev = one;
  T px = x;

  while (true) {
    T tmp;

    tmp = ret + px / prev;
    if (tmp - ret < MATH_MIN && ret - tmp < MATH_MIN) {
      break;
    }
    ret = tmp;
    i += one;
    px = px * x;
    prev = prev * i;
  }

  return ret;
}

template <typename T> T arctan(const T &x) {
  T one = softfloat_cast<uint32_t, typename T::value_type>(1);
  T two = softfloat_cast<uint32_t, typename T::value_type>(2);
  T x2 = x * x;

  T ret = 0;
  T i = one;
  T s = x;
  bool odd = false;

  while (true) {
    T tmp;
    if (odd) {
      tmp = ret - s / i;
    } else {
      tmp = ret + s / i;
    }
    if (tmp - ret < MATH_MIN && ret - tmp < MATH_MIN) {
      break;
    }
    ret = tmp;
    odd = !odd;
    i += two;
    s = s * x2;
  }
  return ret;
}

template <typename T> T sin(const T &x) {
  T one = softfloat_cast<uint32_t, typename T::value_type>(1);
  T two = softfloat_cast<uint32_t, typename T::value_type>(2);
  T x2 = x * x;

  T ret = 0;
  T i = one;
  T ji = one;
  T s = x;
  bool odd = false;

  while (true) {
    T tmp;
    if (odd) {
      tmp = ret - s / ji;
    } else {
      tmp = ret + s / ji;
    }
    LOG(INFO) << "tmp: " << tmp;
    if (tmp - ret < MATH_MIN && ret - tmp < MATH_MIN) {
      break;
    }
    ret = tmp;
    odd = !odd;
    i += one;
    ji = ji * i;
    i += one;
    ji = ji * i;
    s = s * x2;
  }
  return ret;
}

template <typename T> T ln(const T &x) {
    T one = softfloat_cast<uint32_t, typename T::value_type>(1);
    T two = softfloat_cast<uint32_t, typename T::value_type>(2);
    T v;
    auto func = [&](T v) {
      T ret = softfloat_cast<uint32_t, typename T::value_type>(0);
      bool odd = true;

      T s = v;
      T i = one;

      while (true) {
        T tmp;
        if (odd) {
          tmp = ret + s / i;
        } else {
          tmp = ret - s / i;
        }

        if (tmp - ret < MATH_MIN && ret - tmp < MATH_MIN) {
          break;
        }
        ret = tmp;
        i += one;
        odd = !odd;
        s = s * v;
      }
      return ret;
    };
    if (x <= two) {
      v = x - one;
      return func(v);
    } else {
      return -func(one / x - one);
    }
}
namespace internal {
union float16_detail_t {
  float16_t v;
  struct {
    uint64_t ey : 10;
    uint16_t exponent : 5;
    uint8_t sign : 1;
  } detail;
};
union float32_detail_t {
  float32_t v;
  struct {
    uint64_t fraction : 23;
    uint16_t exponent : 8;
    uint8_t sign : 1;
  } detail;
};
union float64_detail_t {
  float64_t v;
  struct {
    uint64_t fraction : 52;
    uint16_t exponent : 11;
    uint8_t sign : 1;
  } detail;
};
union float128_detail_t {
  float128_t v;
  struct {
    uint64_t fraction0 : 48;
    uint64_t fraction1 : 64;
    uint16_t exponent : 15;
    uint8_t sign : 1;
  } detail;
};

template <typename T> struct float_detail {};
template <> struct float_detail<float16> {
  typedef float16_detail_t value_type;
  constexpr static uint16_t bias = 15;
};
template <> struct float_detail<float32> {
  typedef float32_detail_t value_type;
  constexpr static uint16_t bias = 127;
};
template <> struct float_detail<float64> {
  typedef float64_detail_t value_type;
  constexpr static uint16_t bias = 1023;
};
template <> struct float_detail<float128> {
  typedef float128_detail_t value_type;
  constexpr static uint16_t bias = 16383;
};

template <typename T> struct float_math_helper {
  typedef typename T::value_type value_type;
  typedef typename float_detail<T>::value_type detail_type;

  static T get_exponent(const T &x) {
    detail_type dt;
    dt.v = (T)x;
    detail_type ret;
    ret.detail.sign = dt.detail.sign;
    ret.detail.exponent = dt.detail.exponent;
    return T(ret.v);
  }
  static T get_one_plus_fraction(const T &x) {
    detail_type dt;
    dt.v = (T)x;
    detail_type ret;
    ret.v = dt.v;
    ret.detail.exponent = detail_type::bias;
    return T(ret.v);
  }
};
} // namespace internal

template <typename T> T log2(const T &x) { return ln(x) / constants<T>::s_ln2; }

//! return x^y
template <typename T> T pow(const T &x, const T &y) {
  // auto ex = internal::float_math_helper<T>::get_exponent(x);
  // auto ey = internal::float_math_helper<T>::get_one_plus_fraction(x);
  // auto J = log2(ey);
  // auto pow_ex = y * (ex + J);
  // return exp(pow_ex * constants<T>::s_ln2);
  return exp(y * ln(x));
}

} // namespace math

} // namespace neb
