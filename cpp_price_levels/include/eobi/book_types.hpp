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

}  // namespace eobi::basic
