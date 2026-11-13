# Fiscal Lens — Codebase and Data Report

Updated 12 Nov 2026. The LevyAdvisor, levy data, CLI and integration sections reflect the current state. Other sections are carried over from the 9 Nov review and were not re-verified.

## Snapshot

Fiscal Lens is a two-module DSA/ML project:

1. **Commodity Trend Tracker** forecasts Brent crude prices with an ARI-style model (log transform, first differencing, autoregression).
2. **Petroleum Levy Advisor** uses a hand-built C++ regression tree to recommend a Pakistan petroleum levy from crude, affordability, fiscal, and pricing inputs.

The repository contains C++ source, static frontend assets, CSV data, documentation, and compiled Windows executables. There is no build configuration, package manifest, server, database, or automated test suite.

## Structure and responsibilities

| Location | Contents and role |
|---|---|
| `src/` | C++ implementation of the AR model, regression tree, levy advisor (live/backtest), forecast-column generator, and `tree_test.exe`. |
| `data/raw/` | Source-style input files. Contains the uncleaned PDFs and CSVs used in the data processing pipeline. |
| `data/processed/` | Backend-consumable cleaned Brent data, the real-data levy training file (`levy_training.csv`), and the generated forecast column (`forecast_col.csv`). Backup copies (`*.bak`, `levy_training_placeholder_crude.csv`) may appear here after script runs and should be deleted once verified. |
| `scripts/regenerate_labels.py` | Merges `forecast_col.csv` into the levy file and recomputes `levy_value` for all rows. Run manually. |
| `scripts/clean_brent.py` | Cleans the raw data and prepares it for processing. |
| `frontend/` | Static HTML/CSS/JavaScript dashboard and visual assets. |
| `docs/`, `data/Data_Readme.md` | Design notes, documentation relating to the architecture & working,the original blueprint, and column-level data provenance. |
    
## C++ backend

### Commodity Trend Tracker

| File | Purpose |
|---|---|
| `src/arima.h`, `src/arima.cpp` | Defines `ARIMA`, CSV loading, and `forecastMean`. Prices are logged and first-differenced; AR coefficients are estimated with Yule–Walker autocovariances and Levinson–Durbin recursion. Order `p` is chosen from `1..maxP` by rolling-window RMSE, then forecasts are recursively integrated and exponentiated. Defaults: `maxP=10`, 30-day validation horizon, 7-day validation step, 700-row initial window; `forecastMean` uses a 10-day horizon and a 250-row minimum. |
| `src/gen_forecast_col.cpp` | Reads cleaned Brent rows, takes only rows before each levy period, calls `forecastMean(..., 10)`, and writes `data/processed/forecast_col.csv`. |

The AR implementation treats invalid/non-positive CSV prices as unusable. Autocovariance calculation is O(n p); Levinson–Durbin is O(p^2) after autocovariances are available. The current order-selection strategy is rolling validation, not the AIC/BIC approach proposed in the blueprint.

### Petroleum Levy Advisor

| File | Purpose |
|---|---|
| `src/RegressionTree.h`, `src/RegressionTree.cpp` | Implements a CART-like regression tree from scratch. Each node searches every feature by sorting candidate values and minimizing child SSE with running sums; leaves emit their mean target. Includes CSV dataset loading, RMSE/MAE/MAPE/R² evaluation, chronological train/test evaluation, walk-forward hyperparameter selection, baselines, tree printing, and CLI dispatch. |
| `src/LevyAdvisor.h`, `src/LevyAdvisor.cpp` | Implements the `live` and `backtest` modes. It reads no raw files: every input except the crude price is carried from a row of `levy_training.csv` (the last row for live, the last row before the date for backtest). Live period start is computed from the last row's `period_start`. |

`RegressionTree` defaults to depth 4, minimum split size 6, and minimum leaf size 3. The default experiment searches depths 3–6 and minimum leaf sizes 8, 5, and 3 on the oldest 80% of data and reserves the newest 20% for testing. A node split costs O(f n log n) for f features and n node samples, because each feature is sorted.

The learned target is normally a **multiplier** rather than the levy directly:

```text
required_levy = levy_revenue_target_pkr_bn / levied_volume_bn_litres
target_multiplier = levy_value / required_levy
recommended_levy = predicted_multiplier × required_levy
```

Results reported in earlier reviews (test RMSE 0.671, 12/14 band matches) came from the previous dataset and are out of date. Rerun `tree_test.exe` on the current `levy_training.csv` and record fresh numbers. Whatever they are, they are rule-recovery results: the labels are synthetic and rule-based, not evidence of real-world forecasting accuracy.

### CLI

From `src/`, the documented build for the levy executable is:

```powershell
g++ -std=c++17 -O2 RegressionTree.cpp LevyAdvisor.cpp -o tree_test.exe
```

