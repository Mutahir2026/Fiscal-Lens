# Data provenance for `levy_training_real.csv`

## Bottom line

`levy_training.csv` is a currently a **mixed-provenance training dataset**. It contains direct observations from the supplied CPI and petrol-price files, calculated measures based on those observations, and several carried-forward or estimated inputs required by the existing project schema. It is not a fully audited administrative dataset, and its `levy_value` is a rule-generated training label rather than an observed policy outcome.

The table below is the definitive classification of every column.

| Column | Classification | How it was obtained | Can it be described as observed real-world data? |
|---|---|---|---|
| `period_start`, `period_end` | Carried forward / appended | Originally copied from `data/processed/levy_training.csv` (70 fortnights) and extended with 1 appended fortnight (2026-10-16 to 2026-10-31), totaling 71 periods. | Calendar dates are real; they are not observations from a supplied report. |
| `forecast_crude_price` | Pre-existing model output | Matched by `period_start` from `data/processed/forecast_col.csv`. | Not a market observation. It is a forecast derived earlier by the project's Brent model. Its upstream provenance was not independently audited in this task. |
| `affordability_index` | Calculated | Calculated from the supplied CPI file, supplied wage-history image, and supplied petrol prices. | No - it is a constructed indicator, but its inputs and formula are explicit. |
| `levy_revenue_target_pkr_bn` | Carried forward / unverified | Copied as fiscal-year constants from the existing training file. | No. No source document for these targets was supplied or independently verified. |
| `fiscal_gap_pkr_bn` | Calculated estimate | Target minus levy multiplied by the estimated levied volume. | No. It is not a reported government fiscal balance. |
| `current_levy` | Carried forward / unverified / notified | Retained as the existing rate schedule (60.0, 70.0, 77.5, 83.0 PKR/litre), with the new 2026-10-16 fortnight using 85.0 PKR/litre (notified petroleum levy 80.0 + climate levy 5.0). | No. Earlier rates were not verified against notified government price build-ups in this task; the 85.0 rate reflects notified 80 + 5. |
| `retail_petrol_price` | Direct observation, aligned to fortnight | Pakistan value from the supplied weekly petrol-price file; the latest observation on or before each fortnight end was used. | Yes, subject to the source file's own methodology. |
| `levied_volume_bn_litres` | Estimated from reported sales | Annual or quarterly OCAC MS and HSD sales were converted to litres and allocated evenly across calendar days. | The reported tonnes are source data; the conversion, allocation, and 2026 annualisation are estimates. |
| `levy_value` | Synthetic label | Recomputed with the rule in `docs/working.md`. | No. It is not an observed petroleum levy. |

## Sources used

### Direct data sources

- `data/raw/Consumer Price Index (CPI).csv`
  - Monthly national CPI year-on-year and month-on-month changes.
  - The month-on-month series is used to reconstruct a normalized CPI index.

- `data/raw/Petrol Price.csv`
  - Weekly city-level prices and a `Pakistan` national value.
  - The national value is used as `retail_petrol_price`.

- `data/raw/Import-Export-2026-27-Jul-Sep.pdf`
  - FY 2026-27 import and export data for the specified period.

- `data/raw/Jul-2023-Jun-2024-ENE-Pak.pdf`
  - FY 2023-24 internal-sales totals: MS 7,145,004 tonnes; HSD 6,234,221 tonnes.

- `data/raw/Jul-Jun-2024-25-ENE-Pak-1.pdf-1.pdf`
  - FY 2024-25 internal-sales totals: MS 7,603,246 tonnes; HSD 6,858,461 tonnes.

- `data/raw/Jul-Jun-2025-26-ENE-Pak-2.pdf`
  - FY 2025-26 internal-sales totals: MS 7,840,129 tonnes; HSD 7,056,746 tonnes.

- `data/raw/Jul-Sep-2026-27-ENE-Pak.pdf`
  - Jul-Sep 2026 internal-sales totals: MS 2,043,354 tonnes; HSD 1,569,890 tonnes.

