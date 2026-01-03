#ifndef CAPTCHADIALOG_H
#define CAPTCHADIALOG_H

#include <QDialog>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

QT_BEGIN_NAMESPACE
class QNetworkAccessManager;
class QNetworkReply;
class QSslError;
QT_END_NAMESPACE


namespace Ui {
class CaptchaDialog;
}

// Moat Circumvention API endpoint - provides bridges without captcha
const QString moatBuiltinUrl = "https://bridges.torproject.org/moat/circumvention/builtin";

class CaptchaDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CaptchaDialog(QWidget *parent = 0);
    ~CaptchaDialog();

    const QStringList & getBridges() const {return bridges;}

signals:
    // Fired when a message should be reported to the user
    void message(const QString& title, const QString& message, unsigned int style);

private slots:
    void managerFinished(QNetworkReply* reply);
    void reportSslErrors(QNetworkReply*, const QList<QSslError>&);

    void on_okButton_clicked();
    void on_cancelButton_clicked();
    void on_refreshCaptchaButton_clicked();

private:

    void requestBridges();
    void parseBridgesFromJson(const QByteArray& data);

    Ui::CaptchaDialog *ui;

    QNetworkAccessManager *manager;

    QStringList bridges;

};

#endif // CAPTCHADIALOG_H
