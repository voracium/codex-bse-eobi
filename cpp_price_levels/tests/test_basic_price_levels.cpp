#include <cassert>

#include "eobi/basic_price_level_book.hpp"

namespace {

using eobi::basic::AddOrder;
using eobi::basic::DeleteOrder;
using eobi::basic::FullOrderExecution;
using eobi::basic::MassDelete;
using eobi::basic::ModifyOrder;
using eobi::basic::ModifyOrderSamePriority;
using eobi::basic::PartialOrderExecution;
using eobi::basic::PriceLevelBooks;
using eobi::basic::Side;

void test_add_and_publish_top_levels() {
    PriceLevelBooks books;

    auto p1 = books.apply(AddOrder{.security_id = 1, .side = Side::Buy, .price = 100, .display_qty = 10});
    assert(p1.has_value());
    assert(p1->bid[0] == 100);
    assert(p1->bid_size[0] == 10);

    auto p2 = books.apply(AddOrder{.security_id = 1, .side = Side::Buy, .price = 100, .display_qty = 5});
    assert(p2.has_value());
    assert(p2->bid_size[0] == 15);
}

void test_modify_using_prev_fields_only() {
    PriceLevelBooks books;
    books.apply(AddOrder{.security_id = 1, .side = Side::Sell, .price = 110, .display_qty = 20});

    auto out = books.apply(
        ModifyOrder{
            .security_id = 1,
            .side = Side::Sell,
            .prev_price = 110,
            .prev_display_qty = 20,
            .price = 111,
            .display_qty = 12,
        });
    assert(out.has_value());
    assert(out->ask[0] == 111);
    assert(out->ask_size[0] == 12);
}

void test_modify_same_priority_delta() {
    PriceLevelBooks books;
    books.apply(AddOrder{.security_id = 1, .side = Side::Buy, .price = 100, .display_qty = 10});

    auto out = books.apply(
        ModifyOrderSamePriority{
            .security_id = 1,
            .side = Side::Buy,
            .price = 100,
            .prev_display_qty = 10,
            .display_qty = 7,
        });
    assert(out.has_value());
    assert(out->bid_size[0] == 7);
}

void test_delete_and_exec() {
    PriceLevelBooks books;
    books.apply(AddOrder{.security_id = 1, .side = Side::Buy, .price = 100, .display_qty = 10});
    books.apply(AddOrder{.security_id = 1, .side = Side::Buy, .price = 99, .display_qty = 6});

    auto part = books.apply(
        PartialOrderExecution{
            .security_id = 1,
            .side = Side::Buy,
            .last_px = 100,
            .last_qty = 4,
        });
    assert(part.has_value());
    assert(part->bid[0] == 100);
    assert(part->bid_size[0] == 6);

    auto full = books.apply(
        FullOrderExecution{
            .security_id = 1,
            .side = Side::Buy,
            .last_px = 100,
            .last_qty = 6,
        });
    assert(full.has_value());
    assert(full->bid[0] == 99);
    assert(full->bid_size[0] == 6);

    auto del = books.apply(
        DeleteOrder{
            .security_id = 1,
            .side = Side::Buy,
            .price = 99,
            .display_qty = 6,
        });
    assert(del.has_value());
    assert(del->bid_size[0] == 0);
}

void test_publish_only_when_top_n_changes() {
    PriceLevelBooks books;
    books.apply(AddOrder{.security_id = 1, .side = Side::Buy, .price = 105, .display_qty = 1});
    books.apply(AddOrder{.security_id = 1, .side = Side::Buy, .price = 104, .display_qty = 1});
    books.apply(AddOrder{.security_id = 1, .side = Side::Buy, .price = 103, .display_qty = 1});
    books.apply(AddOrder{.security_id = 1, .side = Side::Buy, .price = 102, .display_qty = 1});
    books.apply(AddOrder{.security_id = 1, .side = Side::Buy, .price = 101, .display_qty = 1});

    auto unchanged = books.apply(AddOrder{.security_id = 1, .side = Side::Buy, .price = 100, .display_qty = 1});
    assert(!unchanged.has_value());

    auto changed = books.apply(AddOrder{.security_id = 1, .side = Side::Buy, .price = 106, .display_qty = 1});
    assert(changed.has_value());
    assert(changed->bid[0] == 106);
    assert(changed->bid[1] == 105);
}

void test_mass_delete() {
    PriceLevelBooks books;
    books.apply(AddOrder{.security_id = 1, .side = Side::Buy, .price = 100, .display_qty = 10});
    books.apply(AddOrder{.security_id = 1, .side = Side::Sell, .price = 110, .display_qty = 10});

    auto out = books.apply(MassDelete{.security_id = 1});
    assert(out.has_value());
    assert(out->bid_size[0] == 0);
    assert(out->ask_size[0] == 0);
}

void test_mtick_timestamps_set_on_publish() {
    PriceLevelBooks books;
    auto out = books.apply(AddOrder{.security_id = 1, .side = Side::Buy, .price = 100, .display_qty = 10});
    assert(out.has_value());
    assert(out->tsec[0] == 0);
    assert(out->tnsec[0] == 0);
    assert(out->tsec[1] == 0);
    assert(out->tnsec[1] == 0);
    assert(out->tsec[2] > 0);
    assert(out->tnsec[2] >= 0);
    assert(out->tsec[3] > 0);
    assert(out->tnsec[3] >= 0);
}

}  // namespace

int main() {
    test_add_and_publish_top_levels();
    test_modify_using_prev_fields_only();
    test_modify_same_priority_delta();
    test_delete_and_exec();
    test_publish_only_when_top_n_changes();
    test_mass_delete();
    test_mtick_timestamps_set_on_publish();
    return 0;
}
