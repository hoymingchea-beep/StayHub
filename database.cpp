#include "database.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDate>
#include <QDebug>
#include <QCryptographicHash>
#include <QUuid>
#include <QSet>
#include <QMap>
#include <QStandardPaths>
#include <QDir>
#include <algorithm>

QString Database::hashPassword(const QString &password, const QString &salt)
{
    QByteArray data = (password + salt).toUtf8();
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex();
}

// Plain SQL "ORDER BY number" sorts as TEXT, so "10" comes before "2" alphabetically.
// This compares room numbers the way a person would: letters compared as letters,
// runs of digits compared as numbers (so "D2" < "D10", "10" comes after "2", etc.)
static bool roomNumberLessThan(const QString &a, const QString &b)
{
    int ia = 0, ib = 0;
    while (ia < a.length() && ib < b.length()) {
        QChar ca = a[ia], cb = b[ib];

        if (ca.isDigit() && cb.isDigit()) {
            int startA = ia, startB = ib;
            while (ia < a.length() && a[ia].isDigit()) ++ia;
            while (ib < b.length() && b[ib].isDigit()) ++ib;

            qlonglong numA = a.mid(startA, ia - startA).toLongLong();
            qlonglong numB = b.mid(startB, ib - startB).toLongLong();
            if (numA != numB)
                return numA < numB;
            // equal value (e.g. "01" vs "1") - keep going, don't decide yet
        } else {
            QChar la = ca.toLower(), lb = cb.toLower();
            if (la != lb)
                return la < lb;
            ++ia; ++ib;
        }
    }
    return (a.length() - ia) < (b.length() - ib);
}

