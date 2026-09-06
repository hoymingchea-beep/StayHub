#include "utilitydialog.h"
#include "telegramservice.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QDialogButtonBox>
#include <QDoubleValidator>
#include <QDate>
#include <QFrame>
#include <QLocale>
#include <QScrollArea>
#include <QMessageBox>

// Formats a $ amount with the fewest decimals that don't lose precision,
// but always at least 2 (so "45" becomes "45.00", while "1.525" stays as-is).
static QString formatMoney(double value)
{
    QString s = QString::number(value, 'f', 3);
    while (s.endsWith('0'))
        s.chop(1);
    if (s.endsWith('.'))
        s += "00";

    int dotPos = s.indexOf('.');
    int decimals = s.length() - dotPos - 1;
    if (decimals < 2)
        s += QString(2 - decimals, '0');
    return s;
}

// Formats a KHR amount as a whole number with comma thousands separators,
// e.g. 207050 -> "207,050".
static QString formatKhr(double value)
{
    QLocale locale(QLocale::English);
    return locale.toString(qRound64(value));
}

// Builds one "label above box" field. If readOnly, the box is greyed out to
// show it's locked in automatically (previous month's reading).
static QWidget *makeField(QLineEdit *&editOut, const QString &label, double value, bool readOnly = false)
{
    QWidget *wrap = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(wrap);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    QLabel *lbl = new QLabel(label);
    lbl->setStyleSheet("color: #64748b; font-size: 13px;");

    editOut = new QLineEdit(QString::number(value, 'f', 2));
    editOut->setValidator(new QDoubleValidator(0, 999999999, 4, editOut));
    editOut->setReadOnly(readOnly);
    editOut->setMinimumHeight(36);
    if (readOnly) {
        editOut->setStyleSheet("background-color: #f1f5f9; color: #64748b; border: 1px solid #e5e7eb; "
                                "border-radius: 8px; padding: 6px 10px; font-size: 14px;");
    } else {
        editOut->setStyleSheet("background-color: #ffffff; color: #0f172a; border: 1px solid #cbd5e1; "
                                "border-radius: 8px; padding: 6px 10px; font-size: 14px;");
    }

    layout->addWidget(lbl);
    layout->addWidget(editOut);
    return wrap;
}

