#ifndef TABLESELECTORDIALOG_H
#define TABLESELECTORDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QVBoxLayout>
#include <QDialogButtonBox>

namespace Ui {
class TableSelectorDialog;
}

class TableSelectorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TableSelectorDialog(const QStringList &tables, const QStringList &selectedTables, QWidget *parent = nullptr);
    ~TableSelectorDialog();
    const QStringList selectedTables();
    const QStringList getAllTables();

private:
    Ui::TableSelectorDialog *ui;
    QListWidget *listWidget;
};

#endif // TABLESELECTORDIALOG_H
