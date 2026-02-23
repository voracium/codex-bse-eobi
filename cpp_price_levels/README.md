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
- `tests/replay_validate.cpp`

## Event handling model

Updates use only message fields needed for aggregated levels:

- `AddOrder`: `+display_qty` at `(side, price)`
- `ModifyOrder`: `-prev_display_qty` at `(side, prev_price)`, then `+display_qty` at `(side, price)`
- `ModifyOrderSamePriority`: apply delta `(display_qty - prev_display_qty)` at `(side, price)`
- `DeleteOrder`: `-display_qty` at `(side, price)`
- `PartialOrderExecution`: `-last_qty` at `(side, exec_price)`
- `FullOrderExecution`: `-last_qty` at `(side, exec_price)`
- `MassDelete`: clear both sides for instrument
- `ExecutionSummary`: publishes tentative MTICK only when instrument is in `CONTINUOUS`

`exec_price` rule:
- default: `LastPrice`
- in `OPENING_AUCTION_FREEZE` / `INTRADAY_AUCTION_FREEZE`: use `Price` when present

## Instrument state and limits

- `InstrumentInfo` updates upper/lower circuit limits.
- `InstrumentStateChange` updates security/trading status.
- Circuit filtering is enforced in core delta application (`apply_delta`), so invalid prices are ignored consistently across add/modify/delete/exec flows.

## Publish behavior

`PriceLevelBooks::apply(...)` returns:
- `std::nullopt` if top book levels did not change
- `MTICK` snapshot if top book levels changed

## Build test (no CMake required)

```bash
g++ -std=c++20 -O3 -Wall -Wextra \
  -I cpp_price_levels/include \
  cpp_price_levels/tests/test_basic_price_levels.cpp \
  -o /tmp/test_basic_price_levels && /tmp/test_basic_price_levels
```

## Replay validation

Build:

```bash
g++ -std=c++20 -O3 -Wall -Wextra \
  -I cpp_price_levels/include \
  cpp_price_levels/tests/replay_validate.cpp \
  -o /tmp/replay_validate
```

Run:

```bash
/tmp/replay_validate <raw_log> <reference_csv> <security_id> [mode] [out_csv]
```

- `mode`:
  - `strict` (default): match by `(SeqNum, Type)`
  - `seq_only`: match by `SeqNum` only
- `out_csv` (optional): writes generated MTICK stream in dinfra-compatible header format.
- Validator prints latency stats from MTICK timestamps using `t[3] - t[2]` for:
  - overall
  - per type: `N/M/X/F/T/E`
