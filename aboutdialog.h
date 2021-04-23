#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <QDialog>

namespace Ui {
class AboutDialog;
}

class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(const QString &appName, const QString &version, const QString &email, const QString &codeURL, const QString &updateURL, QWidget *parent = nullptr);
    ~AboutDialog();

private slots:
    void on_pushButton_email_clicked();

    void on_pushButton_code_clicked();

    void on_pushButton_OK_clicked();

    void on_pushButton_update_clicked();

private:
    Ui::AboutDialog *ui;
    QString appName;
    QString version;
    QString email;
    QString codeURL;
    QString updateURL;
};

#endif // ABOUTDIALOG_H
