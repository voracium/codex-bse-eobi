#include <cassert>
#include <cstdint>

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
using eobi::basic::PartialOrderExecution;
using eobi::basic::PriceLevelBooks;
using eobi::basic::SecTrdStatus;
using eobi::basic::Side;

void test_add_modify_delete_exec() {
    PriceLevelBooks books;

    auto a = books.apply(AddOrder{.seq_no = 1, .security_id = 1, .side = Side::Buy, .price = 100, .display_qty = 10});
    assert(a.has_value());
    assert(a->bid[0] == 100 && a->bid_size[0] == 10);

    auto m = books.apply(ModifyOrder{
        .seq_no = 2,
        .security_id = 1,
        .side = Side::Buy,
        .prev_price = 100,
        .prev_display_qty = 10,
        .price = 101,
        .display_qty = 12,
    });
    assert(m.has_value());
    assert(m->bid[0] == 101 && m->bid_size[0] == 12);

    auto ms = books.apply(ModifyOrderSamePriority{
        .seq_no = 3,
        .security_id = 1,
        .side = Side::Buy,
        .price = 101,
        .prev_display_qty = 12,
        .display_qty = 7,
    });
    assert(ms.has_value());
    assert(ms->bid[0] == 101 && ms->bid_size[0] == 7);

    auto pe = books.apply(PartialOrderExecution{
        .seq_no = 4,
        .security_id = 1,
        .side = Side::Buy,
        .price = 101,
        .last_px = 101,
        .last_qty = 2,
    });
    assert(pe.has_value());
    assert(pe->bid[0] == 101 && pe->bid_size[0] == 5);

    auto fe = books.apply(FullOrderExecution{
        .seq_no = 5,
        .security_id = 1,
        .side = Side::Buy,
        .price = 101,
        .last_px = 101,
        .last_qty = 5,
    });
    assert(fe.has_value());
    assert(fe->bid_size[0] == 0);

    auto d = books.apply(DeleteOrder{.seq_no = 6, .security_id = 1, .side = Side::Buy, .price = 101, .display_qty = 1});
    assert(!d.has_value());
}

void test_publish_only_on_top_depth_change() {
    PriceLevelBooks books;

    books.apply(AddOrder{.seq_no = 1, .security_id = 2, .side = Side::Buy, .price = 105, .display_qty = 1});
    books.apply(AddOrder{.seq_no = 2, .security_id = 2, .side = Side::Buy, .price = 104, .display_qty = 1});
    books.apply(AddOrder{.seq_no = 3, .security_id = 2, .side = Side::Buy, .price = 103, .display_qty = 1});
    books.apply(AddOrder{.seq_no = 4, .security_id = 2, .side = Side::Buy, .price = 102, .display_qty = 1});
    books.apply(AddOrder{.seq_no = 5, .security_id = 2, .side = Side::Buy, .price = 101, .display_qty = 1});

    auto no_top_change =
        books.apply(AddOrder{.seq_no = 6, .security_id = 2, .side = Side::Buy, .price = 100, .display_qty = 5});
    assert(!no_top_change.has_value());

    auto top_change =
        books.apply(AddOrder{.seq_no = 7, .security_id = 2, .side = Side::Buy, .price = 106, .display_qty = 5});
    assert(top_change.has_value());
    assert(top_change->bid[0] == 106 && top_change->bid_size[0] == 5);
}

void test_circuit_limit_filtering() {
    PriceLevelBooks books;

    books.apply(InstrumentInfo{
        .seq_no = 1,
        .security_id = 3,
        .upper_ckt_lmt = 110,
        .lower_ckt_lmt = 90,
    });

    auto in = books.apply(AddOrder{.seq_no = 2, .security_id = 3, .side = Side::Buy, .price = 100, .display_qty = 10});
    assert(in.has_value());

    auto low = books.apply(AddOrder{.seq_no = 3, .security_id = 3, .side = Side::Buy, .price = 80, .display_qty = 10});
    auto high = books.apply(AddOrder{.seq_no = 4, .security_id = 3, .side = Side::Sell, .price = 120, .display_qty = 10});
    assert(!low.has_value());
    assert(!high.has_value());
}

