#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace eobi::basic {

#ifndef EOBI_BOOK_DEPTH
#define EOBI_BOOK_DEPTH 5
#endif

using SymbolIdType = std::int64_t;
using PxType = std::int64_t;
using OBSizeType = std::int64_t;
using UTCTimestamp = std::int64_t;
using QtyType = std::int64_t;
using PriceType = std::int64_t;

inline constexpr std::size_t kBookDepth = EOBI_BOOK_DEPTH;
inline constexpr std::int64_t kPriceMultiplier = 1'000'000;
inline constexpr std::int64_t kTickSize = 5;

struct MsgHdr {
    std::uint16_t body_len{};
    std::uint16_t template_id{};
    std::uint32_t msg_seq_num{};
};

enum class Side : std::uint8_t {
    Buy = 1,
    Sell = 2,
};

enum class FastMarIndi : std::uint8_t {
    No = 0,
    Yes = 1,
};

enum class TradeCondition : std::uint8_t {
    Unknown = 0,
    ImpliedTrade = 1,
};

enum class SecurityStatus : std::uint8_t {
    Unknown = 0,
    Active = 1,
    Inactive = 2,
    Expired = 4,
    Suspended = 9,
};

enum class SecTrdStatus : std::uint8_t {
    Unknown = 0,
    Closed = 200,
    Restricted = 201,
    Book = 202,
    Continuous = 203,
    OpeningAuction = 204,
    OpeningAuctionFreeze = 205,
    IntradayAuction = 206,
    IntradayAuctionFreeze = 207,
    CircuitBreakerAuction = 208,
    CircuitBreakerAuctionFreeze = 209,
    ClosingAuction = 210,
    ClosingAuctionFreeze = 211,
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

// Wire-compatible message structs. Legacy fields are kept for existing tests and CSV replay.
// Accessors read legacy fields first, then fall back to wire fields.

struct AddOrder {
    MsgHdr msg_hdr{};
    UTCTimestamp trd_reg_ts_time_in{};
    std::int64_t wire_security_id{};
    UTCTimestamp trd_reg_ts_time_priority{};
    QtyType wire_disp_qty{};
    Side wire_side{};
    PriceType wire_price{};

    std::uint32_t seq_no{};
    std::int64_t security_id{};
    Side side{};
    std::int64_t price{};
    std::int64_t display_qty{};

    std::uint32_t seq() const noexcept { return seq_no != 0 ? seq_no : msg_hdr.msg_seq_num; }
    std::int64_t security() const noexcept { return security_id != 0 ? security_id : wire_security_id; }
    Side side_value() const noexcept { return wire_side != Side{} ? wire_side : side; }
    std::int64_t price_value() const noexcept { return price != 0 ? price : wire_price; }
    std::int64_t qty_value() const noexcept { return display_qty != 0 ? display_qty : wire_disp_qty; }
};

struct ModifyOrder {
    MsgHdr msg_hdr{};
    UTCTimestamp trd_reg_ts_time_in{};
    UTCTimestamp trd_reg_ts_prev_time_priority{};
    PriceType wire_prev_price{};
    QtyType wire_prev_disp_qty{};
    std::int64_t wire_security_id{};
    UTCTimestamp trd_reg_ts_time_priority{};
    QtyType wire_disp_qty{};
    Side wire_side{};
    PriceType wire_price{};

    std::uint32_t seq_no{};
    std::int64_t security_id{};
    Side side{};
    std::int64_t prev_price{};
    std::int64_t prev_display_qty{};
    std::int64_t price{};
    std::int64_t display_qty{};

    std::uint32_t seq() const noexcept { return seq_no != 0 ? seq_no : msg_hdr.msg_seq_num; }
    std::int64_t security() const noexcept { return security_id != 0 ? security_id : wire_security_id; }
    Side side_value() const noexcept { return wire_side != Side{} ? wire_side : side; }
    std::int64_t prev_price_value() const noexcept { return prev_price != 0 ? prev_price : wire_prev_price; }
    std::int64_t prev_qty_value() const noexcept { return prev_display_qty != 0 ? prev_display_qty : wire_prev_disp_qty; }
    std::int64_t price_value() const noexcept { return price != 0 ? price : wire_price; }
    std::int64_t qty_value() const noexcept { return display_qty != 0 ? display_qty : wire_disp_qty; }
};

struct ModifyOrderSamePriority {
    MsgHdr msg_hdr{};
    UTCTimestamp trd_reg_ts_time_in{};
    UTCTimestamp transc_time{};
    QtyType wire_prev_disp_qty{};
    std::int64_t wire_security_id{};
    UTCTimestamp trd_reg_ts_time_priority{};
    QtyType wire_disp_qty{};
    Side wire_side{};
    PriceType wire_price{};