UtilityDialog::UtilityDialog(Database *db, const Room &room, int ownerId, QWidget *parent)
    : QDialog(parent), db(db), room(room), ownerId(ownerId)
{
    QString month = QDate::currentDate().toString("yyyy-MM");
    utility = db->getUtilityForMonth(room.id, month);

    QString dialogHeading = QString("កត់ថ្លៃទឹកភ្លើង — Room %1").arg(room.number);
    setWindowTitle(dialogHeading);
    setMinimumWidth(460);
    resize(480, 640); // fits most screens; the scroll area below handles anything taller

    // Use a font that actually supports Khmer glyphs well, falling back to
    // whatever's available. Scoped to labels/inputs only - NOT QWidget broadly,
    // because styling QWidget strips buttons of their native background/border
    // and makes them render blank.
    setStyleSheet("QLabel, QLineEdit { font-family: 'Kantumruy Pro', 'Khmer OS Battambang', 'Leelawadee UI', 'Segoe UI'; }");

    // Outer layout: scrollable fields on top, buttons pinned at the bottom so
    // they're always reachable no matter how tall the content or how small the screen.
    QVBoxLayout *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    QWidget *scrollContent = new QWidget;
    QVBoxLayout *mainLayout = new QVBoxLayout(scrollContent);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QLabel *heading = new QLabel(dialogHeading);
    heading->setStyleSheet("font-size: 18px; font-weight: 700; color: #0f172a;");
    mainLayout->addWidget(heading);

    QLabel *note = new QLabel(
        "សូមបញ្ចូលចំនួនឡើងទឹក និងភ្លើងសម្រាប់ខែនេះ។ ចំនួនខែមុនត្រូវបានចាក់សោដោយស្វ័យប្រវត្តិ។"
    );
    note->setWordWrap(true);
    note->setStyleSheet("background-color: #eef4fd; color: #3b5878; font-size: 13px; padding: 10px; border-radius: 8px;");
    mainLayout->addWidget(note);
    mainLayout->addSpacing(10);

    // ---- Electricity row ----
    QHBoxLayout *elecRow = new QHBoxLayout;
    elecRow->addWidget(makeField(elecPrevEdit, "អគ្គិសនីខែមុន (Kw/h)", utility.elecPrev, true));
    elecRow->addWidget(makeField(elecCurrEdit, "អគ្គិសនីខែនេះ (Kw/h)", utility.elecCurr, false));
    mainLayout->addLayout(elecRow);

    elecUsageLabel = new QLabel;
    elecUsageLabel->setStyleSheet("color: #4f5eff; font-size: 13px; font-weight: 600; padding: 3px 0 10px 2px;");
    mainLayout->addWidget(elecUsageLabel);

    // ---- Water row ----
    QHBoxLayout *waterRow = new QHBoxLayout;
    waterRow->addWidget(makeField(waterPrevEdit, "ទឹកខែមុន (m³)", utility.waterPrev, true));
    waterRow->addWidget(makeField(waterCurrEdit, "ទឹកខែនេះ (m³)", utility.waterCurr, false));
    mainLayout->addLayout(waterRow);

    waterUsageLabel = new QLabel;
    waterUsageLabel->setStyleSheet("color: #4f5eff; font-size: 13px; font-weight: 600; padding: 3px 0 10px 2px;");
    mainLayout->addWidget(waterUsageLabel);

    // ---- Rates row ----
    QHBoxLayout *rateRow = new QHBoxLayout;
    rateRow->addWidget(makeField(elecRateEdit, "ថ្លៃអគ្គិសនី ($/Kw/h)", utility.elecRate, false));
    rateRow->addWidget(makeField(waterRateEdit, "ថ្លៃទឹក ($/m³)", utility.waterRate, false));
    mainLayout->addLayout(rateRow);
    mainLayout->addSpacing(4);

    mainLayout->addWidget(makeField(exchangeRateEdit, "អត្រាប្តូរប្រាក់ (KHR → 1 USD)", utility.exchangeRate, false));
    mainLayout->addSpacing(14);

    // ---- Room charge / cost breakdown / grand total ----
    QFrame *sep1 = new QFrame; sep1->setFrameShape(QFrame::HLine); sep1->setStyleSheet("color:#e5e7eb;");
    mainLayout->addWidget(sep1);
    mainLayout->addSpacing(6);

    QString roomChargeDescription;
    if (room.stayType == "shortterm") {
        QDate checkIn = QDate::fromString(room.checkInDate, "yyyy-MM-dd");
        QDate checkOut = QDate::fromString(room.checkOutDate, "yyyy-MM-dd");
        int nights = checkIn.isValid()
            ? qMax(1, static_cast<int>(checkIn.daysTo(checkOut.isValid() ? checkOut : QDate::currentDate())))
            : 0;
        roomCharge = nights * room.dailyRate;
        roomChargeDescription = QString("%1 night(s) × $%2 = $%3")
            .arg(nights).arg(room.dailyRate, 0, 'f', 2).arg(roomCharge, 0, 'f', 2);
    } else {
        roomCharge = room.rentPerMonth;
        roomChargeDescription = QString("$%1 (ថ្លៃប្រចាំខែ)").arg(roomCharge, 0, 'f', 2);
    }

    auto addSummaryRow = [](QGridLayout *grid, int row, const QString &labelText, QLabel *&valueLabelOut) {
        QLabel *label = new QLabel(labelText);
        label->setStyleSheet("font-size: 13px; color: #334155;");
        grid->addWidget(label, row, 0);
        valueLabelOut = new QLabel;
        valueLabelOut->setStyleSheet("font-size: 13px; color: #0f172a;");
        grid->addWidget(valueLabelOut, row, 1);
    };

    QGridLayout *summary = new QGridLayout;
    summary->setVerticalSpacing(8);
    summary->setColumnStretch(1, 1);

    addSummaryRow(summary, 0, "ថ្លៃបន្ទប់ (Room charge):", roomChargeLabel);
    roomChargeLabel->setText(roomChargeDescription);

    addSummaryRow(summary, 1, "ថ្លៃអគ្គិសនី:", elecCostLabel);
    addSummaryRow(summary, 2, "ថ្លៃទឹក:", waterCostLabel);

    mainLayout->addLayout(summary);
    mainLayout->addSpacing(10);

    QFrame *sep2 = new QFrame; sep2->setFrameShape(QFrame::HLine); sep2->setStyleSheet("color:#e5e7eb;");
    mainLayout->addWidget(sep2);

    QLabel *totalCaption = new QLabel("សរុបត្រូវបង់ (Total)");
    totalCaption->setStyleSheet("font-size: 13px; color: #64748b; padding-top: 8px;");
    mainLayout->addWidget(totalCaption);

    grandTotalLabel = new QLabel;
    grandTotalLabel->setStyleSheet(
        "font-family: 'Plus Jakarta Sans ExtraBold'; font-size: 26px; color: #4f5eff;");
    mainLayout->addWidget(grandTotalLabel);

    mainLayout->addStretch();

    scrollArea->setWidget(scrollContent);
    outerLayout->addWidget(scrollArea, 1);

    // ---- Fixed button bar (always visible, never scrolls off-screen) ----
    QFrame *buttonBar = new QFrame;
    buttonBar->setStyleSheet("background-color: #ffffff; border-top: 1px solid #e5e7eb;");
    QVBoxLayout *buttonLayout = new QVBoxLayout(buttonBar);
    buttonLayout->setContentsMargins(20, 14, 20, 16);
    buttonLayout->setSpacing(8);

    sendTelegramBtn = new QPushButton("📩  ផ្ញើវិក្កយបត្រ (Telegram)", this);
    sendTelegramBtn->setMinimumHeight(42);
    sendTelegramBtn->setCursor(Qt::PointingHandCursor);
    sendTelegramBtn->setStyleSheet(
        "QPushButton { background-color: #0ea5e9; color: white; border: none; "
        "border-radius: 8px; padding: 10px; font-size: 14px; font-weight: 600; "
        "font-family: 'Kantumruy Pro', 'Khmer OS Battambang', 'Leelawadee UI', 'Segoe UI'; }"
        "QPushButton:hover { background-color: #0b8fcf; }"
        "QPushButton:disabled { background-color: #94a3b8; }"
    );
    connect(sendTelegramBtn, &QPushButton::clicked, this, &UtilityDialog::onSendTelegram);
    buttonLayout->addWidget(sendTelegramBtn);

    QPushButton *saveBtn = new QPushButton("រក្សាទុក  (Save)", this);
    saveBtn->setMinimumHeight(44);
    saveBtn->setCursor(Qt::PointingHandCursor);
    saveBtn->setStyleSheet(
        "QPushButton { background-color: #4f5eff; color: white; border: none; "
        "border-radius: 8px; padding: 10px; font-size: 15px; font-weight: 700; "
        "font-family: 'Kantumruy Pro', 'Khmer OS Battambang', 'Leelawadee UI', 'Segoe UI'; }"
        "QPushButton:hover { background-color: #4048d8; }"
    );
    connect(saveBtn, &QPushButton::clicked, this, &UtilityDialog::onSave);
    buttonLayout->addWidget(saveBtn);

    QPushButton *cancelBtn = new QPushButton("Cancel", this);
    cancelBtn->setCursor(Qt::PointingHandCursor);
    cancelBtn->setStyleSheet(
        "QPushButton { background-color: transparent; color: #64748b; border: none; padding: 8px; font-size: 13px; "
        "font-family: 'Kantumruy Pro', 'Khmer OS Battambang', 'Leelawadee UI', 'Segoe UI'; }"
        "QPushButton:hover { color: #334155; }"
    );
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(cancelBtn);

    outerLayout->addWidget(buttonBar);

    connect(elecCurrEdit, &QLineEdit::textChanged, this, &UtilityDialog::recalcTotals);
    connect(elecRateEdit, &QLineEdit::textChanged, this, &UtilityDialog::recalcTotals);
    connect(waterCurrEdit, &QLineEdit::textChanged, this, &UtilityDialog::recalcTotals);
    connect(waterRateEdit, &QLineEdit::textChanged, this, &UtilityDialog::recalcTotals);
    connect(exchangeRateEdit, &QLineEdit::textChanged, this, &UtilityDialog::recalcTotals);

    recalcTotals();
}

