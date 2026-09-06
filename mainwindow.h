#pragma once
#include <QMainWindow>
#include <QStackedWidget>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QVector>
#include "database.h"
#include "clickablecard.h"
#include "incomechartwidget.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(Database *db, int ownerId, const QString &ownerUsername, QWidget *parent = nullptr);

signals:
    void logoutRequested();

private slots:
    void showDashboard();
    void showRooms();
    void showPayments();
    void showIncome();
    void showHistory();
    void showSettings();

    void onAddRoom();
    void onEditRoom();
    void onDeleteRoom();
    void onTogglePaid();
    void onRecordPayment();
    void onSaveTelegramSettings();
    void onTestTelegramConnection();
    void onUploadAbaQr();
    void onRemoveAbaQr();

private:
    Database *db;          // owned by main(), not by MainWindow
    int ownerId;
    QString ownerUsername;

    QStackedWidget *stack;
    QVector<QPushButton*> navButtons; // so we can highlight the active one

    QWidget *dashboardPage;
    QLabel *statTotalRooms;
    QLabel *statOccupied;
    QLabel *statAvailable;
    QLabel *statMonthlyRent;
    QListWidget *alertsListWidget; // due-soon / overdue tenants shown on the dashboard
    class QGridLayout *roomGridLayout; // "Room Overview" clickable status grid

    QWidget *roomsPage;
    QListWidget *roomListWidget;
    class QLineEdit *roomSearchEdit;
    class QComboBox *roomStatusFilterCombo;

    QWidget *paymentsPage;
    QListWidget *paymentListWidget;
    class QComboBox *paymentMonthFilterCombo;
    class QComboBox *paymentStatusFilterCombo;

    QWidget *incomePage;
    IncomeChartWidget *incomeChartWidget;
    QLabel *incomeExpectedLabel;
    QLabel *incomeCollectedLabel;
    QLabel *incomeOutstandingLabel;
    QLabel *incomePartialLabel;
    QLabel *incomeUnpaidLabel;
    QLabel *incomeOverdueLabel;
    QListWidget *incomeOutstandingList;

    QWidget *historyPage;
    QListWidget *historyListWidget;
    class QLineEdit *tenantSearchEdit;
    class QComboBox *historyMonthFilterCombo;

    QWidget *settingsPage;
    class QLineEdit *botTokenEdit;
    QLabel *testConnectionStatusLabel;
    QLabel *qrPreviewLabel;     // shows the owner's saved ABA KHQR image, or a placeholder
    QPushButton *removeQrBtn;   // only enabled once a QR image is saved

    QWidget *buildDashboardPage();
    QWidget *buildRoomsPage();
    QWidget *buildPaymentsPage();
    QWidget *buildIncomePage();
    QWidget *buildHistoryPage();
    QWidget *buildSettingsPage();

    // Builds one styled row widget for a room / payment, used inside the lists.
    QWidget *buildRoomRow(const Room &r);
    QWidget *buildPaymentRow(const Payment &p);

    void refreshDashboard();
    void refreshRoomList();
    void refreshPaymentList();
    void refreshIncomePage();
    void refreshHistoryList();
    void setActiveNav(int index);

    // Reloads the ABA QR preview (or placeholder) in Settings from the database.
    void refreshQrPreview();

    // Sends a "please pay" reminder to a tenant on Telegram, from the dashboard alert list.
    void sendTelegramReminder(const Room &room, int days, bool overdue, double remaining = -1);

    // Offers to send a "we received your payment" receipt on Telegram, after marking paid.
    void offerPaymentReceipt(const Payment &payment);

    int selectedRoomId() const;
    int selectedPaymentId() const;

    // Looks up the currently-selected payment row's full Payment struct, or
    // returns a default-constructed one (id == -1) if nothing's selected.
    Payment selectedPayment() const;
};
