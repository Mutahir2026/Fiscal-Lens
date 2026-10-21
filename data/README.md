# DSA Project Data — README

## Read first
**None of this data was downloaded from a real source.** The workspace that built it could not reach FRED, EIA, Stooq or Yahoo Finance, so every daily value is simulated. It lets us build and test the C++ code now. Replace it with real data before reporting results (see "Before submission").

## Files
| File | Used by | Purpose |
|---|---|---|
| `processed/commodities_daily.csv` | Module 1 | 10 commodities, one column each (`brent, wti, henry_hub, gold, silver, copper, platinum, gasoline, wheat, cotton`). 783 weekday rows, 2023-10-18 to 2026-10-16. |
| `processed/<name>_daily.csv` | Module 1 | Same data, one file per commodity (`date,price`). |
| `processed/levy_training.csv` | Module 2 | The only file the decision tree reads. 70 fortnightly rows, 2023-11-16 to 2026-10-15. |
| `raw/*.csv` | None directly | Inputs used to compute `levy_training.csv` columns: `usd_pkr_daily`, `cpi_and_income_monthly`, `petrol_price_buildup_fortnightly`. |

Units: Brent/WTI USD/bbl; Henry Hub USD/MMBtu; gold/silver/platinum USD/oz; copper USD/tonne; gasoline USD/gal; wheat USD/bu; cotton US cents/lb.

## How the two modules connect
```
brent column (commodities_daily.csv)
        │  Module 1: ARI + Levinson-Durbin, fit on data before the fortnight starts
        ▼
mean forecast of daily Brent over the next fortnight (10 trading days)
        │  = forecast_crude_price (USD/bbl)
        ▼
levy_training.csv row:  forecast_crude_price + usd_pkr + affordability_index
                        + fiscal target/gap + current_levy + retail price + volume
        │  Module 2: regression tree (levy_value) / classification tree (levy_band)
        ▼
recommended levy (PKR/litre) or band (LOW / MEDIUM / HIGH)
```
- Only **Brent** feeds Module 2. The other nine commodities are for the Trend Tracker dashboard only.
- **Definition:** `forecast_crude_price` = the mean of Module 1's forecasted daily Brent prices for the next 10 trading days, made using only data **before** `period_start` (no look-ahead).
- **Current file:** the column is a placeholder (mean of the last 10 trading days of Brent plus half the change from the 10 days before). Replace it once Module 1 works.
- **To replace it:** for each of the 70 rows, refit the ARI model on Brent up to the day before `period_start`, forecast 10 days ahead, average the forecast, and overwrite the column.
- **Then regenerate the labels.** `levy_value` and `levy_band` depend on this column through the rule below. If you replace the forecasts but keep the old labels, features and labels no longer match.
- **Live use (dashboard):** run the same step on the latest Brent data, add the current USD/PKR, affordability, fiscal gap, current levy, retail price and volume, and pass the row to the tree.
- **Build order:** Module 1 first, then regenerate `forecast_crude_price` and the labels, then Module 2.

## Label rule (synthetic by design)
`levy_value` and `levy_band` were written by a plain if/else formula in a Python script. They are **not** model outputs. They are the answer keys for our decision tree to learn from. The tree itself is what we write in C++.
```
required_levy = levy_revenue_target_pkr_bn / levied_volume_bn_litres
stress        = affordability_index < 27.907   (40th percentile of the 70 rows)
high_crude    = forecast_crude_price > 80
multiplier    = 0.80 if stress and high_crude
                0.92 if stress only
                0.95 if high_crude only
                1.00 otherwise
levy_value    = clip(round(required_levy * multiplier, 1), 30, 85)   # PKR/litre
levy_band     = LOW if levy_value < 55; MEDIUM if < 70; else HIGH
```
The tree will learn this rule back. The report must say so openly. `current_levy` is the levy applied in the previous fortnight and is an **input**. `levy_value` is what the rule recommends now and is the **target**.