void UtilityDialog::recalcTotals()
{
    Utility preview = utility;
    preview.elecCurr = elecCurrEdit->text().toDouble();
    preview.elecRate = elecRateEdit->text().toDouble();
    preview.waterCurr = waterCurrEdit->text().toDouble();
    preview.waterRate = waterRateEdit->text().toDouble();
    preview.exchangeRate = exchangeRateEdit->text().toDouble();

    elecUsageLabel->setText(QString("→ ប្រើ: %1 Kw/h").arg(formatMoney(preview.elecUnits())));
    waterUsageLabel->setText(QString("→ ប្រើ: %1 m³").arg(formatMoney(preview.waterUnits())));

    elecCostLabel->setText(QString("%1 kWh × $%2 = $%3")
        .arg(formatMoney(preview.elecUnits()))
        .arg(formatMoney(preview.elecRate))
        .arg(formatMoney(preview.elecCost())));

    waterCostLabel->setText(QString("%1 m³ × $%2 = $%3")
        .arg(formatMoney(preview.waterUnits()))
        .arg(formatMoney(preview.waterRate))
        .arg(formatMoney(preview.waterCost())));

    double grandTotalUsd = roomCharge + preview.totalCost();
    double grandTotalKhr = grandTotalUsd * (preview.exchangeRate > 0 ? preview.exchangeRate : 4100);
    grandTotalLabel->setText(QString("$%1   (≈ %2៛)")
        .arg(formatMoney(grandTotalUsd))
        .arg(formatKhr(grandTotalKhr)));
}

