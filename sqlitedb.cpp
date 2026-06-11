#include "sqlitedb.h"

#include <cstring>
#include <QMessageBox>
#include <QApplication>
#include <QRegularExpression>
#include <QRegularExpressionMatch>

extern "C" {
#include "sqlite3.h"
}

// Helper: find column index by name (returns -1 if not found)
static int columnIndex(sqlite3_stmt *stmt, const char *name) {
    int count = sqlite3_column_count(stmt);
    for (int i = 0; i < count; i++) {
        if (std::strcmp(sqlite3_column_name(stmt, i), name) == 0) {
            return i;
        }
    }
    return -1;
}

// Helper: safely get column text, returning empty QString for NULL
static QString columnText(sqlite3_stmt *stmt, int idx) {
    if (sqlite3_column_type(stmt, idx) == SQLITE_NULL) {
        return QString();
    }
    return QString::fromUtf8(
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, idx))
    );
}

SqliteDB::SqliteDB(const QDir &appDir, const QString &datasetName, QObject *parent) : QObject(parent)
{
    //连接SQLite3数据库"jcr.db"，该数据集应放在运行目录下
    QString dbPath = appDir.absoluteFilePath(datasetName);
    int rc = sqlite3_open_v2(
        dbPath.toUtf8().constData(),
        &db,
        SQLITE_OPEN_READONLY,
        nullptr
    );
    if (rc != SQLITE_OK)
    {
        qWarning() << "Error: Failed to connect database." << __FUNCTION__ << sqlite3_errmsg(db);
        QMessageBox::warning(QApplication::activeWindow(), "期刊信息数据库缺失！", sqlite3_errmsg(db));
        sqlite3_close(db);
        db = nullptr;
    }
    else
    {
        qDebug() << "Successed to connect database.";
    }

    // database.tables() equivalent: query sqlite_master
    if (db) {
        const char *tablesSQL = "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name";
        sqlite3_stmt *stmt = nullptr;
        rc = sqlite3_prepare_v2(db, tablesSQL, -1, &stmt, nullptr);
        if (rc == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                allTableNames << columnText(stmt, 0);
            }
        }
        sqlite3_finalize(stmt);
    }

    // 重排表名顺序
    allTableNames = sortSpecialStrings(allTableNames);
    //selectTableNames(allTableNames);	//避免启动时执行两次
}

SqliteDB::~SqliteDB()
{
    if (db) {
        sqlite3_close(db);
        db = nullptr;
    }
}

QStringList SqliteDB::getAllTableNames()
{
    return allTableNames;
}

QStringList SqliteDB::getAllJournalNames()
{
    return allJournalNamesList;
}

QList<Pair> SqliteDB::getJournalInfo(const QString &journalName, bool allowSelectAgain)
{
    Q_ASSERT(allJournalNamesList.contains(journalName, Qt::CaseInsensitive));
    Q_ASSERT(allKeyNames.size() == tablePrimaryKeys.size());

    QList<Pair> journalInfo;
    QList<QString> journalInfoFieldNames;
    sqlite3_stmt *stmt = nullptr;

    for(int i = 0; i < allKeyNames.size(); i++){
        if(allKeyNames[i].contains(journalName, Qt::CaseInsensitive)){
            const QString &table = tablePrimaryKeys[i].first;
            const QString &primaryKey = tablePrimaryKeys[i].second;
            if(db){
                // Use parameterized query to handle journal names with special characters safely
                const char *sql = "SELECT * FROM \"%1\" WHERE \"%2\" = ?1 COLLATE NOCASE";
                QString select = QString(sql).arg(table, primaryKey);
                int rc = sqlite3_prepare_v2(db, select.toUtf8().constData(), -1, &stmt, nullptr);
                if (rc != SQLITE_OK) {
                    qWarning() << "Error: Failed to prepare statement for" << table << __FUNCTION__ << sqlite3_errmsg(db);
                    continue;
                }
                // Bind the journal name as a parameter
                QByteArray journalUtf8 = journalName.toUtf8();
                sqlite3_bind_text(stmt, 1, journalUtf8.constData(), -1, SQLITE_TRANSIENT);

                //CCF推荐期刊中不同领域存在重复的期刊
                while (sqlite3_step(stmt) == SQLITE_ROW){
                    QStringList fieldNames = tableFields[tableNames.indexOf(table)];
                    foreach(const QString &fieldName, fieldNames){
                        int colIdx = columnIndex(stmt, fieldName.toUtf8().constData());
                        if (colIdx < 0)
                            continue;
                        QString value = columnText(stmt, colIdx);
                        if(value.isEmpty() || value.isNull())
                            continue;
                        //排除字段名称重复的数据，主要是避免defaultPrimaryKeyValue（Journal字段）重复出现
                        if(!journalInfoFieldNames.contains(fieldName) || fieldName != defaultPrimaryKeyValue){
                            Pair pair(fieldName, columnText(stmt, colIdx));
                            journalInfo << pair;
                            journalInfoFieldNames << fieldName;
                        }
                    }
                }
                sqlite3_finalize(stmt);
                stmt = nullptr;
            }
        }
    }
    //查询输入不是期刊全称时，自动进行二次查询，显示完整信息;allowSelectAgain避免进入死循环
    if(allowSelectAgain and journalInfo.size() > 0 and journalInfo[0].first != defaultPrimaryKeyValue){
        foreach(const Pair &info, journalInfo){
            if(info.first == defaultPrimaryKeyValue){
                journalInfo = getJournalInfo(info.second, false);
                qInfo() << "auto select" << info.second;
                break;
            }
        }
    }
    return journalInfo;
}

