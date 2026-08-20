#!/usr/bin/env python3
"""Import FQBJCR (中科院分区) CSV files into jcr.db."""

from __future__ import annotations

import argparse
import csv
import sqlite3
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DATA_DIR = ROOT / "中科院分区表及JCR原始数据文件"
DEFAULT_DB = DATA_DIR / "jcr.db"

IMPORTS = (
    ("FQBJCR2021", DATA_DIR / "FQBJCR2021-UTF8.csv"),
    ("FQBJCR2022", DATA_DIR / "FQBJCR2022-UTF8.csv"),
    ("FQBJCR2023", DATA_DIR / "FQBJCR2023-UTF8.csv"),
)


def quote_ident(name: str) -> str:
    return '"' + name.replace('"', '""') + '"'


def import_csv(conn: sqlite3.Connection, table: str, csv_path: Path) -> int:
    with csv_path.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.reader(handle)
        try:
            header = next(reader)
        except StopIteration:
            raise SystemExit(f"Empty CSV: {csv_path}")

        columns = [column.strip() for column in header if column.strip()]
        if not columns or columns[0] != "Journal":
            raise SystemExit(f"Unexpected header in {csv_path}: {header}")

        placeholders = ", ".join("?" for _ in columns)
        column_sql = ", ".join(quote_ident(column) for column in columns)
        create_sql = f"CREATE TABLE {quote_ident(table)} ({column_sql})"
        insert_sql = (
            f"INSERT INTO {quote_ident(table)} ({column_sql}) VALUES ({placeholders})"
        )

        conn.execute(f"DROP TABLE IF EXISTS {quote_ident(table)}")
        conn.execute(create_sql)

        rows = 0
        batch: list[list[str]] = []
        for row in reader:
            if not row or not any(cell.strip() for cell in row):
                continue
            values = list(row[: len(columns)])
            if len(values) < len(columns):
                values.extend([""] * (len(columns) - len(values)))
            batch.append(values)
            if len(batch) >= 1000:
                conn.executemany(insert_sql, batch)
                rows += len(batch)
                batch.clear()

        if batch:
            conn.executemany(insert_sql, batch)
            rows += len(batch)

    return rows


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--db", type=Path, default=DEFAULT_DB, help="Path to jcr.db")
    args = parser.parse_args()

    if not args.db.exists():
        print(f"Database not found: {args.db}", file=sys.stderr)
        return 1

    conn = sqlite3.connect(args.db)
    try:
        for table, csv_path in IMPORTS:
            if not csv_path.exists():
                print(f"Skip missing CSV: {csv_path}")
                continue
            count = import_csv(conn, table, csv_path)
            conn.commit()
            print(f"Imported {table}: {count} rows from {csv_path.name}")
    finally:
        conn.close()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
