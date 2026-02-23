#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <set>
#include <map>

#include "eobi/basic_price_level_book.hpp"

namespace {

using eobi::basic::AddOrder;
using eobi::basic::DeleteOrder;
using eobi::basic::ExecutionSummary;
using eobi::basic::FullOrderExecution;
using eobi::basic::InstrumentInfo;
using eobi::basic::InstrumentStateChange;
using eobi::basic::MassDelete;
using eobi::basic::ModifyOrder;
using eobi::basic::ModifyOrderSamePriority;
using eobi::basic::MTICK;
using eobi::basic::PartialOrderExecution;
using eobi::basic::PriceLevelBooks;
using eobi::basic::SecTrdStatus;
using eobi::basic::SecurityStatus;
using eobi::basic::Side;

constexpr std::size_t kDepth = eobi::basic::kBookDepth;
constexpr std::int64_t kPriceScale = 1'000'000;

struct Row {
    std::int64_t seq{};
    char type{};
    std::array<std::int64_t, kDepth> bid_size{};
    std::array<std::int64_t, kDepth> bid_px{};
    std::array<std::int64_t, kDepth> ask_px{};
    std::array<std::int64_t, kDepth> ask_size{};
};

std::vector<std::string_view> split_view(std::string_view s, char delim) {
    std::vector<std::string_view> out;
    std::size_t start = 0;
    while (start <= s.size()) {
        const auto pos = s.find(delim, start);
        if (pos == std::string_view::npos) {
            out.push_back(s.substr(start));
            break;
        }
        out.push_back(s.substr(start, pos - start));
        start = pos + 1;
    }
    return out;
}

std::int64_t to_i64(std::string_view sv) {
    std::int64_t v = 0;
    std::from_chars(sv.data(), sv.data() + sv.size(), v);
    return v;
}

Side parse_side(const std::string& raw) {
    if (raw == "BUY" || raw == "B") {
        return Side::Buy;
    }
    return Side::Sell;
}

SecurityStatus parse_security_status(const std::string& raw) {
    if (raw == "ACTIVE") {
        return SecurityStatus::Active;
    }
    if (raw == "SUSPENDED") {
        return SecurityStatus::Suspended;
    }
    if (raw == "INACTIVE") {
        return SecurityStatus::Inactive;
    }
    return SecurityStatus::Unknown;
}

SecTrdStatus parse_sec_trd_status(const std::string& raw) {
    if (raw == "RESTRICTED") {
        return SecTrdStatus::Restricted;
    }
    if (raw == "CLOSED") {
        return SecTrdStatus::Closed;
    }
    if (raw == "OPENING_AUCTION") {
        return SecTrdStatus::OpeningAuction;
    }
    if (raw == "CONTINUOUS") {
        return SecTrdStatus::Continuous;
    }
    if (raw == "OPENING_AUCTION_FREEZE") {
        return SecTrdStatus::OpeningAuctionFreeze;
    }
    if (raw == "INTRADAY_AUCTION_FREEZE") {
        return SecTrdStatus::IntradayAuctionFreeze;
    }
    return SecTrdStatus::Unknown;
}

Row to_row(std::int64_t seq, char type, const MTICK& top) {
    Row row{};
    row.seq = seq;
    row.type = type;
    row.bid_px.fill(-2147483648LL);
    row.ask_px.fill(2147483647LL);
    row.bid_size.fill(0);
    row.ask_size.fill(0);
    for (std::size_t i = 0; i < kDepth; ++i) {
        row.bid_size[i] = top.bid_size[i];
        row.ask_size[i] = top.ask_size[i];
        if (top.bid_size[i] > 0) {
            row.bid_px[i] = top.bid[i] / kPriceScale;
        }
        if (top.ask_size[i] > 0) {
            row.ask_px[i] = top.ask[i] / kPriceScale;
        }
    }
    return row;
}

bool same_book(const Row& a, const Row& b) {
    return a.bid_size == b.bid_size && a.bid_px == b.bid_px && a.ask_px == b.ask_px && a.ask_size == b.ask_size;
}

void write_csv(const std::string& path, std::int64_t security_id, const std::vector<Row>& rows) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("failed to open output csv: " + path);
    }
    out << "MachineTS,ExchTstamp,SeqNum,SecurityId,Type,Side,"
        << "bid_size[0],bid[0],ask[0],ask_size[0],"
        << "bid_size[1],bid[1],ask[1],ask_size[1],"
        << "bid_size[2],bid[2],ask[2],ask_size[2],"
        << "bid_size[3],bid[3],ask[3],ask_size[3],"
        << "bid_size[4],bid[4],ask[4],ask_size[4],"
        << "price,qty,old_price,old_qty,likely_bid_size,likely_bid,likely_ask,likely_ask_size,"
        << "btc,atc,btv,atv,dprLow,dprHigh,ExchId1,ExchId2\n";
    for (const auto& r : rows) {
        out << "0,0," << r.seq << "," << security_id << "," << r.type << ",N";
        for (std::size_t i = 0; i < kDepth; ++i) {
            out << "," << r.bid_size[i] << "," << r.bid_px[i] << "," << r.ask_px[i] << "," << r.ask_size[i];
        }
        out << ",0,0,0,0,-1,-1,-1,-1,0,0,0,0,0,0,0,0\n";
    }
}

