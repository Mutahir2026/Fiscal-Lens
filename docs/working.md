# Fiscal-Lens: How the System Works

Status (12 Nov 2026): Module 1 and Module 2 are built and linked through CSV files. The frontend is not yet connected to the C++ backend (no WASM or API bridge). The repo README is not yet maintained.

## 1. Big picture

```
Brent daily prices (Module 1 data)
   |  Module 1: log -> difference once -> AR(p) via Levinson-Durbin -> forecast
   v
forecast_crude_price = mean of the next 10 trading days of forecasted Brent (USD/bbl)
   |  (only link between the modules)
   v
Module 2: one input row for a fortnight, then the regression tree
predicts a multiplier  ->  recommended levy = required_levy x multiplier
```

- Only Brent feeds Module 2. Module 1 does not track retail petrol price. USD/PKR is not used by Module 2.
- No live data feeds, no database. Everything is read from CSV files.
- The levy labels are synthetic (see section 8). The input columns are mixed-provenance real data (see `data/Data_Readme.md`).
- The modules are linked by files and run by hand. No single program runs the whole pipeline (see section 7).

## 2. Module 1: Commodity Trend Tracker (owner: Module 1 team)

**What it does**
- Forecasts Brent crude and shows it on the dashboard graph (once the frontend is connected).
- Provides one number to Module 2: `forecast_crude_price`.

**How**
1. Load Brent daily prices (`date,price`).
2. Take logs, difference once (the "I" in ARI) to remove the trend.
3. Fit AR(p) on the differenced series: `x[t] = c + phi1*x[t-1] + ... + phip*x[t-p] + e[t]`. Coefficients come from the Yule-Walker equations solved with the Levinson-Durbin recursion, O(p^2) per order (naive normal equations are O(p^3)).
4. Choose p by rolling-window validation on past data (current implementation). The blueprint planned AIC/BIC; the final choice must be stated in the report.
5. Forecast recursively, add the predicted differences back onto the last known log price, then take the antilog.
6. `forecast_crude_price` = mean of the forecast for the next 10 trading days, using only data before the cutoff date (no look-ahead).

**Outputs**
- `data/processed/forecast_col.csv` (columns `period_start,forecast_crude_price`): one out-of-sample forecast per levy row.
- A callable `forecastMean(prices_up_to_cutoff, 10)`. Module 2's live mode does not call it yet; it takes the value from `--crude`.
- The dashboard Brent graph.

**Benchmark (report)**: Levinson-Durbin vs naive solver across p and n; the coefficients must match before timings are compared.

## 3. Module 2: Petroleum Levy Advisor (owner: Module 2 team)

**Data file**: `data/processed/levy_training.csv`, the real-data (mixed-provenance) version, fortnightly rows, ISO dates, sorted oldest to newest. Columns: `period_start, period_end, forecast_crude_price, affordability_index, levy_revenue_target_pkr_bn, fiscal_gap_pkr_bn, current_levy, retail_petrol_price, levied_volume_bn_litres, levy_value`. Column-by-column provenance, formulas and carried or estimated values are in `data/Data_Readme.md`; do not describe the file as fully real or fully official. Coverage: 70 rows from 2023-11-16 to 2026-10-15; a row for 2026-10-16 to 2026-10-31 is being added (section 7).

**Tree**: a regression tree written in C++ (`RegressionTree`). It learns the multiplier, not the levy directly:
- Features (8): `forecast_crude_price, affordability_index, levy_revenue_target_pkr_bn, fiscal_gap_pkr_bn, current_levy, retail_petrol_price, levied_volume_bn_litres, required_levy`.
- Split: for each feature, sort the rows, sweep the cut point with running sums, and pick the split with the lowest SSE (`SSE = sum(y^2) - (sum y)^2 / n`). Greedy, recursive, no backtracking. Leaves predict the mean. Cost is O(n log n) per feature per node because of the sort.
- Final levy: `required_levy x predicted multiplier`.

**Testing (default report, no arguments)**
- Chronological split: oldest 80% train, newest 20% test. Never shuffled.
- Depth and leaf size are chosen by walk-forward validation on training rows only.
- Baselines on the same test rows: training mean, previous levy, current levy.
- A shuffled-split run as a diagnostic that the tree recovers the rule; it is not a forecast.
- Error metrics are reported as RMSE/MAE in PKR per litre, not as "accuracy".

## 4. Formulas

