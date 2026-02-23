#include <array>
#include <algorithm>
#include <cassert>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "eobi/basic_price_level_book.hpp"

namespace {

using eobi::basic::AddOrder;
using eobi::basic::DeleteOrder;
using eobi::basic::ExecutionSummary;
using eobi::basic::FullOrderExecution;
using eobi::basic::MassDelete;
using eobi::basic::ModifyOrder;
using eobi::basic::ModifyOrderSamePriority;
using eobi::basic::PartialOrderExecution;
using eobi::basic::PriceLevelBooks;
using eobi::basic::Side;
using eobi::basic::MTICK;

constexpr std::int64_t kSecurityId = 504212;
constexpr std::int64_t kPriceScale = 1'000'000;
constexpr std::size_t kDepth = eobi::basic::kBookDepth;

struct SnapshotRow {
    std::int64_t seq_num{};
    char type{};
    std::array<std::int64_t, kDepth> bid_size{};
    std::array<std::int64_t, kDepth> bid_px{};
    std::array<std::int64_t, kDepth> ask_px{};
    std::array<std::int64_t, kDepth> ask_size{};

    friend bool operator==(const SnapshotRow& a, const SnapshotRow& b) {
        return a.seq_num == b.seq_num && a.type == b.type && a.bid_size == b.bid_size &&
               a.bid_px == b.bid_px && a.ask_px == b.ask_px && a.ask_size == b.ask_size;
    }
};

struct ReplayResult {
    std::vector<SnapshotRow> rows;
    std::vector<std::int64_t> latencies_ns;
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

SnapshotRow make_snapshot_row(std::int64_t seq_num, char type, const MTICK& top) {
    SnapshotRow row{};
    row.seq_num = seq_num;
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

std::vector<SnapshotRow> load_reference_csv(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("failed to open reference csv: " + path);
    }

    std::string header;
    std::getline(in, header);

    std::vector<SnapshotRow> rows;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        const auto cols = split_view(line, ',');
        if (cols.size() < 26) {
            throw std::runtime_error("bad csv row: " + line);
        }

        SnapshotRow row{};
        row.seq_num = to_i64(cols[2]);
        row.type = cols[4].empty() ? '-' : cols[4].front();
        for (std::size_t i = 0; i < kDepth; ++i) {
            const std::size_t base = 6 + (i * 4);
            row.bid_size[i] = to_i64(cols[base + 0]);
            row.bid_px[i] = to_i64(cols[base + 1]);
            row.ask_px[i] = to_i64(cols[base + 2]);
            row.ask_size[i] = to_i64(cols[base + 3]);
        }
        rows.push_back(row);
    }
    return rows;
}

ReplayResult replay_raw_ticks(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("failed to open raw log: " + path);
    }

    PriceLevelBooks books;
    ReplayResult out;

    out.rows.push_back(make_snapshot_row(0, '-', books.snapshot(kSecurityId)));

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
        if (tid_it == kv.end()) {
            continue;
        }
        const auto sid_it = kv.find("SecurityId");
        if (sid_it == kv.end() || to_i64(sid_it->second) != kSecurityId) {
            continue;
        }

        const auto msg_seq_it = kv.find("MsgSeqNum");
        if (msg_seq_it == kv.end()) {
            continue;
        }
        const std::int64_t seq = to_i64(msg_seq_it->second);
        const std::string& tid = tid_it->second;

        std::optional<MTICK> maybe_top;
        char type = '\0';

