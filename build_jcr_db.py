#!/usr/bin/env python3
"""
jcr.db 生成脚本 —— 从 CSV 源文件构建规范化 SQLite 数据库。

规范化策略:
  1. 期刊名去重:   _journals(id, name) — 36626 个唯一名 → 各表存 INTEGER 引用
  2. 分类名去重:   _categories(id, en, zh) — ~21 大类 + ~310 小类
  3. 出版机构去重: _publishers(id, name) — ~5000 个
  4. 枚举 INTEGER 化: _partition/_yn/_wos/_lang/_db 等 — 节省重复 TEXT
  5. 删除 100% 空列
  6. 每张原始表 → 同名 VIEW（JOIN 还原完整 TEXT，对 sqlitedb.cpp 查询透明）

用法:
  python build_jcr_db.py [csv_dir] [output.db]

默认: csv_dir=中科院分区表及JCR原始数据文件, output=jcr.db

sqlitedb.cpp 需要的 1 处改动 (第 27 行):
  // 旧: allTableNames = database.tables();
  // 新:
  QSqlQuery q(database);
  q.exec("SELECT name FROM sqlite_master WHERE type IN ('table','view')"
         " AND name NOT LIKE '\\_%' ESCAPE '\\' ORDER BY name");
  while (q.next()) allTableNames << q.value(0).toString();
"""

import csv
import os
import re
import sqlite3
import sys

# ===========================================================================
# 表定义
# ===========================================================================
# 列类型:
#   "text"       → 直接存 TEXT
#   "int"        → INTEGER（年份等）
#   "real"       → REAL（影响因子等）
#   "journal"    → 引用 _journals.id（刊名去重）
#   "enum:<tbl>" → 引用 _enum_<tbl>.id
#   "cat"        → 大类分类引用 _categories.id（与相邻中文名列共享同一 ID）
#   "scat"       → 小类分类引用 _subcategories.id（与相邻中文名列共享同一 ID）
#   "pub"        → 引用 _publishers.id
#
# 特殊前缀 "!" → 跳过此列（100% 空，不导入）