void SqliteDB::selectTableNames(const QStringList &selectedtableNames)
{
    tableNames = selectedtableNames;
    // qDebug() << allTableNames;
    // qDebug() << tableNames;
    selectTableFields();
    setTablePrimaryKeys();
    selectAllJournalNames();
}

void SqliteDB::selectTableFields()
{
    tableFields.clear();
    sqlite3_stmt *stmt = nullptr;

    foreach(const QString &table, tableNames){
        QStringList fieldNames;
        if(db){
            QString pragma = QString("PRAGMA table_info(\"%1\")").arg(table);
            int rc = sqlite3_prepare_v2(db, pragma.toUtf8().constData(), -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                qWarning() << "Error: Failed to selectTableFields." << table << __FUNCTION__ << sqlite3_errmsg(db);
                tableFields << fieldNames;
                continue;
            }
            while (sqlite3_step(stmt) == SQLITE_ROW){
                // PRAGMA table_info returns: cid(0), name(1), type(2), notnull(3), dflt_value(4), pk(5)
                QString fieldName = columnText(stmt, 1);
                fieldNames << fieldName;
            }
            sqlite3_finalize(stmt);
        }
        tableFields << fieldNames;
    }
//    qDebug() << tableFields;

    Q_ASSERT(tableNames.size() == tableFields.size());
}

void SqliteDB::setTablePrimaryKeys()
{
    tablePrimaryKeys.clear();
    Q_ASSERT(tableNames.size() == tableFields.size());

    for(int i = 0; i < tableNames.size(); i++){

        Q_ASSERT(tableFields[i].contains(defaultPrimaryKeyValue));

        if(tableFields[i].contains(defaultPrimaryKeyValue)){
            tablePrimaryKeys << Pair(tableNames[i], defaultPrimaryKeyValue);
        }
        if(tableFields[i][0] != defaultPrimaryKeyValue){
            tablePrimaryKeys << Pair(tableNames[i], tableFields[i][0]);
        }
    }
//    qDebug() << tablePrimaryKeys;

    Q_ASSERT(tablePrimaryKeys.size() >= tableNames.size());
}

void SqliteDB::selectAllJournalNames()
{
    allKeyNames.clear();
    allJournalNamesList.clear();
    sqlite3_stmt *stmt = nullptr;

    foreach(const Pair &pair, tablePrimaryKeys){
        const QString &table = pair.first;
        const QString &primaryKey = pair.second;
        QStringList keyNames;
        if(db){
            QString select = QString("SELECT \"%1\" FROM \"%2\"").arg(primaryKey, table);
            int rc = sqlite3_prepare_v2(db, select.toUtf8().constData(), -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                qWarning() << "Error: Failed to select" << table << __FUNCTION__ << sqlite3_errmsg(db);
                allKeyNames << QStringList();
                continue;
            }
            while (sqlite3_step(stmt) == SQLITE_ROW){
                QString journalName = columnText(stmt, 0);
                keyNames << journalName;
            }
            sqlite3_finalize(stmt);
        }
//        qDebug() << keyNames.length();
        allKeyNames << keyNames;
//        allJournalNamesList += keyNames;
        //输入提示项去除大小写不一致的重复项
        foreach(const QString &keyName, keyNames){
            if(!allJournalNamesList.contains(keyName, Qt::CaseInsensitive))
                allJournalNamesList << keyName;
        }
    }
//    allJournalNamesList.removeDuplicates(); //  去重
//    allJournalNamesList.removeAll({});  //    去除空关键字
////    qDebug() << allJournalNamesList.length();
//    //不分区大小写排序，然后删除只有大小写不一致的项
//    allJournalNamesList.sort(Qt::CaseInsensitive);
//    for(int i = 1; i < allJournalNamesList.length(); i++){
//        if(allJournalNamesList[i].toLower() == allJournalNamesList[i-1].toLower()){
//            allJournalNamesList.removeAt(i);
//            i--;
//        }
//    }
    qDebug() << allJournalNamesList.length();

    Q_ASSERT(allKeyNames.size() == tablePrimaryKeys.size());
}

// 按照优先级重新排序表名
QStringList SqliteDB::sortSpecialStrings(const QStringList &input) {
    struct StringItem {
        QString original;  // 原始字符串
        QString prefix;    // 提取的前缀
        int year = 0;      // 提取的年份
        int priority = 0;  // 前缀优先级
    };
    // 定义前缀优先级规则
    const QHash<QString, int> kPrefixPriority = {
        {"GJQKYJMD", 0},
        {"JCR", 1},
        {"CCF", 2},
        {"CCFT", 3},
        {"XR", 4},
        {"FQBJCR", 5}
    };
    // 正则表达式提取前缀和年份
    const QRegularExpression kPattern("^(\\D+)(\\d+)$"); // 非数字部分+数字部分
    // 解析所有字符串
    QList<StringItem> items;
    for (const QString &s : input) {
        QRegularExpressionMatch match = kPattern.match(s);
        if (match.hasMatch()) {
            StringItem item;
            item.original = s;
            item.prefix = match.captured(1);
            item.year = match.captured(2).toInt();
            item.priority = kPrefixPriority.value(item.prefix, INT_MAX); // 未定义前缀设为最低优先级
            items.append(item);
        } else {
            // 无法解析的项放在末尾
            items.append({s, s, 0, INT_MAX});
        }
    }
    // 自定义排序规则
    std::sort(items.begin(), items.end(), [](const StringItem &a, const StringItem &b) {
        // 1. 按前缀优先级升序
        if (a.priority != b.priority) return a.priority < b.priority;
        // 2. 相同前缀按年份降序
        if (a.year != b.year) return a.year > b.year;
        // 3. 年份相同按原始字符串升序（可选）
        return a.original < b.original;
    });
    // 提取排序后的结果
    QStringList result;
    for (const auto &item : items) {
        result << item.original;
    }
    return result;
}
