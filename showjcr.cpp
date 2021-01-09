#include "showjcr.h"
#include "./ui_showjcr.h"
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>
#include <QMessageBox>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QCompleter>
#include <QMimeData>
#include <QDir>
#include <QMenu>

const QString ShowJCR::author = "hitfyd";
const QString ShowJCR::iconName = "jcr-logo.jpg";
const QString ShowJCR::datasetName = "jcr.db";
const QString ShowJCR::defaultJournal = "National Science Review";

ShowJCR::ShowJCR(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ShowJCR)
{
    ui->setupUi(this);
    appName = QApplication::applicationName();//程序名称
    appPath = QApplication::applicationFilePath();// 程序路径
    QApplication::setWindowIcon(QIcon(iconName));//设置程序图标

    //设置系统托盘
    m_systray.setToolTip(appName);//设置提示文字
    m_systray.setIcon(QIcon(iconName));//设置托盘图标
    QMenu * menu = new QMenu();
    menu->addAction(ui->actionExit);
    m_systray.setContextMenu(menu);//托盘菜单项
    m_systray.show();//显示托盘
    connect(&m_systray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(OnSystemTrayClicked(QSystemTrayIcon::ActivationReason)));//关联托盘事件
    connect(ui->actionExit, SIGNAL(triggered()), this, SLOT(OnExit()));//关联托盘菜单响应

    //设置剪切板监听
    connect(QApplication::clipboard(), SIGNAL(dataChanged()), this, SLOT(getClipboard()));

    //连接SQLite3数据库"jcr.db"，该数据集应放在运行目录下
    database = QSqlDatabase::addDatabase("QSQLITE");
    database.setDatabaseName(datasetName);
    database.setConnectOptions("QSQLITE_OPEN_READONLY");//设置连接属性：当数据库不存在时不自动创建
    if (!database.open())
    {
        qDebug() << "Error: Failed to connect database." << database.lastError();
        QMessageBox::warning(this, "期刊信息数据库缺失！", database.lastError().text());
    }
    else
    {
        qDebug() << "Successed to connect database.";
        //使用默认期刊进行查询，设置界面初始默认显示
        journalName = defaultJournal;
        run();
    }

    //设置期刊输入自动联想
    QCompleter *pCompleter=new QCompleter(selectAllJournalNames(),this);
    pCompleter->setFilterMode(Qt::MatchContains);    //部分内容匹配
    pCompleter->setCaseSensitivity(Qt::CaseInsensitive);    //设置为大小写不敏感
    ui->lineEdit_journalName->setCompleter(pCompleter);

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
    if(database.isOpen()){
        database.close();
    }
    //存储程序运行参数
    settings->setValue("autoStart", autoStart);
    settings->setValue("exit2Taskbar", exit2Taskbar);
    settings->setValue("monitorClipboard", monitorClipboard);
    settings->setValue("autoActivateWindow", autoActivateWindow);
}

void ShowJCR::on_pushButton_selectJournal_clicked()
{
    journalName = ui->lineEdit_journalName->text();
    if(journalName.isEmpty()){
        ui->lineEdit_journalName->setText("请输入期刊名称！");
        return;
    }
    run();
}

void ShowJCR::on_lineEdit_journalName_returnPressed()
{
    on_pushButton_selectJournal_clicked();
}

void ShowJCR::selectZKYFQB()
{
    if(database.isOpen()){
        QSqlQuery query;
        QString select = "select * from fqb where Journal = '" + journalName + "'COLLATE NOCASE";   //设置查询不区分大小写
        if (!query.exec(select)){
            qDebug() << "Error: Failed to selectZKYFQB." << database.lastError();
        }
        if (query.first()){
            journalName = query.value(0).toString();
            year = query.value(1).toInt();
            ISSN = query.value(2).toString();
            review = query.value(3).toString();
            openAccess = query.value(4).toString();
            webofScience = query.value(5).toString();
            majorDivision.name = query.value(6).toString();
            majorDivision.level = query.value(7).toInt();
            top = query.value(8).toString();
            int subDivisionIndex = 9;
            subDivisions.clear();
            while(query.value(subDivisionIndex).isValid() & !query.value(subDivisionIndex).isNull()){
                Division subDivision;
                subDivision.name = query.value(subDivisionIndex).toString();
                subDivision.level = query.value(subDivisionIndex + 1).toInt();
                subDivisions.append(subDivision);
                subDivisionIndex += 2;
            }
            selectSuccess = true;

            qDebug() << journalName << year << ISSN << webofScience << majorDivision.name << majorDivision.level << subDivisions[0].name << subDivisions[0].level;
        }
    }
}

void ShowJCR::selectImpactFactor()
{
    if(database.isOpen() & selectSuccess){
        QSqlQuery query;
        QString select = "select * from jcr where Journal = '" + journalName + "' COLLATE NOCASE";
        if (!query.exec(select)){
            qDebug() << "Error: Failed to selectImpactFactor." << database.lastError();
        }
        if (query.first()){
            impactFactor = query.value(2).toString();
        }
        else{
            impactFactor = "";  //查询无结果，SCI中不包含该期刊
        }
        qDebug() << journalName << impactFactor;
    }
}