bool Database::init()
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");

    // Keep user data in the OS-specific application-data folder instead of the
    // executable directory. This makes the released app writable even when it is
    // installed under Program Files or launched from a desktop shortcut.
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!QDir().mkpath(dataDir)) {
        qDebug() << "Could not create app data directory:" << dataDir;
        return false;
    }
    db.setDatabaseName(QDir(dataDir).filePath("rental.db"));

    if (!db.open()) {
        qDebug() << "DB open failed:" << db.lastError().text();
        return false;
    }

    QSqlQuery query;
    query.exec(
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "username TEXT UNIQUE,"
        "passwordHash TEXT,"
        "salt TEXT,"
        "telegramBotToken TEXT DEFAULT ''"
        ")"
    );

    // Migration: older databases may have Telegram settings columns from earlier versions.
    // Legacy payment-QR columns are intentionally ignored because QR generation was removed.
    {
        QSqlQuery cols("PRAGMA table_info(users)");
        QSet<QString> existing;
        while (cols.next())
            existing.insert(cols.value(1).toString());

        auto addUserColumnIfMissing = [&](const QString &name, const QString &definition) {
            if (!existing.contains(name)) {
                QSqlQuery alter;
                alter.exec(QString("ALTER TABLE users ADD COLUMN %1 %2").arg(name, definition));
            }
        };

        addUserColumnIfMissing("telegramBotToken", "TEXT DEFAULT ''");
        addUserColumnIfMissing("abaQrImage", "BLOB DEFAULT NULL");
    }

    query.exec(
        "CREATE TABLE IF NOT EXISTS rooms ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "ownerId INTEGER,"
        "number TEXT,"
        "floor TEXT,"
        "rentPerMonth REAL,"
        "status TEXT,"
        "tenantName TEXT,"
        "phone TEXT,"
        "dueDay INTEGER DEFAULT 1,"
        "stayType TEXT DEFAULT 'monthly',"
        "checkInDate TEXT DEFAULT '',"
        "checkOutDate TEXT DEFAULT '',"
        "dailyRate REAL DEFAULT 0,"
        "telegramChatId TEXT DEFAULT '',"
        "dueDate TEXT DEFAULT ''"
        ")"
    );

    // Migration: older databases won't have columns added in later versions of the app.
    // This adds any that are missing, so upgrading never loses existing data.
    {
        QSqlQuery cols("PRAGMA table_info(rooms)");
        QSet<QString> existing;
        while (cols.next())
            existing.insert(cols.value(1).toString());

        auto addColumnIfMissing = [&](const QString &name, const QString &definition) {
            if (!existing.contains(name)) {
                QSqlQuery alter;
                alter.exec(QString("ALTER TABLE rooms ADD COLUMN %1 %2").arg(name, definition));
            }
        };

        addColumnIfMissing("dueDay", "INTEGER DEFAULT 1");
        addColumnIfMissing("stayType", "TEXT DEFAULT 'monthly'");
        addColumnIfMissing("checkInDate", "TEXT DEFAULT ''");
        addColumnIfMissing("checkOutDate", "TEXT DEFAULT ''");
        addColumnIfMissing("dailyRate", "REAL DEFAULT 0");
        addColumnIfMissing("telegramChatId", "TEXT DEFAULT ''");
        addColumnIfMissing("dueDate", "TEXT DEFAULT ''");
    }

    query.exec(
        "CREATE TABLE IF NOT EXISTS payments ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "roomId INTEGER,"
        "month TEXT,"
        "amount REAL,"
        "paid INTEGER,"
        "paidOn TEXT,"
        "paidAmount REAL DEFAULT 0,"
        "tenantName TEXT DEFAULT '',"
        "phone TEXT DEFAULT '',"
        "FOREIGN KEY(roomId) REFERENCES rooms(id)"
        ")"
    );

    // Migration: freeze the tenant's name/phone onto each payment record at the
    // time it's created, instead of always joining the room's CURRENT tenant.
    // Without this, if a room gets a new tenant later, old payment history would
    // incorrectly start showing the new tenant's name instead of who actually paid.
    {
        QSqlQuery cols("PRAGMA table_info(payments)");
        QSet<QString> existing;
        while (cols.next())
            existing.insert(cols.value(1).toString());
        if (!existing.contains("tenantName")) {
            QSqlQuery alter;
            alter.exec("ALTER TABLE payments ADD COLUMN tenantName TEXT DEFAULT ''");
            // Backfill old rows once, using whatever tenant is on the room right now -
            // it's the best guess we have for pre-existing data.
            alter.exec(
                "UPDATE payments SET tenantName = ("
                "SELECT tenantName FROM rooms WHERE rooms.id = payments.roomId"
                ") WHERE tenantName = '' OR tenantName IS NULL"
            );
        }
        if (!existing.contains("phone")) {
            QSqlQuery alter;
            alter.exec("ALTER TABLE payments ADD COLUMN phone TEXT DEFAULT ''");
            alter.exec(
                "UPDATE payments SET phone = ("
                "SELECT phone FROM rooms WHERE rooms.id = payments.roomId"
                ") WHERE phone = '' OR phone IS NULL"
            );
        }
    }

    // Migration: partial-payment support. Older payment rows get their full
    // billed amount as paidAmount when they were already marked paid.
    {
        QSqlQuery cols("PRAGMA table_info(payments)");
        QSet<QString> existing;
        while (cols.next())
            existing.insert(cols.value(1).toString());
        if (!existing.contains("paidAmount")) {
            QSqlQuery alter;
            alter.exec("ALTER TABLE payments ADD COLUMN paidAmount REAL DEFAULT 0");
            alter.exec("UPDATE payments SET paidAmount = amount WHERE paid = 1");
        }
    }

    query.exec(
        "CREATE TABLE IF NOT EXISTS utilities ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "roomId INTEGER,"
        "month TEXT,"
        "elecPrev REAL DEFAULT 0,"
        "elecCurr REAL DEFAULT 0,"
        "elecRate REAL DEFAULT 0.25,"
        "waterPrev REAL DEFAULT 0,"
        "waterCurr REAL DEFAULT 0,"
        "waterRate REAL DEFAULT 1.0,"
        "exchangeRate REAL DEFAULT 4100,"
        "FOREIGN KEY(roomId) REFERENCES rooms(id)"
        ")"
    );

    // Migration: older databases won't have the exchangeRate column yet.
    {
        QSqlQuery cols("PRAGMA table_info(utilities)");
        QSet<QString> existing;
        while (cols.next())
            existing.insert(cols.value(1).toString());
        if (!existing.contains("exchangeRate")) {
            QSqlQuery alter;
            alter.exec("ALTER TABLE utilities ADD COLUMN exchangeRate REAL DEFAULT 4100");
        }
    }

    // Note: no more auto-seeding here. Every new account starts with an empty room list,
    // since rooms now belong to a specific owner.
    return true;
}

