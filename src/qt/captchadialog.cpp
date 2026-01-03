#include "captchadialog.h"
#include "ui_captchadialog.h"
#include "ui_interface.h"


#include <QFile>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>


CaptchaDialog::CaptchaDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CaptchaDialog)
{
    ui->setupUi(this);

    // Update window title - no longer using captcha
    this->setWindowTitle(tr("Retrieve Bridges"));

    ui->refreshCaptchaButton->setIcon(QIcon(":/icons/refresh").pixmap(16, 16));
    ui->refreshCaptchaButton->setText(tr("&Refresh"));

    manager = new QNetworkAccessManager(this);

    connect(manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(managerFinished(QNetworkReply*)));
    connect(manager, SIGNAL(sslErrors(QNetworkReply*, const QList<QSslError>&)),
        this, SLOT(reportSslErrors(QNetworkReply*, const QList<QSslError>&)));

    connect(ui->okButton, SIGNAL(clicked(bool)), this, SLOT(on_okButton_clicked()));

    // Hide the captcha input field - no longer needed with Moat API
    ui->captchaEdit->hide();
    ui->label->hide();

    // Update the label to show loading status
    ui->captchaLabel->setText(tr("Fetching bridges from Tor Project...\n\nPlease wait."));
    ui->captchaLabel->setAlignment(Qt::AlignCenter);

    // Auto-fetch bridges on dialog open
    requestBridges();
}

CaptchaDialog::~CaptchaDialog()
{
    delete ui;
    if (manager != NULL)
        delete manager;
}

void CaptchaDialog::requestBridges()
{
    this->setCursor(Qt::WaitCursor);
    bridges.clear();

    ui->captchaLabel->setText(tr("Fetching bridges from Tor Project...\n\nPlease wait."));

    QNetworkRequest request;
    request.setUrl(QUrl(moatBuiltinUrl));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/vnd.api+json");

    // Moat API requires POST request
    manager->post(request, QByteArray("{}"));
}

void CaptchaDialog::managerFinished(QNetworkReply* reply)
{
    this->setCursor(Qt::ArrowCursor);

    if (reply->error()) {
        qDebug() << "Network error:" << reply->errorString();
        ui->captchaLabel->setText(tr("Error fetching bridges:\n\n%1\n\nClick Refresh to try again.")
            .arg(reply->errorString()));
        return;
    }

    QByteArray data = reply->readAll();
    parseBridgesFromJson(data);
}

void CaptchaDialog::parseBridgesFromJson(const QByteArray& data)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "JSON parse error:" << parseError.errorString();
        ui->captchaLabel->setText(tr("Error parsing response:\n\n%1\n\nClick Refresh to try again.")
            .arg(parseError.errorString()));
        return;
    }

    QJsonObject root = doc.object();

    // Check for error response
    if (root.contains("errors")) {
        QJsonArray errors = root["errors"].toArray();
        QString errorMsg;
        for (const QJsonValue& err : errors) {
            QJsonObject errObj = err.toObject();
            errorMsg += errObj["detail"].toString() + "\n";
        }
        ui->captchaLabel->setText(tr("Server error:\n\n%1\n\nClick Refresh to try again.")
            .arg(errorMsg));
        return;
    }

    // Parse obfs4 bridges - API returns them at root level
    if (root.contains("obfs4")) {
        QJsonArray obfs4Array = root["obfs4"].toArray();
        for (const QJsonValue& val : obfs4Array) {
            QString bridgeLine = val.toString().trimmed();
            if (!bridgeLine.isEmpty()) {
                // Prepend "bridge " if not already present
                if (!bridgeLine.startsWith("bridge ")) {
                    bridgeLine = "bridge " + bridgeLine;
                }
                bridges.append(bridgeLine);
            }
        }
    }

    // Fallback: check for bridges_builtin wrapper (alternative API format)
    if (bridges.isEmpty() && root.contains("bridges_builtin")) {
        QJsonObject builtinBridges = root["bridges_builtin"].toObject();
        if (builtinBridges.contains("obfs4")) {
            QJsonArray obfs4Array = builtinBridges["obfs4"].toArray();
            for (const QJsonValue& val : obfs4Array) {
                QString bridgeLine = val.toString().trimmed();
                if (!bridgeLine.isEmpty()) {
                    if (!bridgeLine.startsWith("bridge ")) {
                        bridgeLine = "bridge " + bridgeLine;
                    }
                    bridges.append(bridgeLine);
                }
            }
        }
    }

    // Check if we found any bridges
    if (bridges.isEmpty()) {
        ui->captchaLabel->setText(tr("No obfs4 bridges available.\n\nClick Refresh to try again,\nor get bridges manually from:\nhttps://bridges.torproject.org"));
        return;
    }

    // Success - display the bridges
    QString displayText = tr("Found %1 obfs4 bridge(s).\n\n").arg(bridges.size());

    // Show abbreviated bridge info
    for (int i = 0; i < bridges.size() && i < 4; i++) {
        QString shortBridge = bridges[i];
        if (shortBridge.length() > 60) {
            shortBridge = shortBridge.left(60) + "...";
        }
        displayText += QString("%1. %2\n").arg(i + 1).arg(shortBridge);
    }
    if (bridges.size() > 4) {
        displayText += tr("   ... and %1 more\n").arg(bridges.size() - 4);
    }

    displayText += tr("\nClick OK to enable obfs4.");

    ui->captchaLabel->setText(displayText);
    ui->captchaLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    qDebug() << "Successfully fetched" << bridges.size() << "bridges from Moat API";
}

void CaptchaDialog::reportSslErrors(QNetworkReply* reply, const QList<QSslError>& errs)
{
    this->setCursor(Qt::ArrowCursor);
    Q_UNUSED(reply);

    QString errString;
    Q_FOREACH (const QSslError& err, errs) {
        errString += err.errorString() + "\n";
    }

    ui->captchaLabel->setText(tr("SSL Error:\n\n%1\n\nClick Refresh to try again.")
        .arg(errString));

    emit message(tr("Network request error"), errString, CClientUIInterface::MSG_ERROR);
}

void CaptchaDialog::on_okButton_clicked()
{
    // Check if we have bridges to use
    if (bridges.isEmpty()) {
        QMessageBox::warning(this, windowTitle(),
            tr("No bridges available. Please click Refresh to fetch bridges first."),
            QMessageBox::Ok, QMessageBox::Ok);
        return;
    }

    // Accept the dialog - bridges will be retrieved via getBridges()
    accept();
}

void CaptchaDialog::on_cancelButton_clicked()
{
    reject();
}

void CaptchaDialog::on_refreshCaptchaButton_clicked()
{
    requestBridges();
}
