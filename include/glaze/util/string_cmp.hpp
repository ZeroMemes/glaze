// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "fmt/format.h"

namespace glz
{
   constexpr uint64_t to_uint64(const char* bytes, size_t n = 8) noexcept
   {
      // In cases where n < 8 we do overread in the runtime context for perf and special care should be taken:
      //  * Alignment or page boundary checks should probably be done before calling this function
      //  * Garbage bits should be handled by caller. Ignore them shift them its up to you.
      // https://stackoverflow.com/questions/37800739/is-it-safe-to-read-past-the-end-of-a-buffer-within-the-same-page-on-x86-and-x64
      uint64_t res{};
      if (std::is_constant_evaluated()) {
         assert(n <= 8);
         for (size_t i = 0; i < n; ++i) {
            res |= static_cast<uint64_t>(bytes[i]) << (8 * i);
         }
      }
      else {
         // Note: memcpy is way faster with compiletime known length
         std::memcpy(&res, bytes, 8);
      }
      return res;
   }

   // Note: This relies on undefined behavior but should generally be ok
   // https://stackoverflow.com/questions/37800739/is-it-safe-to-read-past-the-end-of-a-buffer-within-the-same-page-on-x86-and-x64
   // Gets better perf then memcmp on small strings like keys but worse perf on longer strings (>40 or so)
   constexpr bool string_cmp(auto &&s0, auto &&s1) noexcept
   {
      const auto n = s0.size();
      if (s1.size() != n) {
         return false;
      }

      if (n < 8) {
         // TODO add option to skip page checks when we know they are not needed like the compile time known keys or
         // stringviews in the middle of the buffer
         if (!std::is_constant_evaluated() && (((reinterpret_cast<std::uintptr_t>(s0.data()) & 4095) > 4088) ||
                                               ((reinterpret_cast<std::uintptr_t>(s1.data()) & 4095) > 4088)))
            [[unlikely]] {
            // Buffer over-read may cross page boundary
            // There are faster things we could do here but this branch is unlikely
            return std::memcmp(s0.data(), s1.data(), n);
         }
         else {
            const auto shift = 64 - 8 * n;
            return (to_uint64(s0.data()) << shift) == (to_uint64(s1.data()) << shift);
         }
      }

      const char *b0 = s0.data();
      const char *b1 = s1.data();
      const char *end7 = s0.data() + n - 7;

      for (; b0 < end7; b0 += 8, b1 += 8) {
         if (to_uint64(b0) != to_uint64(b1)) {
            return false;
         }
      }

      const uint64_t nm8 = n - 8;
      return (to_uint64(s0.data() + nm8) == to_uint64(s1.data() + nm8));
   }
}