```
required_levy       = levy_revenue_target_pkr_bn / levied_volume_bn_litres
affordability_index = min_wage_pkr_month / (40 L x retail_petrol_price) x (100 / cpi_index) x 10
stress              = affordability_index < 40th percentile of affordability_index over all rows
high_crude          = forecast_crude_price > 80
multiplier          = 0.80 (stress and high_crude) | 0.92 (stress only) | 0.95 (high_crude only) | 1.00
levy_value          = clip(round(required_levy x multiplier, 1), 30, 85)     PKR/litre

```

- The stress cut-off is recomputed by `scripts/regenerate_labels.py` on every run. It was 27.907 on the earlier synthetic file and 29.2176 on the 70-row real-data file; it changes whenever rows are added.
- Wage is the fiscal-year minimum wage (FY2026-27: 40,700); CPI is the index rebuilt from monthly changes (Oct 2023 = 100).
- Crude forecast: used only in `high_crude`. Retail price: used through `affordability_index`, which drives `stress`.
- `levy_value` is the training label only. It is never an input and is not computed for live rows.

## 4a. Parameters (model inputs and what they do)

Units: PKR = Pakistani rupees; bn = billion. All rows are fortnightly.

| Parameter | Unit | Meaning | Role in the model |
|---|---|---|---|
| `forecast_crude_price` | USD/bbl | Mean of Module 1's Brent forecast for the next 10 trading days (no look-ahead). The only Module 1 -> Module 2 link. | Label rule: `high_crude` (> 80). Also a tree feature. |
| `retail_petrol_price` | PKR/litre | Latest weekly Pakistan pump price on or before the period end. Already includes the levy. | Label rule: only through `affordability_index`. Also a tree feature. |
| `affordability_index` | index | Minimum wage buys how many 40-litre tanks, adjusted by CPI (formula in section 4). Higher = fuel is more affordable. | Label rule: `stress` (below the 40th percentile). Main driver of the multiplier. |
| `levy_revenue_target_pkr_bn` | PKR bn per fortnight | Revenue the levy is meant to collect in the fortnight (the fiscal/IMF target). Constant within a fiscal year. | Label rule: sets `required_levy`. Tree feature. |
| `levied_volume_bn_litres` | bn litres per fortnight | Estimated petrol + diesel sold in the fortnight. | Label rule: sets `required_levy`. Tree feature. |
| `required_levy` | PKR/litre | `target / volume`: the levy that would exactly hit the target. Derived, never stored in the CSV. | Scale of the output: `levy = required_levy x multiplier`. Tree feature. |
| `current_levy` | PKR/litre | Total levy in force (petroleum levy + climate levy). | Not in the label rule. Tree feature and the "status quo" baseline. |
| `fiscal_gap_pkr_bn` | PKR bn per fortnight | Shortfall if the levy stays at `current_levy`: `target - current_levy x volume`. Positive = under-collecting, negative = over-collecting. Equals `volume x (required_levy - current_levy)`. | Not in the label rule and fully derived from target, volume and current_levy, so it adds no new information. Tree feature (background). |
| `multiplier` | ratio | Relief factor applied to `required_levy` (0.80 / 0.92 / 0.95 / 1.00). This is what the tree learns. | Prediction target. |
| `levy_value` | PKR/litre | `clip(round(required_levy x multiplier, 1), 30, 85)`. Synthetic label. | Training label only; never an input. |

Why each is included:
- Rule drivers (`forecast_crude_price`, `affordability_index`, `levy_revenue_target_pkr_bn`, `levied_volume_bn_litres`): they define the label, so the tree should recover them.
- Context columns (`retail_petrol_price`, `current_levy`, `fiscal_gap_pkr_bn`): kept as plausible policy inputs, but they do not enter the rule. They can only help by proxying a rule driver and can mislead the tree. Example: the 71-row run splits on `fiscal_gap_pkr_bn <= 0.475`, which is not part of the rule.
- Comparing all columns against only the four rule drivers is a planned check (`RULE_FEATURES_ONLY`).

## 5. How one input row is built (`LevyAdvisor`)

`LevyAdvisor` reads no raw files. Everything except the crude price is carried from a training row.

| Column | Live mode | Backtest mode |
|---|---|---|
| `forecast_crude_price` | `--crude` value from Module 1 | Stored value in the target row |
| `retail_petrol_price`, `current_levy`, `affordability_index`, `levy_revenue_target_pkr_bn`, `levied_volume_bn_litres`, `fiscal_gap_pkr_bn` | Carried from the last row of `levy_training.csv` | Carried from the last row before the chosen date |
| `required_levy` | target / volume (calculated) | Same |

