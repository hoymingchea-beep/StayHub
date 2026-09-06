#include "tenantdetaildialog.h"
#include "utilitydialog.h"
#include "roomdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QListWidget>
#include <QListWidgetItem>
#include <QDate>
#include <QLocale>
#include <QColor>
#include <QMessageBox>

// ---- small local formatting helpers (mirrors utilitydialog.cpp's) ----
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

static QString formatKhr(double value)
{
    QLocale locale(QLocale::English);
    return locale.toString(qRound64(value));
}

static QLabel *makeSectionTitle(const QString &text)
{
    QLabel *lbl = new QLabel(text);
    lbl->setStyleSheet("font-size: 14px; font-weight: 700; color: #0f172a;");
    return lbl;
}

static QFrame *makeCard()
{
    QFrame *card = new QFrame;
    card->setObjectName("rowCard");
    return card;
}

TenantDetailDialog::TenantDetailDialog(Database *db, const Room &room, int ownerId, QWidget *parent)
    : QDialog(parent), db(db), room(room), ownerId(ownerId)
{
    setWindowTitle(QString("Room %1").arg(room.number));
    setMinimumWidth(480);
    resize(520, 700);

    QVBoxLayout *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    QWidget *content = new QWidget;
    QVBoxLayout *mainLayout = new QVBoxLayout(content);
    mainLayout->setContentsMargins(24, 22, 24, 22);
    mainLayout->setSpacing(16);

    mainLayout->addWidget(buildHeader());
    mainLayout->addWidget(buildInfoCard());
    mainLayout->addWidget(buildCurrentBillCard());
    mainLayout->addWidget(buildPaymentHistoryCard());
    mainLayout->addStretch();

    scrollArea->setWidget(content);
    outerLayout->addWidget(scrollArea, 1);

    // ---- Fixed button bar ----
    QFrame *buttonBar = new QFrame;
    buttonBar->setStyleSheet("background-color: #ffffff; border-top: 1px solid #e5e7eb;");
    QHBoxLayout *buttonLayout = new QHBoxLayout(buttonBar);
    buttonLayout->setContentsMargins(20, 12, 20, 14);
    buttonLayout->setSpacing(10);

    QPushButton *billBtn = new QPushButton("\U0001F4B0 Bill");
    billBtn->setMinimumHeight(40);
    connect(billBtn, &QPushButton::clicked, this, [this]() {
        UtilityDialog dlg(this->db, this->room, this->ownerId, this);
        dlg.exec();
    });
    buttonLayout->addWidget(billBtn);

    QPushButton *editBtn = new QPushButton("Edit Room Info");
    editBtn->setMinimumHeight(40);
    connect(editBtn, &QPushButton::clicked, this, [this]() {
        RoomDialog dialog(this, this->room, this->db, this->ownerId);
        if (dialog.exec() == QDialog::Accepted) {
            Room updated = dialog.getRoom();
            if (this->db->updateRoom(updated)) {
                // Close so the Rooms list (and this dashboard, if reopened)
                // picks up the change right away.
                accept();
            } else {
                QMessageBox::warning(this, "Error", "Could not update room.");
            }
        }
    });
    buttonLayout->addWidget(editBtn);

    buttonLayout->addStretch();

    QPushButton *closeBtn = new QPushButton("Close");
    closeBtn->setMinimumHeight(40);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(closeBtn);

    outerLayout->addWidget(buttonBar);
}

QWidget *TenantDetailDialog::buildHeader()
{
    QWidget *header = new QWidget;
    QHBoxLayout *layout = new QHBoxLayout(header);
    layout->setContentsMargins(0, 0, 0, 0);

    QVBoxLayout *textCol = new QVBoxLayout;
    textCol->setSpacing(2);

    QLabel *nameLbl = new QLabel(room.tenantName.isEmpty() ? "No tenant" : room.tenantName);
    nameLbl->setStyleSheet("font-size: 22px; font-weight: 800; color: #0f172a;");
    textCol->addWidget(nameLbl);

    QLabel *roomLbl = new QLabel(QString("Room %1  \u00B7  %2").arg(room.number, room.floor));
    roomLbl->setStyleSheet("color: #64748b; font-size: 13px;");
    textCol->addWidget(roomLbl);

    layout->addLayout(textCol);
    layout->addStretch();

    QLabel *badge = new QLabel(room.status.toUpper());
    badge->setProperty("status", room.status);
    layout->addWidget(badge, 0, Qt::AlignTop);

    return header;
}

