#pragma once
// Exact match counts: a non-negative integer that stays in unsigned __int128 while it fits and moves to
// GMP (mpz_t) on the first overflow. Used for per-compressed-group counts and per-thread totals so that counts above
// 2^64 (and above 2^128) are reported exactly instead of wrapping or saturating at 2^64-1.

#include <gmp.h>

#include <cstdint>
#include <cstring>
#include <string>

namespace circinus {

using u128 = unsigned __int128;

inline void mpzSetU128(mpz_t z, u128 v) {
  uint64_t parts[2] = {(uint64_t)v, (uint64_t)(v >> 64)};
  mpz_import(z, 2, -1, sizeof(uint64_t), 0, 0, parts);
}

inline std::string u128ToString(u128 v) {
  if (v == 0) return "0";
  char buf[48];
  int i = 47;
  buf[i] = 0;
  while (v != 0) {
    buf[--i] = (char)('0' + (int)(v % 10));
    v /= 10;
  }
  return std::string(buf + i);
}

class ExactCount {
  u128 small_ = 0;
  bool big_ = false;
  mpz_t z_;

  void toBig() {
    if (big_) return;
    mpz_init(z_);
    mpzSetU128(z_, small_);
    small_ = 0;
    big_ = true;
  }

 public:
  ExactCount() {}
  explicit ExactCount(u128 v) : small_(v) {}
  ExactCount(const ExactCount& o) : small_(o.small_), big_(o.big_) {
    if (big_) mpz_init_set(z_, o.z_);
  }
  ExactCount& operator=(const ExactCount& o) {
    if (this != &o) {
      clear();
      small_ = o.small_;
      big_ = o.big_;
      if (big_) mpz_init_set(z_, o.z_);
    }
    return *this;
  }
  ~ExactCount() { clear(); }

  void clear() {
    if (big_) mpz_clear(z_);
    big_ = false;
    small_ = 0;
  }

  bool isZero() const { return !big_ && small_ == 0; }
  bool isBig() const { return big_; }

  void mul(u128 f) {
    if (!big_) {
      u128 r;
      if (!__builtin_mul_overflow(small_, f, &r)) {
        small_ = r;
        return;
      }
      toBig();
    }
    mpz_t t;
    mpz_init(t);
    mpzSetU128(t, f);
    mpz_mul(z_, z_, t);
    mpz_clear(t);
  }

  void add(u128 a) {
    if (!big_) {
      u128 r;
      if (!__builtin_add_overflow(small_, a, &r)) {
        small_ = r;
        return;
      }
      toBig();
    }
    mpz_t t;
    mpz_init(t);
    mpzSetU128(t, a);
    mpz_add(z_, z_, t);
    mpz_clear(t);
  }

  void add(const ExactCount& o) {
    if (!o.big_) {
      add(o.small_);
      return;
    }
    toBig();
    mpz_add(z_, z_, o.z_);
  }

  std::string toString() const {
    if (!big_) return u128ToString(small_);
    std::string r(mpz_sizeinbase(z_, 10) + 2, '\0');
    mpz_get_str(&r[0], 10, z_);
    r.resize(std::strlen(r.c_str()));
    return r;
  }
};

}  // namespace circinus
