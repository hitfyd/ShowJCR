#include "showjcr.h"
#include "./ui_showjcr.h"
#include <QClipboard>
#include <QMessageBox>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QCompleter>
#include <QMimeData>
#include <QMenu>
#include <QTabWidget>
#include <QTableView>
#include <QHeaderView>
#include <QCompleter>
#include <QAbstractListModel>
#include <QCheckBox>
#include <QGroupBox>
#include <QWidgetAction>
#include <QFrame>
#include <QFile>
#include <QProcess>
#include <QFont>
#include <QLabel>
#include <QVBoxLayout>
#include <QPushButton>
#include <QFontMetrics>
#include <QSizePolicy>
#include <QFileInfo>
#include <QAbstractItemView>
#include <QRegularExpression>
#include <algorithm>
#ifdef Q_OS_LINUX
#include <QStandardPaths>
#endif

const QString ShowJCR::author = "hitfyd";
const QString ShowJCR::version = "v2026-1.3";
const QString ShowJCR::email = "hitfyd@foxmail.com";
const QString ShowJCR::codeURL = "https://github.com/hitfyd/ShowJCR";
#ifndef Q_OS_LINUX
const QString ShowJCR::updateURL = "https://github.com/hitfyd/ShowJCR/releases";
#elif defined(Q_OS_LINUX)
const QString ShowJCR::updateURL = "";//我不觉得会有人会发布linux版本
#endif
const QString ShowJCR::windowTitile = tr("分区表2026");
const QString ShowJCR::logoIconName = ":/image/jcr-logo.jpg";
const QString ShowJCR::datasetName = "jcr.db";  //数据集暂时无法使用资源文件；在程序自启动时，程序的运行目录是C:/WINDOWS/system32而不是程序目录，因此需要结合QApplication::applicationFilePath()修改
const QString ShowJCR::defaultJournal = "National Science Review";

