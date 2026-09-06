#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QVector>
#include <QPair>
#include <QString>

// Thin wrapper around the Telegram Bot HTTPS API. Every call is async - connect
// to the matching signal to find out what happened. One instance can be reused
// for multiple calls.
class TelegramService : public QObject {
    Q_OBJECT

public:
    explicit TelegramService(QObject *parent = nullptr);

    // Sends a plain text message to chatId.
    void sendMessage(const QString &botToken, const QString &chatId, const QString &text);

    // Sends an image (e.g. the owner's ABA KHQR payment code) to chatId, with an
    // optional caption shown under the photo in Telegram (e.g. the bill text).
    void sendPhoto(const QString &botToken, const QString &chatId, const QByteArray &imageData, const QString &caption = QString());

    // Looks up everyone who has messaged the bot recently, so the owner can pick
    // the right chat id without needing to know it ahead of time.
    void fetchRecentStarters(const QString &botToken);

    // Validates a bot token by asking Telegram who it belongs to (no message sent).
    // Used by the "Test Connection" button in Settings.
    void testConnection(const QString &botToken);

signals:
    // success == false means errorMessage explains what went wrong.
    void sendFinished(bool success, const QString &errorMessage);

    // names: list of (display name, chat id) pairs, most recent first, de-duplicated.
    void startersFetched(bool success, const QVector<QPair<QString, QString>> &names, const QString &errorMessage);

    // botUsername is set (e.g. "MyStayHubBot") only when success is true.
    void connectionTested(bool success, const QString &botUsername, const QString &errorMessage);

private:
    QNetworkAccessManager *manager;
};
