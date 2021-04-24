#include "showjcr.h"
#include "./ui_showjcr.h"
#include <QMessageBox>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QCompleter>
#include <QMimeData>
#include <QMenu>

const QString ShowJCR::author = "hitfyd";
const QString ShowJCR::version = "v2021-1.3";
const QString ShowJCR::email = "hitfyd@foxmail.com";
const QString ShowJCR::codeURL = "https://gitee.com/hitfyd/ShowJCR";
const QString ShowJCR::updateURL = "https://gitee.com/hitfyd/ShowJCR/releases";
const QString ShowJCR::logoIconName = ":/image/jcr-logo.jpg";
const QString ShowJCR::datasetName = "jcr.db";  //数据集暂时无法使用资源文件；在程序自启动时，程序的运行目录是C:/WINDOWS/system32而不是程序目录，因此需要结合QApplication::applicationFilePath()修改
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

    qDebug() << "start check:" << appName << appDir.path() << appPath;

    //设置菜单
    menu = new QMenu();
    menu->addAction(ui->actionAbout);
    menu->addSeparator();//添加分隔线
    menu->addAction(ui->actionExit);
    //关联托盘菜单响应
    connect(ui->actionAbout, SIGNAL(triggered()), this, SLOT(show_about()));
    connect(ui->actionExit, SIGNAL(triggered()), this, SLOT(OnExit()));

    //设置系统托盘
    m_systray.setToolTip(appName);//设置提示文字
    m_systray.setIcon(QIcon(logoIconName));//设置托盘图标
    m_systray.setContextMenu(menu);//托盘菜单项
    m_systray.show();//显示托盘
    connect(&m_systray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(OnSystemTrayClicked(QSystemTrayIcon::ActivationReason)));//关联托盘事件

    //初始化关于窗口
    aboutDialog = new AboutDialog(appName, version, email, codeURL, updateURL, this);

    //设置剪切板监听
    connect(QApplication::clipboard(), SIGNAL(dataChanged()), this, SLOT(getClipboard()));

    //初始化期刊数据库
    sqliteDB = new SqliteDB(appDir, datasetName);

    //设置期刊名称输入框提示文字
    ui->lineEdit_journalName->setPlaceholderText(cueWords[0]);

    //设置期刊输入自动联想
    QCompleter *pCompleter=new QCompleter(sqliteDB->getAllJournalNames(),this);
    pCompleter->setFilterMode(Qt::MatchContains);    //部分内容匹配
    pCompleter->setCaseSensitivity(Qt::CaseInsensitive);    //设置为大小写不敏感
    ui->lineEdit_journalName->setCompleter(pCompleter);

    //使用默认期刊进行查询，设置界面初始默认显示
//    run(defaultJournal);

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
    //存储程序运行参数
    settings->setValue("autoStart", autoStart);
    settings->setValue("exit2Taskbar", exit2Taskbar);
    settings->setValue("monitorClipboard", monitorClipboard);
    settings->setValue("autoActivateWindow", autoActivateWindow);

    delete menu;
    delete aboutDialog;
    delete ui;
    delete sqliteDB;
    delete settings;
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
    QString tempJournalName = input.simplified();
    //忽略重复查询
    if(journalName == tempJournalName){
        return;
    }
    journalName = tempJournalName;
    //检查输入是否为空
    if(journalName.isEmpty()){
//        ui->lineEdit_journalName->setText(cueWords[0]);
        return;
    }
    //检查输入是否在期刊数据库中，不区分大小写；如果输入为带'.'的缩写，删除'.'后重新检查
    if(!sqliteDB->getAllJournalNames().contains(journalName, Qt::CaseInsensitive) and !sqliteDB->getAllJournalNames().contains(journalName.remove('.'), Qt::CaseInsensitive)){   //不区分大小写
        if(!ui->lineEdit_journalName->text().contains(cueWords[1])){
            ui->lineEdit_journalName->setText(cueWords[1] + ui->lineEdit_journalName->text());
        }
        return;
    }
    //输入正确，执行查询
    qDebug() << "select the journal:" << journalName;
    journalInfo = sqliteDB->getJournalInfo(journalName);
    updateGUI();
}

void ShowJCR::updateGUI()
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

void ShowJCR::on_lineEdit_journalName_textEdited(const QString &arg1)
{
    if(arg1.contains(cueWords[1])){
        ui->lineEdit_journalName->setText(arg1.split(cueWords[1]).last());
    }
}

void ShowJCR::on_toolButton_list_clicked()
{
    QPoint pos;
    pos.setX(0);
    pos.setY(ui->toolButton_list->sizeHint().height());
    menu->exec(ui->toolButton_list->mapToGlobal(pos));
}

void ShowJCR::show_about()
{
    aboutDialog->show();
}