- `data/raw/MinWageHistory.png`
  - Wage schedule transcribed for the affordability calculation: 32,000 PKR/month in FY 2023-24, 37,000 in FY 2024-25, 40,000 in FY 2025-26, and 40,700 in FY 2026-27.
  - The image distinguishes federal and provincial rates in later years. The 40,000 FY 2025-26 value is a provincial reference rate; 40,700 for FY 2026-27 is the federal figure shown. This is a modelling choice, not a single national statutory series.

### Project files used as definitions or carry-forward sources

- `data/processed/levy_training.csv`
  - Defines the required schema, the 71 period dates (through 2026-10-31), revenue-target constants, and current-levy schedule.

- `data/processed/forecast_col.csv`
  - Provides the existing model-generated crude forecast for each exact period.

- `docs/working.md`
  - Defines the affordability, required-levy, multiplier, label, and fiscal-gap relationships.

- `scripts/regenerate_labels.py`
  - Confirms the project label-generation logic and the 40th-percentile stress threshold.

### Reviewed but not used as direct inputs

- `Import-Export-2023-24-Jul-Jun.pdf`
- `Import-Export-2024-25-Jul-Jun-2.pdf`
- `Import-Export-2025-26-Jul-Jun.pdf`

These reports contain monthly import/export quantities, not a crude-price series, statutory levy rate, fiscal target, or reported fiscal gap. They were therefore not used to populate a column in the final CSV.

## Alignment and transformations

### Periods and petrol prices

The output contains 71 fortnightly periods from 2023-11-16 through 2026-10-31. For each row, the petrol price is:

```text
latest Pakistan weekly price with Period End <= period_end
```

This is an alignment convention. It uses an actual weekly observation, but it is not necessarily the retail price known at the *start* of the fortnight.

For the appended fortnight (2026-10-16 to 2026-10-31), the retail petrol price is 379.34 PKR/litre, which is the latest Pakistan weekly observation on or before 2026-10-31 (observed on 29 Oct 2026; preceding weekly values were 395.3 on 15 Oct, 380.74 on 22 Oct, and 379.34 on 29 Oct).

### CPI index reconstruction

The supplied CPI file contains monthly change rates rather than a price-level index. A normalized index was rebuilt in chronological order:

```text
CPI_index[t] = CPI_index[t-1] x (1 + month_over_month[t])
```

The reconstructed series is normalized to 100.0 in October 2023:

```text
CPI_index[Oct-2023] = 100
```

This normalizes the supplied month-on-month CPI information; it does not claim to reproduce an official base-year CPI index.

For the appended fortnight row ending 2026-10-31, the Oct 2026 MoM change of 0.011 from the supplied CPI file is chained to yield a normalized CPI index of 127.868.

### Affordability index

For each row, the wage reference is selected by fiscal year (July-June), then:

```text
affordability_index = min_wage_pkr_month
                      / (40 x retail_petrol_price)
                      x (100 / cpi_index)
                      x 10
```

This formula comes directly from `docs/working.md`.

### Volume estimate

The energy reports give MS and HSD in metric tonnes. They were converted using the following density assumptions:

```text
MS litres  = MS tonnes  x 1,000 / 0.745
HSD litres = HSD tonnes x 1,000 / 0.832
```

For FY 2023-24 to FY 2025-26, the resulting annual total is spread uniformly over each fiscal year's calendar days. Each fortnight receives its inclusive day count divided by 365 (366 for FY 2023-24), then is expressed in billions of litres.

For FY 2026-27, only the Jul-Sep report was available. Its quarterly MS and HSD totals were multiplied by four before the same daily allocation. This is an annualisation assumption, not an observed full-year volume.

```text
levied_volume_bn_litres = (annual MS litres + annual HSD litres)
                           x inclusive_period_days / fiscal_year_days
                           / 1,000,000,000
```

The output rounds this field to three decimal places. Calculations used more precision before display rounding.

### Fiscal gap

The file's fiscal gap is deliberately a constructed gap, consistent with the project dataset's relationship:

```text
fiscal_gap_pkr_bn = levy_revenue_target_pkr_bn
                    - (current_levy x levied_volume_bn_litres)
```

Because volume is displayed to three decimals after the calculation, recomputing the gap from the CSV can differ by up to 0.03 PKR billion.

### Training label

The label is not historical truth. It follows the project's rule exactly:

```text
required_levy = levy_revenue_target_pkr_bn / levied_volume_bn_litres

stress     = affordability_index < 40th_percentile(affordability_index)
high_crude = forecast_crude_price > 80

multiplier = 0.80  if stress and high_crude
           = 0.92  if stress only
           = 0.95  if high_crude only
           = 1.00  otherwise

levy_value = clip(round(required_levy x multiplier, 1), 30, 85)
```

The calculated 40th-percentile affordability cut-off recomputed over all 71 rows is 28.7910 (previously 29.2176 over 70 rows).

## 71st row provenance (2026-10-16 to 2026-10-31)

The appended 71st row reflects the following sources and classification of inputs:

- `period_start` (2026-10-16) and `period_end` (2026-10-31): Fortnightly calendar dates.
- `forecast_crude_price` (126.61): Pre-existing model forecast matched by `period_start` from `data/processed/forecast_col.csv` (Module 1).
- `retail_petrol_price` (379.34): Direct observation, latest Pakistan weekly price on or before 2026-10-31 (the 29 Oct 2026 observation; preceding weekly prices were 395.3 on 15 Oct, 380.74 on 22 Oct, and 379.34 on 29 Oct).
- `current_levy` (85.0): Notified petroleum levy 80.0 + climate levy 5.0 PKR/litre.
- `levy_revenue_target_pkr_bn` (65.0): Carried forward FY2026-27 constant from the preceding row.
- `levied_volume_bn_litres` (0.812): Estimated volume using the documented annualised FY2026-27 OCAC sales method: (annual MS litres + annual HSD litres) x 16 days / 365 / 1e9 = 0.811773 bn litres, rounded to 0.812.
- `fiscal_gap_pkr_bn` (-4.0): Calculated estimate as target - current_levy x volume = 65.0 - 85.0 x 0.811773 = -4.00 PKR bn.
- `affordability_index` (20.977): Calculated as min_wage / (40 x retail_petrol_price) x (100 / cpi_index) x 10, using federal min_wage = 40,700 PKR/month and Oct 2026 chained CPI index = 127.868.
- `levy_value` (64.0): Synthetic training label generated by `scripts/regenerate_labels.py` (with stress=True since 20.977 < 28.791, high_crude=True since 126.61 > 80, multiplier=0.80, required_levy=65.0/0.812=80.049, clipped product=64.0).

## What is carried forward or assumed

The following values should be replaced before making real-world policy claims or presenting the model as an operational forecast:

1. `levy_revenue_target_pkr_bn` - fiscal-year constants inherited from the old training file; no supporting budget or IMF document was supplied.
2. `current_levy` - inherited rate schedule; not verified from official fortnightly price notifications.
3. `forecast_crude_price` - an existing model forecast series, not spot/settlement crude prices. It is treated as previously vetted only because the project already contained it.
4. The density constants, uniform daily volume allocation, and FY 2026-27 annualisation - all necessary estimation choices.
5. The single wage reference chosen from an image that reports both federal and provincial values.
6. `fiscal_gap_pkr_bn` and `levy_value` - calculated model fields, not government-reported observations.

## Appropriate description of the file

Use the following wording:

> A mixed-source, partially real, fortnightly training dataset. Retail petrol prices, CPI changes, and reported fuel-sales totals come from supplied source files. Affordability, volume allocation, fiscal gap, and levy label are reproducible calculations. Revenue targets, levy rates, and crude forecasts are inherited project inputs that still require source verification.

Do **not** describe the file as a fully real, fully official, or fully observed levy dataset.

## How to make it fully auditable

Replace the inherited and estimated parts with dated primary-source series:

1. Official fortnightly OGRA / Finance Division petroleum-product price notifications for the levy and price build-up.
2. Budget documents or IMF programme tables for fiscal-year petroleum-levy collection targets.
3. A documented Brent source and an archived, no-look-ahead forecast-generation run for every row.
4. Monthly petroleum-product sales, preferably MS and HSD litres, rather than annual tonnes allocated uniformly.
5. A declared wage jurisdiction or a national statutory wage series.
6. A reported fiscal balance measure if `fiscal_gap_pkr_bn` is intended to describe the government budget rather than a modelled levy shortfall.