- Live period start = the fortnight after the last row's `period_start` (a start on the 1st gives the 16th of that month; a start on the 16th gives the 1st of the next month). With the 16-31 Oct row present, live mode recommends for 2026-11-01.
- The latest completed fortnight's retail price and affordability are the best information available when predicting the next one. If a newer weekly price or CPI month arrives, append a row (section 7).
- Target and wage are fixed per fiscal year (changes on 1 July); update by hand when the new target and budget wage are known.
- Volume is carried from the latest row, which is a real uncertainty (section 8).

## 6. Modes and commands

Build (from `src/`, never compile one file alone): `g++ -std=c++17 -O2 RegressionTree.cpp LevyAdvisor.cpp -o tree_test.exe`

- `.\tree_test.exe` runs the default experiment report.
- `.\tree_test.exe live --crude <value>` recommends the levy for the next fortnight. It will be the dashboard's main feature.
- `.\tree_test.exe backtest YYYY-MM-DD` uses only earlier rows and compares with the stored row. The date must be a `period_start` in the CSV and have at least 10 earlier rows.
- None of these modes writes to any CSV. Appending live rows is deliberately not done: they have no real label.

**Checks that must pass after any data or code change**
- `live`: the printed period start is the fortnight after the last CSV row.
- `backtest <date>`: "Training samples" equals the number of rows before that date.
- In both: required levy = carried target / carried volume, and recommended levy = required levy x predicted multiplier.
- Record new reference numbers from a fresh run; older figures from the synthetic dataset no longer apply.

## 7. Updating the data (manual, in this order)

Nothing in the C++ code appends rows or runs the scripts. When a new fortnight is added:

1. Add the new row to `data/processed/levy_training.csv` with its inputs (weekly retail price, notified levy, CPI month, wage, carried target, volume by the Data_Readme method, fiscal gap). Put any placeholder in `forecast_crude_price`; it is replaced in step 3. Keep a backup first.
2. Module 1 owner extends `data/processed/brent_daily.csv` and appends the forecast for that `period_start` to `data/processed/forecast_col.csv` (only data before the period start).
3. From the repo root: `python scripts/regenerate_labels.py`. It checks that `period_start` values match one-to-one with `forecast_col.csv`, replaces `forecast_crude_price`, recomputes the stress cut-off and `levy_value` for all rows, and rewrites the CSV. It needs `pandas` and `numpy`.
4. Rebuild if code changed, then rerun `tree_test.exe`, `live` and `backtest`; update the numbers in the report.
5. Update `data/Data_Readme.md` (row count, end date, cut-off, sources of the new row).

Delete `.bak` and `levy_training_placeholder_crude.csv` once the new CSV is verified and committed. The script overwrites `levy_training_placeholder_crude.csv` with the current file on every run, so it is a copy of the previous state, not the old placeholder data.

History: the first regeneration (synthetic data, 27.907 cut-off) changed 13 rows; walk-forward RMSE went from 2.740 to 1.262.

## 8. Limitations to state in the report

- The label is a rule we wrote, so a high score means the tree recovered our rule, not that it predicts real levies.
- The training file is mixed-provenance (see `Data_Readme.md`): revenue targets, the levy schedule and crude forecasts are inherited project inputs that need source verification; volume allocation, fiscal gap and labels are calculated or estimated.
- The recommendation hinges on the crude forecast crossing 80 USD/bbl; a forecast just below or above flips the multiplier and the band.
- Next fortnight's volume, retail price and affordability are unknown; carrying the latest row adds error. Backtests inherit the same effect.
- Only about 70 rows and 14 test rows: small differences between models mean nothing.
- Retail price already contains the levy, so affordability is partly circular.
- Adding rows moves the 40th-percentile cut-off and can change earlier labels.

## 9. File map and ownership

| Path | Owner |
|---|---|
| `src/arima.*`, Brent data, `data/processed/forecast_col.csv`, generator tool | Module 1 |
| `src/RegressionTree.*`, `src/LevyAdvisor.*`, `data/processed/levy_training.csv`, `data/Data_Readme.md`, `scripts/regenerate_labels.py`, source files for the levy columns | Module 2 |
| `frontend/` (not yet connected; remove retail price and USD/PKR from `FEATURES` if not shown) | Frontend pair |
