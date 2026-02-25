#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "eobi/book_types.hpp"
#include "eobi/level_bitmap.hpp"

namespace eobi::basic {

#if defined(__GNUC__) || defined(__clang__)
#define EOBI_UNLIKELY(x) (__builtin_expect(!!(x), 0))
#else
#define EOBI_UNLIKELY(x) (x)
#endif

class InstrumentBook {
   public:
    InstrumentBook() {
        mtick_.bid.fill(std::numeric_limits<PxType>::min());
        mtick_.ask.fill(std::numeric_limits<PxType>::max());
        mtick_.bid_size.fill(0);
        mtick_.ask_size.fill(0);
    }

    void set_security_id(SymbolIdType security_id) noexcept { mtick_.id = security_id; }

    void add(Side side, std::int64_t price, std::int64_t qty) { apply_delta(side, price, qty); }

    void modify(Side side, std::int64_t prev_price, std::int64_t prev_qty, std::int64_t new_price, std::int64_t new_qty) {
        apply_delta(side, prev_price, -prev_qty);
        apply_delta(side, new_price, new_qty);
    }

    void modify_same_priority(Side side, std::int64_t price, std::int64_t prev_qty, std::int64_t new_qty) {
        apply_delta(side, price, new_qty - prev_qty);
    }

    void remove(Side side, std::int64_t price, std::int64_t qty) { apply_delta(side, price, -qty); }

    void partial_exec(Side side, std::int64_t last_px, std::int64_t last_qty) { apply_delta(side, last_px, -last_qty); }

    void full_exec(Side side, std::int64_t last_px, std::int64_t last_qty) { apply_delta(side, last_px, -last_qty); }

    void clear() {
        std::fill(bid_levels_.begin(), bid_levels_.end(), 0);
        std::fill(ask_levels_.begin(), ask_levels_.end(), 0);
        std::fill(bid_nonzero_.begin(), bid_nonzero_.end(), 0);
        std::fill(ask_nonzero_.begin(), ask_nonzero_.end(), 0);
        buy_max_idx_ = -1;
        sell_min_idx_ = -1;
    }

    void update_instrument_info(const InstrumentInfo& info) {
        if (EOBI_UNLIKELY(info.upper_limit() < info.lower_limit() || info.lower_limit() < 0)) {
            return;
        }
        const auto new_lower = info.lower_limit() / kPriceMultiplier;
        const auto new_upper = info.upper_limit() / kPriceMultiplier;
        if (EOBI_UNLIKELY(new_upper < new_lower)) {
            return;
        }
        const auto span = static_cast<std::uint64_t>((new_upper - new_lower) / kTickSize + 1);
        LevelsArray new_bid;
        LevelsArray new_ask;
        NonZeroBits new_bid_nonzero;
        NonZeroBits new_ask_nonzero;
        new_bid.assign(static_cast<std::size_t>(span), 0);
        new_ask.assign(static_cast<std::size_t>(span), 0);
        new_bid_nonzero.assign(LevelBitmap::word_count_for_levels(static_cast<std::size_t>(span)), 0);
        new_ask_nonzero.assign(LevelBitmap::word_count_for_levels(static_cast<std::size_t>(span)), 0);

        if (has_circuit_limits_) {
            for (std::size_t new_idx = 0; new_idx < new_bid.size(); ++new_idx) {
                const auto px_norm = new_lower + static_cast<std::int64_t>(new_idx) * kTickSize;
                if (px_norm < lower_circuit_limit_ || px_norm > upper_circuit_limit_) {
                    continue;
                }
                const auto delta = px_norm - lower_circuit_limit_;
                if (delta % kTickSize != 0) {
                    continue;
                }
                const auto old_idx = static_cast<std::size_t>(delta / kTickSize);
                if (old_idx < bid_levels_.size()) {
                    new_bid[new_idx] = bid_levels_[old_idx];
                    new_ask[new_idx] = ask_levels_[old_idx];
                    if (new_bid[new_idx] > 0) {
                        LevelBitmap::set_bit(new_bid_nonzero, new_idx);
                    }
                    if (new_ask[new_idx] > 0) {
                        LevelBitmap::set_bit(new_ask_nonzero, new_idx);
                    }
                }
            }
        }
        bid_levels_.swap(new_bid);
        ask_levels_.swap(new_ask);
        bid_nonzero_.swap(new_bid_nonzero);
        ask_nonzero_.swap(new_ask_nonzero);
        upper_circuit_limit_ = new_upper;
        lower_circuit_limit_ = new_lower;
        has_circuit_limits_ = true;
        recompute_top_indices();
    }