void test_execution_summary_continuous_only() {
    PriceLevelBooks books;

    books.apply(AddOrder{.seq_no = 1, .security_id = 4, .side = Side::Buy, .price = 100, .display_qty = 5});
    books.apply(AddOrder{.seq_no = 2, .security_id = 4, .side = Side::Buy, .price = 99, .display_qty = 5});
    books.apply(AddOrder{.seq_no = 3, .security_id = 4, .side = Side::Sell, .price = 101, .display_qty = 5});

    auto before_cont = books.apply(
        ExecutionSummary{.seq_no = 4, .security_id = 4, .agg_side = Side::Sell, .last_px = 100, .last_qty = 7});
    assert(!before_cont.has_value());

    books.apply(InstrumentStateChange{
        .seq_no = 5,
        .security_id = 4,
        .sec_trd_status = SecTrdStatus::Continuous,
    });

    auto cont = books.apply(
        ExecutionSummary{.seq_no = 6, .security_id = 4, .agg_side = Side::Sell, .last_px = 100, .last_qty = 7});
    assert(cont.has_value());
    assert(cont->bid[0] == 99 && cont->bid_size[0] == 3);
}

void test_execution_summary_stpc_clamps_top_to_last_px_or_worse() {
    PriceLevelBooks books;
    books.apply(AddOrder{.seq_no = 1, .security_id = 9, .side = Side::Buy, .price = 100, .display_qty = 5});
    books.apply(AddOrder{.seq_no = 2, .security_id = 9, .side = Side::Buy, .price = 99, .display_qty = 5});
    books.apply(InstrumentStateChange{
        .seq_no = 3,
        .security_id = 9,
        .sec_trd_status = SecTrdStatus::Continuous,
    });

    auto t = books.apply(
        ExecutionSummary{.seq_no = 4, .security_id = 9, .agg_side = Side::Sell, .last_px = 99, .last_qty = 1});
    assert(t.has_value());
    assert(t->bid[0] == 99);
    assert(t->bid_size[0] == 5);
}

void test_exec_price_uses_price_in_freeze_and_checks_circuit() {
    PriceLevelBooks books;
    books.apply(InstrumentInfo{
        .seq_no = 1,
        .security_id = 7,
        .upper_ckt_lmt = 110,
        .lower_ckt_lmt = 90,
    });
    books.apply(InstrumentStateChange{
        .seq_no = 2,
        .security_id = 7,
        .sec_trd_status = SecTrdStatus::OpeningAuctionFreeze,
    });
    books.apply(AddOrder{.seq_no = 3, .security_id = 7, .side = Side::Buy, .price = 100, .display_qty = 10});

    auto bad = books.apply(PartialOrderExecution{
        .seq_no = 4,
        .security_id = 7,
        .side = Side::Buy,
        .price = 120,
        .last_px = 100,
        .last_qty = 4,
    });
    assert(!bad.has_value());

    auto good = books.apply(PartialOrderExecution{
        .seq_no = 5,
        .security_id = 7,
        .side = Side::Buy,
        .price = 100,
        .last_px = 120,
        .last_qty = 4,
    });
    assert(good.has_value());
    assert(good->bid[0] == 100);
    assert(good->bid_size[0] == 6);
}

void test_modify_with_invalid_prev_price_keeps_add_leg() {
    PriceLevelBooks books;
    books.apply(InstrumentInfo{
        .seq_no = 1,
        .security_id = 8,
        .upper_ckt_lmt = 110,
        .lower_ckt_lmt = 90,
    });

    auto out = books.apply(ModifyOrder{
        .seq_no = 2,
        .security_id = 8,
        .side = Side::Buy,
        .prev_price = 9223372036854775807LL,
        .prev_display_qty = 15,
        .price = 100,
        .display_qty = 15,
    });
    assert(out.has_value());
    assert(out->bid[0] == 100);
    assert(out->bid_size[0] == 15);
}

void test_timestamps_written_on_publish() {
    PriceLevelBooks books;
    auto out = books.apply(AddOrder{.seq_no = 1, .security_id = 5, .side = Side::Buy, .price = 100, .display_qty = 1});
    assert(out.has_value());
    assert(out->tsec[2] > 0);
    assert(out->tsec[3] > 0);
}

void test_mass_delete() {
    PriceLevelBooks books;
    books.apply(AddOrder{.seq_no = 1, .security_id = 6, .side = Side::Buy, .price = 100, .display_qty = 10});
    books.apply(AddOrder{.seq_no = 2, .security_id = 6, .side = Side::Sell, .price = 101, .display_qty = 8});

    auto out = books.apply(MassDelete{.seq_no = 3, .security_id = 6});
    assert(out.has_value());
    assert(out->bid_size[0] == 0 && out->ask_size[0] == 0);
}

}  // namespace

int main() {
    test_add_modify_delete_exec();
    test_publish_only_on_top_depth_change();
    test_circuit_limit_filtering();
    test_execution_summary_continuous_only();
    test_execution_summary_stpc_clamps_top_to_last_px_or_worse();
    test_exec_price_uses_price_in_freeze_and_checks_circuit();
    test_modify_with_invalid_prev_price_keeps_add_leg();
    test_timestamps_written_on_publish();
    test_mass_delete();
    return 0;
}
