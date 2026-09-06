#include "roomdialog.h"
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDate>
#include <QPushButton>
#include <QMessageBox>
#include <QInputDialog>
#include "telegramservice.h"

RoomDialog::RoomDialog(QWidget *parent, const Room &existing, Database *db, int ownerId)
    : QDialog(parent), db(db), ownerId(ownerId), originalRoom(existing)
{
    setWindowTitle(existing.id == -1 ? "Add Room" : "Edit Room");
    setMinimumWidth(360);

    numberEdit = new QLineEdit(existing.number, this);

    floorCombo = new QComboBox(this);
    floorCombo->addItems({"downstairs", "upstairs"});
    floorCombo->setCurrentText(existing.floor.isEmpty() ? "downstairs" : existing.floor);

    statusCombo = new QComboBox(this);
    statusCombo->addItems({"available", "occupied", "maintenance"});
    statusCombo->setCurrentText(existing.status.isEmpty() ? "available" : existing.status);

    tenantEdit = new QLineEdit(existing.tenantName, this);
    phoneEdit = new QLineEdit(existing.phone, this);

    telegramChatIdEdit = new QLineEdit(existing.telegramChatId, this);
    telegramChatIdEdit->setPlaceholderText("Ask tenant to message the bot, then Detect");

    QPushButton *detectBtn = new QPushButton("Detect from Bot", this);
    connect(detectBtn, &QPushButton::clicked, this, &RoomDialog::onDetectTelegramChatId);

    QHBoxLayout *chatIdRow = new QHBoxLayout;
    chatIdRow->setContentsMargins(0, 0, 0, 0);
    chatIdRow->addWidget(telegramChatIdEdit, 1);
    chatIdRow->addWidget(detectBtn);
    QWidget *chatIdRowWidget = new QWidget;
    chatIdRowWidget->setLayout(chatIdRow);

    stayTypeCombo = new QComboBox(this);
    stayTypeCombo->addItem("Monthly (long-term tenant)", "monthly");
    stayTypeCombo->addItem("Short-term (daily rate)", "shortterm");
    int stayIndex = existing.stayType == "shortterm" ? 1 : 0;
    stayTypeCombo->setCurrentIndex(stayIndex);

    // ---- Monthly-only fields ----
    rentSpin = new QDoubleSpinBox(this);
    rentSpin->setRange(0, 10000);
    rentSpin->setPrefix("$ ");
    rentSpin->setValue(existing.rentPerMonth);

    dueDateEdit = new QDateEdit(this);
    dueDateEdit->setCalendarPopup(true);
    dueDateEdit->setDisplayFormat("dd/MM/yyyy");
    {
        QDate existingDue = QDate::fromString(existing.dueDate, "yyyy-MM-dd");
        if (existingDue.isValid()) {
            dueDateEdit->setDate(existingDue);
        } else if (existing.dueDay > 0) {
            // Legacy rooms only stored a day-of-month (1-28); fall back to that,
            // using today's year/month since we don't have the real history.
            QDate today = QDate::currentDate();
            dueDateEdit->setDate(QDate(today.year(), today.month(), existing.dueDay));
        } else {
            dueDateEdit->setDate(QDate::currentDate());
        }
    }

    monthlySection = new QWidget(this);
    QFormLayout *monthlyForm = new QFormLayout(monthlySection);
    monthlyForm->setContentsMargins(0, 0, 0, 0);
    monthlyForm->addRow("Rent per month:", rentSpin);
    monthlyForm->addRow("Rent due date:", dueDateEdit);

    // ---- Short-term-only fields ----
    checkInEdit = new QDateEdit(this);
    checkInEdit->setCalendarPopup(true);
    checkInEdit->setDate(QDate::fromString(existing.checkInDate, "yyyy-MM-dd").isValid()
                              ? QDate::fromString(existing.checkInDate, "yyyy-MM-dd")
                              : QDate::currentDate());

    stillStayingCheck = new QCheckBox("Still staying (no checkout date yet)", this);
    bool hasCheckout = QDate::fromString(existing.checkOutDate, "yyyy-MM-dd").isValid();
    stillStayingCheck->setChecked(!hasCheckout);

    checkOutEdit = new QDateEdit(this);
    checkOutEdit->setCalendarPopup(true);
    checkOutEdit->setDate(hasCheckout ? QDate::fromString(existing.checkOutDate, "yyyy-MM-dd")
                                       : QDate::currentDate().addDays(1));
    checkOutEdit->setEnabled(hasCheckout);
    connect(stillStayingCheck, &QCheckBox::toggled, checkOutEdit, [this](bool stillStaying) {
        checkOutEdit->setEnabled(!stillStaying);
    });

    dailyRateSpin = new QDoubleSpinBox(this);
    dailyRateSpin->setRange(0, 1000);
    dailyRateSpin->setPrefix("$ ");
    dailyRateSpin->setValue(existing.dailyRate);

    shortTermSection = new QWidget(this);
    QFormLayout *shortTermForm = new QFormLayout(shortTermSection);
    shortTermForm->setContentsMargins(0, 0, 0, 0);
    shortTermForm->addRow("Check-in date:", checkInEdit);
    shortTermForm->addRow("", stillStayingCheck);
    shortTermForm->addRow("Check-out date:", checkOutEdit);
    shortTermForm->addRow("Rate per night:", dailyRateSpin);

    QFormLayout *form = new QFormLayout;
    form->addRow("Room number:", numberEdit);
    form->addRow("Floor:", floorCombo);
    form->addRow("Status:", statusCombo);
    form->addRow("Tenant name:", tenantEdit);
    form->addRow("Phone:", phoneEdit);
    form->addRow("Telegram Chat ID:", chatIdRowWidget);
    form->addRow("Stay type:", stayTypeCombo);
    form->addRow(monthlySection);
    form->addRow(shortTermSection);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(form);
    mainLayout->addWidget(buttons);

    connect(stayTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RoomDialog::onStayTypeChanged);
    onStayTypeChanged(); // show the correct section right away
}