TABLE_DEFS = {
    "JCR2024": {
        "csv": "JCR2024-UTF8.csv",
        "columns": [
            ("Journal",              "journal"),
            ("ISSN",                 "text"),
            ("eISSN",                "text"),
            ("Category",             "text"),
            ("IF(2024)",             "real"),
            ("IF Quartile(2024)",    "text"),
            ("IF Rank(2024)",        "text"),
        ],
    },
    "JCR2023": {
        "csv": "JCR2023-UTF8.csv",
        "columns": [
            ("Journal",              "journal"),
            ("Country",              "text"),
            ("ISSN",                 "text"),
            ("EISSN",                "text"),
            ("Web of Science",       "text"),
            ("IF(2023)",             "real"),
            ("Category",             "text"),
            ("IF Quartile(2023)",    "text"),
            ("Category Rank(2023)",  "text"),
        ],
    },
    "FQBJCR2025": {
        "csv": "FQBJCR2025-UTF8.csv",
        "columns": [
            ("Journal",                     "journal"),
            ("年份",                         "int"),
            ("ISSN/EISSN",                  "text"),
            ("Review",                      "enum:_yn"),
            ("OA Journal Index（OAJ）",      "text"),
            ("Open Access",                 "enum:_yn"),
            ("Web of Science",              "enum:_wos"),
            ("标注",                         "enum:_fqb_note"),
            ("大类",                         "text"),
            ("大类分区",                     "text"),   # 复合排名字符串，非枚举
            ("Top",                         "enum:_yn"),
            ("小类1",                        "scat"),
            ("小类1分区",                    "text"),   # 复合排名字符串
            ("小类2",                        "scat"),
            ("小类2分区",                    "text"),
            ("小类3",                        "scat"),
            ("小类3分区",                    "text"),
            ("小类4",                        "scat"),
            ("小类4分区",                    "text"),
            ("小类5",                        "scat"),
            ("小类5分区",                    "text"),
            ("!小类6",                       "scat"),
            ("!小类6分区",                   "text"),
        ],
        "pairs": [],  # FQBJCR2025 无中英配对
    },
    "XR2026": {
        "csv": "XR2026-UTF8.csv",
        "columns": [
            ("Journal",         "journal"),
            ("年份",             "int"),
            ("预警标记",         "enum:_warning"),
            ("刊名",             "journal"),      # 旧刊名（78行有值，其余=NULL）
            ("中文刊名",         "text"),
            ("CN",               "text"),
            ("ISSN",             "text"),
            ("EISSN",            "text"),
            ("出版机构",         "pub"),
            ("语种",             "enum:_lang"),
            ("期刊类型",         "enum:_journal_type"),
            ("数据库",           "enum:_db"),
            ("标注",             "enum:_xr_note"),
            # 大类 (中英配对，共享 _categories.id)
            ("大类英文名",       "cat"),
            ("大类中文名",       "cat"),
            ("大类新锐分区",     "enum:_partition"),
            ("Top",              "enum:_yn"),
            # 大类2 — 100%空，跳过
            ("!大类2英文名",     "cat"),
            ("!大类2中文名",     "cat"),
            ("!大类2新锐分区",   "enum:_partition"),
            ("!大类2Top",        "enum:_yn"),
            # 小类 (6 组中英配对)
            ("小类1英文名",      "scat"),
            ("小类1中文名",      "scat"),
            ("小类1新锐分区",    "enum:_partition"),
            ("小类2英文名",      "scat"),
            ("小类2中文名",      "scat"),
            ("小类2新锐分区",    "enum:_partition"),
            ("小类3英文名",      "scat"),
            ("小类3中文名",      "scat"),
            ("小类3新锐分区",    "enum:_partition"),
            ("小类4英文名",      "scat"),
            ("小类4中文名",      "scat"),
            ("小类4新锐分区",    "enum:_partition"),
            ("小类5英文名",      "scat"),
            ("小类5中文名",      "scat"),
            ("小类5新锐分区",    "enum:_partition"),
            # 小类6 — 100%空，跳过
            ("!小类6英文名",     "scat"),
            ("!小类6中文名",     "scat"),
            ("!小类6新锐分区",   "enum:_partition"),
        ],
        "pairs": [
            # (en_col, zh_col) — 共享同一个 _categories / _subcategories ID
            ("大类英文名", "大类中文名"),
            ("小类1英文名", "小类1中文名"),
            ("小类2英文名", "小类2中文名"),
            ("小类3英文名", "小类3中文名"),
            ("小类4英文名", "小类4中文名"),
            ("小类5英文名", "小类5中文名"),
        ],
    },
    "XR2026Conferences": {
        "csv": "XR2026Conferences-UTF8.csv",
        "columns": [
            ("会议缩写",  "text"),
            ("Journal",   "journal"),
            ("分区",      "int"),
            ("Top",       "enum:_yn"),
            ("会议网站",  "text"),
        ],
    },
    "CCF2026": {
        "csv": "CCF2026-UTF8.csv",
        "columns": [
            ("刊物名称",                            "text"),
            ("Journal",                             "journal"),
            ("年份",                                "int"),
            ("出版社",                              "text"),
            ("网址",                                "text"),
            ("领域",                                "text"),
            ("CCF推荐类别（国际学术刊物/会议）",      "text"),
            ("CCF推荐类型",                         "text"),
        ],
    },
    "CCFT2025": {
        "csv": "CCFT2025-UTF8.csv",
        "columns": [
            ("中文刊名",      "text"),
            ("Journal",       "journal"),
            ("CN号",          "text"),
            ("语种",          "text"),
            ("主办单位",      "text"),
            ("CCF推荐类别",   "text"),
            ("T分区",         "text"),
        ],
    },
    "GJQKYJMD2025": {
        "csv": "GJQKYJMD2025.csv",
        "columns": [
            ("Journal",          "journal"),
            ("预警原因（2025）",  "text"),
        ],
    },
    "GJQKYJMD2024": {
        "csv": "GJQKYJMD2024.csv",
        "columns": [
            ("Journal",          "journal"),
            ("预警原因（2024）",  "text"),
        ],
    },
    "GJQKYJMD2023": {
        "csv": "GJQKYJMD2023.csv",
        "columns": [
            ("Journal",          "journal"),
            ("预警等级（2023）",  "text"),
        ],
    },
    "GJQKYJMD2021": {
        "csv": "GJQKYJMD2021.csv",
        "columns": [
            ("Journal",          "journal"),
            ("预警等级（2021）",  "text"),
        ],
    },
    "GJQKYJMD2020": {
        "csv": "GJQKYJMD2020.csv",
        "columns": [
            ("Journal",          "journal"),
            ("预警等级（2020）",  "text"),
        ],
    },
}

# ===========================================================================
# 查找表管理器
# ===========================================================================

