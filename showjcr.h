#ifndef SHOWJCR_H
#define SHOWJCR_H

#include "sqlitedb.h"

#include <QWidget>
#include <QClipboard>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QCloseEvent>
#include <QDir>
#include <QColor>
#include <aboutdialog.h>
#include <tableselectordialog.h>

QT_BEGIN_NAMESPACE
namespace Ui { class ShowJCR; }
QT_END_NAMESPACE

class ShowJCR : public QWidget
{
    Q_OBJECT

public:
    ShowJCR(QWidget *parent = nullptr);
    ~ShowJCR();

private slots:
    void on_pushButton_selectJournal_clicked();

    void on_lineEdit_journalName_returnPressed();

    //从剪切板获取期刊名称
    void getClipboard();

    void on_checkBox_autoStart_stateChanged(int arg1);

    void on_checkBox_exit2Taskbar_stateChanged(int arg1);

    void on_checkBox_autoActivateWindow_stateChanged(int arg1);

    void on_checkBox_monitorClipboard_stateChanged(int arg1);

    int OnSystemTrayClicked(QSystemTrayIcon::ActivationReason reason);

    int OnExit();

    void on_lineEdit_journalName_textEdited(const QString &arg1);

    void on_toolButton_list_clicked();

    void show_selectTable();

    void show_about();

private:
    //系统提示语
    const QString cueWords[3] = {"请输入期刊名称！", "请检查期刊名称！", "请至少选择一个表！"};
    //强调颜色
    const QColor color_header = QColor(119, 136, 153);  //lightslategray
    const QColor color_highlight = QColor(240, 128, 128);   //lightcoral
    //软件常量，包括作者信息、资源文件路径等
    static const QString author;
    static const QString version;
    static const QString email;
    static const QString codeURL;
    static const QString updateURL;
    static const QString windowTitile;
    static const QString logoIconName;//程序图标名称（路径）
    static const QString datasetName;//数据库名称（路径）
    static const QString defaultJournal;//默认的查询期刊名称

    Ui::ShowJCR *ui;
    SqliteDB *sqliteDB;
    //初始化数据
    QString appName;//程序名称
    QDir appDir;//程序目录
    QString appPath;// 程序路径

    QMenu * menu;//菜单
    QSystemTrayIcon m_systray;//系统托盘
    TableSelectorDialog *selectTableDialog;//数据表选择窗口
    AboutDialog *aboutDialog;//关于窗口

    //程序运行参数
    QSettings *settings;
    bool autoStart = false; //是否开机自启动
    bool exit2Taskbar = false;  //是否退出到任务栏
    bool monitorClipboard = false;  //是否监听剪切板
    bool autoActivateWindow = false;    //查询成功后是否激活到前台显示，用于根据剪切板查询成功后显示
    QStringList selectedTables;

//    //期刊分区信息结构体
//    struct Division{
//        QString name;   //分区名称
//        int level;  //合法值只包括:1、2、3、4
//    };

    //期刊信息
    QList<Pair> journalInfo;    //期刊详细信息
    QString journalName;    //期刊名称，用于输入和查询
//    int year;   //中科院分区表升级版发布年份
//    QString ISSN;   //国际标准连续出版物号（International Standard Serial Number，ISSN）
//    QString review;    //是否为Review期刊
//    QString openAccess;    //是否为OpenAccess期刊
//    QString webofScience;   //SCI收录类型，分为SCI、SCIE、SSCI、ESCI等
//    Division majorDivision; //大类分区信息
//    QString top;   //是否为大类顶刊
//    QVector<Division> subDivisions; //小类分区信息向量，一本期刊通常包含一至多个小类分区
//    QString impactFactor; //JCR影响因子，极少部分期刊没有影响因子，如ESCI期刊
//    QString warningLevel;   //中科院分区表国际期刊预警等级，分为高、中、低；不在名单中则为无。

    //当前数据库中包含的所有有效期刊名称，用于输入联想和判断输入期刊名称是否正确
//    QStringList allJournalNamesList;

    //输入检查，运行核心查询及更新功能
    void run(const QString &input);
    //更新界面显示的期刊信息
    void updateGUI();
    //设置程序开机自启动（设置和移除）
    void setAutoStart();
    //重写closeEvent
    void closeEvent(QCloseEvent *event);
};
#endif // SHOWJCR_H
