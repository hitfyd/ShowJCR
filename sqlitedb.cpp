#include "sqlitedb.h"

#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QMessageBox>
#include <QApplication>

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

    selectTableNames();
    selectTableFields();
    setTablePrimaryKeys();
    selectAllJournalNames();
}

SqliteDB::~SqliteDB()
{
    if(database.isOpen()){
        database.close();
    }
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

void SqliteDB::selectTableNames()
{
    tableNames = database.tables();
//    qDebug() << tableNames;
}

void SqliteDB::selectTableFields()
{
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
}
