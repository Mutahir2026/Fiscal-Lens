import csv
from pathlib import Path


def clean_brent(input_path: Path, output_path: Path) -> list:
    """
    Cleans raw Brent crude oil daily prices:
    - Reads observation_date and DCOILBRENTEU columns.
    - Strips whitespace.
    - Drops blank or non-numeric rows (no forward-filling).
    - Sorts chronologically ascending by ISO date.
    - Writes date,price to output_path.
    """
    if not input_path.exists():
        raise FileNotFoundError(f"Input file not found: {input_path}")

    cleaned_records = []
    with open(input_path, mode="r", encoding="utf-8") as infile:
        reader = csv.reader(infile)
        header = next(reader, None)
        if not header:
            raise ValueError(f"Empty input file: {input_path}")

        for row in reader:
            if not row or len(row) < 2:
                continue
            date_str = row[0].strip()
            price_str = row[1].strip()

            if not date_str or not price_str:
                continue

            try:
                price_val = float(price_str)
            except ValueError:
                continue

            cleaned_records.append((date_str, price_val))

    # Sort ascending by ISO date
    cleaned_records.sort(key=lambda item: item[0])

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, mode="w", encoding="utf-8", newline="") as outfile:
        writer = csv.writer(outfile)
        writer.writerow(["date", "price"])
        for date_str, price_val in cleaned_records:
            writer.writerow([date_str, f"{price_val:.2f}"])

    return cleaned_records


if __name__ == "__main__":
    repo_root = Path(__file__).resolve().parent.parent
    raw_file = repo_root / "data" / "raw" / "brent_daily.csv"
    proc_file = repo_root / "data" / "processed" / "brent_daily.csv"

    records = clean_brent(raw_file, proc_file)
    print(f"Total rows written: {len(records)}")
    if records:
        prices = [p for _, p in records]
        print(f"Head (first 5):\n{records[:5]}")
        print(f"Tail (last 5):\n{records[-5:]}")
        print(f"Min price: {min(prices):.2f}")
        print(f"Max price: {max(prices):.2f}")
