#ifndef SQLITEDB_H
#define SQLITEDB_H

#include <QObject>
#include <QDir>
#include <QMultiMap>
#include <QtSql/QSqlDatabase>

typedef QPair<QString, QString> Pair;

class SqliteDB : public QObject
{
    Q_OBJECT
public:
    explicit SqliteDB(const QDir &appDir, const QString &datasetName, QObject *parent = nullptr);
    ~SqliteDB();
    QStringList getAllJournalNames();   //  返回当前数据库中包含的所有有效期刊名称，用于输入联想和判断输入期刊名称是否正确
    QList<Pair> getJournalInfo(const QString &journalName);

private:
    QSqlDatabase database;
    QStringList tableNames;  // 存储数据库中所有表的名字
    QList<QStringList> tableFields;  // 存储表对应的字段名称，存储顺序和tableNames一一对应
    QList<Pair> tablePrimaryKeys;    // 存储表（Key）及其对应的主键字段名称(T)，注意一个表可能不止一个主键
    QList<QStringList> allJournalNames;    //存储表及其主键列中的所有值，存储顺序和tablePrimaryKeys一一对应，可以组合成当前数据库中包含的所有有效期刊名称，用于输入联想和判断输入期刊名称是否正确
    QStringList allJournalNamesList;    //组合成当前数据库中包含的所有有效期刊名称，用于输入联想和判断输入期刊名称是否正确

    void selectTableNames();    //  查询数据库获取所有表名称，存储在tableNames中
    void selectTableFields();   //  根据tableNames，依次查询对应表的字段名称，存储在tableFields中
    void setTablePrimaryKeys(); //  设置表及其对应的主键，通常一个表对应一个主键，个别表可能有多个主键，存储在tablePrimaryKeys中
    void selectAllJournalNames();    //根据tablePrimaryKeys，查询数据库中所有表对应主键的值作为期刊目录，存储在allJournalNames中
signals:

};

#endif // SQLITEDB_H