QWidget *TenantDetailDialog::buildInfoCard()
{
    QFrame *card = makeCard();
    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(10);

    layout->addWidget(makeSectionTitle("Tenant & Stay Info"));

    QGridLayout *grid = new QGridLayout;
    grid->setHorizontalSpacing(18);
    grid->setVerticalSpacing(8);
    grid->setColumnStretch(1, 1);

    auto addRow = [&](int row, const QString &label, const QString &value) {
        QLabel *l = new QLabel(label);
        l->setStyleSheet("color: #64748b; font-size: 13px;");
        QLabel *v = new QLabel(value);
        v->setStyleSheet("color: #0f172a; font-size: 13px; font-weight: 600;");
        v->setWordWrap(true);
        grid->addWidget(l, row, 0);
        grid->addWidget(v, row, 1);
    };

    int row = 0;
    addRow(row++, "Phone:", room.phone.isEmpty() ? "\u2014" : room.phone);
    addRow(row++, "Telegram:", room.telegramChatId.trimmed().isEmpty()
                                    ? "Not linked yet" : QString("\u2705 Linked (Chat ID %1)").arg(room.telegramChatId));

    if (room.stayType == "shortterm") {
        QDate checkIn = QDate::fromString(room.checkInDate, "yyyy-MM-dd");
        QDate checkOut = QDate::fromString(room.checkOutDate, "yyyy-MM-dd");
        int nights = checkIn.isValid()
            ? qMax(1, static_cast<int>(checkIn.daysTo(checkOut.isValid() ? checkOut : QDate::currentDate())))
            : 0;
        addRow(row++, "Stay type:", "Short-term");
        addRow(row++, "Check-in:", checkIn.isValid() ? checkIn.toString("MMM d, yyyy") : "\u2014");
        addRow(row++, "Check-out:", checkOut.isValid() ? checkOut.toString("MMM d, yyyy") : "Ongoing");
        addRow(row++, "Rate:", QString("$%1/night  \u00B7  %2 night(s) so far").arg(room.dailyRate, 0, 'f', 2).arg(nights));
    } else {
        addRow(row++, "Stay type:", "Monthly");
        addRow(row++, "Rent:", QString("$%1/month").arg(room.rentPerMonth, 0, 'f', 2));
        addRow(row++, "Due date:", room.dueDate.isEmpty() ? "\u2014" : room.dueDate);
    }

    layout->addLayout(grid);
    return card;
}

