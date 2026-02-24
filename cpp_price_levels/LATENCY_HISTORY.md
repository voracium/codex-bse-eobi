# Latency History

Measured as `MTICK t[3] - t[2]` in nanoseconds from `tests/replay_validate.cpp`.

Dataset used unless noted:
- raw: `cpp_price_levels/testcases/532309_raw_ticks.log`
- ref: `cpp_price_levels/testcases/dinfra_tbt_532309.csv`
- mode: `strict`

Machine details:
- host/kernel: `Linux irage-ThinkCentre-neo-50q-Gen-4 6.17.0-14-generic x86_64`
- CPU: `13th Gen Intel(R) Core(TM) i5-13420H` (`12` logical CPUs, `8` cores, max `4.6 GHz`)
- memory: `30 GiB RAM`, `8 GiB swap`
- compiler: `g++ (Ubuntu 15.2.0-4ubuntu4) 15.2.0`

## Entries

### 2026-02-23T18:05:04+05:30 | commit `c19361f`

| Type | Count | Avg(ns) | Min(ns) | P50(ns) | P90(ns) | P95(ns) | P99(ns) | Max(ns) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| overall | 952656 | 117 | 17 | 98 | 211 | 247 | 311 | 10953 |
| N | 295058 | 87 | 19 | 81 | 145 | 166 | 204 | 6427 |
| M | 339289 | 165 | 20 | 162 | 261 | 289 | 343 | 10953 |
| X | 307444 | 93 | 20 | 84 | 158 | 174 | 205 | 4684 |
| F | 2817 | 128 | 20 | 130 | 208 | 238 | 350 | 708 |
| T | 3561 | 50 | 19 | 29 | 92 | 115 | 220 | 1910 |
| E | 4487 | 101 | 17 | 80 | 213 | 250 | 340 | 9203 |

| Validation Metric | Value |
|---|---:|
| mismatches | 36 |
| missing | 4 |
| extra_actual | 6433 |

### 2026-02-24T14:47:03+05:30 | commit `1d775cb`

Dataset:
- raw: `cpp_price_levels/testcases/532309_raw_ticks.log`
- ref: `cpp_price_levels/testcases/dinfra_tbt_532309.csv`
- mode: `strict`

| Type | Count | Avg(ns) | Min(ns) | P50(ns) | P90(ns) | P95(ns) | P99(ns) | Max(ns) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| overall | 952656 | 86 | 19 | 78 | 142 | 163 | 201 | 12591 |
| N | 295058 | 70 | 19 | 69 | 96 | 105 | 123 | 11170 |
| M | 339289 | 112 | 20 | 114 | 171 | 187 | 223 | 12591 |
| X | 307444 | 72 | 20 | 72 | 101 | 109 | 128 | 11941 |
| F | 2817 | 107 | 21 | 94 | 172 | 223 | 361 | 851 |
| T | 3561 | 78 | 22 | 69 | 120 | 151 | 255 | 783 |
| E | 4487 | 79 | 20 | 69 | 134 | 166 | 274 | 3578 |

| Validation Metric | Value |
|---|---:|
| mismatches | 36 |
| missing | 4 |
| extra_actual | 6433 |

### 2026-02-24T15:14:28+05:30 | commit `499ae47`

Dataset:
- raw: `cpp_price_levels/testcases/532309_raw_ticks.log`
- ref: `cpp_price_levels/testcases/dinfra_tbt_532309.csv`
- mode: `strict`

| Type | Count | Avg(ns) | Min(ns) | P50(ns) | P90(ns) | P95(ns) | P99(ns) | Max(ns) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| overall | 952656 | 72 | 19 | 72 | 105 | 115 | 137 | 158854 |
| N | 295058 | 68 | 19 | 68 | 95 | 104 | 122 | 13645 |
| M | 339289 | 76 | 20 | 79 | 114 | 124 | 145 | 8843 |
| X | 307444 | 72 | 20 | 71 | 99 | 107 | 126 | 158854 |
| F | 2817 | 102 | 21 | 94 | 154 | 194 | 298 | 2138 |
| T | 3561 | 78 | 22 | 75 | 117 | 139 | 237 | 539 |
| E | 4487 | 76 | 19 | 70 | 128 | 150 | 226 | 479 |

| Validation Metric | Value |
|---|---:|
| mismatches | 36 |
| missing | 4 |
| extra_actual | 6433 |

### 2026-02-24T15:33:35+05:30 | commit `32ef89b`

Dataset:
- raw: `cpp_price_levels/testcases/532309_raw_ticks.log`
- ref: `cpp_price_levels/testcases/dinfra_tbt_532309.csv`
- mode: `strict`

| Type | Count | Avg(ns) | Min(ns) | P50(ns) | P90(ns) | P95(ns) | P99(ns) | Max(ns) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| overall | 952656 | 73 | 19 | 73 | 105 | 115 | 138 | 15638 |
| N | 295058 | 70 | 20 | 70 | 97 | 106 | 125 | 4651 |
| M | 339289 | 76 | 20 | 78 | 113 | 123 | 145 | 4635 |
| X | 307444 | 71 | 19 | 71 | 99 | 108 | 127 | 15638 |
| F | 2817 | 107 | 20 | 96 | 162 | 212 | 319 | 2325 |
| T | 3561 | 75 | 22 | 65 | 119 | 158 | 249 | 817 |
| E | 4487 | 78 | 21 | 71 | 134 | 161 | 246 | 1521 |

| Validation Metric | Value |
|---|---:|
| mismatches | 36 |
| missing | 4 |
| extra_actual | 6433 |
