#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace eobi::basic::detail {

#if defined(__GNUC__) || defined(__clang__)
#define EOBI_BITMAP_UNLIKELY(x) (__builtin_expect(!!(x), 0))
#else
#define EOBI_BITMAP_UNLIKELY(x) (x)
#endif

// Storage used for presence/absence flags of non-zero quantity levels.
// Each bit represents one normalized price index:
// - bit = 1 => level has non-zero quantity
// - bit = 0 => level is empty
using NonZeroBits = std::vector<std::uint64_t>;

// This helper owns all bitset traversal logic used by the raw-array orderbook.
// Keeping it isolated makes it easier to:
// 1) tune traversal/mutation in one place,
// 2) reason about correctness at boundaries,
// 3) swap implementations without touching book logic.
struct LevelBitmap {
    // 64-bit words are used because they map directly to builtin bit-scan ops.
    static constexpr std::size_t kBitsPerWord = 64;
    static constexpr std::size_t kBitsShift = 6;  // log2(64)
    static constexpr std::size_t kBitsMask = kBitsPerWord - 1;

    // Number of uint64 words required to represent [0, levels_size).
    static inline std::size_t word_count_for_levels(std::size_t levels_size) {
        return (levels_size + kBitsMask) >> kBitsShift;
    }

    // Marks one level as non-zero.
    static inline void set_bit(NonZeroBits& bits, std::size_t idx) {
        bits[idx >> kBitsShift] |= (std::uint64_t{1} << (idx & kBitsMask));
    }

    // Marks one level as empty.
    static inline void clear_bit(NonZeroBits& bits, std::size_t idx) {
        bits[idx >> kBitsShift] &= ~(std::uint64_t{1} << (idx & kBitsMask));
    }

    // Finds first set bit at or after `start`.
    // Returns -1 if none exists in [start, levels_size).
    //
    // Flow:
    // 1) Jump to containing word for `start`.
    // 2) Mask out bits before `start` in that word.
    // 3) If word has any set bit, use ctz to get lowest set bit.
    // 4) Otherwise advance word-by-word until a non-zero word is found.
    static inline std::int64_t find_next_set_bit(const NonZeroBits& bits, std::size_t start, std::size_t levels_size) {
        if (EOBI_BITMAP_UNLIKELY(start >= levels_size || bits.empty())) {
            return -1;
        }

        std::size_t word = start >> kBitsShift;
        std::uint64_t w = bits[word] & (~std::uint64_t{0} << (start & kBitsMask));

        while (true) {
            if (w != 0) {
#if defined(__GNUC__) || defined(__clang__)
                const auto bit = static_cast<std::size_t>(__builtin_ctzll(w));
#else
                std::size_t bit = 0;
                while (((w >> bit) & 1ULL) == 0ULL) {
                    ++bit;
                }
#endif
                const auto idx = (word << kBitsShift) + bit;
                return idx < levels_size ? static_cast<std::int64_t>(idx) : -1;
            }

            ++word;
            if (EOBI_BITMAP_UNLIKELY(word >= bits.size())) {
                return -1;
            }
            w = bits[word];
        }
    }

    // Finds last set bit at or before `start`.
    // Returns -1 if none exists in [0, start].
    //
    // Flow:
    // 1) Clamp start into valid level range.
    // 2) Mask out bits after `start` in the first word.
    // 3) If word has any set bit, use clz to get highest set bit.
    // 4) Otherwise move backward word-by-word until a non-zero word is found.
    static inline std::int64_t find_prev_set_bit(const NonZeroBits& bits, std::int64_t start, std::size_t levels_size) {
        if (EOBI_BITMAP_UNLIKELY(start < 0 || levels_size == 0 || bits.empty())) {
            return -1;
        }

        std::size_t s = static_cast<std::size_t>(start);
        if (EOBI_BITMAP_UNLIKELY(s >= levels_size)) {
            s = levels_size - 1;
        }

        std::size_t word = s >> kBitsShift;
        const auto bit = s & kBitsMask;
        const std::uint64_t mask =
            (bit == (kBitsPerWord - 1)) ? ~std::uint64_t{0} : ((std::uint64_t{1} << (bit + 1)) - 1);
        std::uint64_t w = bits[word] & mask;

        while (true) {
            if (w != 0) {
#if defined(__GNUC__) || defined(__clang__)
                const auto msb = static_cast<std::size_t>(63 - __builtin_clzll(w));
#else
                std::size_t msb = kBitsPerWord - 1;
                while (((w >> msb) & 1ULL) == 0ULL) {
                    --msb;
                }
#endif
                const auto idx = (word << kBitsShift) + msb;
                return idx < levels_size ? static_cast<std::int64_t>(idx) : -1;
            }

            if (word == 0) {
                return -1;
            }
            --word;
            w = bits[word];
        }
    }
};

#undef EOBI_BITMAP_UNLIKELY

}  // namespace eobi::basic::detail
