#include "tableselectordialog.h"
#include "ui_tableselectordialog.h"

TableSelectorDialog::TableSelectorDialog(const QStringList &tables, const QStringList &selectedTables, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::TableSelectorDialog)
{
    ui->setupUi(this);
    this->setWindowTitle("期刊数据表选择");
    // 初始化UI
    QVBoxLayout *layout = new QVBoxLayout(this);
    listWidget = new QListWidget(this);

    // 设置列表属性
    // listWidget->setDragDropMode(QListWidget::InternalMove);
    // listWidget->setSelectionMode(QListWidget::MultiSelection);
    // listWidget->setDefaultDropAction(Qt::MoveAction);

    // 添加表项到列表
    foreach (const QString &table, tables) {
        QListWidgetItem *item = new QListWidgetItem(table, listWidget);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
        if(!selectedTables.contains(table))
            item->setCheckState(Qt::Unchecked);
    }
    // 添加按钮
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        Qt::Horizontal, this);

    layout->addWidget(listWidget);
    layout->addWidget(buttonBox);

    // 连接信号槽
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

TableSelectorDialog::~TableSelectorDialog()
{
    delete ui;
}

// 获取选中表名（按顺序）
const QStringList TableSelectorDialog::selectedTables()
{
    QStringList result;
    for(int i = 0; i < listWidget->count(); ++i) {
        QListWidgetItem *item = listWidget->item(i);
        if(item->checkState() == Qt::Checked) {
            result << item->text();
        }
    }
    return result;
}

// 获取选中表名（按顺序）
const QStringList TableSelectorDialog::getAllTables()
{
    QStringList allTableNames;
    for(int i = 0; i < listWidget->count(); ++i) {
        QListWidgetItem *item = listWidget->item(i);
        allTableNames << item->text();
    }
    return allTableNames;
}