// ============ Auth ============

bool Database::registerUser(const QString &username, const QString &password, QString &errorOut)
{
    if (username.trimmed().isEmpty() || password.isEmpty()) {
        errorOut = "Username and password are required.";
        return false;
    }

    QSqlQuery check;
    check.prepare("SELECT id FROM users WHERE username = ?");
    check.addBindValue(username);
    check.exec();
    if (check.next()) {
        errorOut = "That username is already taken.";
        return false;
    }

    QString salt = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString hash = hashPassword(password, salt);

    QSqlQuery insert;
    insert.prepare("INSERT INTO users (username, passwordHash, salt) VALUES (?, ?, ?)");
    insert.addBindValue(username);
    insert.addBindValue(hash);
    insert.addBindValue(salt);

    if (!insert.exec()) {
        errorOut = "Could not create account. Try again.";
        return false;
    }
    return true;
}

bool Database::getUserById(int userId, QString &username)
{
    QSqlQuery query;
    query.prepare("SELECT username FROM users WHERE id = ?");
    query.addBindValue(userId);
    if (!query.exec() || !query.next())
        return false;
    username = query.value(0).toString();
    return true;
}

int Database::loginUser(const QString &username, const QString &password)
{
    QSqlQuery query;
    query.prepare("SELECT id, passwordHash, salt FROM users WHERE username = ?");
    query.addBindValue(username);
    query.exec();

    if (!query.next())
        return -1; // no such user

    int id = query.value(0).toInt();
    QString storedHash = query.value(1).toString();
    QString salt = query.value(2).toString();

    QString attemptHash = hashPassword(password, salt);
    if (attemptHash != storedHash)
        return -1; // wrong password

    return id;
}

// ============ Rooms ============

QVector<Room> Database::getAllRooms(int ownerId)
{
    QVector<Room> rooms;
    QSqlQuery query;
    query.prepare("SELECT id, number, floor, rentPerMonth, status, tenantName, phone, dueDay, "
                  "stayType, checkInDate, checkOutDate, dailyRate, telegramChatId, dueDate "
                  "FROM rooms WHERE ownerId = ?");
    query.addBindValue(ownerId);
    query.exec();

    while (query.next()) {
        Room r;
        r.id = query.value(0).toInt();
        r.number = query.value(1).toString();
        r.floor = query.value(2).toString();
        r.rentPerMonth = query.value(3).toDouble();
        r.status = query.value(4).toString();
        r.tenantName = query.value(5).toString();
        r.phone = query.value(6).toString();
        r.dueDay = query.value(7).toInt();
        r.stayType = query.value(8).toString().isEmpty() ? "monthly" : query.value(8).toString();
        r.checkInDate = query.value(9).toString();
        r.checkOutDate = query.value(10).toString();
        r.dailyRate = query.value(11).toDouble();
        r.telegramChatId = query.value(12).toString();
        r.dueDate = query.value(13).toString();
        rooms.append(r);
    }

    std::sort(rooms.begin(), rooms.end(), [](const Room &a, const Room &b) {
        return roomNumberLessThan(a.number, b.number);
    });

    return rooms;
}

