# Basic Price-Level Orderbook (C++)

This is a separate implementation focused only on:
- price levels
- total quantity per level
- publishing top levels only when they change

No per-order lifecycle tracking is used.

Depth is a single compile-time constant:
- `EOBI_BOOK_DEPTH` (default `5`)
- exposed as `eobi::basic::kBookDepth`

## Directory

- `include/eobi/basic_price_level_book.hpp`
- `tests/test_basic_price_levels.cpp`

## Event handling model

Updates use only message fields needed for aggregated levels:

- `AddOrder`: `+display_qty` at `(side, price)`
- `ModifyOrder`: `-prev_display_qty` at `(side, prev_price)`, then `+display_qty` at `(side, price)`
- `ModifyOrderSamePriority`: apply delta `(display_qty - prev_display_qty)` at `(side, price)`
- `DeleteOrder`: `-display_qty` at `(side, price)`
- `PartialOrderExecution`: `-last_qty` at `(side, last_px)`
- `FullOrderExecution`: `-last_qty` at `(side, last_px)`
- `MassDelete`: clear both sides for instrument
- `ExecutionSummary`: no orderbook state change

## Publish behavior

`PriceLevelBooks::apply(...)` returns:
- `std::nullopt` if top book levels did not change
- `MTICK` snapshot if top book levels changed

## Build test (no CMake required)

```bash
g++ -std=c++20 -O3 -Wall -Wextra \
  -I bse-eobi/cpp_price_levels/include \
  bse-eobi/cpp_price_levels/tests/test_basic_price_levels.cpp \
  -o /tmp/test_basic_price_levels && /tmp/test_basic_price_levels
```
