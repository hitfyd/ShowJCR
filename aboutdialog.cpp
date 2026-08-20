#include "aboutdialog.h"
#include "ui_aboutdialog.h"

#include <QDesktopServices>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace {

QFont aboutFont(int pixelSize, QFont::Weight weight = QFont::Normal)
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

AboutDialog::AboutDialog(const QString &appName, const QString &version, const QString &email, const QString &codeURL, const QString &updateURL, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutDialog)
{
    ui->setupUi(this);

    this->appName = appName;
    this->version = version;
    this->email = email;
    this->codeURL = codeURL;
    this->updateURL = updateURL;

    setWindowFlags(windowFlags() & ~Qt::WindowMinMaxButtonsHint);
    setWindowTitle(tr("关于 %1").arg(appName));
    setMinimumSize(380, 300);
    resize(400, 320);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 28, 28, 24);
    layout->setSpacing(16);

    QLabel *logoLabel = new QLabel(this);
    logoLabel->setAlignment(Qt::AlignCenter);
    logoLabel->setPixmap(QPixmap(":/image/jcr-logo.jpg").scaled(72, 72, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    QLabel *titleLabel = new QLabel(appName, this);
    titleLabel->setObjectName("aboutTitle");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setFont(aboutFont(22, QFont::DemiBold));

    QLabel *versionLabel = new QLabel(tr("版本 %1").arg(version), this);
    versionLabel->setObjectName("aboutVersion");
    versionLabel->setAlignment(Qt::AlignCenter);
    versionLabel->setFont(aboutFont(13));

    QLabel *descLabel = new QLabel(tr("快速查询期刊 JCR、中科院等分区信息。"), this);
    descLabel->setObjectName("aboutDesc");
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setWordWrap(true);
    descLabel->setFont(aboutFont(13));

    QHBoxLayout *linksLayout = new QHBoxLayout();
    linksLayout->setSpacing(8);
    QPushButton *emailButton = new QPushButton(tr("联系作者"), this);
    emailButton->setObjectName("linkButton");
    emailButton->setToolTip(email);
    emailButton->setCursor(Qt::PointingHandCursor);
    QPushButton *codeButton = new QPushButton(tr("源代码"), this);
    codeButton->setObjectName("linkButton");
    codeButton->setToolTip(codeURL);
    codeButton->setCursor(Qt::PointingHandCursor);
    QPushButton *updateButton = new QPushButton(tr("检查更新"), this);
    updateButton->setObjectName("linkButton");
    updateButton->setToolTip(updateURL);
    updateButton->setCursor(Qt::PointingHandCursor);
    updateButton->setVisible(!updateURL.isEmpty());
    linksLayout->addStretch();
    linksLayout->addWidget(emailButton);
    linksLayout->addWidget(codeButton);
    linksLayout->addWidget(updateButton);
    linksLayout->addStretch();

    QPushButton *okButton = new QPushButton(tr("关闭"), this);
    okButton->setObjectName("primaryButton");
    okButton->setDefault(true);
    okButton->setCursor(Qt::PointingHandCursor);

    layout->addWidget(logoLabel);
    layout->addWidget(titleLabel);
    layout->addWidget(versionLabel);
    layout->addWidget(descLabel);
    layout->addLayout(linksLayout);
    layout->addStretch();
    layout->addWidget(okButton, 0, Qt::AlignRight);

    connect(emailButton, &QPushButton::clicked, this, &AboutDialog::on_pushButton_email_clicked);
    connect(codeButton, &QPushButton::clicked, this, &AboutDialog::on_pushButton_code_clicked);
    connect(updateButton, &QPushButton::clicked, this, &AboutDialog::on_pushButton_update_clicked);
    connect(okButton, &QPushButton::clicked, this, &AboutDialog::on_pushButton_OK_clicked);

    setStyleSheet(
        "QDialog { background: #f8fafc; color: #1e293b; }"
        "QLabel#aboutTitle { color: #0f172a; }"
        "QLabel#aboutVersion, QLabel#aboutDesc { color: #64748b; }"
        "QPushButton#linkButton {"
        "  background: #ffffff;"
        "  color: #2563eb;"
        "  border: 1px solid #dbeafe;"
        "  border-radius: 8px;"
        "  padding: 8px 12px;"
        "  font-weight: 600;"
        "}"
        "QPushButton#linkButton:hover {"
        "  background: #eff6ff;"
        "  border-color: #93c5fd;"
        "}"
        "QPushButton#primaryButton {"
        "  background: #2563eb;"
        "  color: #ffffff;"
        "  border: none;"
        "  border-radius: 8px;"
        "  padding: 8px 22px;"
        "  font-weight: 600;"
        "  min-width: 88px;"
        "}"
        "QPushButton#primaryButton:hover { background: #1d4ed8; }"
        "QPushButton#primaryButton:pressed { background: #1e40af; }"
    );
}

AboutDialog::~AboutDialog()
{
    delete ui;
}

void AboutDialog::on_pushButton_email_clicked()
{
    QDesktopServices::openUrl(QUrl("mailto:"+email));
}

void AboutDialog::on_pushButton_code_clicked()
{
    QDesktopServices::openUrl(QUrl(codeURL));
}

void AboutDialog::on_pushButton_OK_clicked()
{
    close();
}

void AboutDialog::on_pushButton_update_clicked()
{
    QDesktopServices::openUrl(QUrl(updateURL));
}
