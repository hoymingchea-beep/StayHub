#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QDateEdit>
#include <QCheckBox>
#include <QWidget>
#include "room.h"
#include "database.h"

// A popup form used for BOTH "Add Room" and "Edit Room".
// Pass an existing Room to pre-fill it for editing, or leave it empty for adding new.
// db/ownerId are only used for the "Detect from Bot" Telegram helper button.
class RoomDialog : public QDialog {
    Q_OBJECT

public:
    RoomDialog(QWidget *parent = nullptr, const Room &existing = Room(),
               Database *db = nullptr, int ownerId = -1);

    // Call this after exec() == QDialog::Accepted to get the filled-in data.
    Room getRoom() const;

private slots:
    void onStayTypeChanged();
    void onDetectTelegramChatId();

private:
    Database *db;
    int ownerId;

    Room originalRoom; // keeps the id when editing
    QLineEdit *numberEdit;
    QComboBox *floorCombo;
    QComboBox *statusCombo;
    QLineEdit *tenantEdit;
    QLineEdit *phoneEdit;
    QLineEdit *telegramChatIdEdit;

    QComboBox *stayTypeCombo;

    // Monthly-only fields
    QWidget *monthlySection;
    QDoubleSpinBox *rentSpin;
    QDateEdit *dueDateEdit;

    // Short-term-only fields
    QWidget *shortTermSection;
    QDateEdit *checkInEdit;
    QDateEdit *checkOutEdit;
    QCheckBox *stillStayingCheck;
    QDoubleSpinBox *dailyRateSpin;
};