void ShowJCR::selectWarningLevel()
{
    if(database.isOpen() & selectSuccess){
        QSqlQuery query;
        QString select = "select * from warning where Journal = '" + journalName + "'COLLATE NOCASE";
        if (!query.exec(select)){
            qDebug() << "Error: Failed to selectWarningLevel." << database.lastError();
        }
        if (query.first()){
            warningLevel = query.value(1).toString();
        }
        else{
            warningLevel = ""; //查询无结果，不在国际期刊预警名单中
        }
        qDebug() << journalName << warningLevel;
    }
}

QStringList ShowJCR::selectAllJournalNames()
{
    QStringList journalNames;
    if(database.isOpen()){
        QSqlQuery query;
        QString select = "select Journal from fqb";
        if (!query.exec(select)){
            qDebug() << "Error: Failed to selectJournalisExicted." << database.lastError();
        }
        while (query.next()){
            journalName = query.value(0).toString();
            journalNames << journalName;
        }
    }
    qDebug() << journalNames.length();
    return journalNames;
}

void ShowJCR::run()
{
    selectSuccess = false;
    journalName = journalName.simplified();//输入简化，首尾空格清除，中间空格均变为1个，便于剪切板复制不精确时有效性
    selectZKYFQB();
    selectImpactFactor();
    selectWarningLevel();
    updateGUI();
}

void ShowJCR::updateGUI()
{
    if(selectSuccess){
        ui->tableView_journalInformation->setShowGrid(true);
        ui->tableView_journalInformation->setGridStyle(Qt::DotLine);
        ui->tableView_journalInformation->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

        QStandardItemModel* model = new QStandardItemModel();
        QStandardItem* item = 0;
        item = new QStandardItem("Journal名称");
        model->setItem(0, 0, item);
        item = new QStandardItem(journalName);
        model->setItem(0, 1, item);
        item = new QStandardItem("");
        model->setItem(0, 2, item);
        item = new QStandardItem("年份");
        model->setItem(1, 0, item);
        item = new QStandardItem(QString::number(year));
        model->setItem(1, 1, item);
        item = new QStandardItem("");
        model->setItem(1, 2, item);
        item = new QStandardItem("ISSN");
        model->setItem(2, 0, item);
        item = new QStandardItem(ISSN);
        model->setItem(2, 1, item);
        item = new QStandardItem("");
        model->setItem(2, 2, item);
        item = new QStandardItem("Review");
        model->setItem(3, 0, item);
        item = new QStandardItem(review);
        model->setItem(3, 1, item);
        item = new QStandardItem("");
        model->setItem(3, 2, item);
        item = new QStandardItem("Open Access");
        model->setItem(4, 0, item);
        item = new QStandardItem(openAccess);
        model->setItem(4, 1, item);
        item = new QStandardItem("");
        model->setItem(4, 2, item);
        item = new QStandardItem("Web of Science");
        model->setItem(5, 0, item);
        item = new QStandardItem(webofScience);
        model->setItem(5, 1, item);
        item = new QStandardItem("");
        model->setItem(5, 2, item);

        item = new QStandardItem("Impact Factor(JCR2019)");
        model->setItem(6, 0, item);
        item = new QStandardItem(impactFactor);
        model->setItem(6, 1, item);
        item = new QStandardItem("");
        model->setItem(6, 2, item);
        item = new QStandardItem("Top期刊");
        model->setItem(7, 0, item);
        item = new QStandardItem(top);
        model->setItem(7, 1, item);
        model->item(7, 1)->setBackground(QBrush(Qt::lightGray));    //突出显示
        item = new QStandardItem("");
        model->setItem(7, 2, item);
        item = new QStandardItem("国际期刊预警等级");
        model->setItem(8, 0, item);
        item = new QStandardItem(warningLevel);
        model->setItem(8, 1, item);
        model->item(8, 1)->setBackground(QBrush(Qt::lightGray));    //突出显示
        item = new QStandardItem("");
        model->setItem(8, 2, item);

        item = new QStandardItem("大类");
        model->setItem(9, 0, item);
        item = new QStandardItem(majorDivision.name);
        model->setItem(9, 1, item);
        item = new QStandardItem(QString::number(majorDivision.level));
        model->setItem(9, 2, item);
        for(int i = 0; i < subDivisions.length(); i++){
            item = new QStandardItem("小类");
            model->setItem(10+i, 0, item);
            item = new QStandardItem(subDivisions[i].name);
            model->setItem(10+i, 1, item);
            item = new QStandardItem(QString::number(subDivisions[i].level));
            model->setItem(10+i, 2, item);
        }

        ui->tableView_journalInformation->setModel(model);
        ui->lineEdit_journalName->setText(journalName);
        if(autoActivateWindow){
            this->showNormal();
            this->activateWindow(); //激活窗口到前台
        }
    }
    else {
        ui->lineEdit_journalName->setText("期刊不存在，请检查期刊名称！");
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
            journalName = clipboard->text();
            run();
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