## Module 1 — forecasting (per commodity)
1. Load `date` + one price column.
2. Split chronologically: first 80% train, last 20% test. Never shuffle.
3. Difference the training prices once (the "I" in ARI).
4. Compute autocovariances `r0..r_pmax` of the differenced series.
5. Run Levinson-Durbin once to `p_max` to get coefficients and error variance for every order.
6. Pick the order with the lowest AIC or BIC.
7. Forecast differences with the AR recurrence, then add them up from the last known price.
8. Score on the test set (MAE/RMSE) against the naive baseline "tomorrow = today".
9. Benchmark `NaiveARSolver` vs Levinson-Durbin across `p` and `n`. Check the coefficients match, then compare timings.

The complexity benchmark (9) stays valid on simulated data. The accuracy numbers (8) do not.

## Module 2 — regression tree (`levy_training.csv`)
1. Features (X): `forecast_crude_price, usd_pkr, affordability_index, levy_revenue_target_pkr_bn, fiscal_gap_pkr_bn, current_levy, retail_petrol_price, levied_volume_bn_litres`. Do not use `period_start` / `period_end` as features.
2. Label (y): `levy_value`. **Never** put `levy_value` or `levy_band` in X.
3. Split: first 56 rows train, last 14 test.
4. Build: at each node sort each feature, try midpoints as thresholds, take the split with the lowest weighted variance (SSE), and recurse. A leaf predicts the mean of y. Use a depth limit of about 4–5 and a minimum leaf size of about 3.
5. Predict by walking down to a leaf. Report MAE/RMSE, and band accuracy using the 55/70 cut-offs.
6. Print the decision paths for the report.
7. Optional classification tree: same features, label `levy_band`, Gini impurity, `levy_value` excluded.

Caveat: the FY27 rows (from Jul 2026) have fiscal target and volume values never seen in training, and a tree cannot extrapolate. I expect the time-based split to score noticeably worse than a shuffled one (not yet run). Report the time-split result as the honest one. Use a shuffled split only to check that the tree recovers the rule.

## What is real, approximate or fabricated
| Data | Status |
|---|---|
| All commodity daily prices | **Simulated.** |
| Monthly price levels, Oct 2023 – Dec 2025 | Approximate recollections of real monthly levels. **Not verified or downloaded.** |
| Monthly price levels, Jan – Oct 2026 | **Hypothetical.** The only loose references were Brent ~$92 (20 Aug 2026) and gold ~$4,200 / silver ~$61 (1 Oct 2026) from low-reliability pages. |
| Day-to-day moves, volatility, correlations, WTI–Brent gap | **Generated** to fill the gaps between monthly levels. |
| Weekday dates | Include exchange holidays that real markets skip. |
| `usd_pkr_daily.csv` | Simulated within a 278–285 band. 2026 is hypothetical. |
| CPI | Year-on-year figures approximate to Dec 2025, hypothetical for 2026. The index is derived. |
| Minimum wage | 32k / 37k / 40k approximate. 42k from Jul 2026 is hypothetical. |
| Petrol price build-up | **Model-implied, not OGRA prices.** Real: the price structure, fortnightly pricing on the 1st and 16th, and GST = 0 on petrol. Approximate: margins and levy / carbon-levy steps. Fabricated: IFEM values and all computed price figures. |
| IMF levy targets | FY24–FY26 approximate recollections. FY27 (1,560 bn) is hypothetical. |
| `levied_volume_bn_litres` | Simulated. |
| `affordability_index` | Our own arbitrary definition: (min wage ÷ (40 L × retail price)) × (100 ÷ CPI index) × 10. |
| `forecast_crude_price` | Placeholder until Module 1 is ready. |
| `levy_value`, `levy_band` | **Synthetic**, from the rule above. |

## Before submission
- Replace Module 1 data with real downloads and keep the same column names: EIA or FRED (`DCOILBRENTEU`, `DCOILWTICO`, `DHHNGSP`) for oil and gas; Stooq, Yahoo Finance or Kaggle (`GC=F, SI=F, PL=F, HG=F, ZW=F, CT=F, RB=F`) for the rest.
- Get real USD/PKR from the State Bank of Pakistan, CPI from PBS, petrol prices from OGRA notifications, and levy targets from IMF staff reports.
- The levy label stays synthetic regardless. State it in the report.