std::vector<Row> load_reference(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("failed to open reference csv: " + path);
    }
    std::string header;
    std::getline(in, header);

    std::vector<Row> rows;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        const auto cols = split_view(line, ',');
        if (cols.size() < 26) {
            throw std::runtime_error("bad csv row: " + line);
        }
        Row row{};
        row.seq = to_i64(cols[2]);
        row.type = cols[4].empty() ? '-' : cols[4].front();
        for (std::size_t i = 0; i < kDepth; ++i) {
            const auto base = 6 + (i * 4);
            row.bid_size[i] = to_i64(cols[base + 0]);
            row.bid_px[i] = to_i64(cols[base + 1]);
            row.ask_px[i] = to_i64(cols[base + 2]);
            row.ask_size[i] = to_i64(cols[base + 3]);
        }
        rows.push_back(row);
    }
    return rows;
}

std::vector<Row> replay_raw(const std::string& path, std::int64_t security_id) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("failed to open raw log: " + path);
    }

    PriceLevelBooks books;
    std::vector<Row> rows;
    rows.push_back(to_row(0, '-', books.snapshot(security_id)));

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        const auto parts = split_view(line, '|');
        std::unordered_map<std::string, std::string> kv;
        kv.reserve(parts.size());
        for (const auto part : parts) {
            const auto p = part.find('=');
            if (p == std::string_view::npos) {
                continue;
            }
            kv.emplace(std::string(part.substr(0, p)), std::string(part.substr(p + 1)));
        }

        const auto tid_it = kv.find("TemplateId");
        const auto sid_it = kv.find("SecurityId");
        const auto seq_it = kv.find("MsgSeqNum");
        if (tid_it == kv.end() || sid_it == kv.end() || seq_it == kv.end()) {
            continue;
        }
        if (to_i64(sid_it->second) != security_id) {
            continue;
        }

        const std::int64_t seq = to_i64(seq_it->second);
        const std::string& tid = tid_it->second;
        char type = '\0';
        std::optional<MTICK> top;

        if (tid == "13100") {
            type = 'N';
            top = books.apply(AddOrder{
                .seq_no = static_cast<std::uint32_t>(seq),
                .security_id = security_id,
                .side = parse_side(kv.at("Side")),
                .price = to_i64(kv.at("Price")),
                .display_qty = to_i64(kv.at("Qty")),
            });
        } else if (tid == "13101") {
            type = 'M';
            top = books.apply(ModifyOrder{
                .seq_no = static_cast<std::uint32_t>(seq),
                .security_id = security_id,
                .side = parse_side(kv.at("Side")),
                .prev_price = to_i64(kv.at("PrevPrice")),
                .prev_display_qty = to_i64(kv.at("prevDispQty")),
                .price = to_i64(kv.at("Price")),
                .display_qty = to_i64(kv.at("Qty")),
            });
        } else if (tid == "13106") {
            type = 'M';
            top = books.apply(ModifyOrderSamePriority{
                .seq_no = static_cast<std::uint32_t>(seq),
                .security_id = security_id,
                .side = parse_side(kv.at("Side")),
                .price = to_i64(kv.at("Price")),
                .prev_display_qty = to_i64(kv.at("prevDispQty")),
                .display_qty = to_i64(kv.at("Qty")),
            });
        } else if (tid == "13102") {
            type = 'X';
            top = books.apply(DeleteOrder{
                .seq_no = static_cast<std::uint32_t>(seq),
                .security_id = security_id,
                .side = parse_side(kv.at("Side")),
                .price = to_i64(kv.at("Price")),
                .display_qty = to_i64(kv.at("Qty")),
            });
        } else if (tid == "13103") {
            type = 'X';
            top = books.apply(MassDelete{
                .seq_no = static_cast<std::uint32_t>(seq),
                .security_id = security_id,
            });
        } else if (tid == "13104") {
            type = 'F';
            const auto price_it = kv.find("Price");
            top = books.apply(FullOrderExecution{
                .seq_no = static_cast<std::uint32_t>(seq),
                .security_id = security_id,
                .side = parse_side(kv.at("Side")),
                .price = (price_it == kv.end()) ? to_i64(kv.at("LastPrice")) : to_i64(price_it->second),
                .last_px = to_i64(kv.at("LastPrice")),
                .last_qty = to_i64(kv.at("LastQty")),
            });
        } else if (tid == "13105") {
            type = 'T';
            const auto price_it = kv.find("Price");
            top = books.apply(PartialOrderExecution{
                .seq_no = static_cast<std::uint32_t>(seq),
                .security_id = security_id,
                .side = parse_side(kv.at("Side")),
                .price = (price_it == kv.end()) ? to_i64(kv.at("LastPrice")) : to_i64(price_it->second),
                .last_px = to_i64(kv.at("LastPrice")),
                .last_qty = to_i64(kv.at("LastQty")),
            });
        } else if (tid == "13202") {
            type = 'E';
            top = books.apply(ExecutionSummary{
                .seq_no = static_cast<std::uint32_t>(seq),
                .security_id = security_id,
                .agg_side = parse_side(kv.at("AggSide")),
                .last_px = to_i64(kv.at("LastPx")),
                .last_qty = to_i64(kv.at("LastQty")),
            });
        } else if (tid == "13203") {
            books.apply(InstrumentInfo{
                .seq_no = static_cast<std::uint32_t>(seq),
                .security_id = security_id,
                .upper_ckt_lmt = to_i64(kv.at("UpperCktLmt")),
                .lower_ckt_lmt = to_i64(kv.at("LowerCktLmt")),
            });
        } else if (tid == "13301") {
            books.apply(InstrumentStateChange{
                .seq_no = static_cast<std::uint32_t>(seq),
                .security_id = security_id,
                .security_status = parse_security_status(kv.at("SecurityStat")),
                .sec_trd_status = parse_sec_trd_status(kv.at("SecTrdStat")),
            });
        }

        if (type != '\0' && top.has_value()) {
            rows.push_back(to_row(seq, type, *top));
        }
    }

    return rows;
}

