#pragma once
#include <QString>

// One row in the "payments" table, combined with a bit of room info
// so we don't have to look the room up separately every time we display it.
struct Payment {
    int id = -1;
    int roomId = -1;
    QString roomNumber;
    QString tenantName;
    QString month;      // "2026-08"
    double amount = 0;
    double paidAmount = 0;
    bool paid = false;
    QString paidOn;      // date string, empty if not paid yet
};