        if (tid == "13100") {
            type = 'N';
            maybe_top = books.apply(
                AddOrder{
                    .seq_no = static_cast<std::uint32_t>(seq),
                    .security_id = kSecurityId,
                    .side = parse_side(kv.at("Side")),
                    .price = to_i64(kv.at("Price")),
                    .display_qty = to_i64(kv.at("Qty")),
                });
        } else if (tid == "13101") {
            type = 'M';
            maybe_top = books.apply(
                ModifyOrder{
                    .seq_no = static_cast<std::uint32_t>(seq),
                    .security_id = kSecurityId,
                    .side = parse_side(kv.at("Side")),
                    .prev_price = to_i64(kv.at("PrevPrice")),
                    .prev_display_qty = to_i64(kv.at("prevDispQty")),
                    .price = to_i64(kv.at("Price")),
                    .display_qty = to_i64(kv.at("Qty")),
                });
        } else if (tid == "13106") {
            type = 'M';
            maybe_top = books.apply(
                ModifyOrderSamePriority{
                    .seq_no = static_cast<std::uint32_t>(seq),
                    .security_id = kSecurityId,
                    .side = parse_side(kv.at("Side")),
                    .price = to_i64(kv.at("Price")),
                    .prev_display_qty = to_i64(kv.at("prevDispQty")),
                    .display_qty = to_i64(kv.at("Qty")),
                });
        } else if (tid == "13102") {
            type = 'X';
            maybe_top = books.apply(
                DeleteOrder{
                    .seq_no = static_cast<std::uint32_t>(seq),
                    .security_id = kSecurityId,
                    .side = parse_side(kv.at("Side")),
                    .price = to_i64(kv.at("Price")),
                    .display_qty = to_i64(kv.at("Qty")),
                });
        } else if (tid == "13103") {
            type = 'X';
            maybe_top = books.apply(MassDelete{
                .seq_no = static_cast<std::uint32_t>(seq),
                .security_id = kSecurityId,
            });
        } else if (tid == "13104") {
            type = 'X';
            maybe_top = books.apply(
                FullOrderExecution{
                    .seq_no = static_cast<std::uint32_t>(seq),
                    .security_id = kSecurityId,
                    .side = parse_side(kv.at("Side")),
                    .last_px = to_i64(kv.at("LastPrice")),
                    .last_qty = to_i64(kv.at("LastQty")),
                });
        } else if (tid == "13105") {
            type = 'X';
            maybe_top = books.apply(
                PartialOrderExecution{
                    .seq_no = static_cast<std::uint32_t>(seq),
                    .security_id = kSecurityId,
                    .side = parse_side(kv.at("Side")),
                    .last_px = to_i64(kv.at("LastPrice")),
                    .last_qty = to_i64(kv.at("LastQty")),
                });
        } else if (tid == "13202") {
            type = 'E';
            maybe_top = books.apply(
                ExecutionSummary{
                    .seq_no = static_cast<std::uint32_t>(seq),
                    .security_id = kSecurityId,
                    .agg_side = parse_side(kv.at("AggSide")),
                    .last_px = to_i64(kv.at("LastPx")),
                    .last_qty = to_i64(kv.at("LastQty")),
                });
        }

        if (type == '\0' || !maybe_top.has_value()) {
            continue;
        }
        out.rows.push_back(make_snapshot_row(seq, type, *maybe_top));

        const auto sec_start = static_cast<std::int64_t>(maybe_top->tsec[2]);
        const auto nsec_start = static_cast<std::int64_t>(maybe_top->tnsec[2]);
        const auto sec_end = static_cast<std::int64_t>(maybe_top->tsec[3]);
        const auto nsec_end = static_cast<std::int64_t>(maybe_top->tnsec[3]);
        if (sec_start > 0 && sec_end > 0) {
            const auto latency = ((sec_end - sec_start) * 1'000'000'000LL) + (nsec_end - nsec_start);
            if (latency >= 0) {
                out.latencies_ns.push_back(latency);
            }
        }
    }

    return out;
}

void print_row(const SnapshotRow& r, const char* label) {
    std::cout << label << " seq=" << r.seq_num << " type=" << r.type;
    for (std::size_t i = 0; i < kDepth; ++i) {
        std::cout << " | b" << i << "=" << r.bid_px[i] << "@" << r.bid_size[i];
        std::cout << " a" << i << "=" << r.ask_px[i] << "@" << r.ask_size[i];
    }
    std::cout << "\n";
}

}  // namespace

