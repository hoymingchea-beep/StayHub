#pragma once
#include <QDialog>
#include "database.h"
#include "room.h"

// A read-only "everything about this tenant" dashboard, opened by tapping any
// room row on the Rooms page. Pulls together the room/tenant fields, the
// current month's utility bill, and the room's full payment history into one
// scrollable view, with quick buttons to jump into Bill / Edit Room.
class TenantDetailDialog : public QDialog {
    Q_OBJECT

public:
    TenantDetailDialog(Database *db, const Room &room, int ownerId, QWidget *parent = nullptr);

private:
    Database *db;
    Room room;
    int ownerId;

    QWidget *buildHeader();
    QWidget *buildInfoCard();
    QWidget *buildCurrentBillCard();
    QWidget *buildPaymentHistoryCard();
};
