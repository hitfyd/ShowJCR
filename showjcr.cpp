#include "showjcr.h"
#include "./ui_showjcr.h"
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>
#include <QMessageBox>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QCompleter>
#include <QMimeData>
#include <QMenu>

const QString ShowJCR::author = "hitfyd";
const QString ShowJCR::iconName = "jcr-logo.jpg"; //在程序自启动时，程序的运行目录是C:/WINDOWS/system32而不是程序目录，因此需要结合QApplication::applicationFilePath()修改
const QString ShowJCR::datasetName = "jcr.db";
const QString ShowJCR::defaultJournal = "National Science Review";

ShowJCR::ShowJCR(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ShowJCR)
{
    ui->setupUi(this);

    //获取程序运行信息
    appName = QApplication::applicationName();//程序名称
    appDir = QDir(QApplication::applicationDirPath());//程序目录（QDir类型）
    appPath = QApplication::applicationFilePath();// 程序路径
    QIcon icon(appDir.absoluteFilePath(iconName));
    QApplication::setWindowIcon(icon);//设置程序图标

    qDebug() << "start check:" << appName << appDir.path() << appPath << icon;

    //设置系统托盘
    m_systray.setToolTip(appName);//设置提示文字
    m_systray.setIcon(icon);//设置托盘图标
    QMenu * menu = new QMenu();
    menu->addAction(ui->actionExit);
    m_systray.setContextMenu(menu);//托盘菜单项
    m_systray.show();//显示托盘
    connect(&m_systray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(OnSystemTrayClicked(QSystemTrayIcon::ActivationReason)));//关联托盘事件
    connect(ui->actionExit, SIGNAL(triggered()), this, SLOT(OnExit()));//关联托盘菜单响应

    //设置剪切板监听
    connect(QApplication::clipboard(), SIGNAL(dataChanged()), this, SLOT(getClipboard()));

    //初始化期刊数据库
    sqliteDB = new SqliteDB(appDir, datasetName);

    //设置期刊输入自动联想
    QCompleter *pCompleter=new QCompleter(sqliteDB->getAllJournalNames(),this);
    pCompleter->setFilterMode(Qt::MatchContains);    //部分内容匹配
    pCompleter->setCaseSensitivity(Qt::CaseInsensitive);    //设置为大小写不敏感
    ui->lineEdit_journalName->setCompleter(pCompleter);

    //使用默认期刊进行查询，设置界面初始默认显示
    run(defaultJournal);

    //读取程序运行参数
    settings = new QSettings(author, appName);
    autoStart = settings->value("autoStart").toBool();
    exit2Taskbar = settings->value("exit2Taskbar").toBool();
    monitorClipboard = settings->value("monitorClipboard").toBool();
    autoActivateWindow = settings->value("autoActivateWindow").toBool();
    ui->checkBox_autoStart->setChecked(autoStart);
    ui->checkBox_exit2Taskbar->setChecked(exit2Taskbar);
    ui->checkBox_monitorClipboard->setChecked(monitorClipboard);
    ui->checkBox_autoActivateWindow->setChecked(autoActivateWindow);

    //检查程序自启动设置是否有效
    setAutoStart();
}

ShowJCR::~ShowJCR()
{
    delete ui;
    delete sqliteDB;

    //存储程序运行参数
    settings->setValue("autoStart", autoStart);
    settings->setValue("exit2Taskbar", exit2Taskbar);
    settings->setValue("monitorClipboard", monitorClipboard);
    settings->setValue("autoActivateWindow", autoActivateWindow);
}

void ShowJCR::on_pushButton_selectJournal_clicked()
{
    run(ui->lineEdit_journalName->text());
}

void ShowJCR::on_lineEdit_journalName_returnPressed()
{
    on_pushButton_selectJournal_clicked();
}

void ShowJCR::run(const QString &input)
{
    //输入简化，首尾空格清除，中间空格均变为1个，便于剪切板复制不精确时有效性
    QString journalName = input.simplified();
    //检查输入是否为空
    if(journalName.isEmpty()){
        ui->lineEdit_journalName->setText("请输入期刊名称！");
        return;
    }
    //检查输入是否在期刊数据库中，不区分大小写
    if(!sqliteDB->getAllJournalNames().contains(journalName, Qt::CaseInsensitive)){   //不区分大小写
        ui->lineEdit_journalName->setText("期刊不存在，请检查期刊名称！");
        return;
    }
    //输入正确，执行查询
    qDebug() << "select the journal:" << journalName;
    journalInfo = sqliteDB->getJournalInfo(journalName);
    updateGUI(journalName);
}

void ShowJCR::updateGUI(const QString &journalName)
{
    ui->tableView_journalInformation->setShowGrid(true);
    ui->tableView_journalInformation->setGridStyle(Qt::DashLine);
    ui->tableView_journalInformation->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    QStandardItemModel* model = new QStandardItemModel();
    QStandardItem* item = 0;
    for(int i = 0; i < journalInfo.size(); i++){
        item = new QStandardItem(journalInfo[i].first);
        model->setItem(i, 0, item);
        item = new QStandardItem(journalInfo[i].second);
        model->setItem(i, 1, item);
    }

    ui->tableView_journalInformation->setModel(model);
    ui->lineEdit_journalName->setText(journalName);
    if(autoActivateWindow){
        this->showNormal();
        this->activateWindow(); //激活窗口到前台
    }
}

void ShowJCR::setAutoStart()
{
    QString nativeAppPath = QDir::toNativeSeparators(appPath);
    QString autoStartValue = nativeAppPath + " autoStart";//便于判断程序是否为自启动，注意参数前面有空格
    QString regPath = "HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";//无需管理员权限，写入当前用户注册表
    QSettings reg(regPath, QSettings::NativeFormat);
    QString val = reg.value(appName).toString();// 如果此键不存在，则返回的是空字符串
    if(val != autoStartValue & autoStart){
        reg.setValue(appName,autoStartValue);
    }
    else if(val == autoStartValue & !autoStart){//移除自启动
        reg.remove(appName);
    }
}

void ShowJCR::closeEvent(QCloseEvent *event)
{
    if(exit2Taskbar){
        this->hide();
        event->ignore();
    }
    else{
        event->accept();
    }
}

void ShowJCR::getClipboard()
{
    if(monitorClipboard){
        const QClipboard *clipboard = QApplication::clipboard();
        const QMimeData *mimeData = clipboard->mimeData();
        if (mimeData->hasText()) {
            run(clipboard->text());
        }
    }
}

void ShowJCR::on_checkBox_autoStart_stateChanged(int arg1)
{
    autoStart = arg1;
    setAutoStart();
}

void ShowJCR::on_checkBox_exit2Taskbar_stateChanged(int arg1)
{
    exit2Taskbar = arg1;
}

void ShowJCR::on_checkBox_autoActivateWindow_stateChanged(int arg1)
{
    autoActivateWindow = arg1;
}

void ShowJCR::on_checkBox_monitorClipboard_stateChanged(int arg1)
{
    monitorClipboard = arg1;
}

int ShowJCR::OnSystemTrayClicked(QSystemTrayIcon::ActivationReason reason)
{
    if(reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick){
        // 显示主窗口
        this->showNormal();
    }
    return 0;
}

int ShowJCR::OnExit()
{
    QApplication::exit(0);
    return 0;
}