    void update_instrument_state(const InstrumentStateChange& state) {
        security_status_ = state.security_status_value();
        sec_trd_status_ = state.sec_trd_status_value();
    }

    bool is_continuous_trading() const noexcept { return sec_trd_status_ == SecTrdStatus::Continuous; }
    bool is_auction_freeze() const noexcept {
        return sec_trd_status_ == SecTrdStatus::OpeningAuctionFreeze || sec_trd_status_ == SecTrdStatus::IntradayAuctionFreeze;
    }

    bool refresh_mtick(bool refresh_bids, bool refresh_asks) {
        bool changed = false;
        if (refresh_bids) {
            changed = refresh_one_side_from_array(bid_levels_, true, mtick_.bid, mtick_.bid_size) || changed;
        }
        if (refresh_asks) {
            changed = refresh_one_side_from_array(ask_levels_, false, mtick_.ask, mtick_.ask_size) || changed;
        }
        return changed;
    }

    bool refresh_mtick_incremental(Side side, std::int64_t changed_price) {
        (void)changed_price;
        if (side == Side::Buy) {
            return refresh_one_side_from_array(bid_levels_, true, mtick_.bid, mtick_.bid_size);
        }
        return refresh_one_side_from_array(ask_levels_, false, mtick_.ask, mtick_.ask_size);
    }

    bool refresh_mtick_incremental(Side side, std::int64_t changed_price_a, std::int64_t changed_price_b) {
        (void)changed_price_a;
        (void)changed_price_b;
        if (side == Side::Buy) {
            return refresh_one_side_from_array(bid_levels_, true, mtick_.bid, mtick_.bid_size);
        }
        return refresh_one_side_from_array(ask_levels_, false, mtick_.ask, mtick_.ask_size);
    }

    const MTICK& mtick() const noexcept { return mtick_; }

    void mark_processing_start() { mark_timestamp_at(2); }

    void mark_publish_time() { mark_timestamp_at(3); }

    const MTICK& published_view() const noexcept {
        return (has_tentative_mtick_ && tentative_seq_no_ == seq_no_) ? tentative_mtick_ : mtick_;
    }

    void mark_seq(std::uint32_t seq_no) noexcept {
        has_tentative_mtick_ = false;
        if (seq_no > 0) {
            seq_no_ = seq_no;
            mtick_.seqNo = seq_no_;
            return;
        }
        ++seq_no_;
        mtick_.seqNo = seq_no_;
    }

    const MTICK& apply_execution_summary_tentative(const ExecutionSummary& msg) {
        tentative_mtick_ = mtick_;
        mark_timestamp_at(tentative_mtick_, 2);

        const bool reduce_bids = msg.agg_side_value() == Side::Sell;
        if (reduce_bids) {
            fill_tentative_side_from_levels(
                bid_levels_, true, msg.last_qty_value(), msg.last_px_value(), tentative_mtick_.bid, tentative_mtick_.bid_size);
        } else {
            fill_tentative_side_from_levels(
                ask_levels_, false, msg.last_qty_value(), msg.last_px_value(), tentative_mtick_.ask, tentative_mtick_.ask_size);
        }
        mark_timestamp_at(tentative_mtick_, 3);
        has_tentative_mtick_ = true;
        tentative_seq_no_ = seq_no_;
        return tentative_mtick_;
    }

   private:
    void mark_timestamp_at(std::size_t idx) { mark_timestamp_at(mtick_, idx); }

    static void mark_timestamp_at(MTICK& tick, std::size_t idx) {
        const auto now = std::chrono::system_clock::now().time_since_epoch();
        const auto secs = std::chrono::duration_cast<std::chrono::seconds>(now);
        const auto nsecs = std::chrono::duration_cast<std::chrono::nanoseconds>(now - secs);
        tick.tsec[idx] = static_cast<int>(secs.count());
        tick.tnsec[idx] = static_cast<int>(nsecs.count());
    }

    using LevelsArray = std::vector<std::int64_t>;
    using NonZeroBits = detail::NonZeroBits;
    using LevelBitmap = detail::LevelBitmap;

