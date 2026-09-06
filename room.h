#pragma once
#include <QString>

// One row in the "rooms" table.
struct Room {
    int id = -1;
    QString number;        // e.g. "D01"
    QString floor;         // "downstairs" or "upstairs"
    double rentPerMonth = 0;
    QString status;        // "available", "occupied", "maintenance"
    QString tenantName;
    QString phone;
    int dueDay = 1;         // day of the month rent is due (1-28) - derived from dueDate, used for overdue calc
    QString dueDate;        // "yyyy-MM-dd" - the actual first/next due date (captures month+year too)

    // ---- Stay type: long-term monthly tenant, or short-term daily guest ----
    QString stayType = "monthly";  // "monthly" or "shortterm"
    QString checkInDate;            // "yyyy-MM-dd", used by both types
    QString checkOutDate;           // "yyyy-MM-dd", short-term only. Empty = still staying.
    double dailyRate = 0;           // $ per night, short-term only

    QString telegramChatId;         // set once the tenant has messaged the StayHub bot
};

