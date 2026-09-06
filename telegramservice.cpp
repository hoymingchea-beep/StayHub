#include "telegramservice.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QMimeDatabase>

TelegramService::TelegramService(QObject *parent)
    : QObject(parent), manager(new QNetworkAccessManager(this))
{
}

static QString apiUrl(const QString &botToken, const QString &method)
{
    return QString("https://api.telegram.org/bot%1/%2").arg(botToken, method);
}

// Pulls Telegram's error description (if any) out of a failed response body,
// so the owner sees something useful instead of a raw HTTP error code.
static QString extractApiError(const QByteArray &body)
{
    QJsonDocument doc = QJsonDocument::fromJson(body);
    if (doc.isObject() && doc.object().contains("description"))
        return doc.object().value("description").toString();
    return QString();
}

void TelegramService::sendMessage(const QString &botToken, const QString &chatId, const QString &text)
{
    QNetworkRequest request{QUrl(apiUrl(botToken, "sendMessage"))};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery body;
    body.addQueryItem("chat_id", chatId);
    body.addQueryItem("text", text);

    QNetworkReply *reply = manager->post(request, body.query(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        bool ok = reply->error() == QNetworkReply::NoError;
        QByteArray responseBody = reply->readAll();
        QString err = ok ? QString() : (extractApiError(responseBody).isEmpty() ? reply->errorString() : extractApiError(responseBody));
        emit sendFinished(ok, err);
        reply->deleteLater();
    });
}

void TelegramService::sendPhoto(const QString &botToken, const QString &chatId, const QByteArray &imageData, const QString &caption)
{
    auto *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart chatIdPart;
    chatIdPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"chat_id\""));
    chatIdPart.setBody(chatId.toUtf8());
    multiPart->append(chatIdPart);

    if (!caption.isEmpty()) {
        QHttpPart captionPart;
        captionPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"caption\""));
        captionPart.setBody(caption.toUtf8());
        multiPart->append(captionPart);
    }

    QMimeDatabase mimeDb;
    QString mimeType = mimeDb.mimeTypeForData(imageData).name();
    if (mimeType.isEmpty() || !mimeType.startsWith("image/"))
        mimeType = "image/jpeg";
    QString extension = mimeType.section('/', 1, 1);

    QHttpPart photoPart;
    photoPart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(mimeType));
    photoPart.setHeader(QNetworkRequest::ContentDispositionHeader,
        QVariant(QString("form-data; name=\"photo\"; filename=\"qr.%1\"").arg(extension)));
    photoPart.setBody(imageData);
    multiPart->append(photoPart);

    QNetworkRequest request{QUrl(apiUrl(botToken, "sendPhoto"))};
    QNetworkReply *reply = manager->post(request, multiPart);
    multiPart->setParent(reply); // multiPart is deleted along with the reply

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        bool ok = reply->error() == QNetworkReply::NoError;
        QByteArray responseBody = reply->readAll();
        QString err = ok ? QString() : (extractApiError(responseBody).isEmpty() ? reply->errorString() : extractApiError(responseBody));
        emit sendFinished(ok, err);
        reply->deleteLater();
    });
}

void TelegramService::fetchRecentStarters(const QString &botToken)
{
    QNetworkRequest request{QUrl(apiUrl(botToken, "getUpdates?limit=100"))};
    QNetworkReply *reply = manager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        QByteArray body = reply->readAll();
        bool ok = reply->error() == QNetworkReply::NoError;

        if (!ok) {
            QString err = extractApiError(body).isEmpty() ? reply->errorString() : extractApiError(body);
            emit startersFetched(false, {}, err);
            reply->deleteLater();
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(body);
        QJsonArray results = doc.object().value("result").toArray();

        QVector<QPair<QString, QString>> names;
        QSet<QString> seenChatIds;

        // Walk newest-first so the most recent message from each person wins.
        for (int i = results.size() - 1; i >= 0; --i) {
            QJsonObject update = results[i].toObject();
            QJsonObject message = update.value("message").toObject();
            QJsonObject chat = message.value("chat").toObject();

            if (chat.value("type").toString() != "private")
                continue;

            QString chatId = QString::number(chat.value("id").toVariant().toLongLong());
            if (seenChatIds.contains(chatId))
                continue;
            seenChatIds.insert(chatId);

            QString firstName = chat.value("first_name").toString();
            QString lastName = chat.value("last_name").toString();
            QString displayName = (firstName + " " + lastName).trimmed();
            if (displayName.isEmpty())
                displayName = chat.value("username").toString();
            if (displayName.isEmpty())
                displayName = "Unknown (" + chatId + ")";

            names.append({displayName, chatId});
        }

        emit startersFetched(true, names, QString());
        reply->deleteLater();
    });
}

void TelegramService::testConnection(const QString &botToken)
{
    if (botToken.trimmed().isEmpty()) {
        emit connectionTested(false, QString(), "Bot Token is empty.");
        return;
    }

    QNetworkRequest request{QUrl(apiUrl(botToken, "getMe"))};
    QNetworkReply *reply = manager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        QByteArray body = reply->readAll();
        bool networkOk = reply->error() == QNetworkReply::NoError;

        if (!networkOk) {
            QString err = extractApiError(body).isEmpty() ? reply->errorString() : extractApiError(body);
            emit connectionTested(false, QString(), err);
            reply->deleteLater();
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(body);
        QJsonObject obj = doc.object();

        if (!obj.value("ok").toBool()) {
            QString err = obj.value("description").toString("Invalid bot token.");
            emit connectionTested(false, QString(), err);
            reply->deleteLater();
            return;
        }

        QString username = obj.value("result").toObject().value("username").toString();
        emit connectionTested(true, username, QString());
        reply->deleteLater();
    });
}
