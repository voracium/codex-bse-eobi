#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace eobi::basic {

#ifndef EOBI_BOOK_DEPTH
#define EOBI_BOOK_DEPTH 5
#endif

using SymbolIdType = std::int64_t;
using PxType = std::int64_t;
using OBSizeType = std::int64_t;
inline constexpr std::size_t kBookDepth = EOBI_BOOK_DEPTH;

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
    std::int64_t last_px{};
    std::int64_t last_qty{};
};

struct FullOrderExecution {
    std::uint32_t seq_no{};
    std::int64_t security_id{};
    Side side{};
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
        bids_.clear();
        asks_.clear();
    }

    void update_instrument_info(const InstrumentInfo& info) {
        upper_circuit_limit_ = info.upper_ckt_lmt;
        lower_circuit_limit_ = info.lower_ckt_lmt;
        has_circuit_limits_ = true;
    }

    void update_instrument_state(const InstrumentStateChange& state) {
        security_status_ = state.security_status;
        sec_trd_status_ = state.sec_trd_status;
    }

    bool is_continuous_trading() const noexcept { return sec_trd_status_ == SecTrdStatus::Continuous; }
    bool is_price_within_circuit(std::int64_t price) const noexcept {
        if (!has_circuit_limits_) {
            return true;
        }
        return price >= lower_circuit_limit_ && price <= upper_circuit_limit_;
    }

    bool refresh_mtick(bool refresh_bids, bool refresh_asks) {
        bool changed = false;
        if (refresh_bids) {
            changed = refresh_one_side_from_levels(bids_, true, mtick_.bid, mtick_.bid_size) || changed;
        }
        if (refresh_asks) {
            changed = refresh_one_side_from_levels(asks_, false, mtick_.ask, mtick_.ask_size) || changed;
        }
        return changed;
    }

    bool refresh_mtick_incremental(Side side, std::int64_t changed_price) {
        if (side == Side::Buy) {
            return refresh_one_side_incremental(bids_, true, changed_price, mtick_.bid, mtick_.bid_size);
        }
        return refresh_one_side_incremental(asks_, false, changed_price, mtick_.ask, mtick_.ask_size);
    }

    bool refresh_mtick_incremental(Side side, std::int64_t changed_price_a, std::int64_t changed_price_b) {
        bool changed = refresh_mtick_incremental(side, changed_price_a);
        if (changed_price_b != changed_price_a) {
            changed = refresh_mtick_incremental(side, changed_price_b) || changed;
        }
        return changed;
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
            fill_tentative_side_from_levels(bids_, true, msg.last_qty, tentative_mtick_.bid, tentative_mtick_.bid_size);
        } else {
            fill_tentative_side_from_levels(asks_, false, msg.last_qty, tentative_mtick_.ask, tentative_mtick_.ask_size);
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

    using LevelsMap = std::map<std::int64_t, std::int64_t>;

    static void fill_tentative_side_from_levels(const LevelsMap& levels, bool bids_side, std::int64_t exec_qty,
                                                std::array<PxType, kBookDepth>& px_out,
                                                std::array<OBSizeType, kBookDepth>& qty_out) {
        const auto sentinel = bids_side ? std::numeric_limits<PxType>::min() : std::numeric_limits<PxType>::max();
        px_out.fill(sentinel);
        qty_out.fill(0);

        std::int64_t remaining = exec_qty;
        std::size_t write = 0;
        if (bids_side) {
            for (auto it = levels.rbegin(); it != levels.rend() && write < kBookDepth; ++it) {
                std::int64_t level_qty = it->second;
                if (remaining > 0) {
                    const auto consume = remaining < level_qty ? remaining : level_qty;
                    level_qty -= consume;
                    remaining -= consume;
                }
                if (level_qty <= 0) {
                    continue;
                }
                px_out[write] = static_cast<PxType>(it->first);
                qty_out[write] = static_cast<OBSizeType>(level_qty);
                ++write;
            }
            return;
        }
        for (auto it = levels.begin(); it != levels.end() && write < kBookDepth; ++it) {
            std::int64_t level_qty = it->second;
            if (remaining > 0) {
                const auto consume = remaining < level_qty ? remaining : level_qty;
                level_qty -= consume;
                remaining -= consume;
            }
            if (level_qty <= 0) {
                continue;
            }
            px_out[write] = static_cast<PxType>(it->first);
            qty_out[write] = static_cast<OBSizeType>(level_qty);
            ++write;
        }
    }

    static void apply_delta(LevelsMap& levels, std::int64_t price, std::int64_t delta) {
        if (delta == 0) {
            return;
        }
        const auto it = levels.find(price);
        if (it == levels.end()) {
            if (delta > 0) {
                levels.emplace(price, delta);
            }
            return;
        }
        const auto next_qty = it->second + delta;
        if (next_qty <= 0) {
            levels.erase(it);
            return;
        }
        it->second = next_qty;
    }

    void apply_delta(Side side, std::int64_t price, std::int64_t delta) {
        if (!is_price_within_circuit(price)) {
            return;
        }
        if (side == Side::Buy) {
            apply_delta(bids_, price, delta);
            return;
        }
        apply_delta(asks_, price, delta);
    }

    template <typename ArrPx, typename ArrQty>
    static void fill_side_from_levels(const LevelsMap& levels, bool bids_side, ArrPx& px_out,
                                      ArrQty& qty_out) {
        const auto sentinel_px = bids_side ? std::numeric_limits<PxType>::min() : std::numeric_limits<PxType>::max();
        px_out.fill(sentinel_px);
        qty_out.fill(0);

        std::size_t i = 0;
        if (bids_side) {
            for (auto it = levels.rbegin(); it != levels.rend() && i < kBookDepth; ++it, ++i) {
                px_out[i] = static_cast<PxType>(it->first);
                qty_out[i] = static_cast<OBSizeType>(it->second);
            }
            return;
        }
        for (auto it = levels.begin(); it != levels.end() && i < kBookDepth; ++it, ++i) {
            px_out[i] = static_cast<PxType>(it->first);
            qty_out[i] = static_cast<OBSizeType>(it->second);
        }
    }

    static bool is_valid_price(PxType px, bool bids_side) {
        return bids_side ? px != std::numeric_limits<PxType>::min() : px != std::numeric_limits<PxType>::max();
    }

    static bool better_price(std::int64_t lhs, std::int64_t rhs, bool bids_side) {
        return bids_side ? lhs > rhs : lhs < rhs;
    }

    static int find_price_index(const std::array<PxType, kBookDepth>& px_out,
                                const std::array<OBSizeType, kBookDepth>& qty_out, std::int64_t price,
                                bool bids_side) {
        for (std::size_t i = 0; i < kBookDepth; ++i) {
            if (qty_out[i] <= 0 || !is_valid_price(px_out[i], bids_side)) {
                continue;
            }
            if (px_out[i] == price) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    static std::size_t find_insert_index(const std::array<PxType, kBookDepth>& px_out,
                                         const std::array<OBSizeType, kBookDepth>& qty_out,
                                         std::int64_t price, bool bids_side) {
        for (std::size_t i = 0; i < kBookDepth; ++i) {
            if (qty_out[i] <= 0 || !is_valid_price(px_out[i], bids_side)) {
                return i;
            }
            if (better_price(price, px_out[i], bids_side)) {
                return i;
            }
        }
        return kBookDepth;
    }

    static std::pair<PxType, OBSizeType> best_level(const LevelsMap& levels, bool bids_side) {
        if (levels.empty()) {
            return bids_side ? std::pair<PxType, OBSizeType>{std::numeric_limits<PxType>::min(), 0}
                             : std::pair<PxType, OBSizeType>{std::numeric_limits<PxType>::max(), 0};
        }
        if (bids_side) {
            const auto it = levels.rbegin();
            return {static_cast<PxType>(it->first), static_cast<OBSizeType>(it->second)};
        }
        const auto it = levels.begin();
        return {static_cast<PxType>(it->first), static_cast<OBSizeType>(it->second)};
    }

    static std::pair<PxType, OBSizeType> next_worse_than(const LevelsMap& levels, bool bids_side,
                                                          PxType anchor_price) {
        const auto sentinel =
            bids_side ? std::pair<PxType, OBSizeType>{std::numeric_limits<PxType>::min(), 0}
                      : std::pair<PxType, OBSizeType>{std::numeric_limits<PxType>::max(), 0};
        if (levels.empty()) {
            return sentinel;
        }

        auto it = levels.find(anchor_price);
        if (it == levels.end()) {
            it = levels.lower_bound(anchor_price);
            if (bids_side) {
                if (it == levels.begin()) {
                    return sentinel;
                }
                --it;
                return {static_cast<PxType>(it->first), static_cast<OBSizeType>(it->second)};
            }
            if (it == levels.end()) {
                return sentinel;
            }
            if (it->first == anchor_price) {
                ++it;
            }
            if (it == levels.end()) {
                return sentinel;
            }
            return {static_cast<PxType>(it->first), static_cast<OBSizeType>(it->second)};
        }

        if (bids_side) {
            if (it == levels.begin()) {
                return sentinel;
            }
            --it;
            return {static_cast<PxType>(it->first), static_cast<OBSizeType>(it->second)};
        }

        ++it;
        if (it == levels.end()) {
            return sentinel;
        }
        return {static_cast<PxType>(it->first), static_cast<OBSizeType>(it->second)};
    }

    static bool normalize_tail(std::array<PxType, kBookDepth>& px_out, std::array<OBSizeType, kBookDepth>& qty_out,
                               bool bids_side) {
        const auto sentinel = bids_side ? std::numeric_limits<PxType>::min() : std::numeric_limits<PxType>::max();
        bool changed = false;
        for (std::size_t i = 0; i < kBookDepth; ++i) {
            if (qty_out[i] <= 0) {
                if (px_out[i] != sentinel) {
                    px_out[i] = sentinel;
                    changed = true;
                }
                if (qty_out[i] != 0) {
                    qty_out[i] = 0;
                    changed = true;
                }
            }
        }
        return changed;
    }

    static bool refresh_one_side_incremental(const LevelsMap& levels, bool bids_side, std::int64_t changed_price,
                                             std::array<PxType, kBookDepth>& px_out,
                                             std::array<OBSizeType, kBookDepth>& qty_out) {
        bool changed = normalize_tail(px_out, qty_out, bids_side);
        const auto set_level = [&](std::size_t i, PxType px, OBSizeType qty) {
            bool local_changed = false;
            if (px_out[i] != px) {
                px_out[i] = px;
                local_changed = true;
            }
            if (qty_out[i] != qty) {
                qty_out[i] = qty;
                local_changed = true;
            }
            return local_changed;
        };

        const int old_idx = find_price_index(px_out, qty_out, changed_price, bids_side);
        const auto it = levels.find(changed_price);
        const OBSizeType new_qty = (it == levels.end()) ? 0 : static_cast<OBSizeType>(it->second);

        if (old_idx >= 0) {
            const std::size_t idx = static_cast<std::size_t>(old_idx);
            if (new_qty > 0) {
                if (qty_out[idx] != new_qty) {
                    qty_out[idx] = new_qty;
                    changed = true;
                }
            } else {
                for (std::size_t i = idx; i + 1 < kBookDepth; ++i) {
                    changed = set_level(i, px_out[i + 1], qty_out[i + 1]) || changed;
                }
                std::pair<PxType, OBSizeType> tail{bids_side ? std::numeric_limits<PxType>::min()
                                                              : std::numeric_limits<PxType>::max(),
                                                   0};
                if constexpr (kBookDepth == 1) {
                    tail = best_level(levels, bids_side);
                } else {
                    const auto anchor_qty = qty_out[kBookDepth - 2];
                    if (anchor_qty > 0) {
                        tail = next_worse_than(levels, bids_side, px_out[kBookDepth - 2]);
                    }
                }
                const auto [tail_px, tail_qty] = tail;
                changed = set_level(kBookDepth - 1, tail_px, tail_qty) || changed;
            }
            return changed;
        }

        if (new_qty <= 0) {
            return changed;
        }
        const std::size_t insert_idx = find_insert_index(px_out, qty_out, changed_price, bids_side);
        if (insert_idx >= kBookDepth) {
            return changed;
        }
        for (std::size_t i = kBookDepth - 1; i > insert_idx; --i) {
            changed = set_level(i, px_out[i - 1], qty_out[i - 1]) || changed;
        }
        changed = set_level(insert_idx, changed_price, new_qty) || changed;
        return changed;
    }

    static bool refresh_one_side_from_levels(const LevelsMap& levels, bool bids_side, std::array<PxType, kBookDepth>& px_out,
                                             std::array<OBSizeType, kBookDepth>& qty_out) {
        std::array<PxType, kBookDepth> next_px{};
        std::array<OBSizeType, kBookDepth> next_qty{};
        fill_side_from_levels(levels, bids_side, next_px, next_qty);
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

    LevelsMap bids_;
    LevelsMap asks_;
    MTICK mtick_{};
    MTICK tentative_mtick_{};
    bool has_tentative_mtick_{false};
    std::uint32_t seq_no_{0};
    std::uint32_t tentative_seq_no_{0};
    bool has_circuit_limits_{false};
    std::int64_t upper_circuit_limit_{0};
    std::int64_t lower_circuit_limit_{0};
    SecurityStatus security_status_{SecurityStatus::Unknown};
    SecTrdStatus sec_trd_status_{SecTrdStatus::Unknown};
};

class PriceLevelBooks {
   public:
    std::optional<MTICK> apply(const AddOrder& msg) {
        auto& book = ensure_book(msg.security_id);
        book.mark_seq(msg.seq_no);
        book.mark_processing_start();
        book.add(msg.side, msg.price, msg.display_qty);
        if (!book.refresh_mtick_incremental(msg.side, msg.price)) {
            return std::nullopt;
        }
        book.mark_publish_time();
        return book.mtick();
    }

    std::optional<MTICK> apply(const ModifyOrder& msg) {
        auto& book = ensure_book(msg.security_id);
        book.mark_seq(msg.seq_no);
        book.mark_processing_start();
        book.modify(msg.side, msg.prev_price, msg.prev_display_qty, msg.price, msg.display_qty);
        if (!book.refresh_mtick_incremental(msg.side, msg.prev_price, msg.price)) {
            return std::nullopt;
        }
        book.mark_publish_time();
        return book.mtick();
    }

    std::optional<MTICK> apply(const ModifyOrderSamePriority& msg) {
        auto& book = ensure_book(msg.security_id);
        book.mark_seq(msg.seq_no);
        book.mark_processing_start();
        book.modify_same_priority(msg.side, msg.price, msg.prev_display_qty, msg.display_qty);
        if (!book.refresh_mtick_incremental(msg.side, msg.price)) {
            return std::nullopt;
        }
        book.mark_publish_time();
        return book.mtick();
    }

    std::optional<MTICK> apply(const DeleteOrder& msg) {
        auto& book = ensure_book(msg.security_id);
        book.mark_seq(msg.seq_no);
        book.mark_processing_start();
        book.remove(msg.side, msg.price, msg.display_qty);
        if (!book.refresh_mtick_incremental(msg.side, msg.price)) {
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
        book.partial_exec(msg.side, msg.last_px, msg.last_qty);
        if (!book.refresh_mtick_incremental(msg.side, msg.last_px)) {
            return std::nullopt;
        }
        book.mark_publish_time();
        return book.mtick();
    }

    std::optional<MTICK> apply(const FullOrderExecution& msg) {
        auto& book = ensure_book(msg.security_id);
        book.mark_seq(msg.seq_no);
        book.mark_processing_start();
        book.full_exec(msg.side, msg.last_px, msg.last_qty);
        if (!book.refresh_mtick_incremental(msg.side, msg.last_px)) {
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

}  // namespace eobi::basic