void RoomDialog::onStayTypeChanged()
{
    bool isMonthly = stayTypeCombo->currentData().toString() == "monthly";
    monthlySection->setVisible(isMonthly);
    shortTermSection->setVisible(!isMonthly);
}

void RoomDialog::onDetectTelegramChatId()
{
    if (!db || ownerId == -1) {
        QMessageBox::warning(this, "Not available", "Telegram lookup isn't available here.");
        return;
    }

    TelegramSettings settings = db->getTelegramSettings(ownerId);
    if (settings.botToken.trimmed().isEmpty()) {
        QMessageBox::warning(this, "Bot not set up",
            "Set up your Telegram Bot Token first, in the Settings page.");
        return;
    }

    auto *telegram = new TelegramService(this);
    connect(telegram, &TelegramService::startersFetched, this,
        [this, telegram](bool success, const QVector<QPair<QString, QString>> &names, const QString &error) {
            telegram->deleteLater();

            if (!success) {
                QMessageBox::warning(this, "Lookup failed", error);
                return;
            }
            if (names.isEmpty()) {
                QMessageBox::information(this, "No one found",
                    "No one has messaged your bot yet. Ask the tenant to open the bot's link "
                    "and send it any message (like \"hi\"), then try Detect again.");
                return;
            }

            QStringList labels;
            for (const auto &pair : names)
                labels << QString("%1  (%2)").arg(pair.first, pair.second);

            bool ok = false;
            QString choice = QInputDialog::getItem(this, "Select tenant",
                "Who messaged the bot?", labels, 0, false, &ok);
            if (!ok) return;

            int index = labels.indexOf(choice);
            if (index >= 0)
                telegramChatIdEdit->setText(names[index].second);
        });

    telegram->fetchRecentStarters(settings.botToken);
}

Room RoomDialog::getRoom() const
{
    Room r;
    r.id = originalRoom.id; // -1 if new, real id if editing
    r.number = numberEdit->text();
    r.floor = floorCombo->currentText();
    r.status = statusCombo->currentText();
    r.tenantName = tenantEdit->text();
    r.phone = phoneEdit->text();
    r.telegramChatId = telegramChatIdEdit->text().trimmed();
    r.stayType = stayTypeCombo->currentData().toString();

    if (r.stayType == "monthly") {
        r.rentPerMonth = rentSpin->value();
        r.dueDate = dueDateEdit->date().toString("yyyy-MM-dd");
        r.dueDay = dueDateEdit->date().day(); // derived, still used for the monthly overdue calculation
        r.checkInDate.clear();
        r.checkOutDate.clear();
        r.dailyRate = 0;
    } else {
        r.rentPerMonth = 0;
        r.dueDate.clear();
        r.dueDay = 1;
        r.checkInDate = checkInEdit->date().toString("yyyy-MM-dd");
        r.checkOutDate = stillStayingCheck->isChecked() ? "" : checkOutEdit->date().toString("yyyy-MM-dd");
        r.dailyRate = dailyRateSpin->value();
    }

    return r;
}
