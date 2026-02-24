#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <limits>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace eobi::basic {

#ifndef EOBI_BOOK_DEPTH
#define EOBI_BOOK_DEPTH 5
#endif
#if defined(__GNUC__) || defined(__clang__)
#define EOBI_LIKELY(x) (__builtin_expect(!!(x), 1))
#define EOBI_UNLIKELY(x) (__builtin_expect(!!(x), 0))
#else
#define EOBI_LIKELY(x) (x)
#define EOBI_UNLIKELY(x) (x)
#endif

using SymbolIdType = std::int64_t;
using PxType = std::int64_t;
using OBSizeType = std::int64_t;
inline constexpr std::size_t kBookDepth = EOBI_BOOK_DEPTH;
inline constexpr std::int64_t kPriceMultiplier = 1'000'000;
inline constexpr std::int64_t kTickSize = 5;

enum class Side : std::uint8_t {
    Buy = 1,
    Sell = 2,
};

enum class SecurityStatus : std::uint8_t {
    Unknown = 0,
    Active = 1,
    Suspended = 2,
    Inactive = 3,
};

enum class SecTrdStatus : std::uint8_t {
    Unknown = 0,
    Restricted = 1,
    Closed = 2,
    OpeningAuction = 3,
    Continuous = 4,
    OpeningAuctionFreeze = 5,
    IntradayAuctionFreeze = 6,
};

class MTICK {
   public:
    SymbolIdType id{};

    std::array<PxType, kBookDepth> bid{};
    std::array<OBSizeType, kBookDepth> bid_size{};

    std::array<PxType, kBookDepth> ask{};
    std::array<OBSizeType, kBookDepth> ask_size{};

    std::array<int, 5> tsec{};
    std::array<int, 5> tnsec{};
    std::uint64_t volume{0};

    std::uint32_t seqNo{0};
    char side{'N'};
    char type{'-'};

    friend bool operator==(const MTICK& lhs, const MTICK& rhs) noexcept {
        return lhs.id == rhs.id && lhs.bid == rhs.bid && lhs.bid_size == rhs.bid_size && lhs.ask == rhs.ask &&
               lhs.ask_size == rhs.ask_size && lhs.tsec == rhs.tsec && lhs.tnsec == rhs.tnsec &&
               lhs.volume == rhs.volume && lhs.seqNo == rhs.seqNo && lhs.side == rhs.side && lhs.type == rhs.type;
    }
};

struct AddOrder {
    std::uint32_t seq_no{};
    std::int64_t security_id{};
    Side side{};
    std::int64_t price{};
    std::int64_t display_qty{};
};

struct ModifyOrder {
    std::uint32_t seq_no{};
    std::int64_t security_id{};
    Side side{};
    std::int64_t prev_price{};
    std::int64_t prev_display_qty{};
    std::int64_t price{};
    std::int64_t display_qty{};
};

struct ModifyOrderSamePriority {
    std::uint32_t seq_no{};
    std::int64_t security_id{};
    Side side{};
    std::int64_t price{};
    std::int64_t prev_display_qty{};
    std::int64_t display_qty{};
};

struct DeleteOrder {
    std::uint32_t seq_no{};
    std::int64_t security_id{};
    Side side{};
    std::int64_t price{};
    std::int64_t display_qty{};
};

struct MassDelete {
    std::uint32_t seq_no{};
    std::int64_t security_id{};
};

struct PartialOrderExecution {
    std::uint32_t seq_no{};
    std::int64_t security_id{};
    Side side{};
    std::int64_t price{};
    std::int64_t last_px{};
    std::int64_t last_qty{};
};

struct FullOrderExecution {
    std::uint32_t seq_no{};
    std::int64_t security_id{};
    Side side{};
    std::int64_t price{};
    std::int64_t last_px{};
    std::int64_t last_qty{};
};

struct ExecutionSummary {
    std::uint32_t seq_no{};
    std::int64_t security_id{};
    Side agg_side{};
    std::int64_t last_px{};
    std::int64_t last_qty{};
};

struct InstrumentInfo {
    std::uint32_t seq_no{};
    std::int64_t security_id{};
    std::int64_t upper_ckt_lmt{};
    std::int64_t lower_ckt_lmt{};
};

