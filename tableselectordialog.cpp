#include "tableselectordialog.h"
#include "ui_tableselectordialog.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

QFont dialogFont(int pixelSize, QFont::Weight weight = QFont::Normal)
{
    QFont font;
#if defined(Q_OS_MAC)
    font.setFamilies({QStringLiteral("PingFang SC"), QStringLiteral("Helvetica Neue"), QStringLiteral("Arial")});
#else
    font.setFamilies({QStringLiteral("Microsoft YaHei UI"), QStringLiteral("Segoe UI"), QStringLiteral("Arial")});
#endif
    font.setPixelSize(pixelSize);
    font.setWeight(weight);
    font.setStyleStrategy(QFont::PreferAntialias);
    return font;
}

} // namespace

QString TableSelectorDialog::tableDisplayName(const QString &table)
{
    if (table.startsWith("JCR")) return QStringLiteral("JCR ") + table.mid(3);
    if (table.startsWith("XR")) return QStringLiteral("新锐") + table.mid(2);
    if (table.startsWith("GJQKYJMD")) return QStringLiteral("国际预警") + table.mid(8);
    if (table.startsWith("CCFT")) return QStringLiteral("CCF-T ") + table.mid(4);
    if (table.startsWith("CCF")) return QStringLiteral("CCF ") + table.mid(3);
    if (table.startsWith("FQBJCR")) return QStringLiteral("中科院") + table.mid(6);
    return table;
}

TableSelectorDialog::TableSelectorDialog(const QStringList &tables, const QStringList &selectedTables, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::TableSelectorDialog)
{
    ui->setupUi(this);
    setWindowTitle(tr("选择数据表"));
    setMinimumSize(380, 460);
    resize(400, 480);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(14);

    QLabel *title = new QLabel(tr("期刊数据表"));
    title->setObjectName("dialogTitle");
    title->setFont(dialogFont(18, QFont::DemiBold));

    QLabel *hint = new QLabel(tr("勾选要在查询结果中展示的分区数据源。"));
    hint->setObjectName("dialogHint");
    hint->setWordWrap(true);
    hint->setFont(dialogFont(13));

    QHBoxLayout *toolbar = new QHBoxLayout();
    toolbar->setSpacing(8);
    QPushButton *selectAllButton = new QPushButton(tr("全选"));
    selectAllButton->setObjectName("secondaryButton");
    selectAllButton->setCursor(Qt::PointingHandCursor);
    QPushButton *clearAllButton = new QPushButton(tr("全不选"));
    clearAllButton->setObjectName("secondaryButton");
    clearAllButton->setCursor(Qt::PointingHandCursor);
    toolbar->addWidget(selectAllButton);
    toolbar->addWidget(clearAllButton);
    toolbar->addStretch();

    listWidget = new QListWidget(this);
    listWidget->setObjectName("tableList");
    listWidget->setSpacing(2);
    listWidget->setFont(dialogFont(14));

    foreach (const QString &table, tables) {
        QListWidgetItem *item = new QListWidgetItem(tableDisplayName(table), listWidget);
        item->setData(Qt::UserRole, table);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(selectedTables.contains(table) ? Qt::Checked : Qt::Unchecked);
    }

    QHBoxLayout *actions = new QHBoxLayout();
    actions->setSpacing(10);
    QPushButton *cancelButton = new QPushButton(tr("取消"));
    cancelButton->setObjectName("secondaryButton");
    cancelButton->setCursor(Qt::PointingHandCursor);
    QPushButton *okButton = new QPushButton(tr("应用"));
    okButton->setObjectName("primaryButton");
    okButton->setDefault(true);
    okButton->setCursor(Qt::PointingHandCursor);
    actions->addStretch();
    actions->addWidget(cancelButton);
    actions->addWidget(okButton);

    layout->addWidget(title);
    layout->addWidget(hint);
    layout->addLayout(toolbar);
    layout->addWidget(listWidget, 1);
    layout->addLayout(actions);

    connect(selectAllButton, &QPushButton::clicked, this, [this]() { setAllChecked(true); });
    connect(clearAllButton, &QPushButton::clicked, this, [this]() { setAllChecked(false); });
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);

    setupAppearance();
}

TableSelectorDialog::~TableSelectorDialog()
{
    delete ui;
}

void TableSelectorDialog::setupAppearance()
{
    setStyleSheet(
        "QDialog { background: #f8fafc; color: #1e293b; }"
        "QLabel#dialogTitle { color: #0f172a; }"
        "QLabel#dialogHint { color: #64748b; }"
        "QListWidget#tableList {"
        "  background: #ffffff;"
        "  border: 1px solid #e2e8f0;"
        "  border-radius: 10px;"
        "  padding: 6px;"
        "  outline: none;"
        "}"
        "QListWidget#tableList::item {"
        "  border-radius: 8px;"
        "  padding: 8px 10px;"
        "  color: #334155;"
        "}"
        "QListWidget#tableList::item:hover { background: #f8fafc; }"
        "QListWidget#tableList::item:selected {"
        "  background: #eff6ff;"
        "  color: #1d4ed8;"
        "}"
        "QPushButton#primaryButton {"
        "  background: #2563eb;"
        "  color: #ffffff;"
        "  border: none;"
        "  border-radius: 8px;"
        "  padding: 8px 18px;"
        "  font-weight: 600;"
        "  min-width: 72px;"
        "}"
        "QPushButton#primaryButton:hover { background: #1d4ed8; }"
        "QPushButton#primaryButton:pressed { background: #1e40af; }"
        "QPushButton#secondaryButton {"
        "  background: #ffffff;"
        "  color: #475569;"
        "  border: 1px solid #e2e8f0;"
        "  border-radius: 8px;"
        "  padding: 8px 14px;"
        "  font-weight: 600;"
        "}"
        "QPushButton#secondaryButton:hover {"
        "  background: #f8fafc;"
        "  border-color: #cbd5e1;"
        "}"
    );
}

void TableSelectorDialog::setAllChecked(bool checked)
{
    const Qt::CheckState state = checked ? Qt::Checked : Qt::Unchecked;
    for (int i = 0; i < listWidget->count(); ++i)
        listWidget->item(i)->setCheckState(state);
}

void TableSelectorDialog::setSelectedTables(const QStringList &tables)
{
    for (int i = 0; i < listWidget->count(); ++i) {
        QListWidgetItem *item = listWidget->item(i);
        const QString table = item->data(Qt::UserRole).toString();
        item->setCheckState(tables.contains(table) ? Qt::Checked : Qt::Unchecked);
    }
}

const QStringList TableSelectorDialog::selectedTables()
{
    QStringList result;
    for (int i = 0; i < listWidget->count(); ++i) {
        QListWidgetItem *item = listWidget->item(i);
        if (item->checkState() == Qt::Checked)
            result << item->data(Qt::UserRole).toString();
    }
    return result;
}

const QStringList TableSelectorDialog::getAllTables()
{
    QStringList allTableNames;
    for (int i = 0; i < listWidget->count(); ++i)
        allTableNames << listWidget->item(i)->data(Qt::UserRole).toString();
    return allTableNames;
}