    static std::int64_t norm_to_raw(std::int64_t px_norm) { return px_norm * kPriceMultiplier; }

    void anchor_execsummary_top_from_core(const LevelsArray& levels, bool bids_side, std::int64_t last_px,
                                          std::array<PxType, kBookDepth>& px_out,
                                          std::array<OBSizeType, kBookDepth>& qty_out) const {
        const auto sentinel = bids_side ? std::numeric_limits<PxType>::min() : std::numeric_limits<PxType>::max();
        std::array<PxType, kBookDepth> next_px{};
        std::array<OBSizeType, kBookDepth> next_qty{};
        next_px.fill(sentinel);
        next_qty.fill(0);

        std::size_t w = 0;
        bool anchored = false;
        std::size_t anchor_idx = 0;
        if (last_px > 0) {
            const auto last_px_norm = last_px / kPriceMultiplier;
            const auto delta = last_px_norm - lower_circuit_limit_;
            if (delta >= 0 && (delta % kTickSize) == 0) {
                const auto idx = static_cast<std::size_t>(delta / kTickSize);
                if (idx < levels.size() && levels[idx] > 0) {
                    next_px[w] = static_cast<PxType>(last_px);
                    next_qty[w] = static_cast<OBSizeType>(levels[idx]);
                    ++w;
                    anchored = true;
                    anchor_idx = idx;
                }
            }
        }

        if (!anchored) {
            return;
        }

        if (bids_side) {
            if (anchor_idx > 0) {
                auto idx = LevelBitmap::find_prev_set_bit(bid_nonzero_, static_cast<std::int64_t>(anchor_idx) - 1, levels.size());
                while (idx >= 0 && w < kBookDepth) {
                    const auto q = levels[static_cast<std::size_t>(idx)];
                    if (q > 0) {
                        next_px[w] = static_cast<PxType>(norm_to_raw(lower_circuit_limit_ + idx * kTickSize));
                        next_qty[w] = static_cast<OBSizeType>(q);
                        ++w;
                    }
                    idx = LevelBitmap::find_prev_set_bit(bid_nonzero_, idx - 1, levels.size());
                }
            }
        } else {
            auto idx = LevelBitmap::find_next_set_bit(ask_nonzero_, anchor_idx + 1, levels.size());
            while (idx >= 0 && w < kBookDepth) {
                const auto q = levels[static_cast<std::size_t>(idx)];
                if (q > 0) {
                    next_px[w] = static_cast<PxType>(
                        norm_to_raw(lower_circuit_limit_ + static_cast<std::int64_t>(idx) * kTickSize));
                    next_qty[w] = static_cast<OBSizeType>(q);
                    ++w;
                }
                idx = LevelBitmap::find_next_set_bit(ask_nonzero_, static_cast<std::size_t>(idx + 1), levels.size());
            }
        }

        px_out = next_px;
        qty_out = next_qty;
    }