int main() {
    const std::string raw = "cpp_price_levels/testcases/504212_raw_ticks.log";
    const std::string ref = "cpp_price_levels/testcases/dinfra_tbt_504212.csv";

    const auto expected = load_reference_csv(ref);
    const auto actual_result = replay_raw_ticks(raw);
    const auto& actual = actual_result.rows;

    if (!actual_result.latencies_ns.empty()) {
        auto lats = actual_result.latencies_ns;
        std::sort(lats.begin(), lats.end());
        std::int64_t total = 0;
        for (const auto v : lats) {
            total += v;
        }
        const auto idx = [&](double p) -> std::size_t {
            return static_cast<std::size_t>(p * static_cast<double>(lats.size() - 1));
        };
        std::cout << "MTICK latency from t[2]->t[3] (ns): "
                  << "count=" << lats.size() << " avg=" << (total / static_cast<std::int64_t>(lats.size()))
                  << " min=" << lats.front() << " p50=" << lats[idx(0.50)] << " p90=" << lats[idx(0.90)]
                  << " p99=" << lats[idx(0.99)] << " max=" << lats.back() << "\n";
    } else {
        std::cout << "MTICK latency from t[2]->t[3] (ns): no samples\n";
    }

    using Key = std::tuple<std::int64_t, char>;
    struct KeyHash {
        std::size_t operator()(const Key& k) const noexcept {
            const auto h1 = std::hash<std::int64_t>{}(std::get<0>(k));
            const auto h2 = std::hash<int>{}(static_cast<int>(std::get<1>(k)));
            return h1 ^ (h2 << 1U);
        }
    };

    std::unordered_map<Key, std::vector<std::size_t>, KeyHash> index_by_seq_type;
    index_by_seq_type.reserve(actual.size());
    for (std::size_t i = 0; i < actual.size(); ++i) {
        index_by_seq_type[{actual[i].seq_num, actual[i].type}].push_back(i);
    }

    std::unordered_map<Key, std::size_t, KeyHash> consumed;
    consumed.reserve(index_by_seq_type.size());

    std::size_t mismatches = 0;
    std::size_t missing = 0;
    std::unordered_map<char, std::size_t> mismatches_by_type;
    std::unordered_map<char, std::size_t> missing_by_type;
    std::unordered_set<std::size_t> matched_actual_indices;
    for (std::size_t i = 0; i < expected.size(); ++i) {
        const auto key = Key{expected[i].seq_num, expected[i].type};
        const auto it = index_by_seq_type.find(key);
        if (it == index_by_seq_type.end() || consumed[key] >= it->second.size()) {
            if (missing < 10) {
                std::cout << "Missing actual row for expected row " << i << " (seq=" << expected[i].seq_num
                          << ", type=" << expected[i].type << ")\n";
            }
            ++missing;
            ++missing_by_type[expected[i].type];
            continue;
        }
        const auto actual_idx = it->second[consumed[key]++];
        matched_actual_indices.insert(actual_idx);
        if (!(expected[i] == actual[actual_idx])) {
            if (mismatches < 10) {
                std::cout << "Mismatch at expected row " << i << " (actual idx " << actual_idx << ")\n";
                print_row(expected[i], " expected");
                print_row(actual[actual_idx], " actual  ");
            }
            ++mismatches;
            ++mismatches_by_type[expected[i].type];
        }
    }

    std::size_t extra_actual = actual.size() - matched_actual_indices.size();

    if (mismatches == 0 && missing == 0) {
        std::cout << "PASS: replay matches dinfra_tbt_504212.csv\n";
        return 0;
    }

    std::cout << "FAIL: mismatches=" << mismatches << " missing=" << missing
              << " extra_actual=" << extra_actual << "\n";
    std::cout << "mismatch_by_type:";
    for (const auto& [type, cnt] : mismatches_by_type) {
        std::cout << " " << type << "=" << cnt;
    }
    std::cout << "\n";
    std::cout << "missing_by_type:";
    for (const auto& [type, cnt] : missing_by_type) {
        std::cout << " " << type << "=" << cnt;
    }
    std::cout << "\n";
    return 1;
}