bool Database::addRoom(Room room, int ownerId)
{
    QSqlQuery query;
    query.prepare("INSERT INTO rooms (ownerId, number, floor, rentPerMonth, status, tenantName, phone, dueDay, "
                  "stayType, checkInDate, checkOutDate, dailyRate, telegramChatId, dueDate) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(ownerId);
    query.addBindValue(room.number);
    query.addBindValue(room.floor);
    query.addBindValue(room.rentPerMonth);
    query.addBindValue(room.status);
    query.addBindValue(room.tenantName);
    query.addBindValue(room.phone);
    query.addBindValue(room.dueDay);
    query.addBindValue(room.stayType);
    query.addBindValue(room.checkInDate);
    query.addBindValue(room.checkOutDate);
    query.addBindValue(room.dailyRate);
    query.addBindValue(room.telegramChatId);
    query.addBindValue(room.dueDate);
    return query.exec();
}

bool Database::updateRoom(const Room &room)
{
    QSqlQuery query;
    query.prepare("UPDATE rooms SET number=?, floor=?, rentPerMonth=?, status=?, tenantName=?, phone=?, dueDay=?, "
                  "stayType=?, checkInDate=?, checkOutDate=?, dailyRate=?, telegramChatId=?, dueDate=? WHERE id=?");
    query.addBindValue(room.number);
    query.addBindValue(room.floor);
    query.addBindValue(room.rentPerMonth);
    query.addBindValue(room.status);
    query.addBindValue(room.tenantName);
    query.addBindValue(room.phone);
    query.addBindValue(room.dueDay);
    query.addBindValue(room.stayType);
    query.addBindValue(room.checkInDate);
    query.addBindValue(room.checkOutDate);
    query.addBindValue(room.dailyRate);
    query.addBindValue(room.telegramChatId);
    query.addBindValue(room.dueDate);
    query.addBindValue(room.id);
    return query.exec();
}

bool Database::deleteRoom(int roomId)
{
    QSqlQuery query;
    query.prepare("DELETE FROM rooms WHERE id=?");
    query.addBindValue(roomId);
    bool ok = query.exec();

    QSqlQuery cleanup;
    cleanup.prepare("DELETE FROM payments WHERE roomId=?");
    cleanup.addBindValue(roomId);
    cleanup.exec();

    return ok;
}

// ============ Payments ============

QVector<Payment> Database::getPaymentsForMonth(const QString &month, int ownerId)
{
    // Step 1: make sure every occupied MONTHLY room (belonging to this owner) has a
    // payment row for this month. Short-term rooms don't use this recurring cycle -
    // their charge is billed once at checkout via the bill/utilities dialog.
    QSqlQuery occupied;
    occupied.prepare("SELECT id, rentPerMonth, tenantName, phone FROM rooms "
                     "WHERE status='occupied' AND stayType='monthly' AND ownerId=?");
    occupied.addBindValue(ownerId);
    occupied.exec();

    while (occupied.next()) {
        int roomId = occupied.value(0).toInt();
        double rent = occupied.value(1).toDouble();
        QString tenantName = occupied.value(2).toString();
        QString phone = occupied.value(3).toString();

        QSqlQuery exists;
        exists.prepare("SELECT id FROM payments WHERE roomId=? AND month=?");
        exists.addBindValue(roomId);
        exists.addBindValue(month);
        exists.exec();

        if (!exists.next()) {
            // Snapshot who the tenant is RIGHT NOW onto this payment row. This is what
            // makes payment history stay correct even if this room later gets a new
            // tenant - old rows keep saying who actually incurred that charge.
            QSqlQuery insert;
            insert.prepare("INSERT INTO payments (roomId, month, amount, paid, paidOn, paidAmount, tenantName, phone) "
                           "VALUES (?, ?, ?, 0, '', 0, ?, ?)");
            insert.addBindValue(roomId);
            insert.addBindValue(month);
            insert.addBindValue(rent);
            insert.addBindValue(tenantName);
            insert.addBindValue(phone);
            insert.exec();
        }
    }

    // Step 2: fetch payments for this month. Tenant name comes from the payment's
    // own frozen snapshot (p.tenantName), not a live join to the room.
    QVector<Payment> payments;
    QSqlQuery query;
    query.prepare(
        "SELECT p.id, p.roomId, r.number, p.tenantName, p.month, p.amount, p.paid, p.paidOn, p.paidAmount "
        "FROM payments p JOIN rooms r ON p.roomId = r.id "
        "WHERE p.month = ? AND r.ownerId = ?"
    );
    query.addBindValue(month);
    query.addBindValue(ownerId);
    query.exec();

    while (query.next()) {
        Payment p;
        p.id = query.value(0).toInt();
        p.roomId = query.value(1).toInt();
        p.roomNumber = query.value(2).toString();
        p.tenantName = query.value(3).toString();
        p.month = query.value(4).toString();
        p.amount = query.value(5).toDouble();
        p.paid = query.value(6).toInt() == 1;
        p.paidOn = query.value(7).toString();
        p.paidAmount = query.value(8).toDouble();
        payments.append(p);
    }

    std::sort(payments.begin(), payments.end(), [](const Payment &a, const Payment &b) {
        return roomNumberLessThan(a.roomNumber, b.roomNumber);
    });

    return payments;
}

QVector<Payment> Database::getPaymentsForMonthReadOnly(const QString &month, int ownerId)
{
    // Same SELECT as getPaymentsForMonth, but skips the "create missing rows" step.
    // Safe to call for any month (browsing past/future) with no side effects.
    QVector<Payment> payments;
    QSqlQuery query;
    query.prepare(
        "SELECT p.id, p.roomId, r.number, p.tenantName, p.month, p.amount, p.paid, p.paidOn, p.paidAmount "
        "FROM payments p JOIN rooms r ON p.roomId = r.id "
        "WHERE p.month = ? AND r.ownerId = ?"
    );
    query.addBindValue(month);
    query.addBindValue(ownerId);
    query.exec();

    while (query.next()) {
        Payment p;
        p.id = query.value(0).toInt();
        p.roomId = query.value(1).toInt();
        p.roomNumber = query.value(2).toString();
        p.tenantName = query.value(3).toString();
        p.month = query.value(4).toString();
        p.amount = query.value(5).toDouble();
        p.paid = query.value(6).toInt() == 1;
        p.paidOn = query.value(7).toString();
        p.paidAmount = query.value(8).toDouble();
        payments.append(p);
    }

    std::sort(payments.begin(), payments.end(), [](const Payment &a, const Payment &b) {
        return roomNumberLessThan(a.roomNumber, b.roomNumber);
    });

    return payments;
}

bool Database::syncPaymentAmountForRoomMonth(int roomId, const QString &month, double newAmount)
{
    QSqlQuery exists;
    exists.prepare("SELECT id FROM payments WHERE roomId=? AND month=?");
    exists.addBindValue(roomId);
    exists.addBindValue(month);
    exists.exec();

    if (exists.next()) {
        int paymentId = exists.value(0).toInt();
        QSqlQuery update;
        update.prepare("UPDATE payments SET amount=? WHERE id=?");
        update.addBindValue(newAmount);
        update.addBindValue(paymentId);
        return update.exec();
    }

    // No payment row yet for this room+month (e.g. the Payments page hasn't been
    // opened yet this month) - create one now so the bill dialog's total isn't lost.
    QSqlQuery roomQuery;
    roomQuery.prepare("SELECT tenantName, phone FROM rooms WHERE id=?");
    roomQuery.addBindValue(roomId);
    roomQuery.exec();

    QString tenantName, phone;
    if (roomQuery.next()) {
        tenantName = roomQuery.value(0).toString();
        phone = roomQuery.value(1).toString();
    }

    QSqlQuery insert;
    insert.prepare("INSERT INTO payments (roomId, month, amount, paid, paidOn, paidAmount, tenantName, phone) "
                   "VALUES (?, ?, ?, 0, '', 0, ?, ?)");
    insert.addBindValue(roomId);
    insert.addBindValue(month);
    insert.addBindValue(newAmount);
    insert.addBindValue(tenantName);
    insert.addBindValue(phone);
    return insert.exec();
}

bool Database::markPaid(int paymentId)
{
    QSqlQuery query;
    query.prepare("UPDATE payments SET paid=1, paidAmount=amount, paidOn=? WHERE id=?");
    query.addBindValue(QDate::currentDate().toString("yyyy-MM-dd"));
    query.addBindValue(paymentId);
    return query.exec();
}

bool Database::markUnpaid(int paymentId)
{
    QSqlQuery query;
    query.prepare("UPDATE payments SET paid=0, paidAmount=0, paidOn='' WHERE id=?");
    query.addBindValue(paymentId);
    return query.exec();
}

bool Database::recordPayment(int paymentId, double amount)
{
    if (amount <= 0) return false;

    QSqlQuery find;
    find.prepare("SELECT amount, paidAmount FROM payments WHERE id=?");
    find.addBindValue(paymentId);
    if (!find.exec() || !find.next()) return false;

    double bill = find.value(0).toDouble();
    double alreadyPaid = find.value(1).toDouble();
    double newPaid = qBound(0.0, alreadyPaid + amount, bill);
    bool fullyPaid = newPaid >= bill - 0.00001;

    QSqlQuery update;
    update.prepare("UPDATE payments SET paid=?, paidAmount=?, paidOn=? WHERE id=?");
    update.addBindValue(fullyPaid ? 1 : 0);
    update.addBindValue(newPaid);
    update.addBindValue(QDate::currentDate().toString("yyyy-MM-dd"));
    update.addBindValue(paymentId);
    return update.exec();
}

QVector<Payment> Database::getPaymentHistory(int ownerId)
{
    QVector<Payment> payments;
    QSqlQuery query;
    query.prepare(
        "SELECT p.id, p.roomId, r.number, p.tenantName, p.month, p.amount, p.paid, p.paidOn, p.paidAmount "
        "FROM payments p JOIN rooms r ON p.roomId = r.id "
        "WHERE r.ownerId = ? ORDER BY p.month DESC, p.paidOn DESC"
    );
    query.addBindValue(ownerId);
    query.exec();

    while (query.next()) {
        Payment p;
        p.id = query.value(0).toInt();
        p.roomId = query.value(1).toInt();
        p.roomNumber = query.value(2).toString();
        p.tenantName = query.value(3).toString();
        p.month = query.value(4).toString();
        p.amount = query.value(5).toDouble();
        p.paid = query.value(6).toInt() == 1;
        p.paidOn = query.value(7).toString();
        p.paidAmount = query.value(8).toDouble();
        payments.append(p);
    }
    return payments;
}

QVector<Payment> Database::searchPaymentsByTenant(const QString &nameQuery, int ownerId)
{
    // Unlike getPaymentHistory (paid only), this returns EVERYTHING for a tenant -
    // paid and unpaid, across every room and month - so an owner can answer
    // "did this person ever owe us, and did they pay?" even if they've since
    // moved out and a different tenant has since occupied that room.
    QVector<Payment> payments;
    QSqlQuery query;
    query.prepare(
        "SELECT p.id, p.roomId, r.number, p.tenantName, p.month, p.amount, p.paid, p.paidOn, p.paidAmount "
        "FROM payments p JOIN rooms r ON p.roomId = r.id "
        "WHERE r.ownerId = ? AND p.tenantName LIKE ? "
        "ORDER BY p.month DESC"
    );
    query.addBindValue(ownerId);
    query.addBindValue("%" + nameQuery + "%");
    query.exec();

    while (query.next()) {
        Payment p;
        p.id = query.value(0).toInt();
        p.roomId = query.value(1).toInt();
        p.roomNumber = query.value(2).toString();
        p.tenantName = query.value(3).toString();
        p.month = query.value(4).toString();
        p.amount = query.value(5).toDouble();
        p.paid = query.value(6).toInt() == 1;
        p.paidOn = query.value(7).toString();
        p.paidAmount = query.value(8).toDouble();
        payments.append(p);
    }
    return payments;
}

// ============ Utilities ============

Utility Database::getUtilityForMonth(int roomId, const QString &month)
{
    QSqlQuery find;
    find.prepare("SELECT id, elecPrev, elecCurr, elecRate, waterPrev, waterCurr, waterRate, exchangeRate "
                 "FROM utilities WHERE roomId=? AND month=?");
    find.addBindValue(roomId);
    find.addBindValue(month);
    find.exec();

    if (find.next()) {
        Utility u;
        u.id = find.value(0).toInt();
        u.roomId = roomId;
        u.month = month;
        u.elecPrev = find.value(1).toDouble();
        u.elecCurr = find.value(2).toDouble();
        u.elecRate = find.value(3).toDouble();
        u.waterPrev = find.value(4).toDouble();
        u.waterCurr = find.value(5).toDouble();
        u.waterRate = find.value(6).toDouble();
        u.exchangeRate = find.value(7).toDouble();
        return u;
    }

    // Not created yet this month - carry forward last month's "current" reading
    // as this month's "previous" reading (0 if this room has no history at all).
    QSqlQuery last;
    last.prepare("SELECT elecCurr, elecRate, waterCurr, waterRate, exchangeRate FROM utilities "
                 "WHERE roomId=? ORDER BY month DESC LIMIT 1");
    last.addBindValue(roomId);
    last.exec();

    double prevElec = 0, prevWater = 0;
    double elecRate = 0.25, waterRate = 1.00, exchangeRate = 4100;
    if (last.next()) {
        prevElec = last.value(0).toDouble();
        elecRate = last.value(1).toDouble();
        prevWater = last.value(2).toDouble();
        waterRate = last.value(3).toDouble();
        exchangeRate = last.value(4).toDouble();
    }

    QSqlQuery insert;
    insert.prepare("INSERT INTO utilities (roomId, month, elecPrev, elecCurr, elecRate, waterPrev, waterCurr, waterRate, exchangeRate) "
                   "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
    insert.addBindValue(roomId);
    insert.addBindValue(month);
    insert.addBindValue(prevElec);
    insert.addBindValue(prevElec);   // curr starts equal to prev until the owner updates it
    insert.addBindValue(elecRate);
    insert.addBindValue(prevWater);
    insert.addBindValue(prevWater);
    insert.addBindValue(waterRate);
    insert.addBindValue(exchangeRate);
    insert.exec();

    Utility u;
    u.id = insert.lastInsertId().toInt();
    u.roomId = roomId;
    u.month = month;
    u.elecPrev = prevElec;
    u.elecCurr = prevElec;
    u.elecRate = elecRate;
    u.waterPrev = prevWater;
    u.waterCurr = prevWater;
    u.waterRate = waterRate;
    u.exchangeRate = exchangeRate;
    return u;
}

bool Database::saveUtility(const Utility &u)
{
    QSqlQuery query;
    query.prepare("UPDATE utilities SET elecCurr=?, elecRate=?, waterCurr=?, waterRate=?, exchangeRate=? WHERE id=?");
    query.addBindValue(u.elecCurr);
    query.addBindValue(u.elecRate);
    query.addBindValue(u.waterCurr);
    query.addBindValue(u.waterRate);
    query.addBindValue(u.exchangeRate);
    query.addBindValue(u.id);
    return query.exec();
}

// ============ Dashboard stats ============

// ============ Telegram settings ============

TelegramSettings Database::getTelegramSettings(int ownerId)
{
    TelegramSettings settings;
    QSqlQuery query;
    query.prepare("SELECT telegramBotToken FROM users WHERE id=?");
    query.addBindValue(ownerId);
    query.exec();
    if (query.next()) {
        settings.botToken = query.value(0).toString();
    }
    return settings;
}

bool Database::saveTelegramSettings(int ownerId, const TelegramSettings &settings)
{
    QSqlQuery query;
    query.prepare("UPDATE users SET telegramBotToken=? WHERE id=?");
    query.addBindValue(settings.botToken);
    query.addBindValue(ownerId);
    return query.exec();
}

// ============ ABA KHQR image ============

QByteArray Database::getAbaQrImage(int ownerId)
{
    QByteArray image;
    QSqlQuery query;
    query.prepare("SELECT abaQrImage FROM users WHERE id=?");
    query.addBindValue(ownerId);
    query.exec();
    if (query.next())
        image = query.value(0).toByteArray();
    return image;
}

bool Database::saveAbaQrImage(int ownerId, const QByteArray &imageData)
{
    QSqlQuery query;
    query.prepare("UPDATE users SET abaQrImage=? WHERE id=?");
    query.addBindValue(imageData);
    query.addBindValue(ownerId);
    return query.exec();
}

bool Database::clearAbaQrImage(int ownerId)
{
    QSqlQuery query;
    query.prepare("UPDATE users SET abaQrImage=NULL WHERE id=?");
    query.addBindValue(ownerId);
    return query.exec();
}

// ============ Dashboard stats ============

QVector<MonthlyIncomeSummary> Database::getMonthlyIncomeSummary(int monthsBack, int ownerId)
{
    QDate cursor = QDate::currentDate();

    // Build the ordered list of months we want (oldest first), pre-filled with
    // zeros - so months with no payment rows yet still show up on the chart.
    QVector<QString> monthKeys;
    for (int i = monthsBack - 1; i >= 0; --i)
        monthKeys.append(cursor.addMonths(-i).toString("yyyy-MM"));

    QMap<QString, MonthlyIncomeSummary> byMonth;
    for (const QString &m : monthKeys) {
        MonthlyIncomeSummary s;
        s.month = m;
        byMonth[m] = s;
    }

    QSqlQuery query;
    query.prepare(
        "SELECT p.month, SUM(p.amount), SUM(p.paidAmount) "
        "FROM payments p JOIN rooms r ON p.roomId = r.id "
        "WHERE r.ownerId = ? AND p.month >= ? "
        "GROUP BY p.month"
    );
    query.addBindValue(ownerId);
    query.addBindValue(monthKeys.first());
    query.exec();

    while (query.next()) {
        QString month = query.value(0).toString();
        if (byMonth.contains(month)) {
            byMonth[month].expected = query.value(1).toDouble();
            byMonth[month].collected = query.value(2).toDouble();
        }
    }

    QVector<MonthlyIncomeSummary> result;
    for (const QString &m : monthKeys)
        result.append(byMonth[m]);

    return result;
}

int Database::countRoomsByStatus(const QString &status, int ownerId)
{
    QSqlQuery query;
    query.prepare("SELECT COUNT(*) FROM rooms WHERE status=? AND ownerId=?");
    query.addBindValue(status);
    query.addBindValue(ownerId);
    query.exec();
    if (query.next()) return query.value(0).toInt();
    return 0;
}

double Database::totalMonthlyRentCollected(int ownerId)
{
    QSqlQuery query;
    query.prepare("SELECT SUM(rentPerMonth) FROM rooms WHERE status='occupied' AND ownerId=?");
    query.addBindValue(ownerId);
    query.exec();
    if (query.next()) return query.value(0).toDouble();
    return 0;
}