class LookupManager:
    """管理所有规范化查找表。"""

    def __init__(self, db: sqlite3.Connection):
        self.db = db
        # journal name → id
        self._jn: dict[str, int] = {}
        self._jn_next = 1
        # (en, zh) → id for categories
        self._cat: dict[tuple[str, str], int] = {}
        self._cat_next = 1
        # (en, zh) → id for subcategories
        self._scat: dict[tuple[str, str], int] = {}
        self._scat_next = 1
        # publisher name → id
        self._pub: dict[str, int] = {}
        self._pub_next = 1
        # enum: table_name → {value: id}
        self._enum: dict[str, dict[str, int]] = {}
        self._enum_next: dict[str, int] = {}

    def _ensure_enum(self, tbl: str):
        if tbl not in self._enum:
            self._enum[tbl] = {}
            self._enum_next[tbl] = 1

    def journal(self, name: str) -> int | None:
        """获取期刊名 ID，空值返回 None"""
        name = (name or "").strip()
        if not name:
            return None
        if name not in self._jn:
            self._jn[name] = self._jn_next
            self._jn_next += 1
        return self._jn[name]

    def category(self, en: str, zh: str) -> int | None:
        """获取 (英文名, 中文名) 配对的大类分类 ID"""
        en = (en or "").strip()
        zh = (zh or "").strip()
        if not en and not zh:
            return None
        key = (en, zh)
        if key not in self._cat:
            self._cat[key] = self._cat_next
            self._cat_next += 1
        return self._cat[key]

    def subcategory(self, en: str, zh: str) -> int | None:
        """获取 (英文名, 中文名) 配对的小类分类 ID"""
        en = (en or "").strip()
        zh = (zh or "").strip()
        if not en and not zh:
            return None
        key = (en, zh)
        if key not in self._scat:
            self._scat[key] = self._scat_next
            self._scat_next += 1
        return self._scat[key]

    def publisher(self, name: str) -> int | None:
        name = (name or "").strip()
        if not name:
            return None
        if name not in self._pub:
            self._pub[name] = self._pub_next
            self._pub_next += 1
        return self._pub[name]

    def enum_val(self, tbl: str, val: str) -> int | None:
        val = (val or "").strip()
        if not val:
            return None
        self._ensure_enum(tbl)
        m = self._enum[tbl]
        if val not in m:
            m[val] = self._enum_next[tbl]
            self._enum_next[tbl] += 1
        return m[val]

    def flush(self):
        """将所有查找表写入数据库"""
        db = self.db

        # _journals
        db.execute("CREATE TABLE _journals (id INTEGER PRIMARY KEY, name TEXT UNIQUE)")
        for name, i in sorted(self._jn.items(), key=lambda x: x[1]):
            db.execute("INSERT INTO _journals VALUES (?,?)", (i, name))

        # _categories
        db.execute("CREATE TABLE _categories (id INTEGER PRIMARY KEY, en TEXT, zh TEXT)")
        for (en, zh), i in sorted(self._cat.items(), key=lambda x: x[1]):
            db.execute("INSERT INTO _categories VALUES (?,?,?)", (i, en, zh))

        # _subcategories
        db.execute("CREATE TABLE _subcategories (id INTEGER PRIMARY KEY, en TEXT, zh TEXT)")
        for (en, zh), i in sorted(self._scat.items(), key=lambda x: x[1]):
            db.execute("INSERT INTO _subcategories VALUES (?,?,?)", (i, en, zh))

        # _publishers
        db.execute("CREATE TABLE _publishers (id INTEGER PRIMARY KEY, name TEXT UNIQUE)")
        for name, i in sorted(self._pub.items(), key=lambda x: x[1]):
            db.execute("INSERT INTO _publishers VALUES (?,?)", (i, name))

        # _enum_*
        for tbl, m in sorted(self._enum.items()):
            if not m:
                continue
            db.execute(f"CREATE TABLE _enum_{tbl} (id INTEGER PRIMARY KEY, value TEXT UNIQUE)")
            for val, i in sorted(m.items(), key=lambda x: x[1]):
                db.execute(f"INSERT INTO _enum_{tbl} VALUES (?,?)", (i, val))

        db.commit()

    def stats(self) -> dict[str, int]:
        s = {
            "期刊名 (_journals)": len(self._jn),
            "大类分类 (_categories)": len(self._cat),
            "小类分类 (_subcategories)": len(self._scat),
            "出版机构 (_publishers)": len(self._pub),
        }
        for tbl, m in sorted(self._enum.items()):
            if m:
                s[f"枚举 (_enum_{tbl})"] = len(m)
        return s

    def create_views(self, table_defs: dict):
        """为每张表创建还原 VIEW"""
        db = self.db

        for view_name, tdef in table_defs.items():
            base_table = f"_{view_name}"
            cols = [
                (c, t)
                for c, t in tdef["columns"]
                if not c.startswith("!")
            ]

            selects = []
            joins = []
            join_counter = [0]  # mutable counter

            def _join(join_table: str, join_col: str, select_expr: str) -> str:
                join_counter[0] += 1
                alias = f"j{join_counter[0]}"
                joins.append(
                    f'LEFT JOIN {join_table} {alias} ON b."{join_col}" = {alias}.id'
                )
                # select_expr 中 {a}=alias占位, {}=列名占位（由外部 .format 填入）
                return select_expr.replace("{a}", alias)

            for col_name, col_type in cols:
                if col_type == "journal":
                    expr = _join("_journals", col_name, '{a}.name AS "{}"')
                    selects.append(expr.format(col_name))
                elif col_type == "cat":
                    if "英文" in col_name:
                        expr = _join("_categories", col_name, '{a}.en AS "{}"')
                    else:
                        expr = _join("_categories", col_name, '{a}.zh AS "{}"')
                    selects.append(expr.format(col_name))
                elif col_type == "scat":
                    if "英文" in col_name:
                        expr = _join("_subcategories", col_name, '{a}.en AS "{}"')
                    else:
                        expr = _join("_subcategories", col_name, '{a}.zh AS "{}"')
                    selects.append(expr.format(col_name))
                elif col_type == "pub":
                    expr = _join("_publishers", col_name, '{a}.name AS "{}"')
                    selects.append(expr.format(col_name))
                elif col_type.startswith("enum:"):
                    enum_tbl = f"_enum_{col_type.split(':')[1]}"
                    expr = _join(enum_tbl, col_name, '{a}.value AS "{}"')
                    selects.append(expr.format(col_name))
                else:
                    selects.append(f'b."{col_name}" AS "{col_name}"')

            view_sql = (
                f'CREATE VIEW "{view_name}" AS\n'
                f'SELECT\n    ' + ',\n    '.join(selects) + '\n'
                f'FROM {base_table} b\n    ' +
                '\n    '.join(joins)
            )
            db.execute(view_sql)


