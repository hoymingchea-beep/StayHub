#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include "database.h"
#include "room.h"
#include "utility.h"

// Shows one room's full monthly bill: previous vs current electricity/water
// readings (previous carries forward automatically month to month), rates,
// an optional KHR exchange rate, room charge, and a grand total.
// Opened by clicking "Bill" on a room row.
class UtilityDialog : public QDialog {
    Q_OBJECT

public:
    UtilityDialog(Database *db, const Room &room, int ownerId, QWidget *parent = nullptr);

private slots:
    void onSave();
    void onSendTelegram();
    void recalcTotals();

private:
    Database *db;
    Room room;
    int ownerId;
    Utility utility;
    double roomCharge = 0;

    QLineEdit *elecPrevEdit;  // read-only: locked in automatically from last month
    QLineEdit *elecCurrEdit;
    QLineEdit *elecRateEdit;
    QLineEdit *waterPrevEdit; // read-only
    QLineEdit *waterCurrEdit;
    QLineEdit *waterRateEdit;
    QLineEdit *exchangeRateEdit;

    QLabel *elecUsageLabel;
    QLabel *waterUsageLabel;
    QLabel *roomChargeLabel;
    QLabel *elecCostLabel;
    QLabel *waterCostLabel;
    QLabel *grandTotalLabel;
    QPushButton *sendTelegramBtn;

    // Copies the current field values into `utility` and saves them. Used by
    // both onSave() (which then closes the dialog) and onSendTelegram() (which doesn't).
    bool persistUtility();

    // Builds the invoice text: room charge + electricity + water breakdown + grand total.
    QString buildBillMessage() const;
};
