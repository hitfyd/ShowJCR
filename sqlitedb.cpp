#include "sqlitedb.h"

#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QMessageBox>
#include  <QApplication>

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

QList<Pair> SqliteDB::getJournalInfo(const QString &journalName)
{
    Q_ASSERT(allJournalNamesList.contains(journalName, Qt::CaseInsensitive));
    Q_ASSERT(allJournalNames.size() == tablePrimaryKeys.size());

    QList<Pair> journalInfo;
    QSqlQuery query;
    for(int i = 0; i < allJournalNames.size(); i++){
        if(allJournalNames[i].contains(journalName, Qt::CaseInsensitive)){
            const QString &table = tablePrimaryKeys[i].first;
            const QString &primaryKey = tablePrimaryKeys[i].second;
            if(database.isOpen()){
                QString select = "select * from " + table + " where " + primaryKey + " = '" + journalName + "'COLLATE NOCASE";   //设置查询不区分大小写
                if (!query.exec(select)){
                    qWarning() << "Error: Failed to select " << table << __FUNCTION__ << database.lastError();
                }
                if (query.first()){
                    QStringList fieldNames = tableFields[tableNames.indexOf(table)];
                    foreach(const QString &fieldName, fieldNames){
                        QString value = query.value(fieldName).toString();
                        if(value.isEmpty() || value.isNull())
                            continue;
                        Pair pair(fieldName, query.value(fieldName).toString());
                        if(!journalInfo.contains(pair))
                            journalInfo << pair;
                    }
                }
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
    //临时逻辑：判断表字段是否包含“Journal”，包含则作为主键，同时如果表的第一个字段不是“Journal”，则第一个字段也作为主键
    QString primaryKey = "Journal";

    Q_ASSERT(tableNames.size() == tableFields.size());

    for(int i = 0; i < tableNames.size(); i++){

        Q_ASSERT(tableFields[i].contains(primaryKey));

        if(tableFields[i].contains(primaryKey)){
            tablePrimaryKeys << Pair(tableNames[i], primaryKey);
        }
        if(tableFields[i][0] != primaryKey){
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
        allJournalNames << keyNames;
        allJournalNamesList += keyNames;
    }
    allJournalNamesList.removeDuplicates(); //  去重
    allJournalNamesList.removeAll({});  //    去除空关键字
//    qDebug() << allJournalNamesList.length();

    Q_ASSERT(allJournalNames.size() == tablePrimaryKeys.size());
}
