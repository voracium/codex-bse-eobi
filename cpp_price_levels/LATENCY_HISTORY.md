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

<details>
<summary><code>2026-02-23T18:05:04+05:30</code> | commit <code>c19361f</code> | overall: avg <code>117</code>, p50 <code>98</code>, p90 <code>211</code>, p95 <code>247</code>, p99 <code>311</code></summary>

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
</details>

<details>
<summary><code>2026-02-24T14:47:03+05:30</code> | commit <code>1d775cb</code> | overall: avg <code>86</code>, p50 <code>78</code>, p90 <code>142</code>, p95 <code>163</code>, p99 <code>201</code></summary>

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
</details>

<details>
<summary><code>2026-02-24T15:14:28+05:30</code> | commit <code>499ae47</code> | overall: avg <code>72</code>, p50 <code>72</code>, p90 <code>105</code>, p95 <code>115</code>, p99 <code>137</code></summary>

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
</details>

<details>
<summary><code>2026-02-24T15:33:35+05:30</code> | commit <code>32ef89b</code> | overall: avg <code>73</code>, p50 <code>73</code>, p90 <code>105</code>, p95 <code>115</code>, p99 <code>138</code></summary>

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
</details>

<details>
<summary><code>2026-02-24T16:04:17+05:30</code> | commit <code>59fec83</code> | overall: avg <code>56</code>, p50 <code>55</code>, p90 <code>72</code>, p95 <code>78</code>, p99 <code>90</code></summary>

| Type | Count | Avg(ns) | Min(ns) | P50(ns) | P90(ns) | P95(ns) | P99(ns) | Max(ns) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| overall | 952656 | 56 | 19 | 55 | 72 | 78 | 90 | 143362 |
| N | 295058 | 53 | 26 | 52 | 63 | 68 | 77 | 143362 |
| M | 339289 | 63 | 30 | 62 | 79 | 84 | 96 | 5316 |
| X | 307444 | 53 | 23 | 52 | 65 | 70 | 79 | 4152 |
| F | 2817 | 64 | 26 | 60 | 82 | 97 | 174 | 604 |
| T | 3561 | 51 | 24 | 46 | 68 | 76 | 140 | 441 |
| E | 4487 | 58 | 19 | 46 | 89 | 128 | 221 | 1868 |

| Validation Metric | Value |
|---|---:|
| mismatches | 36 |
| missing | 4 |
| extra_actual | 6433 |
</details>

<details>
<summary><code>2026-02-24T16:21:21+05:30</code> | commit <code>1c011b8</code> | overall: avg <code>56</code>, p50 <code>55</code>, p90 <code>71</code>, p95 <code>77</code>, p99 <code>88</code></summary>

| Type | Count | Avg(ns) | Min(ns) | P50(ns) | P90(ns) | P95(ns) | P99(ns) | Max(ns) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| overall | 952656 | 56 | 20 | 55 | 71 | 77 | 88 | 13403 |
| N | 295058 | 52 | 24 | 51 | 63 | 67 | 76 | 3000 |
| M | 339289 | 62 | 31 | 61 | 78 | 83 | 94 | 13403 |
| X | 307444 | 53 | 21 | 52 | 64 | 69 | 78 | 11063 |
| F | 2817 | 63 | 23 | 59 | 79 | 96 | 156 | 660 |
| T | 3561 | 50 | 25 | 45 | 64 | 73 | 131 | 272 |
| E | 4487 | 55 | 20 | 45 | 82 | 117 | 189 | 503 |

| Validation Metric | Value |
|---|---:|
| mismatches | 36 |
| missing | 4 |
| extra_actual | 6433 |
</details>

<details open>
<summary><code>2026-02-24T17:04:06+05:30</code> | commit <code>14c99ea</code> | overall: avg <code>56</code>, p50 <code>55</code>, p90 <code>72</code>, p95 <code>78</code>, p99 <code>89</code></summary>

| Type | Count | Avg(ns) | Min(ns) | P50(ns) | P90(ns) | P95(ns) | P99(ns) | Max(ns) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| overall | 952656 | 56 | 20 | 55 | 72 | 78 | 89 | 6496 |
| N | 295058 | 53 | 24 | 52 | 63 | 67 | 75 | 6496 |
| M | 339289 | 63 | 29 | 62 | 79 | 84 | 96 | 4650 |
| X | 307444 | 54 | 23 | 53 | 65 | 70 | 78 | 5302 |
| F | 2817 | 63 | 24 | 59 | 80 | 98 | 166 | 561 |
| T | 3561 | 51 | 24 | 45 | 65 | 73 | 138 | 495 |
| E | 4487 | 56 | 20 | 46 | 81 | 110 | 191 | 2211 |

| Validation Metric | Value |
|---|---:|
| mismatches | 36 |
| missing | 4 |
| extra_actual | 6433 |
</details>