    void fill_tentative_side_from_levels(const LevelsArray& levels, bool bids_side, std::int64_t exec_qty, std::int64_t last_px,
                                         std::array<PxType, kBookDepth>& px_out, std::array<OBSizeType, kBookDepth>& qty_out) const {
        const auto sentinel = bids_side ? std::numeric_limits<PxType>::min() : std::numeric_limits<PxType>::max();
        px_out.fill(sentinel);
        qty_out.fill(0);
        if (EOBI_UNLIKELY(!has_circuit_limits_ || levels.empty())) {
            return;
        }

        std::int64_t remaining = exec_qty;
        std::int64_t consumed_at_px = 0;
        std::size_t write = 0;
        if (bids_side) {
            if (EOBI_UNLIKELY(buy_max_idx_ < 0)) {
                return;
            }
            std::int64_t idx = LevelBitmap::find_prev_set_bit(bid_nonzero_, buy_max_idx_, levels.size());
            while (idx >= 0 && write < kBookDepth) {
                std::int64_t level_qty = levels[static_cast<std::size_t>(idx)];
                if (remaining > 0) {
                    const auto consume = remaining < level_qty ? remaining : level_qty;
                    level_qty -= consume;
                    remaining -= consume;
                    if (consume > 0) {
                        consumed_at_px = norm_to_raw(lower_circuit_limit_ + idx * kTickSize);
                    }
                }
                if (level_qty <= 0) {
                    idx = LevelBitmap::find_prev_set_bit(bid_nonzero_, idx - 1, levels.size());
                    continue;
                }
                px_out[write] = static_cast<PxType>(norm_to_raw(lower_circuit_limit_ + idx * kTickSize));
                qty_out[write] = static_cast<OBSizeType>(level_qty);
                ++write;
                idx = LevelBitmap::find_prev_set_bit(bid_nonzero_, idx - 1, levels.size());
            }
            if (consumed_at_px != 0 && consumed_at_px != last_px) {
                anchor_execsummary_top_from_core(levels, true, last_px, px_out, qty_out);
            }
            return;
        }
        if (EOBI_UNLIKELY(sell_min_idx_ < 0)) {
            return;
        }
        std::int64_t idx = LevelBitmap::find_next_set_bit(ask_nonzero_, static_cast<std::size_t>(sell_min_idx_), levels.size());
        while (idx >= 0 && write < kBookDepth) {
            std::int64_t level_qty = levels[idx];
            if (remaining > 0) {
                const auto consume = remaining < level_qty ? remaining : level_qty;
                level_qty -= consume;
                remaining -= consume;
                if (consume > 0) {
                    consumed_at_px = norm_to_raw(lower_circuit_limit_ + static_cast<std::int64_t>(idx) * kTickSize);
                }
            }
            if (level_qty <= 0) {
                idx = LevelBitmap::find_next_set_bit(ask_nonzero_, static_cast<std::size_t>(idx + 1), levels.size());
                continue;
            }
            px_out[write] = static_cast<PxType>(norm_to_raw(lower_circuit_limit_ + static_cast<std::int64_t>(idx) * kTickSize));
            qty_out[write] = static_cast<OBSizeType>(level_qty);
            ++write;
            idx = LevelBitmap::find_next_set_bit(ask_nonzero_, static_cast<std::size_t>(idx + 1), levels.size());
        }
        if (consumed_at_px != 0 && consumed_at_px != last_px) {
            anchor_execsummary_top_from_core(levels, false, last_px, px_out, qty_out);
        }
    }

    bool try_price_to_index(std::int64_t price, std::size_t& out_idx) const {
        if (EOBI_UNLIKELY(!has_circuit_limits_ || price <= 0)) {
            return false;
        }
        if (EOBI_UNLIKELY(price < lower_circuit_limit_ || price > upper_circuit_limit_)) {
            return false;
        }
        const auto delta = price - lower_circuit_limit_;
        if (EOBI_UNLIKELY(delta % kTickSize != 0)) {
            return false;
        }
        out_idx = static_cast<std::size_t>(delta / kTickSize);
        return out_idx < bid_levels_.size();
    }

    void apply_delta(LevelsArray& levels, std::size_t idx, std::int64_t delta, bool bids_side) {
        if (EOBI_UNLIKELY(delta == 0)) {
            return;
        }
        const auto prev_qty = levels[idx];
        const auto next_qty = levels[idx] + delta;
        if (EOBI_UNLIKELY(next_qty <= 0)) {
            levels[idx] = 0;
            if (bids_side) {
                LevelBitmap::clear_bit(bid_nonzero_, idx);
            } else {
                LevelBitmap::clear_bit(ask_nonzero_, idx);
            }
            if (bids_side) {
                if (buy_max_idx_ == static_cast<std::int64_t>(idx)) {
                    buy_max_idx_ = LevelBitmap::find_prev_set_bit(bid_nonzero_, buy_max_idx_ - 1, bid_levels_.size());
                }
            } else {
                if (sell_min_idx_ == static_cast<std::int64_t>(idx)) {
                    sell_min_idx_ =
                        LevelBitmap::find_next_set_bit(ask_nonzero_, static_cast<std::size_t>(sell_min_idx_ + 1), ask_levels_.size());
                }
            }
            return;
        }
        levels[idx] = next_qty;
        if (bids_side) {
            LevelBitmap::set_bit(bid_nonzero_, idx);
        } else {
            LevelBitmap::set_bit(ask_nonzero_, idx);
        }
        if (prev_qty <= 0) {
            if (bids_side) {
                if (buy_max_idx_ < 0 || static_cast<std::int64_t>(idx) > buy_max_idx_) {
                    buy_max_idx_ = static_cast<std::int64_t>(idx);
                }
            } else {
                if (sell_min_idx_ < 0 || static_cast<std::int64_t>(idx) < sell_min_idx_) {
                    sell_min_idx_ = static_cast<std::int64_t>(idx);
                }
            }
        }
    }

