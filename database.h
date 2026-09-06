#pragma once
#include <QVector>
#include <QByteArray>
#include "room.h"
#include "payment.h"
#include "user.h"
#include "utility.h"

// One owner's Telegram bot setup (shared across all their rooms).
struct TelegramSettings {
    QString botToken;
};

// One month's income figures, used by the Income page's chart.
struct MonthlyIncomeSummary {
    QString month;        // "yyyy-MM"
    double expected = 0;  // sum of all payment amounts billed that month
    double collected = 0; // sum of only the ones marked paid
};

// Everything that talks to SQLite lives here.
// No other file should contain raw SQL - they call these functions instead.
class Database {
public:
    bool init();

    // ---- Auth ----
    // Returns false and fills errorOut if the username is taken or input is invalid.
    bool registerUser(const QString &username, const QString &password, QString &errorOut);
    // Returns the user's id on success, -1 if the username/password don't match.
    int loginUser(const QString &username, const QString &password);
    bool getUserById(int userId, QString &username);

    // Rooms (all scoped to ownerId so each account only sees its own data)
    QVector<Room> getAllRooms(int ownerId);
    bool addRoom(Room room, int ownerId);
    bool updateRoom(const Room &room);
    bool deleteRoom(int roomId);

    // Payments
    QVector<Payment> getPaymentsForMonth(const QString &month, int ownerId);

    // Same as above but never creates missing rows - safe to call for any month
    // (past or future) when just viewing/filtering, without side effects.
    QVector<Payment> getPaymentsForMonthReadOnly(const QString &month, int ownerId);

    bool markPaid(int paymentId);
    bool markUnpaid(int paymentId);
    bool recordPayment(int paymentId, double amount);
    QVector<Payment> getPaymentHistory(int ownerId);

    // Updates (or creates, if missing) the payment row's amount for a room+month.
    // Used after saving a utility bill, so the recorded payment always reflects
    // room charge + electricity + water combined, not just the base rent.
    bool syncPaymentAmountForRoomMonth(int roomId, const QString &month, double newAmount);

    // Searches ALL payments (paid or not) by tenant name, across every room/month,
    // so an owner can look up a former tenant's history even after they've moved out.
    QVector<Payment> searchPaymentsByTenant(const QString &nameQuery, int ownerId);

    // Utilities (electricity & water, per room per month)
    // Auto-creates the month's row on first call, carrying forward last month's
    // "current" reading as this month's "previous" reading.
    Utility getUtilityForMonth(int roomId, const QString &month);
    bool saveUtility(const Utility &u);

    // Telegram bot settings (one per owner)
    TelegramSettings getTelegramSettings(int ownerId);
    bool saveTelegramSettings(int ownerId, const TelegramSettings &settings);

    // ABA KHQR payment image (one per owner), shown/sent to tenants alongside
    // bills so they can scan-to-pay. Stored as raw image bytes in the DB so it
    // survives even if the original file on disk gets moved, renamed, or deleted.
    // Returns an empty QByteArray if the owner hasn't set one up yet.
    QByteArray getAbaQrImage(int ownerId);
    bool saveAbaQrImage(int ownerId, const QByteArray &imageData);
    bool clearAbaQrImage(int ownerId);

    // Expected vs collected income for each of the last monthsBack months
    // (oldest first), for the Income page's chart.
    QVector<MonthlyIncomeSummary> getMonthlyIncomeSummary(int monthsBack, int ownerId);

    // Dashboard stats
    int countRoomsByStatus(const QString &status, int ownerId);
    double totalMonthlyRentCollected(int ownerId);

private:
    QString hashPassword(const QString &password, const QString &salt);
};