    std::uint32_t seq_no{};
    std::int64_t security_id{};
    Side side{};
    std::int64_t price{};
    std::int64_t prev_display_qty{};
    std::int64_t display_qty{};

    std::uint32_t seq() const noexcept { return seq_no != 0 ? seq_no : msg_hdr.msg_seq_num; }
    std::int64_t security() const noexcept { return security_id != 0 ? security_id : wire_security_id; }
    Side side_value() const noexcept { return wire_side != Side{} ? wire_side : side; }
    std::int64_t price_value() const noexcept { return price != 0 ? price : wire_price; }
    std::int64_t prev_qty_value() const noexcept { return prev_display_qty != 0 ? prev_display_qty : wire_prev_disp_qty; }
    std::int64_t qty_value() const noexcept { return display_qty != 0 ? display_qty : wire_disp_qty; }
};

struct DeleteOrder {
    MsgHdr msg_hdr{};
    UTCTimestamp trd_reg_ts_time_in{};
    UTCTimestamp transc_time{};
    std::int64_t wire_security_id{};
    UTCTimestamp trd_reg_ts_time_priority{};
    QtyType wire_disp_qty{};
    Side wire_side{};
    PriceType wire_price{};

    std::uint32_t seq_no{};
    std::int64_t security_id{};
    Side side{};
    std::int64_t price{};
    std::int64_t display_qty{};

    std::uint32_t seq() const noexcept { return seq_no != 0 ? seq_no : msg_hdr.msg_seq_num; }
    std::int64_t security() const noexcept { return security_id != 0 ? security_id : wire_security_id; }
    Side side_value() const noexcept { return wire_side != Side{} ? wire_side : side; }
    std::int64_t price_value() const noexcept { return price != 0 ? price : wire_price; }
    std::int64_t qty_value() const noexcept { return display_qty != 0 ? display_qty : wire_disp_qty; }
};

struct MassDelete {
    MsgHdr msg_hdr{};
    std::int64_t wire_security_id{};
    UTCTimestamp transc_time{};

    std::uint32_t seq_no{};
    std::int64_t security_id{};

    std::uint32_t seq() const noexcept { return seq_no != 0 ? seq_no : msg_hdr.msg_seq_num; }
    std::int64_t security() const noexcept { return security_id != 0 ? security_id : wire_security_id; }
};

struct PartialOrderExecution {
    MsgHdr msg_hdr{};
    Side wire_side{};
    PriceType wire_price{};
    UTCTimestamp trd_reg_ts_time_priority{};
    std::int64_t wire_security_id{};
    std::uint32_t trd_match_id{};
    QtyType wire_last_qty{};
    PriceType wire_last_px{};

    std::uint32_t seq_no{};
    std::int64_t security_id{};
    Side side{};
    std::int64_t price{};
    std::int64_t last_px{};
    std::int64_t last_qty{};

    std::uint32_t seq() const noexcept { return seq_no != 0 ? seq_no : msg_hdr.msg_seq_num; }
    std::int64_t security() const noexcept { return security_id != 0 ? security_id : wire_security_id; }
    Side side_value() const noexcept { return wire_side != Side{} ? wire_side : side; }
    std::int64_t price_value() const noexcept { return price != 0 ? price : wire_price; }
    std::int64_t last_px_value() const noexcept { return last_px != 0 ? last_px : wire_last_px; }
    std::int64_t last_qty_value() const noexcept { return last_qty != 0 ? last_qty : wire_last_qty; }
};

struct FullOrderExecution {
    MsgHdr msg_hdr{};
    Side wire_side{};
    PriceType wire_price{};
    UTCTimestamp trd_reg_ts_time_priority{};
    std::int64_t wire_security_id{};
    std::uint32_t trd_match_id{};
    QtyType wire_last_qty{};
    PriceType wire_last_px{};