QWidget *TenantDetailDialog::buildCurrentBillCard()
{
    QFrame *card = makeCard();
    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(10);

    QString month = QDate::currentDate().toString("yyyy-MM");
    layout->addWidget(makeSectionTitle(QString("This Month's Bill (%1)").arg(month)));

    if (room.status != "occupied" && room.tenantName.isEmpty()) {
        QLabel *empty = new QLabel("No tenant currently assigned to this room.");
        empty->setStyleSheet("color: #94a3b8; font-size: 13px;");
        layout->addWidget(empty);
        return card;
    }

    Utility u = db->getUtilityForMonth(room.id, month);

    double roomCharge = 0;
    if (room.stayType == "shortterm") {
        QDate checkIn = QDate::fromString(room.checkInDate, "yyyy-MM-dd");
        QDate checkOut = QDate::fromString(room.checkOutDate, "yyyy-MM-dd");
        int nights = checkIn.isValid()
            ? qMax(1, static_cast<int>(checkIn.daysTo(checkOut.isValid() ? checkOut : QDate::currentDate())))
            : 0;
        roomCharge = nights * room.dailyRate;
    } else {
        roomCharge = room.rentPerMonth;
    }

    double grandTotalUsd = roomCharge + u.totalCost();
    double grandTotalKhr = grandTotalUsd * (u.exchangeRate > 0 ? u.exchangeRate : 4100);

    QGridLayout *grid = new QGridLayout;
    grid->setHorizontalSpacing(18);
    grid->setVerticalSpacing(6);
    grid->setColumnStretch(1, 1);

    auto addRow = [&](int row, const QString &label, const QString &value) {
        QLabel *l = new QLabel(label);
        l->setStyleSheet("color: #64748b; font-size: 13px;");
        QLabel *v = new QLabel(value);
        v->setStyleSheet("color: #0f172a; font-size: 13px;");
        grid->addWidget(l, row, 0);
        grid->addWidget(v, row, 1);
    };

    addRow(0, "Room charge:", QString("$%1").arg(formatMoney(roomCharge)));
    addRow(1, "Electricity:", QString("%1 kWh \u00D7 $%2 = $%3")
        .arg(formatMoney(u.elecUnits())).arg(formatMoney(u.elecRate)).arg(formatMoney(u.elecCost())));
    addRow(2, "Water:", QString("%1 m\u00B3 \u00D7 $%2 = $%3")
        .arg(formatMoney(u.waterUnits())).arg(formatMoney(u.waterRate)).arg(formatMoney(u.waterCost())));

    layout->addLayout(grid);

    QFrame *sep = new QFrame; sep->setFrameShape(QFrame::HLine); sep->setStyleSheet("color:#e5e7eb;");
    layout->addWidget(sep);

    QLabel *total = new QLabel(QString("Total: $%1  (\u2248 %2\u17DB)")
        .arg(formatMoney(grandTotalUsd)).arg(formatKhr(grandTotalKhr)));
    total->setStyleSheet("font-size: 17px; font-weight: 800; color: #4f5eff;");
    layout->addWidget(total);

    return card;
}

QWidget *TenantDetailDialog::buildPaymentHistoryCard()
{
    QFrame *card = makeCard();
    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(10);

    layout->addWidget(makeSectionTitle("Payment History"));

    QVector<Payment> allPayments = db->getPaymentHistory(ownerId);
    QVector<Payment> roomPayments;
    for (const Payment &p : allPayments) {
        if (p.roomId == room.id)
            roomPayments.append(p);
    }

    if (roomPayments.isEmpty()) {
        QLabel *empty = new QLabel("No payment history yet for this room.");
        empty->setStyleSheet("color: #94a3b8; font-size: 13px;");
        layout->addWidget(empty);
        return card;
    }

    QListWidget *list = new QListWidget;
    list->setSpacing(2);
    list->setFrameShape(QFrame::NoFrame);
    // Sized to show a handful of rows without the outer dialog scroll getting
    // awkward (a nested scrollbar appears if the tenant has a long history).
    list->setMinimumHeight(qMin(240, 40 * roomPayments.size() + 10));

    for (const Payment &p : roomPayments) {
        double remaining = qMax(0.0, p.amount - p.paidAmount);
        QString text;
        if (p.paid) {
            text = QString("%1  \u00B7  Bill $%2  \u00B7  Paid $%3 on %4")
                .arg(p.month).arg(p.amount, 0, 'f', 2).arg(p.paidAmount, 0, 'f', 2).arg(p.paidOn);
        } else if (p.paidAmount > 0) {
            text = QString("%1  \u00B7  Bill $%2  \u00B7  Paid $%3  \u00B7  Remaining $%4")
                .arg(p.month).arg(p.amount, 0, 'f', 2).arg(p.paidAmount, 0, 'f', 2).arg(remaining, 0, 'f', 2);
        } else {
            text = QString("%1  \u00B7  Bill $%2  \u00B7  Not paid yet")
                .arg(p.month).arg(p.amount, 0, 'f', 2);
        }

        QListWidgetItem *item = new QListWidgetItem(text);
        item->setForeground(p.paid ? QColor("#16a34a") : (p.paidAmount > 0 ? QColor("#d97706") : QColor("#dc2626")));
        list->addItem(item);
    }

    layout->addWidget(list);
    return card;
}
