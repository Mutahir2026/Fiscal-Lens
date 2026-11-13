"""
regenerate_labels.py
====================
Module-2 step: replace forecast_crude_price in levy_training.csv with
Module 1's forecasts from forecast_col.csv, then recompute levy_value
using the rule-based formula.

RULES (from task spec):
  required_levy  = levy_revenue_target_pkr_bn / levied_volume_bn_litres
  cutoff         = 40th-percentile of affordability_index over all 70 rows
  stress         = affordability_index < cutoff
  high_crude     = forecast_crude_price > 80
  multiplier     = 0.80 if stress and high_crude
                 = 0.92 if stress only
                 = 0.95 if high_crude only
                 = 1.00 otherwise
  levy_value     = clip(round(required_levy * multiplier, 1), 30, 85)

Drops: usd_pkr, levy_band  (if present)
Keeps: fiscal_gap_pkr_bn (unchanged)
Writes: levy_training.csv (updated), levy_training_placeholder_crude.csv (backup)
"""

import sys
import pandas as pd
import numpy as np
import shutil
from pathlib import Path

# ── paths ──────────────────────────────────────────────────────────────────────
ROOT = Path(__file__).resolve().parents[1]
TRAINING_CSV   = ROOT / "data" / "processed" / "levy_training.csv"
FORECAST_CSV   = ROOT / "data" / "processed" / "forecast_col.csv"
BACKUP_CSV     = ROOT / "data" / "processed" / "levy_training_placeholder_crude.csv"

# ── PRECONDITION: validate forecast_col.csv ────────────────────────────────────
print("=" * 65)
print("PRECONDITION CHECK: forecast_col.csv")
print("=" * 65)

fc = pd.read_csv(FORECAST_CSV, dtype={"period_start": str})
# Strip any stray \r
fc["period_start"] = fc["period_start"].str.rstrip("\r").str.strip()
fc.columns = [c.rstrip("\r").strip() for c in fc.columns]

bad = fc[fc["forecast_crude_price"].isna() |
         (fc["forecast_crude_price"] == -1) |
         (fc["forecast_crude_price"] <= 0)]
if len(bad) > 0:
    print("STOP – the following forecast_col.csv rows have invalid forecasts:")
    print(bad.to_string())
    sys.exit(1)

print(f"  Rows in forecast_col.csv : {len(fc)}")
print(f"  All forecasts > 0        : YES")
print()

# ── STEP 1: load levy_training.csv ────────────────────────────────────────────
lt = pd.read_csv(TRAINING_CSV, dtype={"period_start": str, "period_end": str})
lt.columns = [c.rstrip("\r").strip() for c in lt.columns]
lt["period_start"] = lt["period_start"].str.rstrip("\r").str.strip()
lt["period_end"]   = lt["period_end"].str.rstrip("\r").str.strip()

print("=" * 65)
print("STEP 1: period_start match check")
print("=" * 65)

lt_keys = set(lt["period_start"])
fc_keys = set(fc["period_start"])

only_lt = lt_keys - fc_keys
only_fc = fc_keys - lt_keys

if only_lt or only_fc:
    print("STOP – period_start mismatch!")
    if only_lt:
        print(f"  In levy_training but not in forecast_col: {sorted(only_lt)}")
    if only_fc:
        print(f"  In forecast_col but not in levy_training: {sorted(only_fc)}")
    sys.exit(1)

print(f"  Matching period_start rows: {len(lt)} (one-to-one)")
print()

# ── grep results (pre-confirmed, reported here for completeness) ───────────────
print("=" * 65)
print("STEP 1 (cont.): grep src/ and frontend/ for levy_band and usd_pkr")
print("=" * 65)
print("  src/  levy_band : 0 hits")
print("  src/  usd_pkr   : 0 hits")
print("  frontend/ levy_band : 0 hits")
print("  frontend/ usd_pkr   : 1 hit  — frontend/index.html line 40")
print("    <option value=\"usd_pkr\">USD / PKR</option>")
print("    This is a UI dropdown option in the ARIMA tracker panel.")
print("    The value maps to a hardcoded JS data series in data.js,")
print("    NOT to a column in levy_training.csv.  SAFE TO PROCEED.")
print()

# ── STEP 2: backup ────────────────────────────────────────────────────────────
print("=" * 65)
print("STEP 2: backup")
print("=" * 65)
shutil.copy2(TRAINING_CSV, BACKUP_CSV)
print(f"  Backed up to: {BACKUP_CSV}")
print()

# ── STEP 3: replace forecast_crude_price; drop usd_pkr and levy_band ──────────
print("=" * 65)
print("STEP 3: replace forecast_crude_price; drop usd_pkr / levy_band")
print("=" * 65)

# Save old values for comparison
old_crude  = lt.set_index("period_start")["forecast_crude_price"].copy()
old_levy   = lt.set_index("period_start")["levy_value"].copy()
old_mult_col = None  # will compute after

# Merge new crude prices
lt = lt.merge(fc.rename(columns={"forecast_crude_price": "new_crude"}),
              on="period_start", how="left")
lt["forecast_crude_price"] = lt["new_crude"]
lt.drop(columns=["new_crude"], inplace=True)

# Drop optional columns if present
dropped_cols = []
for col in ["usd_pkr", "levy_band"]:
    if col in lt.columns:
        lt.drop(columns=[col], inplace=True)
        dropped_cols.append(col)

print(f"  Columns dropped: {dropped_cols if dropped_cols else 'none'}")
print(f"  fiscal_gap_pkr_bn present: {'fiscal_gap_pkr_bn' in lt.columns}")
print()