    std::uint32_t seq_no{};
    std::int64_t security_id{};
    Side side{};
    std::int64_t price{};
    std::int64_t last_px{};
    std::int64_t last_qty{};

    std::uint32_t seq() const noexcept { return seq_no != 0 ? seq_no : msg_hdr.msg_seq_num; }
    std::int64_t security() const noexcept { return security_id != 0 ? security_id : wire_security_id; }
    Side side_value() const noexcept { return wire_side != Side{} ? wire_side : side; }
    std::int64_t price_value() const noexcept { return price != 0 ? price : wire_price; }
    std::int64_t last_px_value() const noexcept { return last_px != 0 ? last_px : wire_last_px; }
    std::int64_t last_qty_value() const noexcept { return last_qty != 0 ? last_qty : wire_last_qty; }
};

struct ExecutionSummary {
    MsgHdr msg_hdr{};
    std::int64_t wire_security_id{};
    UTCTimestamp agg_timestamp{};
    UTCTimestamp exec_id{};
    QtyType wire_last_qty{};
    Side wire_agg_side{};
    TradeCondition trade_condition{};
    PriceType wire_last_px{};
    QtyType hidden_qty{};

    std::uint32_t seq_no{};
    std::int64_t security_id{};
    Side agg_side{};
    std::int64_t last_px{};
    std::int64_t last_qty{};

    std::uint32_t seq() const noexcept { return seq_no != 0 ? seq_no : msg_hdr.msg_seq_num; }
    std::int64_t security() const noexcept { return security_id != 0 ? security_id : wire_security_id; }
    Side agg_side_value() const noexcept { return wire_agg_side != Side{} ? wire_agg_side : agg_side; }
    std::int64_t last_px_value() const noexcept { return last_px != 0 ? last_px : wire_last_px; }
    std::int64_t last_qty_value() const noexcept { return last_qty != 0 ? last_qty : wire_last_qty; }
};

struct InstrumentInfo {
    MsgHdr msg_hdr{};
    std::int64_t wire_security_id{};
    PriceType close_px{};
    PriceType prev_close_px{};
    PriceType wire_upper_ckt_limit{};
    PriceType wire_lower_ckt_limit{};

    std::uint32_t seq_no{};
    std::int64_t security_id{};
    std::int64_t upper_ckt_lmt{};
    std::int64_t lower_ckt_lmt{};

    std::uint32_t seq() const noexcept { return seq_no != 0 ? seq_no : msg_hdr.msg_seq_num; }
    std::int64_t security() const noexcept { return security_id != 0 ? security_id : wire_security_id; }
    std::int64_t upper_limit() const noexcept { return upper_ckt_lmt != 0 ? upper_ckt_lmt : wire_upper_ckt_limit; }
    std::int64_t lower_limit() const noexcept { return lower_ckt_lmt != 0 ? lower_ckt_lmt : wire_lower_ckt_limit; }
};

struct InstrumentStateChange {
    MsgHdr msg_hdr{};
    std::int64_t wire_security_id{};
    SecurityStatus wire_security_status{SecurityStatus::Unknown};
    SecTrdStatus wire_sec_trd_status{SecTrdStatus::Unknown};
    FastMarIndi fmi{FastMarIndi::No};
    UTCTimestamp transc_time{};

    std::uint32_t seq_no{};
    std::int64_t security_id{};
    SecurityStatus security_status{SecurityStatus::Unknown};
    SecTrdStatus sec_trd_status{SecTrdStatus::Unknown};

    std::uint32_t seq() const noexcept { return seq_no != 0 ? seq_no : msg_hdr.msg_seq_num; }
    std::int64_t security() const noexcept { return security_id != 0 ? security_id : wire_security_id; }
    SecurityStatus security_status_value() const noexcept {
        return wire_security_status != SecurityStatus::Unknown ? wire_security_status : security_status;
    }
    SecTrdStatus sec_trd_status_value() const noexcept {
        return wire_sec_trd_status != SecTrdStatus::Unknown ? wire_sec_trd_status : sec_trd_status;
    }
};

}  // namespace eobi::basic