struct InstrumentStateChange {
    std::uint32_t seq_no{};
    std::int64_t security_id{};
    SecurityStatus security_status{SecurityStatus::Unknown};
    SecTrdStatus sec_trd_status{SecTrdStatus::Unknown};
};

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

    void modify(Side side, std::int64_t prev_price, std::int64_t prev_qty, std::int64_t new_price,
                std::int64_t new_qty) {
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
        buy_max_idx_ = -1;
        sell_min_idx_ = -1;
    }

    void update_instrument_info(const InstrumentInfo& info) {
        if (EOBI_UNLIKELY(info.upper_ckt_lmt < info.lower_ckt_lmt || info.lower_ckt_lmt < 0)) {
            return;
        }
        const auto new_lower = info.lower_ckt_lmt / kPriceMultiplier;
        const auto new_upper = info.upper_ckt_lmt / kPriceMultiplier;
        if (EOBI_UNLIKELY(new_upper < new_lower)) {
            return;
        }
        const auto span = static_cast<std::uint64_t>((new_upper - new_lower) / kTickSize + 1);
        LevelsArray new_bid;
        LevelsArray new_ask;
        new_bid.assign(static_cast<std::size_t>(span), 0);
        new_ask.assign(static_cast<std::size_t>(span), 0);

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
                }
            }
        }
        bid_levels_.swap(new_bid);
        ask_levels_.swap(new_ask);
        upper_circuit_limit_ = new_upper;
        lower_circuit_limit_ = new_lower;
        has_circuit_limits_ = true;
        recompute_top_indices();
    }

    void update_instrument_state(const InstrumentStateChange& state) {
        security_status_ = state.security_status;
        sec_trd_status_ = state.sec_trd_status;
    }

    bool is_continuous_trading() const noexcept { return sec_trd_status_ == SecTrdStatus::Continuous; }
    bool is_auction_freeze() const noexcept {
        return sec_trd_status_ == SecTrdStatus::OpeningAuctionFreeze ||
               sec_trd_status_ == SecTrdStatus::IntradayAuctionFreeze;
    }
    bool is_price_within_circuit(std::int64_t price) const noexcept {
        if (!has_circuit_limits_ || price <= 0) {
            return true;
        }
        if (price < lower_circuit_limit_ || price > upper_circuit_limit_) {
            return false;
        }
        const auto delta = price - lower_circuit_limit_;
        return (delta % kTickSize) == 0;
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

    const MTICK& published_view() const noexcept { return (has_tentative_mtick_ && tentative_seq_no_ == seq_no_) ? tentative_mtick_ : mtick_; }

    void mark_seq(std::uint32_t seq_no) noexcept {
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

        const bool reduce_bids = msg.agg_side == Side::Sell;
        if (reduce_bids) {
            fill_tentative_side_from_levels(
                bid_levels_, true, msg.last_qty, msg.last_px, tentative_mtick_.bid, tentative_mtick_.bid_size);
        } else {
            fill_tentative_side_from_levels(
                ask_levels_, false, msg.last_qty, msg.last_px, tentative_mtick_.ask, tentative_mtick_.ask_size);
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

    static void enforce_top_at_last_px_or_worse(bool bids_side, std::int64_t last_px,
                                                std::array<PxType, kBookDepth>& px_out,
                                                std::array<OBSizeType, kBookDepth>& qty_out) {
        const auto sentinel = bids_side ? std::numeric_limits<PxType>::min() : std::numeric_limits<PxType>::max();
        auto is_better_than_last = [&](PxType px) {
            if (px == sentinel) {
                return false;
            }
            return bids_side ? (px > last_px) : (px < last_px);
        };
        while (qty_out[0] > 0 && is_better_than_last(px_out[0])) {
            for (std::size_t i = 0; i + 1 < kBookDepth; ++i) {
                px_out[i] = px_out[i + 1];
                qty_out[i] = qty_out[i + 1];
            }
            px_out[kBookDepth - 1] = sentinel;
            qty_out[kBookDepth - 1] = 0;
        }
    }

    static std::int64_t norm_to_raw(std::int64_t px_norm) { return px_norm * kPriceMultiplier; }

    void fill_tentative_side_from_levels(const LevelsArray& levels, bool bids_side, std::int64_t exec_qty,
                                         std::int64_t last_px, std::array<PxType, kBookDepth>& px_out,
                                         std::array<OBSizeType, kBookDepth>& qty_out) const {
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
            for (std::int64_t idx = buy_max_idx_; idx >= 0 && write < kBookDepth; --idx) {
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
                    continue;
                }
                px_out[write] = static_cast<PxType>(norm_to_raw(lower_circuit_limit_ + idx * kTickSize));
                qty_out[write] = static_cast<OBSizeType>(level_qty);
                ++write;
            }
            if (consumed_at_px != 0 && consumed_at_px != last_px) {
                enforce_top_at_last_px_or_worse(true, last_px, px_out, qty_out);
            }
            return;
        }
        if (EOBI_UNLIKELY(sell_min_idx_ < 0)) {
            return;
        }
        for (std::size_t idx = static_cast<std::size_t>(sell_min_idx_); idx < levels.size() && write < kBookDepth;
             ++idx) {
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
                continue;
            }
            px_out[write] =
                static_cast<PxType>(norm_to_raw(lower_circuit_limit_ + static_cast<std::int64_t>(idx) * kTickSize));
            qty_out[write] = static_cast<OBSizeType>(level_qty);
            ++write;
        }
        if (consumed_at_px != 0 && consumed_at_px != last_px) {
            enforce_top_at_last_px_or_worse(false, last_px, px_out, qty_out);
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
                if (buy_max_idx_ == static_cast<std::int64_t>(idx)) {
                    while (buy_max_idx_ >= 0 && bid_levels_[static_cast<std::size_t>(buy_max_idx_)] <= 0) {
                        --buy_max_idx_;
                    }
                }
            } else {
                if (sell_min_idx_ == static_cast<std::int64_t>(idx)) {
                    const auto n = static_cast<std::int64_t>(ask_levels_.size());
                    while (sell_min_idx_ < n && ask_levels_[static_cast<std::size_t>(sell_min_idx_)] <= 0) {
                        ++sell_min_idx_;
                    }
                    if (sell_min_idx_ >= n) {
                        sell_min_idx_ = -1;
                    }
                }
            }
            return;
        }
        levels[idx] = next_qty;
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
                for (std::int64_t idx = buy_max_idx_; idx >= 0 && w < kBookDepth; --idx) {
                    const auto q = levels[static_cast<std::size_t>(idx)];
                    if (q <= 0) {
                        continue;
                    }
                    next_px[w] = static_cast<PxType>(
                        norm_to_raw(lower_circuit_limit_ + idx * kTickSize));
                    next_qty[w] = static_cast<OBSizeType>(q);
                    ++w;
                }
            }
        } else {
            if (sell_min_idx_ >= 0) {
                for (std::size_t idx = static_cast<std::size_t>(sell_min_idx_); idx < levels.size() && w < kBookDepth;
                     ++idx) {
                    const auto q = levels[idx];
                    if (q <= 0) {
                        continue;
                    }
                    next_px[w] =
                        static_cast<PxType>(norm_to_raw(lower_circuit_limit_ + static_cast<std::int64_t>(idx) * kTickSize));
                    next_qty[w] = static_cast<OBSizeType>(q);
                    ++w;
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
        buy_max_idx_ = -1;
        for (std::size_t i = bid_levels_.size(); i > 0; --i) {
            if (bid_levels_[i - 1] > 0) {
                buy_max_idx_ = static_cast<std::int64_t>(i - 1);
                break;
            }
        }
        sell_min_idx_ = -1;
        for (std::size_t i = 0; i < ask_levels_.size(); ++i) {
            if (ask_levels_[i] > 0) {
                sell_min_idx_ = static_cast<std::int64_t>(i);
                break;
            }
        }
    }

    LevelsArray bid_levels_;
    LevelsArray ask_levels_;
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

class PriceLevelBooks {
   public:
    std::optional<MTICK> apply(const AddOrder& msg) {
        auto& book = ensure_book(msg.security_id);
        book.mark_seq(msg.seq_no);
        book.mark_processing_start();
        const auto px = normalize_price(msg.price);
        book.add(msg.side, px, msg.display_qty);
        if (!book.refresh_mtick_incremental(msg.side, px)) {
            return std::nullopt;
        }
        book.mark_publish_time();
        return book.mtick();
    }

    std::optional<MTICK> apply(const ModifyOrder& msg) {
        auto& book = ensure_book(msg.security_id);
        book.mark_seq(msg.seq_no);
        book.mark_processing_start();
        const auto prev_px = normalize_price(msg.prev_price);
        const auto px = normalize_price(msg.price);
        book.modify(msg.side, prev_px, msg.prev_display_qty, px, msg.display_qty);
        if (!book.refresh_mtick_incremental(msg.side, prev_px, px)) {
            return std::nullopt;
        }
        book.mark_publish_time();
        return book.mtick();
    }

    std::optional<MTICK> apply(const ModifyOrderSamePriority& msg) {
        auto& book = ensure_book(msg.security_id);
        book.mark_seq(msg.seq_no);
        book.mark_processing_start();
        const auto px = normalize_price(msg.price);
        book.modify_same_priority(msg.side, px, msg.prev_display_qty, msg.display_qty);
        if (!book.refresh_mtick_incremental(msg.side, px)) {
            return std::nullopt;
        }
        book.mark_publish_time();
        return book.mtick();
    }

    std::optional<MTICK> apply(const DeleteOrder& msg) {
        auto& book = ensure_book(msg.security_id);
        book.mark_seq(msg.seq_no);
        book.mark_processing_start();
        const auto px = normalize_price(msg.price);
        book.remove(msg.side, px, msg.display_qty);
        if (!book.refresh_mtick_incremental(msg.side, px)) {
            return std::nullopt;
        }
        book.mark_publish_time();
        return book.mtick();
    }

    std::optional<MTICK> apply(const MassDelete& msg) {
        return apply_with_change(msg.security_id, true, true, msg.seq_no, [](InstrumentBook& book) { book.clear(); });
    }

    std::optional<MTICK> apply(const PartialOrderExecution& msg) {
        auto& book = ensure_book(msg.security_id);
        book.mark_seq(msg.seq_no);
        book.mark_processing_start();
        const auto exec_px = (book.is_auction_freeze() && msg.price != 0) ? msg.price : msg.last_px;
        const auto px = normalize_price(exec_px);
        book.partial_exec(msg.side, px, msg.last_qty);
        if (!book.refresh_mtick_incremental(msg.side, px)) {
            return std::nullopt;
        }
        book.mark_publish_time();
        return book.mtick();
    }

    std::optional<MTICK> apply(const FullOrderExecution& msg) {
        auto& book = ensure_book(msg.security_id);
        book.mark_seq(msg.seq_no);
        book.mark_processing_start();
        const auto exec_px = (book.is_auction_freeze() && msg.price != 0) ? msg.price : msg.last_px;
        const auto px = normalize_price(exec_px);
        book.full_exec(msg.side, px, msg.last_qty);
        if (!book.refresh_mtick_incremental(msg.side, px)) {
            return std::nullopt;
        }
        book.mark_publish_time();
        return book.mtick();
    }

    std::optional<MTICK> apply(const ExecutionSummary& msg) {
        auto& book = ensure_book(msg.security_id);
        book.mark_seq(msg.seq_no);
        if (!book.is_continuous_trading()) {
            return std::nullopt;
        }
        const auto& tentative = book.apply_execution_summary_tentative(msg);
        return tentative;
    }

    std::optional<MTICK> apply(const InstrumentInfo& msg) {
        auto& book = ensure_book(msg.security_id);
        book.mark_seq(msg.seq_no);
        book.update_instrument_info(msg);
        return std::nullopt;
    }

    std::optional<MTICK> apply(const InstrumentStateChange& msg) {
        auto& book = ensure_book(msg.security_id);
        book.mark_seq(msg.seq_no);
        book.update_instrument_state(msg);
        return std::nullopt;
    }

    MTICK snapshot(std::int64_t security_id) const {
        const auto it = books_.find(security_id);
        if (it == books_.end()) {
            InstrumentBook empty;
            empty.set_security_id(security_id);
            empty.refresh_mtick(true, true);
            return empty.mtick();
        }
        return it->second.published_view();
    }

   private:
    static std::int64_t normalize_price(std::int64_t raw_price) noexcept { return raw_price / kPriceMultiplier; }

    InstrumentBook& ensure_book(std::int64_t security_id) {
        auto& book = books_[security_id];
        book.set_security_id(security_id);
        return book;
    }

    template <typename Mutator>
    std::optional<MTICK> apply_with_change(std::int64_t security_id, bool refresh_bids, bool refresh_asks,
                                           std::uint32_t seq_no, Mutator&& mutator) {
        auto& book = ensure_book(security_id);
        book.mark_seq(seq_no);
        book.mark_processing_start();
        mutator(book);
        if (!book.refresh_mtick(refresh_bids, refresh_asks)) {
            return std::nullopt;
        }
        book.mark_publish_time();
        return book.mtick();
    }

    std::unordered_map<std::int64_t, InstrumentBook> books_;
};

#undef EOBI_LIKELY
#undef EOBI_UNLIKELY

}  // namespace eobi::basic
