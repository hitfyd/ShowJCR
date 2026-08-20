# 如果默认没有显示 JCR2025 或中科院分区信息，请在程序右下角「菜单」→「选择数据表」中勾选对应数据表。

# ShowJCR

## 数据来源

新锐期刊分区表来源于 [https://www.xr-scholar.com](https://www.xr-scholar.com)，最新为 2026 年版（2026 年 3 月 24 日发布），涵盖但不限于 SCIE、SSCI、ESCI 等核心来源，共 22299 种期刊和 15 种重要会议论文集（计算机领域）。同时，不再单独发布预警名单，对预警期刊标记 “Under Review”。

中科院分区表升级版数据来源于 [advanced.fenqubiao.com](http://advanced.fenqubiao.com)，信息包括期刊是否为 Review、是否为 Open Access、Web of Science 收录类型（分为 SCI、SCIE、SSCI、ESCI 等）、是否为 Top 期刊、大类分区信息、（一至多个）小类分区信息。程序中对应 CSV 为 `FQBJCR20xx-UTF8.csv`，导入 `jcr.db` 后表名为 `FQBJCR20xx`，界面显示为「中科院20xx」。

JCR 期刊影响因子和分区更新到 2025 版（2026 年 6 月 17 日发布），并保留 2024 年影响因子和分区作为对比。

国际期刊预警等级来源于 [《国际预警期刊名单》（2020、2021、2023、2024、2025 年）](https://ewl.fenqubiao.com/#/README)，2024 年版不再区分预警等级而改为预警原因。程序中对应 CSV 为 `GJQKYJMD20xx.csv`，导入后表名为 `GJQKYJMD20xx`，界面显示为「国际预警20xx」。**注意：这些表仅包含预警名单中的期刊（每年几十条），不是完整的中科院分区表。**

中国计算机学会（CCF）[推荐国际学术会议和期刊目录（2026 年）](https://www.ccf.org.cn/Academic_Evaluation/By_category/)，[计算领域高质量科技期刊分级目录（2025 年）](https://www.ccf.org.cn/ccftjgjxskwml/)。

### 数据表与界面名称对照

| jcr.db 表名 | 原始 CSV | 界面显示 | 说明 |
| ----------- | -------- | -------- | ---- |
| `FQBJCR2025` | `FQBJCR2025-UTF8.csv` | 中科院2025 | 中科院分区表升级版（全量） |
| `FQBJCR2023` | `FQBJCR2023-UTF8.csv` | 中科院2023 | 同上 |
| `FQBJCR2022` | `FQBJCR2022-UTF8.csv` | 中科院2022 | 同上 |
| `FQBJCR2021` | `FQBJCR2021-UTF8.csv` | 中科院2021 | 同上 |
| `GJQKYJMD2025` | `GJQKYJMD2025.csv` | 国际预警2025 | 仅预警期刊 |
| `GJQKYJMD2024` | `GJQKYJMD2024.csv` | 国际预警2024 | 仅预警期刊 |
| `GJQKYJMD2023` | `GJQKYJMD2023.csv` | 国际预警2023 | 仅预警期刊 |
| `GJQKYJMD2021` | `GJQKYJMD2021.csv` | 国际预警2021 | 仅预警期刊 |
| `GJQKYJMD2020` | `GJQKYJMD2020.csv` | 国际预警2020 | 仅预警期刊 |
| `JCR2025` | `JCR2025-UTF8.csv` | JCR 2025 | JCR 影响因子与分区 |
| `JCR2024` | `JCR2024-UTF8.csv` | JCR 2024 | JCR 影响因子与分区 |
| `XR2026` | `XR2026-UTF8.csv` | 新锐2026 | 新锐期刊分区表 |
| `CCF2026` | `CCF2026-UTF8.csv` | CCF 2026 | CCF 推荐目录 |
| `CCFT2025` | `CCFT2025-UTF8.csv` | CCF-T 2025 | CCF 高质量期刊分级 |

### 新锐期刊分区表2026年版特殊情况说明

1. ADVANCED ENGINEERING MATERIALS、Soft Science具有两个大类分区，均为材料科学和工程技术；

2. AUSTRALASIAN PLANT PATHOLOGY、HEART RHYTHM具有两本同名期刊，但是两者的ISSN号不一样。


### SQLite3 数据库生成步骤

国际期刊信息的原始 CSV 随附在 `中科院分区表及JCR原始数据文件/` 目录中，编译后的 `jcr.db` 也位于该目录。

推荐导入顺序（`jcr.db` 中的表名）为：`FQBJCR2025`、`FQBJCR2023`、`FQBJCR2022`、`FQBJCR2021`、`JCR2025`、`JCR2024`、`GJQKYJMD2025`、`GJQKYJMD2024`、`GJQKYJMD2023`、`GJQKYJMD2021`、`GJQKYJMD2020`、`CCF2026`、`CCFT2025`、`XR2026`、`XR2026Conferences`。

#### 导入中科院分区表（FQBJCR）

`FQBJCR2021-UTF8.csv`、`FQBJCR2022-UTF8.csv`、`FQBJCR2023-UTF8.csv` 可使用项目自带脚本导入：

```bash
python3 scripts/import-fqbjcr.py
```

该脚本会更新 `中科院分区表及JCR原始数据文件/jcr.db` 中的 `FQBJCR2021`、`FQBJCR2022`、`FQBJCR2023` 三张表（若 CSV 存在则覆盖重建）。`FQBJCR2025` 需用 [DB Browser for SQLite](https://sqlitebrowser.org/) 手动导入，或扩展该脚本。

**`make install` 不会自动执行 `scripts/import-fqbjcr.py`。** macOS 构建只是把上述目录里现有的 `jcr.db` 复制进 app 包。若更新了 CSV，须先运行导入脚本，再执行：

```bash
python3 scripts/import-fqbjcr.py   # 可选：更新中科院 2021–2023
make install                       # 编译并安装到 /Applications
```

也可使用：

```bash
make build      # 仅构建 build/ShowJCR.app
make open       # 打开已安装的应用
```

### 导入新的分区信息

导入新的分区信息，只需要在jcr.db增加相应的数据表，无需修改程序源代码。

分区信息可以处理为csv格式，作为新的数据表使用[DB Browser for SQLite](https://sqlitebrowser.org/)导入到jcr.db（包含在源代码和可执行版本ShowJCR.7z中）。

新的分区信息表的字段格式可以是两种形式：

1. 表第一个数据字段为“Journal”，例如

   | Journal                  | IF(2021) |
   | ------------------------ | -------- |
   | PROCEEDINGS OF THE  IEEE | 14.91    |
   | ······                   | ······   |

2. 表第一个数据字段为其他检索关键字（比如“期刊简称”、“中文刊名”等），第二个数据字段为为“Journal”，例如

   | 刊物简称   | Journal                 | 领域           | CCF推荐类型 |
   | ---------- | ----------------------- | -------------- | ----------- |
   | Proc. IEEE | Proceedings of the IEEE | 交叉/综合/新兴 | A类         |
   | ······     | ······                  | ······         | ······      |

数据表的设计核心是必须包含“Journal”字段，该字段是程序默认的搜索字段；如果“Journal”不是数据表的第一个字段，则该字段之前的字段也将被增加为搜索字段，其字段数据也可以在程序中查询。

## macOS 构建与安装

依赖：Homebrew 安装的 Qt 6、CMake、Python 3（仅导入脚本需要）。

```bash
make install    # 配置、编译、macdeployqt 打包、签名、安装到 /Applications/ShowJCR.app
make build      # 仅生成 build/ShowJCR.app
make package    # 构建并生成 dist/ShowJCR-<version>-macos.zip
make release    # 打包并在 GitHub fork 上发布 Release（需 gh 登录）
make open       # 打开已安装应用
```

构建过程中会将 `中科院分区表及JCR原始数据文件/jcr.db` 复制到 `ShowJCR.app/Contents/Resources/jcr.db`。数据库内容以该文件为准，**不会**在构建时从 CSV 自动重新生成。

## Release发布版

### 运行依赖

1. **jcr.db**，期刊信息数据库；
2. Qt相关依赖（使用windeployqt获取所有依赖项并删除国际化translations文件夹）。

### 可执行版本

提供两种可执行版本：

1. ShowJCR.7z，解压到任意目录下执行ShowJCR.exe；
2. ShowJCR.exe，使用[Enigma Virtual Box ](http://www.enigmaprotector.com/)对所有依赖执行封包，单一程序即可独立执行。

## 使用说明

软件的使用十分方便，如图所示，输入期刊名称，点击“查询”或输入“回车”即可获得期刊详细信息。

在显示的信息中，“年份“字段设置为浅灰色以简易分隔JCR、不同年份的中科院升级版。IF、预警信息、大类分区和”Top“等字段设置为红色以简易标记重要信息。

![image1](README.assets/image1.png)

期刊名称输入时具备联想功能，并且不区分大小写。

![image2](README.assets/image2.png)

如上图中红框所示，软件还具有4个属性选项：

1. 开机自启动到托盘；
2. 退出到托盘；
3. 监听剪切板：在后台监听剪切板，如果复制文字为期刊名称，将自动进行查询；
4. 自动激活窗口：与监听剪切板配合使用，当监听到期刊名称并自动查询完成后，将窗口显示到桌面最前。

设置中支持自定义选择要查询的期刊数据表。