class JournalCompleterModel : public QAbstractListModel
{
public:
    explicit JournalCompleterModel(QObject *parent = nullptr)
        : QAbstractListModel(parent)
    {
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : matches.size();
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
    {
        if (!index.isValid() || index.row() >= matches.size())
            return QVariant();
        if (role == Qt::DisplayRole)
            return matches.at(index.row());
        return QVariant();
    }

    void updateMatches(SqliteDB *db, const QString &text)
    {
        beginResetModel();
        matches = db ? db->findJournalMatches(text) : QStringList();
        endResetModel();
    }

private:
    QStringList matches;
};

namespace {

void positionMenuAboveButton(QMenu *menu, QToolButton *button, QWidget *panel)
{
    panel->adjustSize();
    const QSize panelSize = panel->sizeHint();
    const int menuWidth = qMax(panelSize.width(), menu->minimumWidth());
    const int menuHeight = panelSize.height();
    const QPoint buttonTopRight = button->mapToGlobal(QPoint(button->width(), 0));
    menu->move(buttonTopRight.x() - menuWidth, buttonTopRight.y() - menuHeight - 8);
}

QString tableFamilyKey(const QString &table)
{
    const QRegularExpression pattern(QStringLiteral("^(\\D+)(\\d+)$"));
    const QRegularExpressionMatch match = pattern.match(table);
    return match.hasMatch() ? match.captured(1) : table;
}

int tableYear(const QString &table)
{
    const QRegularExpression pattern(QStringLiteral("^(\\D+)(\\d+)$"));
    const QRegularExpressionMatch match = pattern.match(table);
    return match.hasMatch() ? match.captured(2).toInt() : 0;
}

QString familyTabTitle(const QString &familyKey, const QString &sampleTable)
{
    if (familyKey == QLatin1String("FQBJCR"))
        return QStringLiteral("中科院");
    if (familyKey == QLatin1String("JCR"))
        return QStringLiteral("JCR");
    if (familyKey == QLatin1String("GJQKYJMD"))
        return QStringLiteral("国际预警");
    if (sampleTable.startsWith(QLatin1String("XR")))
        return QStringLiteral("新锐") + sampleTable.mid(2);
    if (sampleTable.startsWith(QLatin1String("CCFT")))
        return QStringLiteral("CCF-T ") + sampleTable.mid(4);
    if (sampleTable.startsWith(QLatin1String("CCF")))
        return QStringLiteral("CCF ") + sampleTable.mid(3);
    return sampleTable;
}

QString canonicalFieldKey(const QString &field)
{
    QString key = field;
    key.remove(QRegularExpression(QStringLiteral("\\(20\\d{2}\\)")));
    if (key.compare(QStringLiteral("eISSN"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("EISSN");
    return key;
}

QString stripYearFromFieldLabel(const QString &field)
{
    QString label = field;
    label.remove(QRegularExpression(QStringLiteral("\\(20\\d{2}\\)")));
    return label.trimmed();
}

Pair buildMergedField(const QString &label, const QMap<int, QString> &valuesByYear)
{
    if (valuesByYear.isEmpty())
        return {};

    QSet<QString> uniqueValues;
    for (const QString &value : valuesByYear)
        uniqueValues.insert(value.trimmed());

    if (uniqueValues.size() == 1)
        return {label, valuesByYear.first()};

    QStringList parts;
    QList<int> years = valuesByYear.keys();
    std::sort(years.begin(), years.end(), std::greater<int>());
    for (int year : years)
        parts << QStringLiteral("%1（%2）").arg(valuesByYear.value(year), QString::number(year));

    return {label, parts.join(QStringLiteral(" / "))};
}

QList<Pair> mergeYearlyTableInfo(const QList<QPair<int, QList<Pair>>> &yearTables)
{
    if (yearTables.isEmpty())
        return {};
    if (yearTables.size() == 1)
        return yearTables.first().second;

    QMap<QString, QMap<int, QString>> valuesByKey;
    QMap<QString, QString> labelByKey;

    for (const QPair<int, QList<Pair>> &entry : yearTables) {
        const int year = entry.first;
        for (const Pair &pair : entry.second) {
            if (pair.first == QLatin1String("Journal") || pair.first == QLatin1String("年份"))
                continue;

            const QString key = canonicalFieldKey(pair.first);
            valuesByKey[key][year] = pair.second;
            if (!labelByKey.contains(key) || year >= yearTables.first().first)
                labelByKey[key] = stripYearFromFieldLabel(pair.first);
        }
    }

    QList<Pair> merged;
    QSet<QString> emitted;
    const QList<Pair> &newestInfo = yearTables.first().second;

    for (const Pair &pair : newestInfo) {
        const QString key = canonicalFieldKey(pair.first);
        if (key == QLatin1String("Journal") || key == QLatin1String("年份") || emitted.contains(key))
            continue;

        emitted.insert(key);
        merged.append(buildMergedField(labelByKey.value(key, stripYearFromFieldLabel(pair.first)),
                                       valuesByKey.value(key)));
    }

    QList<QString> remainingKeys = valuesByKey.keys();
    std::sort(remainingKeys.begin(), remainingKeys.end());
    for (const QString &key : remainingKeys) {
        if (emitted.contains(key))
            continue;
        merged.append(buildMergedField(labelByKey.value(key, key), valuesByKey.value(key)));
    }

    return merged;
}

} // namespace

ShowJCR::ShowJCR(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ShowJCR)
{
    ui->setupUi(this);
    this->setWindowTitle(windowTitile);
    setupAppearance();
    setupSettingsPanel();

    const QIcon appIcon(logoIconName);
    QApplication::setWindowIcon(appIcon);
    setWindowIcon(appIcon);

    //获取程序运行信息
    appName = QApplication::applicationName();//程序名称
#ifdef Q_OS_LINUX
		//通过循环读取列表来搜索jcr.db
		QStringList locations = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
		for (const QString &path : locations) {
				if (path.isEmpty()) continue;
				QDir dir(path);
				QString fullPath = dir.filePath(ShowJCR::datasetName);
				if (QFileInfo::exists(fullPath)) {
						appDir = path;
						break; // 找到后通常应该跳出循环
				}
		}
#elif defined(Q_OS_MAC)
    {
        const QDir resourceDir(QApplication::applicationDirPath() + "/../Resources");
        const QDir bundleDir(QApplication::applicationDirPath());
        if (QFileInfo::exists(resourceDir.absoluteFilePath(datasetName)))
            appDir = resourceDir;
        else
            appDir = bundleDir;
    }
#else
    appDir = QDir(QApplication::applicationDirPath());//程序目录（QDir类型）
#endif

    appPath = QApplication::applicationFilePath();// 程序路径

    qDebug() << "start check:" << appName << appDir.path() << appPath;

    setupAppMenus();

    //设置系统托盘
    m_systray.setToolTip(appName);//设置提示文字
    m_systray.setIcon(QIcon(logoIconName));//设置托盘图标
    m_systray.show();//显示托盘
    connect(&m_systray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(OnSystemTrayClicked(QSystemTrayIcon::ActivationReason)));//关联托盘事件

    //设置剪切板监听
    connect(QApplication::clipboard(), SIGNAL(dataChanged()), this, SLOT(getClipboard()));

    //初始化期刊数据库
    sqliteDB = new SqliteDB(appDir, datasetName);

    //设置期刊名称输入框提示文字
    ui->lineEdit_journalName->setPlaceholderText(cueWords[0]);

    //使用默认期刊进行查询，设置界面初始默认显示
//    run(defaultJournal);

    //读取程序运行参数
    settings = new QSettings(author, appName);
    autoStart = settings->value("autoStart").toBool();
    exit2Taskbar = settings->value("exit2Taskbar").toBool();
    monitorClipboard = settings->value("monitorClipboard").toBool();
    autoActivateWindow = settings->value("autoActivateWindow").toBool();
    if (autoActivateWindow && !monitorClipboard)
        autoActivateWindow = false;

    checkBox_autoStart->setChecked(autoStart);
    checkBox_exit2Taskbar->setChecked(exit2Taskbar);
    checkBox_monitorClipboard->setChecked(monitorClipboard);
    checkBox_autoActivateWindow->setChecked(autoActivateWindow);
    syncSettingsUi();

    auto applySettings = [this]() {
        exit2Taskbar = checkBox_exit2Taskbar->isChecked();
        autoStart = checkBox_autoStart->isChecked();
        monitorClipboard = checkBox_monitorClipboard->isChecked();
        autoActivateWindow = checkBox_autoActivateWindow->isChecked();
        syncSettingsUi();
        setAutoStart();
    };

    connect(checkBox_exit2Taskbar, &QCheckBox::toggled, this, applySettings);
    connect(checkBox_autoStart, &QCheckBox::toggled, this, applySettings);
    connect(checkBox_monitorClipboard, &QCheckBox::toggled, this, [this, applySettings](bool checked) {
        if (!checked && checkBox_autoActivateWindow->isChecked())
            checkBox_autoActivateWindow->setChecked(false);
        applySettings();
    });
    connect(checkBox_autoActivateWindow, &QCheckBox::toggled, this, [this, applySettings](bool checked) {
        if (checked && !checkBox_monitorClipboard->isChecked())
            checkBox_monitorClipboard->setChecked(true);
        applySettings();
    });

    QStringList old_selectedTables = settings->value("selectedTables").toStringList();
    const QStringList knownTables = settings->value("knownTables").toStringList();
    const QStringList allTables = sqliteDB->getAllTableNames();
    if (old_selectedTables.isEmpty()) {
        selectedTables = allTables;
    } else {
        //检查选择的数据表与数据库中数据表的一致性，避免数据库升级时删除了一些表
        selectedTables.clear();
        for (const QString &item : allTables) {
            if (old_selectedTables.contains(item))
                selectedTables.append(item);
        }
        if (selectedTables.isEmpty())
            selectedTables = allTables;
        // 数据库新增表时默认勾选，但不恢复用户曾取消勾选的旧表
        for (const QString &item : allTables) {
            if (!knownTables.contains(item) && !selectedTables.contains(item))
                selectedTables.append(item);
        }
    }
    settings->setValue("knownTables", allTables);
    sqliteDB->selectTableNames(selectedTables);

    setupJournalCompleter();

    //初始化数据集选择窗口
    selectTableDialog = new TableSelectorDialog(sqliteDB->getAllTableNames(), selectedTables, this);

    //初始化关于窗口
    aboutDialog = new AboutDialog(appName, version, email, codeURL, updateURL, this);

    //检查程序自启动设置是否有效
    setAutoStart();

    run(defaultJournal);
}

ShowJCR::~ShowJCR()
{
    //存储程序运行参数
    settings->setValue("autoStart", autoStart);
    settings->setValue("exit2Taskbar", exit2Taskbar);
    settings->setValue("monitorClipboard", monitorClipboard);
    settings->setValue("autoActivateWindow", autoActivateWindow);
    settings->setValue("selectedTables", selectedTables);

    delete menu;
    delete aboutDialog;
    delete ui;
    delete sqliteDB;
    delete settings;
}

void ShowJCR::setupJournalCompleter()
{
    if (!journalCompleterModel) {
        journalCompleterModel = new JournalCompleterModel(this);
        journalCompleter = new QCompleter(journalCompleterModel, this);
        journalCompleter->setCaseSensitivity(Qt::CaseInsensitive);
        journalCompleter->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
        journalCompleter->setMaxVisibleItems(12);
        ui->lineEdit_journalName->setCompleter(journalCompleter);

        connect(journalCompleter, QOverload<const QString &>::of(&QCompleter::highlighted),
                this, [this](const QString &text) {
                    completerHighlightedText = text;
                });

        connect(journalCompleter, QOverload<const QString &>::of(&QCompleter::activated),
                this, [this](const QString &text) {
                    applyCompleterSelectionAndSearch(text);
                });

        connect(journalCompleter->popup(), &QAbstractItemView::clicked,
                this, [this](const QModelIndex &index) {
                    applyCompleterSelectionAndSearch(index.data(Qt::DisplayRole).toString());
                });
    }
    refreshJournalCompleter(ui->lineEdit_journalName->text());
}

QString ShowJCR::completerSelectionText() const
{
    if (!journalCompleter || !journalCompleter->popup()->isVisible())
        return QString();

    const QModelIndex idx = journalCompleter->popup()->currentIndex();
    if (idx.isValid()) {
        const QString selected = idx.data(Qt::DisplayRole).toString().trimmed();
        if (!selected.isEmpty())
            return selected;
    }

    return completerHighlightedText.trimmed();
}

void ShowJCR::applyCompleterSelectionAndSearch(const QString &text)
{
    const QString selected = text.trimmed();
    if (selected.isEmpty())
        return;

    suppressCompleterRefresh = true;
    ui->lineEdit_journalName->setText(selected);
    suppressCompleterRefresh = false;
    completerHighlightedText.clear();

    if (journalCompleter)
        journalCompleter->popup()->hide();

    run(selected);
}

void ShowJCR::refreshJournalCompleter(const QString &text)
{
    if (!journalCompleterModel || !journalCompleter || suppressCompleterRefresh)
        return;

    completerHighlightedText.clear();
    auto *model = static_cast<JournalCompleterModel *>(journalCompleterModel);
    model->updateMatches(sqliteDB, text);
    if (!text.isEmpty() && model->rowCount() > 0 && ui->lineEdit_journalName->hasFocus()) {
        journalCompleter->complete();
    }
}

void ShowJCR::on_pushButton_selectJournal_clicked()
{
    submitJournalSearch();
}

void ShowJCR::on_lineEdit_journalName_returnPressed()
{
    submitJournalSearch();
}

void ShowJCR::submitJournalSearch()
{
    const QString selected = completerSelectionText();

    if (journalCompleter && journalCompleter->popup()->isVisible())
        journalCompleter->popup()->hide();

    if (!selected.isEmpty()) {
        applyCompleterSelectionAndSearch(selected);
        return;
    }

    QString text = ui->lineEdit_journalName->text().trimmed();
    for (const QString &cue : {cueWords[1], cueWords[2], cueWords[0]}) {
        if (text.startsWith(cue))
            text = text.mid(cue.length()).trimmed();
    }
    if (text.isEmpty())
        return;

    run(text);
}

void ShowJCR::run(const QString &input)
{
    //输入简化，首尾空格清除，中间空格均变为1个，便于剪切板复制不精确时有效性
    QString tempJournalName = input.simplified();
    for (const QString &cue : {cueWords[1], cueWords[2], cueWords[0]}) {
        if (tempJournalName.startsWith(cue))
            tempJournalName = tempJournalName.mid(cue.length()).trimmed();
    }
    journalName = tempJournalName;
    //检查输入是否为空
    if(journalName.isEmpty()){
        return;
    }
    QString resolvedJournalName = sqliteDB->findJournalName(journalName);
    if(resolvedJournalName.isEmpty()){
        const QStringList matches = sqliteDB->findJournalMatches(journalName, 1);
        if (!matches.isEmpty())
            resolvedJournalName = matches.first();
    }
    if(resolvedJournalName.isEmpty()){
        if(!ui->lineEdit_journalName->text().contains(cueWords[1])){
            ui->lineEdit_journalName->setText(cueWords[1] + journalName);
        }
        return;
    }
    journalName = resolvedJournalName;
    //输入正确，执行查询
    qDebug() << "select the journal:" << journalName;
    journalInfoByTable = sqliteDB->getJournalInfoByTable(journalName);
    updateGUI();
}

void ShowJCR::setupAppearance()
{
    QFont appFont = tableFont();
    setFont(appFont);

    ui->label->setFont(tableFont(QFont::DemiBold));

    QFont inputFont = tableFont();
    inputFont.setPixelSize(14);
    ui->lineEdit_journalName->setFont(inputFont);
    ui->pushButton_selectJournal->setAutoDefault(false);
    ui->pushButton_selectJournal->setDefault(false);

    QFont footerFont = tableFont();
    footerFont.setPixelSize(12);
    ui->footerStatusLabel->setFont(footerFont);
    ui->toolButton_settings->setFont(footerFont);
    ui->toolButton_list->setFont(footerFont);

    ui->tabWidget_journalInformation->tabBar()->setExpanding(false);
    ui->tabWidget_journalInformation->tabBar()->setDrawBase(false);

    setStyleSheet(
        "QWidget { background: #f8fafc; color: #1e293b; }"
        "QLineEdit {"
        "  background: #ffffff;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 8px;"
        "  padding: 6px 12px;"
        "  selection-background-color: #bfdbfe;"
        "}"
        "QLineEdit:focus { border: 1px solid #3b82f6; }"
        "QPushButton {"
        "  background: #2563eb;"
        "  color: #ffffff;"
        "  border: none;"
        "  border-radius: 8px;"
        "  padding: 6px 16px;"
        "  font-weight: 600;"
        "}"
        "QPushButton:hover { background: #1d4ed8; }"
        "QPushButton:pressed { background: #1e40af; }"
        "QTabWidget::pane {"
        "  border: 1px solid #e2e8f0;"
        "  border-radius: 10px;"
        "  background: #ffffff;"
        "  top: -1px;"
        "}"
        "QTabBar::tab {"
        "  background: transparent;"
        "  color: #64748b;"
        "  border: none;"
        "  border-bottom: 2px solid transparent;"
        "  padding: 8px 16px;"
        "  margin-right: 4px;"
        "  font-weight: 600;"
        "}"
        "QTabBar::tab:selected {"
        "  color: #2563eb;"
        "  border-bottom: 2px solid #2563eb;"
        "}"
        "QTabBar::tab:hover { color: #1d4ed8; }"
        "QTableView {"
        "  background: #ffffff;"
        "  border: none;"
        "  gridline-color: #f1f5f9;"
        "  selection-background-color: #dbeafe;"
        "  selection-color: #1e293b;"
        "  outline: none;"
        "}"
        "QTableView::item {"
        "  padding: 0 10px;"
        "  border: none;"
        "}"
        "QHeaderView::section {"
        "  background: #f8fafc;"
        "  color: #64748b;"
        "  border: none;"
        "  border-bottom: 1px solid #e2e8f0;"
        "  border-right: 1px solid #eef2f7;"
        "  padding: 6px 10px;"
        "  font-weight: 600;"
        "}"
        "QCheckBox { color: #475569; spacing: 8px; }"
        "QCheckBox:disabled { color: #94a3b8; }"
        "QCheckBox::indicator { width: 15px; height: 15px; }"
        "QGroupBox {"
        "  color: #64748b;"
        "  font-weight: 600;"
        "  border: 1px solid #e2e8f0;"
        "  border-radius: 8px;"
        "  margin-top: 10px;"
        "  padding-top: 8px;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  left: 10px;"
        "  padding: 0 4px;"
        "}"
        "QFrame#footerFrame {"
        "  background: #ffffff;"
        "  border: 1px solid #e2e8f0;"
        "  border-radius: 10px;"
        "}"
        "QLabel#footerStatusLabel { color: #64748b; }"
        "QToolButton#toolButton_settings, QToolButton#toolButton_list {"
        "  background: #ffffff;"
        "  border: 1px solid #e2e8f0;"
        "  border-radius: 9px;"
        "  padding: 0;"
        "}"
        "QToolButton#toolButton_settings:hover, QToolButton#toolButton_list:hover {"
        "  background: #eef2ff;"
        "  border-color: #c7d2fe;"
        "}"
        "QToolButton#toolButton_settings:pressed, QToolButton#toolButton_list:pressed {"
        "  background: #dbeafe;"
        "}"
        "QToolButton#toolButton_settings[active=\"true\"] {"
        "  background: #eff6ff;"
        "  border-color: #93c5fd;"
        "}"
        "QToolButton#toolButton_settings::menu-indicator, QToolButton#toolButton_list::menu-indicator {"
        "  image: none;"
        "  width: 0px;"
        "}"
        "QWidget#settingsPanel { background: #ffffff; }"
        "QLabel#settingsTitle {"
        "  color: #1e293b;"
        "  font-size: 14px;"
        "  font-weight: 600;"
        "  padding-bottom: 2px;"
        "}"
        "QMenu#settingsMenu {"
        "  background: #ffffff;"
        "  border: 1px solid #e2e8f0;"
        "  border-radius: 12px;"
        "  padding: 0;"
        "}"
        "QMenu#appMenu {"
        "  background: #ffffff;"
        "  border: 1px solid #e2e8f0;"
        "  border-radius: 12px;"
        "  padding: 0;"
        "}"
        "QWidget#appMenuPanel { background: #ffffff; }"
        "QLabel#appMenuTitle {"
        "  color: #1e293b;"
        "  font-size: 14px;"
        "  font-weight: 600;"
        "  padding-bottom: 2px;"
        "}"
        "QPushButton#appMenuItem {"
        "  background: transparent;"
        "  color: #334155;"
        "  border: none;"
        "  border-radius: 8px;"
        "  padding: 10px 12px;"
        "  text-align: left;"
        "  font-weight: 500;"
        "}"
        "QPushButton#appMenuItem:hover { background: #f1f5f9; }"
        "QPushButton#appMenuItem:pressed { background: #e2e8f0; }"
        "QPushButton#appMenuExit {"
        "  background: transparent;"
        "  color: #dc2626;"
        "  border: none;"
        "  border-radius: 8px;"
        "  padding: 10px 12px;"
        "  text-align: left;"
        "  font-weight: 500;"
        "}"
        "QPushButton#appMenuExit:hover { background: #fef2f2; }"
        "QPushButton#appMenuExit:pressed { background: #fee2e2; }"
        "QFrame#appMenuDivider {"
        "  background: #e2e8f0;"
        "  max-height: 1px;"
        "  margin-top: 4px;"
        "  margin-bottom: 4px;"
        "}"
        "QMenu {"
        "  background: #ffffff;"
        "  border: 1px solid #e2e8f0;"
        "  border-radius: 10px;"
        "  padding: 4px;"
        "}"
        "QCheckBox { color: #64748b; spacing: 6px; }"
        "QCheckBox::indicator { width: 14px; height: 14px; }"
    );
}

void ShowJCR::setupSettingsPanel()
{
    auto makeCheckBox = [this](const QString &text, const QString &tip) {
        QCheckBox *box = new QCheckBox(text);
        box->setToolTip(tip);
        box->setFont(tableFont());
        return box;
    };

    checkBox_exit2Taskbar = makeCheckBox(
        tr("关闭时隐藏到系统托盘"),
        tr("点击窗口关闭按钮时不退出，保留在菜单栏/系统托盘"));
    checkBox_autoStart = makeCheckBox(
        tr("登录时自动启动"),
        tr("系统登录后自动运行 ShowJCR"));
    checkBox_monitorClipboard = makeCheckBox(
        tr("监听剪切板中的期刊名"),
        tr("复制期刊名称后自动查询匹配结果"));
    checkBox_autoActivateWindow = makeCheckBox(
        tr("  匹配后自动显示窗口"),
        tr("通过剪切板触发查询时，将窗口带到前台"));

    QGroupBox *backgroundGroup = new QGroupBox(tr("后台运行"));
    QVBoxLayout *backgroundLayout = new QVBoxLayout(backgroundGroup);
    backgroundLayout->setContentsMargins(12, 14, 12, 12);
    backgroundLayout->setSpacing(8);
    backgroundLayout->addWidget(checkBox_exit2Taskbar);
    backgroundLayout->addWidget(checkBox_autoStart);

    QGroupBox *clipboardGroup = new QGroupBox(tr("剪切板查询"));
    QVBoxLayout *clipboardLayout = new QVBoxLayout(clipboardGroup);
    clipboardLayout->setContentsMargins(12, 14, 12, 12);
    clipboardLayout->setSpacing(8);
    clipboardLayout->addWidget(checkBox_monitorClipboard);
    clipboardLayout->addWidget(checkBox_autoActivateWindow);

    QWidget *panel = new QWidget();
    panel->setObjectName("settingsPanel");
    panel->setMinimumWidth(320);
    QVBoxLayout *panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(14, 14, 14, 14);
    panelLayout->setSpacing(12);

    QLabel *title = new QLabel(tr("偏好设置"));
    title->setObjectName("settingsTitle");
    title->setFont(tableFont(QFont::DemiBold));
    panelLayout->addWidget(title);
    panelLayout->addWidget(backgroundGroup);
    panelLayout->addWidget(clipboardGroup);

    settingsMenu = new QMenu(this);
    settingsMenu->setObjectName("settingsMenu");
    settingsMenu->setMinimumWidth(320);
    QWidgetAction *panelAction = new QWidgetAction(settingsMenu);
    panelAction->setDefaultWidget(panel);
    settingsMenu->addAction(panelAction);
    ui->toolButton_settings->setMenu(settingsMenu);

    connect(settingsMenu, &QMenu::aboutToShow, this, [this, panel]() {
        positionMenuAboveButton(settingsMenu, ui->toolButton_settings, panel);
    });

#if !(defined(Q_OS_WIN) || defined(Q_OS_LINUX) || defined(Q_OS_MAC))
    checkBox_autoStart->setVisible(false);
#endif

    // auto applySettings = [this]() {
    //     exit2Taskbar = checkBox_exit2Taskbar->isChecked();
    //     autoStart = checkBox_autoStart->isChecked();
    //     monitorClipboard = checkBox_monitorClipboard->isChecked();
    //     autoActivateWindow = checkBox_autoActivateWindow->isChecked();
    //     syncSettingsUi();
    //     setAutoStart();
    // };

    // connect(checkBox_exit2Taskbar, &QCheckBox::toggled, this, applySettings);
    // connect(checkBox_autoStart, &QCheckBox::toggled, this, applySettings);
    // connect(checkBox_monitorClipboard, &QCheckBox::toggled, this, [this, applySettings](bool checked) {
    //     if (!checked && checkBox_autoActivateWindow->isChecked())
    //         checkBox_autoActivateWindow->setChecked(false);
    //     applySettings();
    // });
    // connect(checkBox_autoActivateWindow, &QCheckBox::toggled, this, [this, applySettings](bool checked) {
    //     if (checked && !checkBox_monitorClipboard->isChecked())
    //         checkBox_monitorClipboard->setChecked(true);
    //     applySettings();
    // });
}

void ShowJCR::setupAppMenus()
{
    appMenu = new QMenu(this);
    appMenu->setObjectName("appMenu");
    appMenu->setMinimumWidth(240);
    QWidget *appPanel = createAppMenuPanel(appMenu);
    QWidgetAction *appPanelAction = new QWidgetAction(appMenu);
    appPanelAction->setDefaultWidget(appPanel);
    appMenu->addAction(appPanelAction);
    ui->toolButton_list->setMenu(appMenu);
    connect(appMenu, &QMenu::aboutToShow, this, [this, appPanel]() {
        positionMenuAboveButton(appMenu, ui->toolButton_list, appPanel);
    });

    menu = new QMenu(this);
    menu->setObjectName("appMenu");
    menu->setMinimumWidth(240);
    QWidget *trayPanel = createAppMenuPanel(menu);
    QWidgetAction *trayPanelAction = new QWidgetAction(menu);
    trayPanelAction->setDefaultWidget(trayPanel);
    menu->addAction(trayPanelAction);
#ifndef Q_OS_MAC
    m_systray.setContextMenu(menu);
#endif
}

void ShowJCR::showTrayMenu()
{
    if (!menu)
        return;

    QWidget *panel = menu->findChild<QWidget *>(QStringLiteral("appMenuPanel"));
    if (panel)
        panel->adjustSize();

    const int menuWidth = qMax(panel ? panel->sizeHint().width() : 240, menu->minimumWidth());
    const int menuHeight = panel ? panel->sizeHint().height() : menu->sizeHint().height();
    const QRect trayGeometry = m_systray.geometry();
    const QPoint anchor(trayGeometry.x() + trayGeometry.width(), trayGeometry.y());
    menu->popup(QPoint(anchor.x() - menuWidth, anchor.y() - menuHeight - 8));
}

QWidget *ShowJCR::createAppMenuPanel(QMenu *hostMenu)
{
    auto makeMenuButton = [this](const QString &iconPath, const QString &text, const QString &objectName) {
        QPushButton *button = new QPushButton(text);
        button->setObjectName(objectName);
        button->setIcon(QIcon(iconPath));
        button->setIconSize(QSize(18, 18));
        button->setFont(tableFont());
        button->setCursor(Qt::PointingHandCursor);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        return button;
    };

    QWidget *panel = new QWidget();
    panel->setObjectName("appMenuPanel");
    panel->setMinimumWidth(240);
    QVBoxLayout *panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(10, 10, 10, 10);
    panelLayout->setSpacing(2);

    QLabel *title = new QLabel(tr("应用菜单"));
    title->setObjectName("appMenuTitle");
    title->setFont(tableFont(QFont::DemiBold));
    panelLayout->addWidget(title);
    panelLayout->addSpacing(4);

    QPushButton *selectTableButton = makeMenuButton(
        ":/icon/table.svg", tr("选择数据表"), QStringLiteral("appMenuItem"));
    QPushButton *aboutButton = makeMenuButton(
        ":/icon/info.svg", tr("关于"), QStringLiteral("appMenuItem"));
    QFrame *divider = new QFrame();
    divider->setObjectName("appMenuDivider");
    divider->setFrameShape(QFrame::HLine);
    divider->setFixedHeight(1);
    QPushButton *exitButton = makeMenuButton(
        ":/icon/exit.svg", tr("退出程序"), QStringLiteral("appMenuExit"));

    panelLayout->addWidget(selectTableButton);
    panelLayout->addWidget(aboutButton);
    panelLayout->addWidget(divider);
    panelLayout->addWidget(exitButton);

    connect(selectTableButton, &QPushButton::clicked, this, [this, hostMenu]() {
        hostMenu->close();
        show_selectTable();
    });
    connect(aboutButton, &QPushButton::clicked, this, [this, hostMenu]() {
        hostMenu->close();
        show_about();
    });
    connect(exitButton, &QPushButton::clicked, this, [this, hostMenu]() {
        hostMenu->close();
        OnExit();
    });

    return panel;
}

void ShowJCR::syncSettingsUi()
{
    checkBox_autoActivateWindow->setEnabled(monitorClipboard);
    if (!monitorClipboard) {
        autoActivateWindow = false;
        checkBox_autoActivateWindow->setChecked(false);
    }

    QStringList parts;
    if (exit2Taskbar)
        parts << tr("后台运行");
    if (monitorClipboard) {
        parts << tr("剪切板监听");
        if (autoActivateWindow)
            parts << tr("自动弹出");
    }
    if (autoStart)
        parts << tr("登录自启");

    ui->footerStatusLabel->setText(parts.isEmpty() ? tr("就绪") : parts.join(QStringLiteral(" · ")));

    const bool hasActiveSettings = exit2Taskbar || autoStart || monitorClipboard;
    ui->toolButton_settings->setProperty("active", hasActiveSettings);
    ui->toolButton_settings->setToolTip(hasActiveSettings
        ? tr("偏好设置（已启用 %1 项）").arg(parts.size())
        : tr("偏好设置"));
    ui->toolButton_settings->style()->unpolish(ui->toolButton_settings);
    ui->toolButton_settings->style()->polish(ui->toolButton_settings);
}

QFont ShowJCR::tableFont(QFont::Weight weight) const
{
    QFont f;
#ifdef Q_OS_MAC
    f.setFamilies(QStringList() << "PingFang SC" << "Helvetica Neue" << "Arial");
#else
    f.setFamilies(QStringList() << "Microsoft YaHei UI" << "Segoe UI" << "Arial");
#endif
    f.setPixelSize(13);
    f.setWeight(weight);
    f.setStyleStrategy(QFont::PreferAntialias);
    return f;
}

QString ShowJCR::tableDisplayName(const QString &table)
{
    if (table.startsWith("JCR")) return "JCR " + table.mid(3);
    if (table.startsWith("XR")) return "新锐" + table.mid(2);
    if (table.startsWith("GJQKYJMD")) return "国际预警" + table.mid(8);
    if (table.startsWith("CCFT")) return "CCF-T " + table.mid(4);
    if (table.startsWith("CCF")) return "CCF " + table.mid(3);
    if (table.startsWith("FQBJCR")) return "中科院" + table.mid(6);
    return table;
}

QTableView *ShowJCR::createInfoTableView(const QList<Pair> &info)
{
    const int rowHeight = 34;

    QTableView *tableView = new QTableView();
    tableView->setFont(tableFont());
    tableView->setShowGrid(false);
    tableView->setAlternatingRowColors(true);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableView->setFocusPolicy(Qt::NoFocus);
    tableView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    tableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tableView->horizontalHeader()->setVisible(true);
    tableView->horizontalHeader()->setHighlightSections(false);
    tableView->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    tableView->horizontalHeader()->setFixedHeight(30);
    tableView->verticalHeader()->setVisible(false);
    tableView->verticalHeader()->setDefaultSectionSize(rowHeight);
    tableView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    tableView->setWordWrap(false);
    tableView->setTextElideMode(Qt::ElideNone);

    QStandardItemModel *model = new QStandardItemModel(tableView);
    model->setHorizontalHeaderLabels({tr("字段"), tr("值")});
    const QFont keyFont = tableFont(QFont::DemiBold);
    const QFont valueFont = tableFont(QFont::Normal);
    const QFontMetrics keyMetrics(keyFont);

    int keyColumnWidth = keyMetrics.horizontalAdvance(tr("字段")) + 24;
    for (int i = 0; i < info.size(); i++) {
        keyColumnWidth = qMax(keyColumnWidth, keyMetrics.horizontalAdvance(info[i].first) + 24);

        QStandardItem *keyItem = new QStandardItem(info[i].first);
        QStandardItem *valueItem = new QStandardItem(info[i].second);
        keyItem->setFont(keyFont);
        valueItem->setFont(valueFont);
        keyItem->setEditable(false);
        valueItem->setEditable(false);
        keyItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        valueItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);

        const QString &field = info[i].first;
        if (field.contains("年份")) {
            keyItem->setBackground(color_header);
            keyItem->setForeground(color_headerText);
            valueItem->setBackground(color_header);
            valueItem->setForeground(color_headerText);
        } else {
            keyItem->setBackground(color_keyBg);
            keyItem->setForeground(color_keyText);
        }

        if (field.contains("IF Quartile") || field == QLatin1String("IF") ||
            field.startsWith(QStringLiteral("IF ")) ||
            field.contains("CCF推荐类型") || field.contains("预警") ||
            field.contains("大类分区") || field.contains("Top") ||
            field.contains("标注")) {
            keyItem->setBackground(color_highlight);
            keyItem->setForeground(color_highlightText);
            valueItem->setBackground(color_highlight);
            valueItem->setForeground(color_highlightText);
        }

        model->setItem(i, 0, keyItem);
        model->setItem(i, 1, valueItem);
    }

    tableView->setModel(model);
    tableView->horizontalHeader()->setMinimumSectionSize(80);
    tableView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    tableView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    tableView->setColumnWidth(0, keyColumnWidth);
    return tableView;
}

void ShowJCR::updateGUI()
{
    ui->tabWidget_journalInformation->clear();

    const QStringList orderedTables = sqliteDB->getDisplayTableNames();
    QMap<QString, QList<QPair<int, QString>>> familyTables;

    for (const QString &table : orderedTables) {
        if (journalInfoByTable.value(table).isEmpty())
            continue;
        const QString family = tableFamilyKey(table);
        familyTables[family].append({tableYear(table), table});
    }

    QStringList familyOrder;
    for (const QString &table : orderedTables) {
        const QString family = tableFamilyKey(table);
        if (!familyTables.contains(family) || familyOrder.contains(family))
            continue;
        familyOrder.append(family);
    }

    for (const QString &family : familyOrder) {
        QList<QPair<int, QString>> entries = familyTables.value(family);
        std::sort(entries.begin(), entries.end(),
                  [](const QPair<int, QString> &a, const QPair<int, QString> &b) {
                      return a.first > b.first;
                  });

        QList<QPair<int, QList<Pair>>> yearInfos;
        for (const QPair<int, QString> &entry : entries)
            yearInfos.append({entry.first, journalInfoByTable.value(entry.second)});

        const QList<Pair> merged = mergeYearlyTableInfo(yearInfos);
        if (merged.isEmpty())
            continue;

        ui->tabWidget_journalInformation->addTab(
            createInfoTableView(merged),
            familyTabTitle(family, entries.first().second));
    }

    if (ui->tabWidget_journalInformation->count() == 0) {
        QWidget *emptyPage = new QWidget();
        QVBoxLayout *layout = new QVBoxLayout(emptyPage);
        layout->setContentsMargins(24, 24, 24, 24);
        QLabel *hint = new QLabel(tr("未找到该期刊在所选数据表中的记录。"), emptyPage);
        hint->setAlignment(Qt::AlignCenter);
        hint->setStyleSheet("color: #94a3b8; font-size: 13px;");
        layout->addStretch();
        layout->addWidget(hint);
        layout->addStretch();
        ui->tabWidget_journalInformation->addTab(emptyPage, tr("无结果"));
        ui->tabWidget_journalInformation->setTabEnabled(0, false);
    }

    ui->lineEdit_journalName->setText(journalName);
    if (autoActivateWindow) {
        this->showNormal();
        this->activateWindow();
    }
}

void ShowJCR::setAutoStart()
{
#ifdef Q_OS_WIN
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
#elif defined(Q_OS_MAC)
    const QString launchAgentDir = QDir::homePath() + "/Library/LaunchAgents";
    const QString plistPath = launchAgentDir + "/com.hitfyd.ShowJCR.plist";
    QDir().mkpath(launchAgentDir);

    if (autoStart) {
        const QString plist = QString(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
            "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
            "<plist version=\"1.0\"><dict>\n"
            "  <key>Label</key><string>com.hitfyd.ShowJCR</string>\n"
            "  <key>ProgramArguments</key><array>\n"
            "    <string>%1</string><string>autoStart</string>\n"
            "  </array>\n"
            "  <key>RunAtLoad</key><true/>\n"
            "</dict></plist>\n").arg(appPath);

        QFile file(plistPath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            file.write(plist.toUtf8());
            file.close();
        }
        QProcess::execute("launchctl", {"load", "-w", plistPath});
    } else if (QFile::exists(plistPath)) {
        QProcess::execute("launchctl", {"unload", "-w", plistPath});
        QFile::remove(plistPath);
    }
#elif defined(Q_OS_LINUX)
	//通过循环读取列表来搜索desktop
		QString sourceFile;
		QStringList locations = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
			for (const QString &path : locations) {
					QDir dir(path);
					QString fullPath = dir.filePath("io.hitfyd.ShowJCR.desktop");
					if (QFileInfo::exists(fullPath)) {
							sourceFile=fullPath; // 找到文件，返回路径
							break;
					}
			}
		QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
		QDir autostartDir(configDir + "/autostart/");
		if (!autostartDir.exists()) {
			if (!autostartDir.mkpath(".")) {
					return;
				}
		}
		QString desktopFilePath = autostartDir.filePath("io.hitfyd.ShowJCR.desktop");
		if (QFile::exists(desktopFilePath)&&!autoStart){
				QFile::remove(desktopFilePath);
			}else if(autoStart&&!QFile::exists(desktopFilePath)){
				QFile::copy(sourceFile, desktopFilePath);
			}
#endif
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

int ShowJCR::OnSystemTrayClicked(QSystemTrayIcon::ActivationReason reason)
{
#ifdef Q_OS_MAC
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::Context) {
        showTrayMenu();
        return 0;
    }
    if (reason == QSystemTrayIcon::DoubleClick) {
        showNormal();
        activateWindow();
        return 0;
    }
    return 0;
#else
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
        showNormal();
        activateWindow();
    }
    return 0;
#endif
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
        return;
    }
    refreshJournalCompleter(arg1);
}

void ShowJCR::show_selectTable()
{
    selectTableDialog->setSelectedTables(selectedTables);
    if (selectTableDialog->exec() == QDialog::Accepted) {
        selectedTables = selectTableDialog->selectedTables();
        if(selectedTables.isEmpty()){
            ui->lineEdit_journalName->setText(cueWords[2]);
            return;
        }
        sqliteDB->selectTableNames(selectedTables);
        run(ui->lineEdit_journalName->text());
        setupJournalCompleter();
    }
}

void ShowJCR::show_about()
{
    aboutDialog->exec();
}