# ===========================================================================
# CSV 导入
# ===========================================================================

def read_csv(path: str) -> list[dict]:
    with open(path, "r", encoding="utf-8-sig") as f:
        return list(csv.DictReader(f))


def build_table(db, lm: LookupManager, view_name: str, tdef: dict, csv_dir: str):
    """读取 CSV 并写入规范化基表。"""
    csv_path = os.path.join(csv_dir, tdef["csv"])
    if not os.path.exists(csv_path):
        print(f"  ⚠ 跳过 {view_name}: CSV 不存在 ({tdef['csv']})")
        return False

    rows = read_csv(csv_path)

    # 活跃列（未跳过的）
    all_cols = tdef["columns"]
    active_cols = [(c, t) for c, t in all_cols if not c.startswith("!")]
    pairs = tdef.get("pairs", [])

    # 配对列索引映射: (en_col, zh_col) → 共享同一个 cat/scat ID
    pair_set = set()
    for a, b in pairs:
        pair_set.add(a)
        pair_set.add(b)

    # 创建基表
    col_defs = []
    for col_name, col_type in active_cols:
        if col_type in ("journal", "cat", "scat", "pub") or col_type.startswith("enum:"):
            col_defs.append(f'"{col_name}" INTEGER')
        elif col_type == "int":
            col_defs.append(f'"{col_name}" INTEGER')
        elif col_type == "real":
            col_defs.append(f'"{col_name}" REAL')
        else:
            col_defs.append(f'"{col_name}" TEXT')

    base_table = f"_{view_name}"
    db.execute(f'CREATE TABLE {base_table} (' + ", ".join(col_defs) + ")")

    # 构建列索引映射
    active_names = [c for c, _ in active_cols]
    col_idx = {c: i for i, c in enumerate(active_names)}

    # 配对查找: (en_col, zh_col) → 用于计算共享 ID
    pair_map = {}  # col_name → (pair_key_col, is_en)
    for en_col, zh_col in pairs:
        pair_map[en_col] = (zh_col, True)
        pair_map[zh_col] = (en_col, False)

    converted = []
    刊名_saved = 0

    for row in rows:
        # 预处理配对列的共享 ID
        pair_ids = {}  # (en_col, zh_col) → id
        for en_col, zh_col in pairs:
            en_raw = row.get(en_col, "").strip()
            zh_raw = row.get(zh_col, "").strip()

            lookup_type = dict(active_cols)[en_col]  # "cat" or "scat"
            if lookup_type == "cat":
                pid = lm.category(en_raw, zh_raw)
            else:
                pid = lm.subcategory(en_raw, zh_raw)
            pair_ids[(en_col, zh_col)] = pid

        values = []
        for col_name, col_type in active_cols:
            raw = (row.get(col_name, "") or "").strip()

            if col_type == "journal":
                val = lm.journal(raw)
                # XR2026 刊名==Journal → NULL
                if view_name == "XR2026" and col_name == "刊名":
                    jn_val = values[col_idx["Journal"]] if "Journal" in col_idx else None
                    if val == jn_val:
                        val = None
                        刊名_saved += 1
                values.append(val)

            elif col_type in ("cat", "scat"):
                # 找配对键
                if col_name in pair_map:
                    other, _ = pair_map[col_name]
                    en_col = col_name if "英文" in col_name else other
                    zh_col = other if "英文" in col_name else col_name
                    key = (en_col, zh_col)  # canonical order
                    # 查找 key 的正确顺序
                    found = False
                    for (a, b), pid in pair_ids.items():
                        if (a == en_col and b == zh_col) or (a == zh_col and b == en_col):
                            values.append(pid)
                            found = True
                            break
                    if not found:
                        values.append(None)
                else:
                    # FQBJCR2025 风格：小类列直接存名称，按英文处理
                    if col_type == "cat":
                        values.append(lm.category(raw, ""))
                    else:
                        values.append(lm.subcategory(raw, ""))

            elif col_type == "pub":
                values.append(lm.publisher(raw))

            elif col_type.startswith("enum:"):
                tbl = col_type.split(":")[1]
                values.append(lm.enum_val(tbl, raw))

            elif col_type == "int":
                try:
                    values.append(int(raw) if raw else None)
                except ValueError:
                    values.append(None)

            elif col_type == "real":
                try:
                    values.append(float(raw) if raw else None)
                except ValueError:
                    values.append(None)

            else:  # text
                values.append(raw if raw else None)

        converted.append(values)

    # 写入
    placeholders = ", ".join(["?"] * len(active_cols))
    col_names = ", ".join(f'"{c}"' for c in active_names)
    db.executemany(
        f'INSERT INTO {base_table} ({col_names}) VALUES ({placeholders})',
        converted,
    )
    db.commit()

    info = f"{len(converted)} rows"
    if 刊名_saved:
        info += f" (刊名去重: {刊名_saved})"
    print(f"  {view_name}: {info}")
    return True


