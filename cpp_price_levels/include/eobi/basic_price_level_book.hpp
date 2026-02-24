#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <utility>

#include "eobi/instrument_book.hpp"

namespace eobi::basic {

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

}  // namespace eobi::basic