    void apply_delta(Side side, std::int64_t price, std::int64_t delta) {
        std::size_t idx = 0;
        if (EOBI_UNLIKELY(!try_price_to_index(price, idx))) {
            return;
        }
        if (side == Side::Buy) {
            apply_delta(bid_levels_, idx, delta, true);
            return;
        }
        apply_delta(ask_levels_, idx, delta, false);
    }

    bool refresh_one_side_from_array(const LevelsArray& levels, bool bids_side, std::array<PxType, kBookDepth>& px_out,
                                     std::array<OBSizeType, kBookDepth>& qty_out) const {
        std::array<PxType, kBookDepth> next_px{};
        std::array<OBSizeType, kBookDepth> next_qty{};
        const auto sentinel = bids_side ? std::numeric_limits<PxType>::min() : std::numeric_limits<PxType>::max();
        next_px.fill(sentinel);
        next_qty.fill(0);

        if (EOBI_UNLIKELY(!has_circuit_limits_ || levels.empty())) {
            bool changed = false;
            for (std::size_t i = 0; i < kBookDepth; ++i) {
                if (px_out[i] != next_px[i]) {
                    px_out[i] = next_px[i];
                    changed = true;
                }
                if (qty_out[i] != next_qty[i]) {
                    qty_out[i] = next_qty[i];
                    changed = true;
                }
            }
            return changed;
        }

        std::size_t w = 0;
        if (bids_side) {
            if (buy_max_idx_ >= 0) {
                std::int64_t idx = LevelBitmap::find_prev_set_bit(bid_nonzero_, buy_max_idx_, levels.size());
                while (idx >= 0 && w < kBookDepth) {
                    const auto q = levels[static_cast<std::size_t>(idx)];
                    next_px[w] = static_cast<PxType>(norm_to_raw(lower_circuit_limit_ + idx * kTickSize));
                    next_qty[w] = static_cast<OBSizeType>(q);
                    ++w;
                    idx = LevelBitmap::find_prev_set_bit(bid_nonzero_, idx - 1, levels.size());
                }
            }
        } else {
            if (sell_min_idx_ >= 0) {
                std::int64_t idx = LevelBitmap::find_next_set_bit(ask_nonzero_, static_cast<std::size_t>(sell_min_idx_), levels.size());
                while (idx >= 0 && w < kBookDepth) {
                    const auto q = levels[idx];
                    next_px[w] =
                        static_cast<PxType>(norm_to_raw(lower_circuit_limit_ + static_cast<std::int64_t>(idx) * kTickSize));
                    next_qty[w] = static_cast<OBSizeType>(q);
                    ++w;
                    idx = LevelBitmap::find_next_set_bit(ask_nonzero_, static_cast<std::size_t>(idx + 1), levels.size());
                }
            }
        }
        bool changed = false;
        for (std::size_t i = 0; i < kBookDepth; ++i) {
            if (px_out[i] != next_px[i]) {
                px_out[i] = next_px[i];
                changed = true;
            }
            if (qty_out[i] != next_qty[i]) {
                qty_out[i] = next_qty[i];
                changed = true;
            }
        }
        return changed;
    }

    void recompute_top_indices() {
        buy_max_idx_ =
            LevelBitmap::find_prev_set_bit(bid_nonzero_, static_cast<std::int64_t>(bid_levels_.size()) - 1, bid_levels_.size());
        sell_min_idx_ = LevelBitmap::find_next_set_bit(ask_nonzero_, 0, ask_levels_.size());
    }

    LevelsArray bid_levels_;
    LevelsArray ask_levels_;
    NonZeroBits bid_nonzero_;
    NonZeroBits ask_nonzero_;
    MTICK mtick_{};
    MTICK tentative_mtick_{};
    bool has_tentative_mtick_{false};
    std::uint32_t seq_no_{0};
    std::uint32_t tentative_seq_no_{0};
    bool has_circuit_limits_{false};
    std::int64_t upper_circuit_limit_{0};
    std::int64_t lower_circuit_limit_{0};
    std::int64_t buy_max_idx_{-1};
    std::int64_t sell_min_idx_{-1};
    SecurityStatus security_status_{SecurityStatus::Unknown};
    SecTrdStatus sec_trd_status_{SecTrdStatus::Unknown};
};

}  // namespace eobi::basic