| Command | Behavior |
|---|---|
| `tree_test.exe` | Runs the chronological levy experiment, validation, baselines, and printed decision tree. |
| `tree_test.exe live --crude <positive-number>` | Trains on all levy rows and recommends for the fortnight after the last CSV row. The crude input is manual. |
| `tree_test.exe backtest YYYY-MM-DD` | Trains only on earlier rows, carries inputs from the previous row, uses the stored crude for that date, and compares the recommendation with the stored label. Needs at least 10 earlier rows. |

## Frontend

`frontend/index.html` is a single-page, responsive dashboard with Overview, Commodity Tracker, Levy Advisor, and Benchmarks sections. It references `styles.css`, then `app.js` and an expected `data.js`.

| File/group | Role |
|---|---|
| `frontend/app.js` | Renders reference cards and pump-price build-up from global `PREDICTIONS`/`HISTORY`, persists light/dark theme, updates navigation, animates the levy recommendation steps, and adds reveal/cursor-glow effects. |
| `frontend/base.css` | Theme variables, reset, typography, and shared base rules. |
| `frontend/layout.css` | Shell, sidebar, section, responsive layout, and page geometry. |
| `frontend/components.css` | Buttons, cards, charts, controls, price build-up, animation, and component styles. |
| `frontend/styles.css` | Imports the three CSS layers and applies the custom cursor on fine pointers. |
| `frontend/assets/` | Hero/decorative imagery, cursor, icons, and font files. |

The frontend is not connected to the backend and no changes are planned until it is. As of the 9 Nov review, `data.js` (which must define `PREDICTIONS` and `HISTORY`) was referenced but absent, the Tracker's "Run forecast" button had no handler, and the Levy button only animated supplied values. No C++→browser bridge (WASM or API) is implemented.

## Data catalogue

All CSVs are comma-delimited and have headers.

| File | Coverage | Columns and meaning | Current use |
|---|---|---|---|
| `data/raw/brent_daily.csv` | 1,784 rows, 2020-01-02–2026-11-03 (53 missing/non-numeric entries) | `observation_date`; `DCOILBRENTEU` (Brent USD/bbl) | Raw source series. |
| `data/processed/brent_daily.csv` | Cleaned Brent series (783 rows to 2026-10-16 at the 9 Nov review; being extended to 2026-10-31) | `date`; `price` | Read by `gen_forecast_col.cpp`. |
| `data/processed/levy_training.csv` | 70 fortnightly rows, 2023-11-16 to 2026-10-15; a 2026-10-16 to 2026-10-31 row is being added | Dates; crude forecast (Module 1); affordability; revenue target; fiscal gap; current levy; retail price; levied volume; `levy_value` | Primary tree-training data. Real-data, mixed-provenance version; see `Data_Readme.md`. |
| `data/processed/forecast_col.csv` | One row per levy period | `period_start`; `forecast_crude_price` | Generator output merged into `levy_training.csv` by `scripts/regenerate_labels.py`. |

The eight active tree features are `forecast_crude_price`, `affordability_index`, `levy_revenue_target_pkr_bn`, `fiscal_gap_pkr_bn`, `current_levy`, `retail_petrol_price`, `levied_volume_bn_litres`, and derived `required_levy`. Data ranges and band counts should be re-read from the current file; the figures in earlier reviews described the synthetic dataset.

## Documentation

| File | Content |
|---|---|
| `docs/working.md` | Current operating design: data flow, formulas, labels, CLI modes, data-update procedure, limitations, ownership. |
| `data/Data_Readme.md` | Column-level provenance, source list, formulas, estimates, carry-forwards, and limitations for the real-data levy file. |
| `docs/dsa-project-blueprint.md` | Original course/project proposal: scope, stack alternatives, source ideas, algorithms, complexity justification, and planned structure. |

## Current integration and maintenance gaps

1. Data updates are manual: nothing in the C++ code appends rows or runs `regenerate_labels.py` or the forecast generator (see `docs/working.md`, section 7). `regenerate_labels.py` also overwrites `levy_training_placeholder_crude.csv` with a copy of the current file on every run.
2. The 2026-10-16 to 2026-10-31 row is pending; until it is added, live mode recommends for the fortnight starting 2026-10-16, not 2026-11-01.
3. `LevyAdvisor` cannot call `forecastMean` directly; live mode requires `--crude`. The requested backend-to-frontend mechanism is likewise absent.
4. The training loader requires rows to be chronologically sorted and requires the named primary columns. It accepts CSV or TSV, but does not handle quoted delimiters.
5. Levy labels are intentionally synthetic. Small sample size, carried-forward volume and affordability for live/backtest rows, threshold sensitivity around crude price 80, and retail-price circularity limit any policy interpretation.
6. `Data_Readme.md` is the authoritative disclosure of which levy-file columns are observed, calculated, estimated, or carried forward.
7. The repo README is not yet maintained.

## Practical state

The levy-model command-line demo, including `live` and `backtest`, no longer depends on the removed raw files. The commodity forecaster and forecast-column generator are connected to the levy data through `forecast_col.csv`. The browser interface is not yet connected to any backend output.