void print_latency_stats(const char* label, const std::vector<std::int64_t>& samples) {
    if (samples.empty()) {
        std::cout << label << ": no samples\n";
        return;
    }
    auto lats = samples;
    std::sort(lats.begin(), lats.end());
    std::int64_t total = 0;
    for (const auto v : lats) {
        total += v;
    }
    const auto idx = [&](double p) -> std::size_t {
        return static_cast<std::size_t>(p * static_cast<double>(lats.size() - 1));
    };
    std::cout << label << ": "
              << "count=" << lats.size() << " avg=" << (total / static_cast<std::int64_t>(lats.size()))
              << " min=" << lats.front() << " p50=" << lats[idx(0.50)] << " p90=" << lats[idx(0.90)]
              << " p99=" << lats[idx(0.99)] << " max=" << lats.back() << "\n";
}

void print_row(const Row& r, const char* label) {
    std::cout << label << " seq=" << r.seq << " type=" << r.type;
    for (std::size_t i = 0; i < kDepth; ++i) {
        std::cout << " | b" << i << "=" << r.bid_px[i] << "@" << r.bid_size[i];
        std::cout << " a" << i << "=" << r.ask_px[i] << "@" << r.ask_size[i];
    }
    std::cout << "\n";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "usage: replay_validate <raw_log> <reference_csv> <security_id> [mode] [out_csv]\n";
        std::cerr << "mode: strict (default) | seq_only\n";
        return 2;
    }

    const std::string raw = argv[1];
    const std::string ref = argv[2];
    const std::int64_t security_id = std::atoll(argv[3]);
    const std::string mode = (argc > 4) ? argv[4] : "strict";
    const std::string out_csv = (argc > 5) ? argv[5] : "";

    const auto expected = load_reference(ref);
    const auto actual = replay_raw(raw, security_id);

    // Second pass to collect latency from MTICK timestamps for published ticks.
    std::ifstream in_lat(raw);
    PriceLevelBooks books_lat;
    std::vector<std::int64_t> lat_all;
    std::map<char, std::vector<std::int64_t>> lat_by_type;
    std::string line_lat;
    while (std::getline(in_lat, line_lat)) {
        if (line_lat.empty()) {
            continue;
        }
        const auto parts = split_view(line_lat, '|');
        std::unordered_map<std::string, std::string> kv;
        kv.reserve(parts.size());
        for (const auto part : parts) {
            const auto p = part.find('=');
            if (p == std::string_view::npos) {
                continue;
            }
            kv.emplace(std::string(part.substr(0, p)), std::string(part.substr(p + 1)));
        }
        const auto tid_it = kv.find("TemplateId");
        const auto sid_it = kv.find("SecurityId");
        const auto seq_it = kv.find("MsgSeqNum");
        if (tid_it == kv.end() || sid_it == kv.end() || seq_it == kv.end()) {
            continue;
        }
        if (to_i64(sid_it->second) != security_id) {
            continue;
        }
        const std::int64_t seq = to_i64(seq_it->second);
        const std::string& tid = tid_it->second;
        char type = '\0';
        std::optional<MTICK> top;
        if (tid == "13100") {
            type = 'N';
            top = books_lat.apply(AddOrder{.seq_no = static_cast<std::uint32_t>(seq),
                                           .security_id = security_id,
                                           .side = parse_side(kv.at("Side")),
                                           .price = to_i64(kv.at("Price")),
                                           .display_qty = to_i64(kv.at("Qty"))});
        } else if (tid == "13101") {
            type = 'M';
            top = books_lat.apply(ModifyOrder{.seq_no = static_cast<std::uint32_t>(seq),
                                              .security_id = security_id,
                                              .side = parse_side(kv.at("Side")),
                                              .prev_price = to_i64(kv.at("PrevPrice")),
                                              .prev_display_qty = to_i64(kv.at("prevDispQty")),
                                              .price = to_i64(kv.at("Price")),
                                              .display_qty = to_i64(kv.at("Qty"))});
        } else if (tid == "13106") {
            type = 'M';
            top = books_lat.apply(ModifyOrderSamePriority{.seq_no = static_cast<std::uint32_t>(seq),
                                                          .security_id = security_id,
                                                          .side = parse_side(kv.at("Side")),
                                                          .price = to_i64(kv.at("Price")),
                                                          .prev_display_qty = to_i64(kv.at("prevDispQty")),
                                                          .display_qty = to_i64(kv.at("Qty"))});
        } else if (tid == "13102") {
            type = 'X';
            top = books_lat.apply(DeleteOrder{.seq_no = static_cast<std::uint32_t>(seq),
                                              .security_id = security_id,
                                              .side = parse_side(kv.at("Side")),
                                              .price = to_i64(kv.at("Price")),
                                              .display_qty = to_i64(kv.at("Qty"))});
        } else if (tid == "13103") {
            type = 'X';
            top = books_lat.apply(MassDelete{.seq_no = static_cast<std::uint32_t>(seq), .security_id = security_id});
        } else if (tid == "13104") {
            type = 'F';
            const auto price_it = kv.find("Price");
            top = books_lat.apply(FullOrderExecution{.seq_no = static_cast<std::uint32_t>(seq),
                                                     .security_id = security_id,
                                                     .side = parse_side(kv.at("Side")),
                                                     .price = (price_it == kv.end()) ? to_i64(kv.at("LastPrice"))
                                                                                     : to_i64(price_it->second),
                                                     .last_px = to_i64(kv.at("LastPrice")),
                                                     .last_qty = to_i64(kv.at("LastQty"))});
        } else if (tid == "13105") {
            type = 'T';
            const auto price_it = kv.find("Price");
            top = books_lat.apply(PartialOrderExecution{.seq_no = static_cast<std::uint32_t>(seq),
                                                        .security_id = security_id,
                                                        .side = parse_side(kv.at("Side")),
                                                        .price = (price_it == kv.end()) ? to_i64(kv.at("LastPrice"))
                                                                                        : to_i64(price_it->second),
                                                        .last_px = to_i64(kv.at("LastPrice")),
                                                        .last_qty = to_i64(kv.at("LastQty"))});
        } else if (tid == "13202") {
            type = 'E';
            top = books_lat.apply(ExecutionSummary{.seq_no = static_cast<std::uint32_t>(seq),
                                                   .security_id = security_id,
                                                   .agg_side = parse_side(kv.at("AggSide")),
                                                   .last_px = to_i64(kv.at("LastPx")),
                                                   .last_qty = to_i64(kv.at("LastQty"))});
        } else if (tid == "13203") {
            books_lat.apply(InstrumentInfo{.seq_no = static_cast<std::uint32_t>(seq),
                                           .security_id = security_id,
                                           .upper_ckt_lmt = to_i64(kv.at("UpperCktLmt")),
                                           .lower_ckt_lmt = to_i64(kv.at("LowerCktLmt"))});
        } else if (tid == "13301") {
            books_lat.apply(InstrumentStateChange{.seq_no = static_cast<std::uint32_t>(seq),
                                                  .security_id = security_id,
                                                  .security_status = parse_security_status(kv.at("SecurityStat")),
                                                  .sec_trd_status = parse_sec_trd_status(kv.at("SecTrdStat"))});
        }
        if (type == '\0' || !top.has_value()) {
            continue;
        }
        const auto sec_start = static_cast<std::int64_t>(top->tsec[2]);
        const auto nsec_start = static_cast<std::int64_t>(top->tnsec[2]);
        const auto sec_end = static_cast<std::int64_t>(top->tsec[3]);
        const auto nsec_end = static_cast<std::int64_t>(top->tnsec[3]);
        if (sec_start > 0 && sec_end > 0) {
            const auto latency = ((sec_end - sec_start) * 1'000'000'000LL) + (nsec_end - nsec_start);
            if (latency >= 0) {
                lat_all.push_back(latency);
                lat_by_type[type].push_back(latency);
            }
        }
    }
    print_latency_stats("MTICK latency overall t[3]-t[2] ns", lat_all);
    for (const char t : std::array<char, 6>{'N', 'M', 'X', 'F', 'T', 'E'}) {
        const auto it = lat_by_type.find(t);
        const std::vector<std::int64_t> empty{};
        const auto& v = (it == lat_by_type.end()) ? empty : it->second;
        std::string label = std::string("MTICK latency type=") + t + " t[3]-t[2] ns";
        print_latency_stats(label.c_str(), v);
    }
    if (!out_csv.empty()) {
        write_csv(out_csv, security_id, actual);
        std::cout << "Wrote actual csv: " << out_csv << "\n";
    }

    std::size_t mismatches = 0;
    std::size_t missing = 0;
    std::size_t extra = 0;
    std::set<std::int64_t> mismatch_seqs;
    std::set<std::int64_t> missing_seqs;

    if (mode == "seq_only") {
        std::unordered_map<std::int64_t, std::vector<std::size_t>> by_seq;
        by_seq.reserve(actual.size());
        for (std::size_t i = 0; i < actual.size(); ++i) {
            by_seq[actual[i].seq].push_back(i);
        }
        std::unordered_map<std::int64_t, std::size_t> consumed;
        consumed.reserve(by_seq.size());
        std::unordered_set<std::size_t> matched_actual;
        matched_actual.reserve(actual.size());

        for (std::size_t i = 0; i < expected.size(); ++i) {
            const auto it = by_seq.find(expected[i].seq);
            if (it == by_seq.end() || consumed[expected[i].seq] >= it->second.size()) {
                ++missing;
                missing_seqs.insert(expected[i].seq);
                if (missing <= 10) {
                    std::cout << "Missing actual row for expected seq=" << expected[i].seq << "\n";
                }
                continue;
            }
            const auto actual_idx = it->second[consumed[expected[i].seq]++];
            matched_actual.insert(actual_idx);
            if (!same_book(expected[i], actual[actual_idx])) {
                ++mismatches;
                mismatch_seqs.insert(expected[i].seq);
                if (mismatches <= 10) {
                    std::cout << "Mismatch at seq=" << expected[i].seq << " (expected type=" << expected[i].type
                              << ", actual type=" << actual[actual_idx].type << ")\n";
                    print_row(expected[i], " expected");
                    print_row(actual[actual_idx], " actual  ");
                }
            }
        }
        extra = actual.size() - matched_actual.size();
    } else {
        using Key = std::pair<std::int64_t, char>;
        struct KeyHash {
            std::size_t operator()(const Key& k) const noexcept {
                return std::hash<std::int64_t>{}(k.first) ^ (std::hash<int>{}(static_cast<int>(k.second)) << 1U);
            }
        };

        std::unordered_map<Key, std::vector<std::size_t>, KeyHash> by_key;
        by_key.reserve(actual.size());
        for (std::size_t i = 0; i < actual.size(); ++i) {
            by_key[{actual[i].seq, actual[i].type}].push_back(i);
        }

        std::unordered_map<Key, std::size_t, KeyHash> consumed;
        consumed.reserve(by_key.size());
        std::unordered_set<std::size_t> matched_actual;
        matched_actual.reserve(actual.size());

        for (std::size_t i = 0; i < expected.size(); ++i) {
            const Key key{expected[i].seq, expected[i].type};
            const auto it = by_key.find(key);
            if (it == by_key.end() || consumed[key] >= it->second.size()) {
                ++missing;
                missing_seqs.insert(expected[i].seq);
                if (missing <= 10) {
                    std::cout << "Missing actual row for expected seq=" << expected[i].seq
                              << " type=" << expected[i].type << "\n";
                }
                continue;
            }
            const auto actual_idx = it->second[consumed[key]++];
            matched_actual.insert(actual_idx);
            if (!same_book(expected[i], actual[actual_idx])) {
                ++mismatches;
                mismatch_seqs.insert(expected[i].seq);
                if (mismatches <= 10) {
                    std::cout << "Mismatch at seq=" << expected[i].seq << " type=" << expected[i].type << "\n";
                    print_row(expected[i], " expected");
                    print_row(actual[actual_idx], " actual  ");
                }
            }
        }
        extra = actual.size() - matched_actual.size();
    }

    if (mismatches == 0 && missing == 0 && extra == 0) {
        std::cout << "PASS\n";
        return 0;
    }

    std::cout << "FAIL mismatches=" << mismatches << " missing=" << missing << " extra_actual=" << extra << "\n";
    std::cout << "mismatch_seq_count=" << mismatch_seqs.size() << "\n";
    std::cout << "missing_seq_count=" << missing_seqs.size() << "\n";
    if (!mismatch_seqs.empty()) {
        std::cout << "mismatch_seq_nums=";
        bool first = true;
        for (const auto s : mismatch_seqs) {
            if (!first) {
                std::cout << ",";
            }
            std::cout << s;
            first = false;
        }
        std::cout << "\n";
    }
    if (!missing_seqs.empty()) {
        std::cout << "missing_seq_nums=";
        bool first = true;
        for (const auto s : missing_seqs) {
            if (!first) {
                std::cout << ",";
            }
            std::cout << s;
            first = false;
        }
        std::cout << "\n";
    }
    return 1;
}