# ===========================================================================
# 主流程
# ===========================================================================

def main():
    csv_dir = sys.argv[1] if len(sys.argv) > 1 else "中科院分区表及JCR原始数据文件"
    db_path = sys.argv[2] if len(sys.argv) > 2 else "jcr.db"

    if not os.path.isdir(csv_dir):
        print(f"错误: CSV 目录不存在: {csv_dir}")
        sys.exit(1)
    if os.path.exists(db_path):
        os.remove(db_path)

    db = sqlite3.connect(db_path)
    db.execute("PRAGMA page_size = 32768")
    db.execute("PRAGMA journal_mode = OFF")
    db.execute("PRAGMA synchronous = OFF")
    db.execute("PRAGMA foreign_keys = ON")

    lm = LookupManager(db)

    # Phase 1: 导入所有 CSV
    print("Phase 1: 导入 CSV → 规范化基表\n")
    for view_name, tdef in TABLE_DEFS.items():
        build_table(db, lm, view_name, tdef, csv_dir)

    # Phase 2: 写入查找表
    print("\nPhase 2: 写入查找表")
    lm.flush()
    for k, v in lm.stats().items():
        print(f"  {k}: {v} 条")

    # Phase 3: 创建 VIEW
    print("\nPhase 3: 创建 VIEW（还原原始表结构）")
    lm.create_views(TABLE_DEFS)
    for view_name in TABLE_DEFS:
        print(f"  CREATE VIEW {view_name}")

    # Phase 4: VACUUM
    print("\nPhase 4: VACUUM + ANALYZE")
    db.execute("VACUUM")
    db.execute("ANALYZE")
    db.close()

    size = os.path.getsize(db_path)
    print(f"\n{'='*50}")
    print(f"数据库生成完成: {db_path}")
    print(f"大小: {size / (1024*1024):.1f} MB")
    print(f"\n>>> sqlitedb.cpp 第 27 行改动 <<<")
    print(f"将 allTableNames = database.tables();")
    print(f"替换为脚本顶部注释中的 4 行代码（包含 VIEW 发现）。")


if __name__ == "__main__":
    main()
