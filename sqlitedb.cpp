#include "sqlitedb.h"

#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QMessageBox>
#include <QApplication>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <algorithm>
#include <climits>

SqliteDB::SqliteDB(const QDir &appDir, const QString &datasetName, QObject *parent) : QObject(parent)
{
    //连接SQLite3数据库"jcr.db"，该数据集应放在运行目录下
    database = QSqlDatabase::addDatabase("QSQLITE");
    database.setDatabaseName(appDir.absoluteFilePath(datasetName));
    database.setConnectOptions("QSQLITE_OPEN_READONLY");//设置连接属性：当数据库不存在时不自动创建
    //    qDebug() << database;
    if (!database.open())
    {
        qWarning() << "Error: Failed to connect database." << __FUNCTION__ << database.lastError();
        QMessageBox::warning(QApplication::activeWindow(), "期刊信息数据库缺失！", database.lastError().text());
    }
    else
    {
        qDebug() << "Successed to connect database.";
    }

   allTableNames = database.tables();
    // 重排表名顺序
    allTableNames = sortSpecialStrings(allTableNames);
    //selectTableNames(allTableNames);	//避免启动时执行两次
}

SqliteDB::~SqliteDB()
{
    if(database.isOpen()){
        database.close();
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
    QSqlQuery query;
    for(int i = 0; i < allKeyNames.size(); i++){
        if(allKeyNames[i].contains(journalName, Qt::CaseInsensitive)){
            const QString &table = tablePrimaryKeys[i].first;
            const QString &primaryKey = tablePrimaryKeys[i].second;
            if(database.isOpen()){
                QString select = "select * from " + table + " where " + primaryKey + " = '" + journalName + "' COLLATE NOCASE";   //设置查询不区分大小写
                if (!query.exec(select)){
                    qWarning() << "Error: Failed to select " << table << __FUNCTION__ << database.lastError();
                }
                //CCF推荐期刊中不同领域存在重复的期刊
                while (query.next()){
                    QStringList fieldNames = tableFields[tableNames.indexOf(table)];
                    foreach(const QString &fieldName, fieldNames){
                        QString value = query.value(fieldName).toString();
                        if(value.isEmpty() || value.isNull())
                            continue;
                        //排除字段名称重复的数据，主要是避免defaultPrimaryKeyValue（Journal字段）重复出现
                        if(!journalInfoFieldNames.contains(fieldName) || fieldName != defaultPrimaryKeyValue){
                            Pair pair(fieldName, query.value(fieldName).toString());
                            journalInfo << pair;
                            journalInfoFieldNames << fieldName;
                        }
                    }
                }
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
    QSqlQuery query;
    foreach(const QString &table, tableNames){
        QStringList fieldNames;
        if(database.isOpen()){
            QString select = "PRAGMA table_info(" + table + ")";
            if (!query.exec(select)){
                qWarning() << "Error: Failed to selectTableFields." << table << __FUNCTION__ << database.lastError();
            }
            while (query.next()){
                QString fieldName = query.value(1).toString();  //  返回格式为：“字段序号、字段名称、字段类型”，这里只提取字段名称
                fieldNames << fieldName;
            }
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
    QSqlQuery query;
    foreach(const Pair &pair, tablePrimaryKeys){
        const QString &table = pair.first;
        const QString &primaryKey = pair.second;
        QStringList keyNames;
        if(database.isOpen()){
            QString select = "select " + primaryKey + " from " + table;
            if (!query.exec(select)){
                qWarning() << "Error: Failed to select" << table << __FUNCTION__ << database.lastError();
            }
            while (query.next()){
                QString journalName = query.value(0).toString();
                keyNames << journalName;
            }
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
    rebuildJournalAliases();
}

void SqliteDB::rebuildJournalAliases()
{
    journalAliasToCanonical.clear();
    if (!database.isOpen())
        return;

    static const QStringList aliasFields = {
        QStringLiteral("刊名"),
        QStringLiteral("中文刊名"),
    };

    QSqlQuery query;
    for (int i = 0; i < tableNames.size(); ++i) {
        const QString &table = tableNames[i];
        const QStringList &fields = tableFields[i];
        if (!fields.contains(defaultPrimaryKeyValue))
            continue;

        for (const QString &aliasField : aliasFields) {
            if (!fields.contains(aliasField))
                continue;

            const QString select = QString("SELECT %1, %2 FROM %3 WHERE %2 IS NOT NULL AND TRIM(%2) != ''")
                                       .arg(defaultPrimaryKeyValue, aliasField, table);
            if (!query.exec(select))
                continue;

            while (query.next()) {
                const QString journal = query.value(0).toString().trimmed();
                const QString alias = query.value(1).toString().trimmed();
                if (journal.isEmpty() || alias.isEmpty())
                    continue;

                journalAliasToCanonical.insert(alias.toLower(), journal);

                QString simplified = alias;
                simplified.remove(QRegularExpression(QStringLiteral("\\s*\\([^)]*\\)\\s*$")));
                simplified = simplified.trimmed();
                if (!simplified.isEmpty())
                    journalAliasToCanonical.insert(simplified.toLower(), journal);
            }
        }
    }
}

QString SqliteDB::resolveToCanonicalJournal(const QString &input) const
{
    const QString query = input.trimmed();
    if (query.isEmpty())
        return QString();

    for (const QString &name : allJournalNamesList) {
        if (name.compare(query, Qt::CaseInsensitive) == 0)
            return name;
    }

    const QString lowerQuery = query.toLower();
    if (journalAliasToCanonical.contains(lowerQuery))
        return journalAliasToCanonical.value(lowerQuery);

    for (auto it = journalAliasToCanonical.constBegin(); it != journalAliasToCanonical.constEnd(); ++it) {
        if (it.key().compare(query, Qt::CaseInsensitive) == 0)
            return it.value();
    }

    return QString();
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
        {"FQBJCR", 0},
        {"GJQKYJMD", 1},
        {"JCR", 2},
        {"CCF", 3},
        {"CCFT", 4},
        {"XR", 5}
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

QStringList SqliteDB::tokenizeJournalName(const QString &text)
{
    QString normalized = text.toLower();
    normalized.replace(QRegularExpression("[^a-z0-9]+"), " ");
    return normalized.simplified().split(' ', Qt::SkipEmptyParts);
}

QString SqliteDB::expandJournalToken(const QString &token)
{
    static const QHash<QString, QString> abbreviations = {
        {"sci", "science"},
        {"math", "mathematics"},
        {"mat", "mathematics"},
        {"maths", "mathematics"},
        {"chem", "chemistry"},
        {"phys", "physics"},
        {"eng", "engineering"},
        {"comp", "computer"},
        {"comm", "communications"},
        {"comms", "communications"},
        {"jour", "journal"},
        {"intl", "international"},
        {"int", "international"},
        {"appl", "applied"},
        {"res", "research"},
        {"rev", "review"},
        {"lett", "letters"},
        {"proc", "proceedings"},
        {"trans", "transactions"},
        {"acad", "academy"},
        {"nat", "national"},
        {"natl", "national"},
        {"soc", "society"},
        {"tech", "technology"},
        {"info", "information"},
        {"sys", "systems"},
        {"med", "medicine"},
        {"bio", "biology"},
        {"biol", "biology"},
        {"econ", "economics"},
        {"fin", "finance"},
        {"manag", "management"},
        {"admin", "administration"},
        {"edu", "education"},
        {"psych", "psychology"},
        {"stat", "statistics"},
        {"astro", "astronomical"},
        {"nucl", "nuclear"},
        {"mol", "molecular"},
        {"physiol", "physiology"},
        {"arch", "archives"},
        {"bull", "bulletin"},
        {"ann", "annals"},
        {"rep", "reports"},
        {"adv", "advanced"},
        {"advances", "advanced"},
        {"acta", "acta"},
        {"china", "china"},
    };
    return abbreviations.value(token, token);
}

QSet<QString> SqliteDB::expandedJournalTokens(const QString &text)
{
    static const QSet<QString> stopWords = {
        "a", "an", "the", "of", "and", "in", "on", "for", "to", "with", "by", "at", "or", "from"
    };

    QSet<QString> tokens;
    for (const QString &token : tokenizeJournalName(text)) {
        if (token.size() < 2 || stopWords.contains(token))
            continue;
        tokens.insert(expandJournalToken(token));
    }
    return tokens;
}

bool SqliteDB::journalTokenMatches(const QString &queryToken, const QSet<QString> &nameTokens, bool allowPrefix)
{
    const QString expanded = expandJournalToken(queryToken);
    if (nameTokens.contains(expanded))
        return true;

    if (!allowPrefix || queryToken.size() < 2)
        return false;

    for (const QString &nameToken : nameTokens) {
        if (nameToken.startsWith(queryToken) || nameToken.startsWith(expanded))
            return true;
    }
    return false;
}

bool SqliteDB::journalMatchesQuery(const QStringList &queryTokens, const QSet<QString> &nameTokens, bool allowPrefixOnLast)
{
    if (queryTokens.isEmpty())
        return false;

    for (int i = 0; i < queryTokens.size(); ++i) {
        const bool allowPrefix = allowPrefixOnLast && i == queryTokens.size() - 1;
        if (!journalTokenMatches(queryTokens[i], nameTokens, allowPrefix))
            return false;
    }
    return true;
}

int SqliteDB::journalMatchScore(const QSet<QString> &queryTokens, const QSet<QString> &nameTokens, int matchedCount)
{
    const int extraTokens = nameTokens.size() - queryTokens.size();
    return matchedCount * 100 - qMax(0, extraTokens);
}

QString SqliteDB::findJournalName(const QString &input) const
{
    const QString query = input.trimmed();
    if (query.isEmpty())
        return QString();

    const QString canonicalFromAlias = resolveToCanonicalJournal(query);
    if (allJournalNamesList.contains(canonicalFromAlias, Qt::CaseInsensitive))
        return canonicalFromAlias;

    for (const QString &name : allJournalNamesList) {
        if (name.compare(query, Qt::CaseInsensitive) == 0)
            return name;
    }

    QString withoutDots = query;
    withoutDots.remove('.');
    for (const QString &name : allJournalNamesList) {
        if (name.compare(withoutDots, Qt::CaseInsensitive) == 0)
            return name;
    }

    if (query.size() >= 2) {
        QString bestContains;
        int bestLength = INT_MAX;
        for (const QString &name : allJournalNamesList) {
            if (!name.contains(query, Qt::CaseInsensitive))
                continue;
            if (name.length() < bestLength) {
                bestLength = name.length();
                bestContains = name;
            }
        }
        if (!bestContains.isEmpty())
            return bestContains;
    }

    static const QSet<QString> stopWords = {
        "a", "an", "the", "of", "and", "in", "on", "for", "to", "with", "by", "at", "or", "from"
    };

    QStringList queryTokens;
    for (const QString &token : tokenizeJournalName(query)) {
        if (token.size() < 2 || stopWords.contains(token))
            continue;
        queryTokens.append(token);
    }
    if (queryTokens.isEmpty())
        return QString();

    QString bestMatch;
    int bestScore = 0;
    for (const QString &name : allJournalNamesList) {
        const QSet<QString> nameTokens = expandedJournalTokens(name);
        if (!journalMatchesQuery(queryTokens, nameTokens, true))
            continue;

        const int score = journalMatchScore(expandedJournalTokens(query), nameTokens, queryTokens.size());
        if (score > bestScore) {
            bestScore = score;
            bestMatch = name;
        }
    }

    return bestMatch;
}

QStringList SqliteDB::findJournalMatches(const QString &input, int limit) const
{
    QStringList results;
    const QString query = input.trimmed();
    if (query.isEmpty() || limit <= 0)
        return results;

    QSet<QString> seen;

    for (const QString &name : allJournalNamesList) {
        if (name.contains(query, Qt::CaseInsensitive)) {
            results.append(name);
            seen.insert(name);
            if (results.size() >= limit)
                return results;
        }
    }

    const QString lowerQuery = query.toLower();
    for (auto it = journalAliasToCanonical.constBegin(); it != journalAliasToCanonical.constEnd(); ++it) {
        if (!it.key().contains(lowerQuery))
            continue;
        const QString &journal = it.value();
        if (seen.contains(journal))
            continue;
        results.append(journal);
        seen.insert(journal);
        if (results.size() >= limit)
            return results;
    }

    static const QSet<QString> stopWords = {
        "a", "an", "the", "of", "and", "in", "on", "for", "to", "with", "by", "at", "or", "from"
    };

    QStringList queryTokens;
    for (const QString &token : tokenizeJournalName(query)) {
        if (token.size() < 2 || stopWords.contains(token))
            continue;
        queryTokens.append(token);
    }
    if (queryTokens.isEmpty())
        return results;

    QList<QPair<int, QString>> scoredMatches;
    const QSet<QString> expandedQueryTokens = expandedJournalTokens(query);
    for (const QString &name : allJournalNamesList) {
        if (seen.contains(name))
            continue;

        const QSet<QString> nameTokens = expandedJournalTokens(name);
        if (!journalMatchesQuery(queryTokens, nameTokens, true))
            continue;

        int matched = 0;
        for (const QString &token : expandedQueryTokens) {
            if (nameTokens.contains(token))
                matched++;
        }
        scoredMatches.append(qMakePair(
            journalMatchScore(expandedQueryTokens, nameTokens, matched),
            name));
    }

    std::sort(scoredMatches.begin(), scoredMatches.end(),
              [](const QPair<int, QString> &a, const QPair<int, QString> &b) {
                  if (a.first != b.first)
                      return a.first > b.first;
                  return a.second.size() < b.second.size();
              });

    for (const QPair<int, QString> &item : scoredMatches) {
        results.append(item.second);
        if (results.size() >= limit)
            break;
    }

    return results;
}

QStringList SqliteDB::getDisplayTableNames()
{
    return tableNames;
}

QMap<QString, QList<Pair>> SqliteDB::getJournalInfoByTable(const QString &journalName)
{
    QMap<QString, QList<Pair>> result;

    const QString canonicalName = resolveToCanonicalJournal(journalName);
    if (!allJournalNamesList.contains(canonicalName, Qt::CaseInsensitive))
        return result;

    QString resolvedName = canonicalName;

    // 先尝试解析全称（与 getJournalInfo 的二次查询逻辑一致）
    QSqlQuery query;
    for (int i = 0; i < allKeyNames.size(); i++) {
        if (allKeyNames[i].contains(resolvedName, Qt::CaseInsensitive)) {
            const QString &table = tablePrimaryKeys[i].first;
            const QString &primaryKey = tablePrimaryKeys[i].second;
            if (primaryKey != defaultPrimaryKeyValue && database.isOpen()) {
                QString select = "select " + defaultPrimaryKeyValue + " from " + table
                                 + " where " + primaryKey + " = '" + resolvedName + "' COLLATE NOCASE";
                if (query.exec(select) && query.next()) {
                    resolvedName = query.value(0).toString();
                    break;
                }
            }
        }
    }

    for (int i = 0; i < allKeyNames.size(); i++) {
        if (allKeyNames[i].contains(resolvedName, Qt::CaseInsensitive)) {
            const QString &table = tablePrimaryKeys[i].first;
            const QString &primaryKey = tablePrimaryKeys[i].second;
            if (!database.isOpen()) continue;

            QString select = "select * from " + table + " where " + primaryKey
                             + " = '" + resolvedName + "' COLLATE NOCASE";
            if (!query.exec(select)) continue;

            while (query.next()) {
                QStringList fieldNames = tableFields[tableNames.indexOf(table)];
                QList<Pair> &tableInfo = result[table];
                for (const QString &fieldName : fieldNames) {
                    QString value = query.value(fieldName).toString();
                    if (value.isEmpty() || value.isNull()) continue;
                    tableInfo << Pair(fieldName, value);
                }
            }
        }
    }

    return result;
}