bool UtilityDialog::persistUtility()
{
    utility.elecCurr = elecCurrEdit->text().toDouble();
    utility.elecRate = elecRateEdit->text().toDouble();
    utility.waterCurr = waterCurrEdit->text().toDouble();
    utility.waterRate = waterRateEdit->text().toDouble();
    utility.exchangeRate = exchangeRateEdit->text().toDouble();

    if (!db->saveUtility(utility))
        return false;

    // Keep the Payments/History record in sync with the real total (room + utilities),
    // not just the base rent - so what's tracked as "paid" matches what was actually billed.
    // Short-term stays don't use the recurring monthly payment cycle, so skip them.
    if (room.stayType == "monthly") {
        double totalDue = roomCharge + utility.totalCost();
        db->syncPaymentAmountForRoomMonth(room.id, utility.month, totalDue);
    }

    return true;
}

void UtilityDialog::onSave()
{
    if (persistUtility())
        accept();
    else
        reject();
}

QString UtilityDialog::buildBillMessage() const
{
    QString tenantName = room.tenantName.isEmpty() ? "there" : room.tenantName;
    double grandTotalUsd = roomCharge + utility.totalCost();
    double grandTotalKhr = grandTotalUsd * (utility.exchangeRate > 0 ? utility.exchangeRate : 4100);

    return QString(
        "Hello, %1!\n"
        "This is your Payment invoice for this month:\n"
        "Room %2: %3 $\n"
        "Electricity: %4Kw * %5$ = %6 $\n"
        "Water: %7m^3 * %8$ = %9 $\n"
        "Total bill: %10 $ (≈ %11៛)\n\n"
        "Please pay by cash or your preferred banking method."
    )
    .arg(tenantName)
    .arg(room.number)
    .arg(formatMoney(roomCharge))
    .arg(formatMoney(utility.elecUnits()))
    .arg(formatMoney(utility.elecRate))
    .arg(formatMoney(utility.elecCost()))
    .arg(formatMoney(utility.waterUnits()))
    .arg(formatMoney(utility.waterRate))
    .arg(formatMoney(utility.waterCost()))
    .arg(formatMoney(grandTotalUsd))
    .arg(formatKhr(grandTotalKhr));
}

void UtilityDialog::onSendTelegram()
{
    if (room.telegramChatId.trimmed().isEmpty()) {
        QMessageBox::warning(this, "No Telegram set up",
            "This room has no Telegram Chat ID yet. Edit the room and use "
            "\"Detect from Bot\" once the tenant has messaged your bot.");
        return;
    }

    TelegramSettings settings = db->getTelegramSettings(ownerId);
    if (settings.botToken.trimmed().isEmpty()) {
        QMessageBox::warning(this, "Bot not set up",
            "Set up your Telegram Bot Token first, in the Settings page.");
        return;
    }

    // Save the current readings before sending, so what's texted always matches what's stored.
    if (!persistUtility()) {
        QMessageBox::warning(this, "Couldn't save", "Failed to save the utility readings. Nothing was sent.");
        return;
    }

    QString message = buildBillMessage();

    // If the owner has an ABA KHQR image saved, ask each time whether to attach
    // it to this bill so the tenant can scan-to-pay right away.
    QByteArray qrImage = db->getAbaQrImage(ownerId);
    bool attachQr = false;
    if (!qrImage.isEmpty()) {
        attachQr = QMessageBox::question(this, "Attach ABA QR?",
            "Attach your ABA KHQR payment code to this bill?",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes;
    }

    sendTelegramBtn->setEnabled(false);
    sendTelegramBtn->setText("Sending...");

    auto *telegram = new TelegramService(this);
    connect(telegram, &TelegramService::sendFinished, this,
        [this, telegram](bool success, const QString &error) {
            telegram->deleteLater();
            sendTelegramBtn->setEnabled(true);
            sendTelegramBtn->setText("📩  ផ្ញើវិក្កយបត្រ (Telegram)");

            if (success)
                QMessageBox::information(this, "Sent", "The bill was sent to the tenant on Telegram.");
            else
                QMessageBox::warning(this, "Send failed", error);
        });

    if (attachQr)
        telegram->sendPhoto(settings.botToken, room.telegramChatId, qrImage, message);
    else
        telegram->sendMessage(settings.botToken, room.telegramChatId, message);
}