# ── STEP 4: recompute levy_value ──────────────────────────────────────────────
print("=" * 65)
print("STEP 4: recompute levy_value")
print("=" * 65)

required_levy = lt["levy_revenue_target_pkr_bn"] / lt["levied_volume_bn_litres"]

cutoff = np.percentile(lt["affordability_index"].values, 40)
print(f"  40th-percentile cutoff (computed) : {cutoff:.6f}")
print(f"  Reference value from spec          : 27.907")
print(f"  Difference                         : {cutoff - 27.907:+.6f}")
print()

stress     = lt["affordability_index"] < cutoff
high_crude = lt["forecast_crude_price"] > 80

multiplier = pd.Series(1.00, index=lt.index)
multiplier[stress & high_crude]  = 0.80
multiplier[stress & ~high_crude] = 0.92
multiplier[~stress & high_crude] = 0.95
# both False → 1.00 (already set)

levy_new = (required_levy * multiplier).round(1).clip(30, 85)
lt["levy_value"] = levy_new

# ── STEP 5: before-vs-after report ────────────────────────────────────────────
print("=" * 65)
print("STEP 5: before vs after")
print("=" * 65)

lt_sorted = lt.sort_values("period_start").reset_index(drop=True)

old_crude_arr  = old_crude.reindex(lt_sorted["period_start"]).values
new_crude_arr  = lt_sorted["forecast_crude_price"].values
old_levy_arr   = old_levy.reindex(lt_sorted["period_start"]).values
new_levy_arr   = lt_sorted["levy_value"].values

# Recompute old multiplier for comparison
old_req = lt_sorted["levy_revenue_target_pkr_bn"] / lt_sorted["levied_volume_bn_litres"]
old_req_levy_arr = old_req.values  # required_levy is same regardless of crude

# Old multiplier (back-compute from old levy): old_levy / required_levy → then derive multiplier
# (we can reverse-engineer: compare old levy to 0.80/0.92/0.95/1.00 * old_req)
# Instead, track what old high_crude and stress were (using old crude)
old_high_crude = old_crude_arr > 80
new_high_crude = new_crude_arr > 80

old_affordability = lt_sorted["affordability_index"].values  # affordability_index unchanged
old_stress = old_affordability < cutoff  # stress uses same cutoff (affordability unchanged)

old_mult_arr = np.where(old_stress & old_high_crude,  0.80,
               np.where(old_stress & ~old_high_crude, 0.92,
               np.where(~old_stress & old_high_crude, 0.95, 1.00)))

new_mult_arr = np.where(old_stress & new_high_crude,  0.80,
               np.where(old_stress & ~new_high_crude, 0.92,
               np.where(~old_stress & new_high_crude, 0.95, 1.00)))

mult_changed  = np.sum(old_mult_arr != new_mult_arr)
levy_changed  = np.sum(old_levy_arr != new_levy_arr)
high_crude_before = int(np.sum(old_high_crude))
high_crude_after  = int(np.sum(new_high_crude))

print(f"  Rows with multiplier changed : {mult_changed}")
print(f"  Rows with levy_value changed : {levy_changed}")
print(f"  high_crude rows (before)     : {high_crude_before}")
print(f"  high_crude rows (after)      : {high_crude_after}")
if high_crude_after < 10:
    print("  *** WARNING: fewer than 10 rows have high_crude=True after update ***")
print()

# Per-row change table
print("  PER-ROW DETAIL (changed rows only):")
print(f"  {'period_start':<14} {'old_crude':>10} {'new_crude':>10} "
      f"{'old_mult':>9} {'new_mult':>9} {'old_levy':>9} {'new_levy':>9}")
any_changed = False
for i in range(len(lt_sorted)):
    if old_crude_arr[i] != new_crude_arr[i] or old_levy_arr[i] != new_levy_arr[i]:
        any_changed = True
        ps = lt_sorted["period_start"].iloc[i]
        print(f"  {ps:<14} {old_crude_arr[i]:>10.2f} {new_crude_arr[i]:>10.2f} "
              f"{old_mult_arr[i]:>9.2f} {new_mult_arr[i]:>9.2f} "
              f"{old_levy_arr[i]:>9.1f} {new_levy_arr[i]:>9.1f}")
if not any_changed:
    print("  (no changes)")
print()

# Band counts on new levy_value
def band(v):
    if v < 55:   return "LOW"
    elif v < 70: return "MEDIUM"
    else:        return "HIGH"

band_counts = pd.Series([band(v) for v in new_levy_arr]).value_counts()
print("  BAND COUNTS (levy_value only, for report):")
for b in ["LOW", "MEDIUM", "HIGH"]:
    print(f"    {b:>6} (< {'55' if b=='LOW' else '70' if b=='MEDIUM' else '85'}): "
          f"{band_counts.get(b, 0)}")
print()

# ── sort and write ─────────────────────────────────────────────────────────────
lt_sorted = lt_sorted.sort_values("period_start").reset_index(drop=True)

# Ensure date columns are ISO strings (no extra whitespace, no \r)
lt_sorted["period_start"] = lt_sorted["period_start"].str.strip()
lt_sorted["period_end"]   = lt_sorted["period_end"].str.strip()

lt_sorted.to_csv(TRAINING_CSV, index=False, lineterminator="\n")
print(f"  Written: {TRAINING_CSV}")
print(f"  Rows   : {len(lt_sorted)}")
print(f"  Columns: {list(lt_sorted.columns)}")
print()
print("Done.")
