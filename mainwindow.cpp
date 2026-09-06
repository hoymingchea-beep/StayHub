#include "mainwindow.h"
#include "roomdialog.h"
#include "utilitydialog.h"
#include "tenantdetaildialog.h"
#include "telegramservice.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QFrame>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QScrollArea>
#include <QMessageBox>
#include <QInputDialog>
#include <QDate>
#include <QTime>
#include <QTimer>
#include <QListWidgetItem>
#include <QGraphicsDropShadowEffect>
#include <QColor>
#include <QMap>
#include <QStyle>
#include <QPixmap>
#include <QIcon>
#include <QRandomGenerator>
#include <QFileDialog>
#include <QFile>
#include <QByteArray>
#include <algorithm>

MainWindow::MainWindow(Database *db, int ownerId, const QString &ownerUsername, QWidget *parent)
    : QMainWindow(parent), db(db), ownerId(ownerId), ownerUsername(ownerUsername)
{
    setWindowTitle("StayHub - " + ownerUsername);
    setWindowIcon(QIcon(":/icons/app_icon_64.png"));
    resize(820, 560);

    // ---- Sidebar ----
    QWidget *sidebar = new QWidget(this);
    sidebar->setObjectName("sidebar");
    QVBoxLayout *sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(16, 24, 16, 16);
    sidebarLayout->setSpacing(6);
    sidebar->setFixedWidth(190);

    QHBoxLayout *brandRow = new QHBoxLayout;
    brandRow->setSpacing(10);

    // Logo mark sits on a small white rounded badge so its navy/teal colors
    // stay visible against the dark sidebar background.
    QLabel *logoBadge = new QLabel;
    logoBadge->setObjectName("logoBadge");
    logoBadge->setFixedSize(40, 40);
    logoBadge->setAlignment(Qt::AlignCenter);
    QPixmap logoPixmap(":/icons/app_icon_64.png");
    logoBadge->setPixmap(logoPixmap.scaled(30, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    brandRow->addWidget(logoBadge);

    QVBoxLayout *brandTextCol = new QVBoxLayout;
    brandTextCol->setSpacing(0);
    QLabel *appTitle = new QLabel("StayHub");
    appTitle->setObjectName("appTitle");
    QLabel *appSubtitle = new QLabel("Rental Manager");
    appSubtitle->setObjectName("appSubtitle");
    brandTextCol->addWidget(appTitle);
    brandTextCol->addWidget(appSubtitle);

    brandRow->addLayout(brandTextCol);
    brandRow->addStretch();
    sidebarLayout->addLayout(brandRow);
    sidebarLayout->addSpacing(18);

    struct NavItem { QString iconPath; QString label; };
    QVector<NavItem> items = {
        {":/icons/icon_dashboard.png",    "Dashboard"},
        {":/icons/icon_key.png",          "Rooms"},
        {":/icons/icon_card.png",         "Payments"},
        {":/icons/icon_expected_rent.png","Income"},
        {":/icons/icon_history.png",      "History"},
        {":/icons/icon_settings.png",     "Settings"}
    };

    for (const NavItem &it : items) {
        QPushButton *btn = new QPushButton("   " + it.label, this);
        btn->setObjectName("navButton");
        btn->setIcon(QIcon(it.iconPath));
        btn->setIconSize(QSize(18, 18));
        btn->setCursor(Qt::PointingHandCursor);
        btn->setMinimumHeight(40);
        btn->setProperty("active", false);
        sidebarLayout->addWidget(btn);
        navButtons.append(btn);
    }
    sidebarLayout->addStretch();

    QLabel *footer = new QLabel("College Project 2026");
    footer->setObjectName("sidebarFooter");
    sidebarLayout->addWidget(footer);

    connect(navButtons[0], &QPushButton::clicked, this, &MainWindow::showDashboard);
    connect(navButtons[1], &QPushButton::clicked, this, &MainWindow::showRooms);
    connect(navButtons[2], &QPushButton::clicked, this, &MainWindow::showPayments);
    connect(navButtons[3], &QPushButton::clicked, this, &MainWindow::showIncome);
    connect(navButtons[4], &QPushButton::clicked, this, &MainWindow::showHistory);
    connect(navButtons[5], &QPushButton::clicked, this, &MainWindow::showSettings);

    // ---- Pages ----
    stack = new QStackedWidget(this);
    dashboardPage = buildDashboardPage();
    roomsPage = buildRoomsPage();
    paymentsPage = buildPaymentsPage();
    incomePage = buildIncomePage();
    historyPage = buildHistoryPage();
    settingsPage = buildSettingsPage();

    stack->addWidget(dashboardPage);
    stack->addWidget(roomsPage);
    stack->addWidget(paymentsPage);
    stack->addWidget(incomePage);
    stack->addWidget(historyPage);
    stack->addWidget(settingsPage);

    QWidget *central = new QWidget(this);
    QHBoxLayout *mainLayout = new QHBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(sidebar);
    mainLayout->addWidget(stack);
    setCentralWidget(central);

    setStyleSheet(R"(
        QMainWindow, QWidget#stack {
            background-color: #f7f4ee;
        }
        QWidget {
            color: #111827;
            font-family: "Kantumruy Pro", "Segoe UI";
        }
        #sidebar {
            background-color: #16213a;
        }
        #logoBadge {
            background-color: #ffffff;
            border-radius: 10px;
        }
        #appTitle {
            font-family: "Plus Jakarta Sans ExtraBold";
            color: #ffffff;
            font-size: 18px;
        }
        #appSubtitle {
            color: #8896b3;
            font-size: 11px;
        }
        #sidebarFooter {
            color: #566083;
            font-size: 10px;
        }
        #navButton {
            background-color: transparent;
            color: #b7c0d8;
            border: none;
            border-radius: 8px;
            text-align: left;
            padding-left: 12px;
            font-size: 13px;
        }
        #navButton:hover {
            background-color: #223052;
            color: #ffffff;
        }
        #navButton[active="true"] {
            background-color: #4f5eff;
            color: #ffffff;
            font-weight: 600;
        }
        QLabel[role="pageTitle"] {
            font-family: "Plus Jakarta Sans ExtraBold";
            font-size: 24px;
            color: #0f172a;
        }
        QLabel[role="pageSubtitle"] {
            font-size: 12px;
            color: #64748b;
            padding-bottom: 4px;
        }
        #statCard {
            background-color: #ffffff;
            border: 1px solid #e8e2d5;
            border-radius: 12px;
        }
        #statCard[hovered="true"] {
            border: 1px solid #4f5eff;
            background-color: #f7f7ff;
        }
        #dashboardHero {
            background-color: #16213a;
            border-radius: 18px;
        }
        #heroTitle {
            color: #ffffff;
            font-family: "Plus Jakarta Sans ExtraBold";
            font-size: 20px;
        }
        #heroSubtitle {
            color: #aeb9d3;
            font-size: 12px;
        }
        #heroBadge {
            background-color: #203b32;
            color: #6ee7b7;
            border-radius: 12px;
            padding: 6px 10px;
            font-size: 10px;
            font-weight: 700;
        }
        #heroMini {
            background-color: rgba(255,255,255,0.08);
            border: 1px solid rgba(255,255,255,0.10);
            border-radius: 12px;
            min-width: 92px;
        }
        #heroMiniLabel {
            color: #8290ad;
            font-size: 8px;
            font-weight: 700;
        }
        #heroMiniValue {
            color: #6ee7b7;
            font-size: 11px;
            font-weight: 700;
        }
        #liveLabel {
            background: #ecfdf5;
            color: #15803d;
            border-radius: 10px;
            padding: 5px 9px;
            font-size: 10px;
            font-weight: 700;
        }
        QLabel[role="cardIcon"] {
            font-size: 20px;
        }
        QLabel[role="cardValue"] {
            font-family: "Plus Jakarta Sans";
            font-size: 26px;
            font-weight: 700;
            color: #0f172a;
        }
        QLabel[role="cardCaption"] {
            font-size: 11px;
            color: #64748b;
        }
        QListWidget {
            background-color: transparent;
            border: none;
        }
        QListWidget::item {
            border: none;
            margin-bottom: 6px;
        }
        QListWidget::item:selected {
            background-color: transparent;
        }
        #rowCard {
            background-color: #ffffff;
            border: 1px solid #e8e2d5;
            border-radius: 10px;
        }
        #rowCard:hover {
            background-color: #f4efe4;
            border: 1px solid #d8cfb8;
        }
        #rowCard[selected="true"] {
            background-color: #eef2ff;
            border: 2px solid #4f5eff;
        }
        #miniButton {
            background-color: #eef2ff;
            color: #4f5eff;
            border: none;
            border-radius: 6px;
            padding: 5px 10px;
            font-size: 11px;
            font-weight: 600;
        }
        #miniButton:hover {
            background-color: #dfe4ff;
        }
        QLabel[role="rowTitle"] {
            font-size: 13px;
            font-weight: 600;
            color: #0f172a;
        }
        QLabel[role="rowSubtitle"] {
            font-size: 11px;
            color: #64748b;
        }
        QLabel[status="available"] {
            background-color: #dcfce7;
            color: #166534;
            border-radius: 9px;
            padding: 3px 10px;
            font-size: 11px;
            font-weight: 600;
        }
        QLabel[status="occupied"] {
            background-color: #dbeafe;
            color: #1e40af;
            border-radius: 9px;
            padding: 3px 10px;
            font-size: 11px;
            font-weight: 600;
        }
        QLabel[status="maintenance"] {
            background-color: #fef3c7;
            color: #92400e;
            border-radius: 9px;
            padding: 3px 10px;
            font-size: 11px;
            font-weight: 600;
        }
        QLabel[status="paid"] {
            background-color: #dcfce7;
            color: #166534;
            border-radius: 9px;
            padding: 3px 10px;
            font-size: 11px;
            font-weight: 600;
        }
        QLabel[status="partial"] {
            background-color: #fef3c7;
            color: #92400e;
            border-radius: 9px;
            padding: 3px 10px;
            font-size: 11px;
            font-weight: 600;
        }
        QLabel[status="unpaid"] {
            background-color: #fee2e2;
            color: #991b1b;
            border-radius: 9px;
            padding: 3px 10px;
            font-size: 11px;
            font-weight: 600;
        }
        QPushButton {
            background-color: #4f5eff;
            color: white;
            border: none;
            border-radius: 8px;
            padding: 9px 16px;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton:hover {
            background-color: #4048d8;
        }
        QPushButton:pressed {
            background-color: #333ab0;
        }
    )");

    showDashboard();
}

// ============ Page builders ============

QWidget *MainWindow::buildDashboardPage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *outerLayout = new QVBoxLayout(page);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    QScrollArea *scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    QWidget *content = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(content);
    layout->setContentsMargins(28, 26, 28, 26);
    layout->setSpacing(4);

    QFrame *hero = new QFrame;
    hero->setObjectName("dashboardHero");
    QHBoxLayout *heroLayout = new QHBoxLayout(hero);
    heroLayout->setContentsMargins(26, 22, 26, 22);
    heroLayout->setSpacing(18);

    QVBoxLayout *heroText = new QVBoxLayout;
    heroText->setSpacing(5);
    QStringList greetings = {
        "Good morning",
        "Good afternoon",
        "Good evening",
        "Welcome back",
        "Nice to see you",
        "Ready to manage?"
    };
    QString greeting = greetings.at(QRandomGenerator::global()->bounded(greetings.size()));
    QLabel *welcome = new QLabel(QString("%1, %2 👋").arg(greeting, ownerUsername));
    welcome->setObjectName("heroTitle");
    QLabel *heroSubtitle = new QLabel("Your rental business at a glance.");

    QTimer::singleShot(1000, welcome, [welcome]() {
        welcome->setText("Dashboard Overview");
    });
    heroSubtitle->setObjectName("heroSubtitle");
    heroText->addWidget(welcome);
    heroText->addWidget(heroSubtitle);
    heroLayout->addLayout(heroText);
    heroLayout->addStretch();

    QFrame *heroMini = new QFrame;
    heroMini->setObjectName("heroMini");
    QVBoxLayout *miniLayout = new QVBoxLayout(heroMini);
    miniLayout->setContentsMargins(14, 10, 14, 10);
    miniLayout->setSpacing(1);
    QLabel *miniTop = new QLabel("PROPERTY STATUS");
    miniTop->setObjectName("heroMiniLabel");
    QLabel *miniValue = new QLabel("●  LIVE");
    miniValue->setObjectName("heroMiniValue");
    miniLayout->addWidget(miniTop);
    miniLayout->addWidget(miniValue);
    heroLayout->addWidget(heroMini, 0, Qt::AlignVCenter);

    auto *heroShadow = new QGraphicsDropShadowEffect;
    heroShadow->setBlurRadius(26);
    heroShadow->setOffset(0, 7);
    heroShadow->setColor(QColor(15, 23, 42, 38));
    hero->setGraphicsEffect(heroShadow);
    layout->addWidget(hero);
    layout->addSpacing(18);

    QHBoxLayout *sectionHeader = new QHBoxLayout;
    QLabel *title = new QLabel("Overview");
    title->setProperty("role", "pageTitle");
    sectionHeader->addWidget(title);
    sectionHeader->addStretch();
    QLabel *updated = new QLabel("Live data");
    updated->setObjectName("liveLabel");
    sectionHeader->addWidget(updated);
    layout->addLayout(sectionHeader);
    layout->addSpacing(12);

    auto makeCard = [](const QString &icon, const QString &caption, QLabel **valueOut) -> ClickableCard* {
        ClickableCard *card = new ClickableCard;
        card->setObjectName("statCard");

        QVBoxLayout *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(18, 16, 18, 16);
        cardLayout->setSpacing(6);

        QLabel *iconLabel = new QLabel;
        iconLabel->setProperty("role", "cardIcon");
        if (icon.startsWith(":/")) {
            iconLabel->setPixmap(QPixmap(icon).scaled(28, 28, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            iconLabel->setText(icon);
        }

        QLabel *valueLabel = new QLabel("0");
        valueLabel->setProperty("role", "cardValue");

        QLabel *captionLabel = new QLabel(caption);
        captionLabel->setProperty("role", "cardCaption");

        cardLayout->addWidget(iconLabel);
        cardLayout->addWidget(valueLabel);
        cardLayout->addWidget(captionLabel);

        auto *shadow = new QGraphicsDropShadowEffect;
        shadow->setBlurRadius(18);
        shadow->setOffset(0, 4);
        shadow->setColor(QColor(15, 23, 42, 30));
        card->setGraphicsEffect(shadow);

        *valueOut = valueLabel;
        return card;
    };

    QGridLayout *grid = new QGridLayout;
    grid->setSpacing(14);
    grid->setContentsMargins(0, 0, 0, 0);

    ClickableCard *cardTotal = makeCard(":/icons/icon_total_rooms.png", "Total Rooms", &statTotalRooms);
    ClickableCard *cardOccupied = makeCard(":/icons/status_occupied_32.png", "Occupied", &statOccupied);
    ClickableCard *cardAvailable = makeCard(":/icons/status_available_32.png", "Available", &statAvailable);
    ClickableCard *cardRent = makeCard(":/icons/icon_expected_rent.png", "Expected Rent / mo", &statMonthlyRent);

    // Each card jumps to the page that explains that number. Total/Occupied/
    // Available go to Rooms pre-filtered to the matching status; the rent card
    // goes to the new Income page (which didn't exist before this feature).
    connect(cardTotal, &ClickableCard::clicked, this, [this]() {
        showRooms();
        if (roomStatusFilterCombo) roomStatusFilterCombo->setCurrentIndex(0); // All Statuses
    });
    connect(cardOccupied, &ClickableCard::clicked, this, [this]() {
        showRooms();
        if (roomStatusFilterCombo) roomStatusFilterCombo->setCurrentIndex(2); // Occupied
    });
    connect(cardAvailable, &ClickableCard::clicked, this, [this]() {
        showRooms();
        if (roomStatusFilterCombo) roomStatusFilterCombo->setCurrentIndex(1); // Available
    });
    connect(cardRent, &ClickableCard::clicked, this, &MainWindow::showIncome);

    grid->addWidget(cardTotal, 0, 0);
    grid->addWidget(cardOccupied, 0, 1);
    grid->addWidget(cardAvailable, 0, 2);
    grid->addWidget(cardRent, 0, 3);
    for (int c = 0; c < 4; ++c) grid->setColumnStretch(c, 1);

    layout->addLayout(grid);
    layout->addSpacing(22);

    QLabel *roomGridTitle = new QLabel("Room Overview");
    roomGridTitle->setStyleSheet("font-size: 15px; font-weight: 700; color: #0f172a;");
    layout->addWidget(roomGridTitle);
    layout->addSpacing(6);

    auto makeLegendItem = [](const QString &color, const QString &label) -> QWidget* {
        QWidget *item = new QWidget;
        QHBoxLayout *itemLayout = new QHBoxLayout(item);
        itemLayout->setContentsMargins(0, 0, 0, 0);
        itemLayout->setSpacing(6);
        QLabel *dot = new QLabel;
        dot->setFixedSize(10, 10);
        dot->setStyleSheet(QString("background-color: %1; border-radius: 5px;").arg(color));
        QLabel *text = new QLabel(label);
        text->setStyleSheet("font-size: 11px; color: #64748b;");
        itemLayout->addWidget(dot);
        itemLayout->addWidget(text);
        return item;
    };

    QHBoxLayout *legendRow = new QHBoxLayout;
    legendRow->setSpacing(18);
    legendRow->addWidget(makeLegendItem("#22c55e", "Available"));
    legendRow->addWidget(makeLegendItem("#4f5eff", "Occupied"));
    legendRow->addWidget(makeLegendItem("#f59e0b", "Maintenance"));
    legendRow->addStretch();
    layout->addLayout(legendRow);
    layout->addSpacing(10);

    QWidget *roomGridContainer = new QWidget;
    roomGridLayout = new QGridLayout(roomGridContainer);
    roomGridLayout->setSpacing(8);
    roomGridLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    layout->addWidget(roomGridContainer);
    layout->addSpacing(22);

    QLabel *alertsTitle = new QLabel("Payment Alerts");
    alertsTitle->setStyleSheet("font-size: 15px; font-weight: 700; color: #0f172a;");
    layout->addWidget(alertsTitle);
    layout->addSpacing(6);

    alertsListWidget = new QListWidget;
    alertsListWidget->setSpacing(0);
    alertsListWidget->setMaximumHeight(230);
    layout->addWidget(alertsListWidget);

    layout->addStretch();

    scrollArea->setWidget(content);
    outerLayout->addWidget(scrollArea);

    return page;
}

QWidget *MainWindow::buildRoomsPage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 26, 28, 26);
    layout->setSpacing(4);

    QLabel *title = new QLabel("Rooms");
    title->setProperty("role", "pageTitle");
    layout->addWidget(title);

    QLabel *subtitle = new QLabel("Manage every room in the building");
    subtitle->setProperty("role", "pageSubtitle");
    layout->addWidget(subtitle);
    layout->addSpacing(10);

    roomSearchEdit = new QLineEdit;
    roomSearchEdit->setPlaceholderText("\U0001F50D Search by room number or tenant name...");
    roomSearchEdit->setMinimumHeight(38);
    roomSearchEdit->setStyleSheet(
        "QLineEdit { background-color: #ffffff; border: 1px solid #cbd5e1; border-radius: 8px; "
        "padding: 6px 12px; font-size: 13px; }"
        "QLineEdit:focus { border: 1px solid #4f5eff; }"
    );
    connect(roomSearchEdit, &QLineEdit::textChanged, this, [this](const QString &) { refreshRoomList(); });
    layout->addWidget(roomSearchEdit);
    layout->addSpacing(8);

    roomStatusFilterCombo = new QComboBox;
    roomStatusFilterCombo->addItem("All Statuses", "");
    roomStatusFilterCombo->addItem("Available", "available");
    roomStatusFilterCombo->addItem("Occupied", "occupied");
    roomStatusFilterCombo->addItem("Maintenance", "maintenance");
    roomStatusFilterCombo->setMinimumHeight(34);
    roomStatusFilterCombo->setStyleSheet(
        "QComboBox { background-color: #ffffff; border: 1px solid #cbd5e1; border-radius: 8px; "
        "padding: 4px 10px; font-size: 13px; }"
    );
    connect(roomStatusFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this](int) { refreshRoomList(); });
    layout->addWidget(roomStatusFilterCombo);
    layout->addSpacing(10);

    roomListWidget = new QListWidget;
    roomListWidget->setSpacing(0);
    layout->addWidget(roomListWidget);

    // Give clear visual feedback on click: highlight the previously selected
    // row back to normal, and highlight the newly clicked row.
    connect(roomListWidget, &QListWidget::currentItemChanged, this,
        [this](QListWidgetItem *current, QListWidgetItem *previous) {
            if (previous) {
                QWidget *w = roomListWidget->itemWidget(previous);
                if (w) { w->setProperty("selected", false); w->style()->unpolish(w); w->style()->polish(w); }
            }
            if (current) {
                QWidget *w = roomListWidget->itemWidget(current);
                if (w) { w->setProperty("selected", true); w->style()->unpolish(w); w->style()->polish(w); }
            }
        });

    // Single click just selects the row (so "Edit Selected" below still works
    // as before). Double-click or pressing Enter/Return opens the full tenant
    // detail dashboard instead - Qt's itemActivated signal covers both.
    connect(roomListWidget, &QListWidget::itemActivated, this,
        [this](QListWidgetItem *item) {
            int id = item->data(Qt::UserRole).toInt();
            for (const Room &r : db->getAllRooms(ownerId)) {
                if (r.id == id) {
                    TenantDetailDialog dlg(db, r, ownerId, this);
                    dlg.exec();
                    refreshRoomList();
                    break;
                }
            }
        });

    QHBoxLayout *buttonRow = new QHBoxLayout;
    QPushButton *addBtn = new QPushButton("+  Add Room");
    QPushButton *editBtn = new QPushButton("Edit Selected");
    QPushButton *deleteBtn = new QPushButton("Delete Selected");
    buttonRow->addWidget(addBtn);
    buttonRow->addWidget(editBtn);
    buttonRow->addWidget(deleteBtn);
    buttonRow->addStretch();
    layout->addLayout(buttonRow);

    connect(addBtn, &QPushButton::clicked, this, &MainWindow::onAddRoom);
    connect(editBtn, &QPushButton::clicked, this, &MainWindow::onEditRoom);
    connect(deleteBtn, &QPushButton::clicked, this, &MainWindow::onDeleteRoom);

    return page;
}

QWidget *MainWindow::buildPaymentsPage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 26, 28, 26);
    layout->setSpacing(4);

    QLabel *title = new QLabel("Payments");
    title->setProperty("role", "pageTitle");
    layout->addWidget(title);

    QLabel *subtitle = new QLabel("Rent status by month");
    subtitle->setProperty("role", "pageSubtitle");
    layout->addWidget(subtitle);
    layout->addSpacing(10);

    paymentMonthFilterCombo = new QComboBox;
    {
        QDate cursor = QDate::currentDate();
        for (int i = 0; i < 12; ++i) {
            QDate d = cursor.addMonths(-i);
            paymentMonthFilterCombo->addItem(d.toString("MMMM yyyy"), d.toString("yyyy-MM"));
        }
    }
    paymentMonthFilterCombo->setMinimumHeight(34);
    paymentMonthFilterCombo->setStyleSheet(
        "QComboBox { background-color: #ffffff; border: 1px solid #cbd5e1; border-radius: 8px; "
        "padding: 4px 10px; font-size: 13px; }"
    );
    connect(paymentMonthFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this](int) { refreshPaymentList(); });

    paymentStatusFilterCombo = new QComboBox;
    paymentStatusFilterCombo->addItem("All Statuses", "");
    paymentStatusFilterCombo->addItem("Paid", "paid");
    paymentStatusFilterCombo->addItem("Partial", "partial");
    paymentStatusFilterCombo->addItem("Unpaid", "unpaid");
    paymentStatusFilterCombo->setMinimumHeight(34);
    paymentStatusFilterCombo->setStyleSheet(
        "QComboBox { background-color: #ffffff; border: 1px solid #cbd5e1; border-radius: 8px; "
        "padding: 4px 10px; font-size: 13px; }"
    );
    connect(paymentStatusFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this](int) { refreshPaymentList(); });

    QHBoxLayout *filterRow = new QHBoxLayout;
    filterRow->setSpacing(8);
    filterRow->addWidget(paymentMonthFilterCombo, 1);
    filterRow->addWidget(paymentStatusFilterCombo, 1);
    layout->addLayout(filterRow);
    layout->addSpacing(10);

    paymentListWidget = new QListWidget;
    paymentListWidget->setSpacing(0);
    layout->addWidget(paymentListWidget);

    connect(paymentListWidget, &QListWidget::currentItemChanged, this,
        [this](QListWidgetItem *current, QListWidgetItem *previous) {
            if (previous) {
                QWidget *w = paymentListWidget->itemWidget(previous);
                if (w) { w->setProperty("selected", false); w->style()->unpolish(w); w->style()->polish(w); }
            }
            if (current) {
                QWidget *w = paymentListWidget->itemWidget(current);
                if (w) { w->setProperty("selected", true); w->style()->unpolish(w); w->style()->polish(w); }
            }
        });

    QHBoxLayout *actionRow = new QHBoxLayout;

    QPushButton *toggleBtn = new QPushButton("Record Payment for Selected");
    connect(toggleBtn, &QPushButton::clicked, this, &MainWindow::onTogglePaid);
    actionRow->addWidget(toggleBtn);

    layout->addLayout(actionRow);

    return page;
}

QWidget *MainWindow::buildIncomePage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 26, 28, 26);
    layout->setSpacing(4);

    QLabel *title = new QLabel("Income Dashboard");
    title->setProperty("role", "pageTitle");
    layout->addWidget(title);

    QLabel *subtitle = new QLabel("Track expected income, collected money, and outstanding tenant payments");
    subtitle->setProperty("role", "pageSubtitle");
    layout->addWidget(subtitle);
    layout->addSpacing(12);

    auto makeSummaryCard = [](const QString &caption, const QString &accent, QLabel **valueOut) -> QFrame* {
        QFrame *card = new QFrame;
        card->setObjectName("statCard");
        QVBoxLayout *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(14, 12, 14, 12);
        cardLayout->setSpacing(3);

        QLabel *dot = new QLabel;
        dot->setFixedSize(9, 9);
        dot->setStyleSheet(QString("background-color: %1; border-radius: 4px;").arg(accent));

        QLabel *valueLabel = new QLabel("$0.00");
        valueLabel->setStyleSheet("font-family: 'Plus Jakarta Sans'; font-size: 19px; font-weight: 700; color: #0f172a;");

        QLabel *captionLabel = new QLabel(caption);
        captionLabel->setStyleSheet("font-size: 10px; color: #64748b;");

        cardLayout->addWidget(dot);
        cardLayout->addWidget(valueLabel);
        cardLayout->addWidget(captionLabel);

        *valueOut = valueLabel;
        return card;
    };

    QHBoxLayout *moneyRow = new QHBoxLayout;
    moneyRow->setSpacing(10);
    moneyRow->addWidget(makeSummaryCard("Expected this month", "#4f5eff", &incomeExpectedLabel));
    moneyRow->addWidget(makeSummaryCard("Collected this month", "#22c55e", &incomeCollectedLabel));
    moneyRow->addWidget(makeSummaryCard("Outstanding", "#f59e0b", &incomeOutstandingLabel));
    layout->addLayout(moneyRow);
    layout->addSpacing(10);

    QHBoxLayout *statusRow = new QHBoxLayout;
    statusRow->setSpacing(10);
    statusRow->addWidget(makeSummaryCard("Partially paid", "#eab308", &incomePartialLabel));
    statusRow->addWidget(makeSummaryCard("Unpaid", "#ef4444", &incomeUnpaidLabel));
    statusRow->addWidget(makeSummaryCard("Overdue", "#f97316", &incomeOverdueLabel));
    layout->addLayout(statusRow);
    layout->addSpacing(14);

    QLabel *listTitle = new QLabel("Outstanding Payments");
    listTitle->setStyleSheet("font-size: 15px; font-weight: 700; color: #0f172a;");
    layout->addWidget(listTitle);

    incomeOutstandingList = new QListWidget;
    incomeOutstandingList->setSpacing(5);
    incomeOutstandingList->setMinimumHeight(150);
    incomeOutstandingList->setMaximumHeight(240);
    layout->addWidget(incomeOutstandingList);

    layout->addSpacing(8);
    QLabel *chartTitle = new QLabel("Income Trend");
    chartTitle->setStyleSheet("font-size: 15px; font-weight: 700; color: #0f172a;");
    layout->addWidget(chartTitle);

    incomeChartWidget = new IncomeChartWidget;
    layout->addWidget(incomeChartWidget);
    layout->addStretch();

    return page;
}

QWidget *MainWindow::buildHistoryPage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 26, 28, 26);
    layout->setSpacing(4);

    QLabel *title = new QLabel("Payment History");
    title->setProperty("role", "pageTitle");
    layout->addWidget(title);

    QLabel *subtitle = new QLabel("Every rent payment ever recorded");
    subtitle->setProperty("role", "pageSubtitle");
    layout->addWidget(subtitle);
    layout->addSpacing(10);

    tenantSearchEdit = new QLineEdit;
    tenantSearchEdit->setPlaceholderText("\U0001F50D Look up a tenant by name (shows paid AND unpaid, even old rooms)...");
    tenantSearchEdit->setMinimumHeight(38);
    tenantSearchEdit->setStyleSheet(
        "QLineEdit { background-color: #ffffff; border: 1px solid #cbd5e1; border-radius: 8px; "
        "padding: 6px 12px; font-size: 13px; }"
        "QLineEdit:focus { border: 1px solid #4f5eff; }"
    );
    connect(tenantSearchEdit, &QLineEdit::textChanged, this, [this](const QString &) { refreshHistoryList(); });
    layout->addWidget(tenantSearchEdit);
    layout->addSpacing(8);

    historyMonthFilterCombo = new QComboBox;
    historyMonthFilterCombo->addItem("All Months", "");
    {
        QDate cursor = QDate::currentDate();
        for (int i = 0; i < 12; ++i) {
            QDate d = cursor.addMonths(-i);
            historyMonthFilterCombo->addItem(d.toString("MMMM yyyy"), d.toString("yyyy-MM"));
        }
    }
    historyMonthFilterCombo->setMinimumHeight(34);
    historyMonthFilterCombo->setStyleSheet(
        "QComboBox { background-color: #ffffff; border: 1px solid #cbd5e1; border-radius: 8px; "
        "padding: 4px 10px; font-size: 13px; }"
    );
    connect(historyMonthFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this](int) { refreshHistoryList(); });
    layout->addWidget(historyMonthFilterCombo);
    layout->addSpacing(10);

    historyListWidget = new QListWidget;
    historyListWidget->setSpacing(0);
    layout->addWidget(historyListWidget);

    return page;
}

QWidget *MainWindow::buildSettingsPage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *outerLayout = new QVBoxLayout(page);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    QScrollArea *scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QWidget *content = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(content);
    layout->setContentsMargins(28, 26, 28, 26);
    layout->setSpacing(4);

    QLabel *title = new QLabel("Settings");
    title->setProperty("role", "pageTitle");
    layout->addWidget(title);

    QLabel *subtitle = new QLabel("Connect your Telegram bot to send bills to tenants");
    subtitle->setProperty("role", "pageSubtitle");
    layout->addWidget(subtitle);
    layout->addSpacing(14);

    QFrame *card = new QFrame;
    card->setObjectName("rowCard");
    QVBoxLayout *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(20, 18, 20, 18);
    cardLayout->setSpacing(10);

    QLabel *helpLabel = new QLabel(
        "1. Open Telegram, message @BotFather, send /newbot, and follow the steps.\n"
        "2. Paste the Bot Token BotFather gives you below.\n"
        "3. Each tenant opens your bot's link and sends it any message once.\n"
        "4. In Edit Room, click \"Detect from Bot\" to find their Chat ID."
    );
    helpLabel->setWordWrap(true);
    helpLabel->setStyleSheet("color: #64748b; font-size: 12px;");
    cardLayout->addWidget(helpLabel);

    QFormLayout *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(10);

    botTokenEdit = new QLineEdit;
    botTokenEdit->setPlaceholderText("123456789:AA...  (from @BotFather)");
    botTokenEdit->setMinimumHeight(42);
    form->addRow("Bot Token:", botTokenEdit);

    cardLayout->addLayout(form);

    QHBoxLayout *testRow = new QHBoxLayout;
    QPushButton *testBtn = new QPushButton("Test Connection");
    connect(testBtn, &QPushButton::clicked, this, &MainWindow::onTestTelegramConnection);
    testConnectionStatusLabel = new QLabel;
    testConnectionStatusLabel->setStyleSheet("font-size: 12px;");
    testRow->addWidget(testBtn);
    testRow->addWidget(testConnectionStatusLabel, 1);
    cardLayout->addLayout(testRow);
    cardLayout->addSpacing(4);

    layout->addWidget(card);
    layout->addSpacing(18);

    // ---- ABA KHQR card ----
    QFrame *qrCard = new QFrame;
    qrCard->setObjectName("rowCard");
    QVBoxLayout *qrCardLayout = new QVBoxLayout(qrCard);
    qrCardLayout->setContentsMargins(20, 18, 20, 18);
    qrCardLayout->setSpacing(10);

    QLabel *qrTitle = new QLabel("ABA KHQR");
    qrTitle->setStyleSheet("font-size: 15px; font-weight: 700; color: #0f172a;");
    qrCardLayout->addWidget(qrTitle);

    QLabel *qrHelp = new QLabel(
        "Upload a screenshot of your ABA KHQR payment code. You'll be asked each "
        "time whether to attach it when a bill is sent to a tenant."
    );
    qrHelp->setWordWrap(true);
    qrHelp->setStyleSheet("color: #64748b; font-size: 12px;");
    qrCardLayout->addWidget(qrHelp);

    QHBoxLayout *qrRow = new QHBoxLayout;
    qrRow->setSpacing(16);

    qrPreviewLabel = new QLabel;
    qrPreviewLabel->setFixedSize(140, 140);
    qrPreviewLabel->setAlignment(Qt::AlignCenter);
    qrPreviewLabel->setStyleSheet(
        "background-color: #f8fafc; border: 1px dashed #cbd5e1; border-radius: 10px; "
        "color: #94a3b8; font-size: 12px;");
    qrPreviewLabel->setScaledContents(false);
    qrRow->addWidget(qrPreviewLabel);

    QVBoxLayout *qrBtnCol = new QVBoxLayout;
    qrBtnCol->setSpacing(8);
    QPushButton *uploadQrBtn = new QPushButton("Upload QR Image...");
    uploadQrBtn->setMinimumHeight(38);
    connect(uploadQrBtn, &QPushButton::clicked, this, &MainWindow::onUploadAbaQr);
    qrBtnCol->addWidget(uploadQrBtn);

    removeQrBtn = new QPushButton("Remove QR");
    removeQrBtn->setMinimumHeight(38);
    removeQrBtn->setStyleSheet("QPushButton { background: #fff1f2; color: #be123c; border: 1px solid #fecdd3; "
                                "border-radius: 10px; } QPushButton:hover { background: #ffe4e6; } "
                                "QPushButton:disabled { color: #cbd5e1; background: #f8fafc; border-color: #e2e8f0; }");
    connect(removeQrBtn, &QPushButton::clicked, this, &MainWindow::onRemoveAbaQr);
    qrBtnCol->addWidget(removeQrBtn);
    qrBtnCol->addStretch();

    qrRow->addLayout(qrBtnCol);
    qrRow->addStretch();
    qrCardLayout->addLayout(qrRow);

    layout->addWidget(qrCard);
    layout->addSpacing(18);

    QPushButton *saveBtn = new QPushButton("Save Settings");
    saveBtn->setMinimumHeight(42);
    connect(saveBtn, &QPushButton::clicked, this, &MainWindow::onSaveTelegramSettings);
    layout->addWidget(saveBtn);

    QPushButton *logoutBtn = new QPushButton("Log out");
    logoutBtn->setMinimumHeight(42);
    logoutBtn->setCursor(Qt::PointingHandCursor);
    logoutBtn->setStyleSheet("QPushButton { background: #fff1f2; color: #be123c; border: 1px solid #fecdd3; border-radius: 10px; font-weight: 600; padding: 8px 14px; } QPushButton:hover { background: #ffe4e6; }");
    connect(logoutBtn, &QPushButton::clicked, this, [this]() {
        if (QMessageBox::question(this, "Log out", "Are you sure you want to log out?",
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) {
            emit logoutRequested();
            close();
        }
    });
    layout->addWidget(logoutBtn);

    layout->addStretch();

    scrollArea->setWidget(content);
    outerLayout->addWidget(scrollArea);

    return page;
}

// ============ Row builders (styled cards inside the lists) ============

QWidget *MainWindow::buildRoomRow(const Room &r)
{
    QFrame *card = new QFrame;
    card->setObjectName("rowCard");

    QHBoxLayout *rowLayout = new QHBoxLayout(card);
    rowLayout->setContentsMargins(14, 10, 14, 10);

    QVBoxLayout *textCol = new QVBoxLayout;
    textCol->setSpacing(2);

    QLabel *titleLbl = new QLabel(QString("Room %1  \u00B7  %2").arg(r.number, r.floor));
    titleLbl->setProperty("role", "rowTitle");

    QString subtitle = r.tenantName.isEmpty() ? "No tenant" : r.tenantName;
    if (r.stayType == "shortterm") {
        QDate checkIn = QDate::fromString(r.checkInDate, "yyyy-MM-dd");
        QDate checkOut = QDate::fromString(r.checkOutDate, "yyyy-MM-dd");
        int nights = checkIn.isValid()
            ? qMax(1, static_cast<int>(checkIn.daysTo(checkOut.isValid() ? checkOut : QDate::currentDate())))
            : 0;
        subtitle += QString("  \u00B7  $%1/night  \u00B7  %2 night(s)%3")
            .arg(r.dailyRate, 0, 'f', 2)
            .arg(nights)
            .arg(checkOut.isValid() ? "" : " (ongoing)");
    } else {
        subtitle += QString("  \u00B7  $%1/mo").arg(r.rentPerMonth, 0, 'f', 2);
    }
    QLabel *subLbl = new QLabel(subtitle);
    subLbl->setProperty("role", "rowSubtitle");

    textCol->addWidget(titleLbl);
    textCol->addWidget(subLbl);

    QLabel *badge = new QLabel(r.status.toUpper());
    badge->setProperty("status", r.status);

    QLabel *statusIcon = nullptr;
    if (r.status == "occupied") {
        statusIcon = new QLabel;
        statusIcon->setFixedSize(20, 20);
        statusIcon->setPixmap(QPixmap(":/icons/status_occupied_20.png")
                                   .scaled(20, 20, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else if (r.status == "available") {
        statusIcon = new QLabel;
        statusIcon->setFixedSize(20, 20);
        statusIcon->setPixmap(QPixmap(":/icons/status_available_20.png")
                                   .scaled(20, 20, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    QPushButton *utilBtn = new QPushButton("\U0001F4B0 Bill");
    utilBtn->setObjectName("miniButton");
    utilBtn->setCursor(Qt::PointingHandCursor);
    Room roomCopy = r; // captured by value for the lambda below
    connect(utilBtn, &QPushButton::clicked, this, [this, roomCopy]() {
        UtilityDialog dlg(db, roomCopy, ownerId, this);
        dlg.exec();
    });

    rowLayout->addLayout(textCol);
    rowLayout->addStretch();
    rowLayout->addWidget(utilBtn);
    if (statusIcon) rowLayout->addWidget(statusIcon);
    rowLayout->addWidget(badge);

    return card;
}

QWidget *MainWindow::buildPaymentRow(const Payment &p)
{
    QFrame *card = new QFrame;
    card->setObjectName("rowCard");

    QHBoxLayout *rowLayout = new QHBoxLayout(card);
    rowLayout->setContentsMargins(14, 10, 14, 10);

    QVBoxLayout *textCol = new QVBoxLayout;
    textCol->setSpacing(2);

    QLabel *titleLbl = new QLabel(QString("Room %1  ·  %2").arg(p.roomNumber, p.tenantName));
    titleLbl->setProperty("role", "rowTitle");

    double remaining = qMax(0.0, p.amount - p.paidAmount);
    QLabel *subLbl = new QLabel(QString("Bill $%1  ·  Paid $%2  ·  Remaining $%3")
        .arg(p.amount, 0, 'f', 2).arg(p.paidAmount, 0, 'f', 2).arg(remaining, 0, 'f', 2));
    subLbl->setProperty("role", "rowSubtitle");

    textCol->addWidget(titleLbl);
    textCol->addWidget(subLbl);

    QLabel *badge = new QLabel;
    if (p.paid) {
        badge->setText("PAID");
        badge->setProperty("status", "paid");
    } else if (p.paidAmount > 0) {
        badge->setText("PARTIAL");
        badge->setProperty("status", "partial");
    } else {
        badge->setText("UNPAID");
        badge->setProperty("status", "unpaid");
    }

    rowLayout->addLayout(textCol);
    rowLayout->addStretch();
    rowLayout->addWidget(badge);

    return card;
}

// ============ Navigation ============

void MainWindow::setActiveNav(int index)
{
    for (int i = 0; i < navButtons.size(); i++) {
        navButtons[i]->setProperty("active", i == index);
        navButtons[i]->style()->unpolish(navButtons[i]);
        navButtons[i]->style()->polish(navButtons[i]);
    }
}

void MainWindow::showDashboard() { refreshDashboard(); stack->setCurrentWidget(dashboardPage); setActiveNav(0); }
void MainWindow::showRooms()     { refreshRoomList();  stack->setCurrentWidget(roomsPage); setActiveNav(1); }
void MainWindow::showPayments()  { refreshPaymentList(); stack->setCurrentWidget(paymentsPage); setActiveNav(2); }
void MainWindow::showIncome()    { refreshIncomePage(); stack->setCurrentWidget(incomePage); setActiveNav(3); }
void MainWindow::showHistory()   { refreshHistoryList(); stack->setCurrentWidget(historyPage); setActiveNav(4); }
void MainWindow::showSettings()  {
    TelegramSettings settings = db->getTelegramSettings(ownerId);
    botTokenEdit->setText(settings.botToken);
    testConnectionStatusLabel->clear();
    refreshQrPreview();

    stack->setCurrentWidget(settingsPage);
    setActiveNav(5);
}

void MainWindow::refreshQrPreview()
{
    QByteArray imageData = db->getAbaQrImage(ownerId);
    if (imageData.isEmpty()) {
        qrPreviewLabel->setPixmap(QPixmap());
        qrPreviewLabel->setText("No QR\nset up yet");
        removeQrBtn->setEnabled(false);
        return;
    }

    QPixmap pixmap;
    pixmap.loadFromData(imageData);
    qrPreviewLabel->setText(QString());
    qrPreviewLabel->setPixmap(pixmap.scaled(
        qrPreviewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    removeQrBtn->setEnabled(true);
}

// ============ Refresh logic ============

void MainWindow::refreshDashboard()
{
    QVector<Room> allRooms = db->getAllRooms(ownerId);

    int total = allRooms.size();
    int occupied = db->countRoomsByStatus("occupied", ownerId);
    int available = db->countRoomsByStatus("available", ownerId);
    double rent = db->totalMonthlyRentCollected(ownerId);

    statTotalRooms->setText(QString::number(total));
    statOccupied->setText(QString::number(occupied));
    statAvailable->setText(QString::number(available));
    statMonthlyRent->setText(QString("$%1").arg(rent, 0, 'f', 2));

    // ---- Room Overview grid: one clickable tile per room, colored by status ----
    QLayoutItem *gridItem;
    while ((gridItem = roomGridLayout->takeAt(0)) != nullptr) {
        if (gridItem->widget()) gridItem->widget()->deleteLater();
        delete gridItem;
    }

    if (allRooms.isEmpty()) {
        QLabel *emptyLabel = new QLabel("No rooms yet - add your first one from the Rooms page.");
        emptyLabel->setStyleSheet("color: #64748b; font-size: 12px;");
        roomGridLayout->addWidget(emptyLabel, 0, 0);
    } else {
        const int columns = 10;
        int col = 0, row = 0;
        for (const Room &r : allRooms) {
            QString tileColor = r.status == "available" ? "#22c55e"
                               : r.status == "maintenance" ? "#f59e0b"
                               : "#4f5eff";

            QPushButton *tile = new QPushButton(r.number);
            tile->setFixedSize(46, 46);
            tile->setCursor(Qt::PointingHandCursor);
            tile->setStyleSheet(QString(
                "QPushButton { background-color: %1; color: white; border: none; "
                "border-radius: 8px; font-size: 11px; font-weight: 700; }"
                "QPushButton:hover { border: 2px solid #0f172a; }"
            ).arg(tileColor));

            QString tooltip = r.tenantName.isEmpty()
                ? QString("Room %1 \u00B7 %2").arg(r.number, r.status)
                : QString("Room %1 \u00B7 %2 \u00B7 %3").arg(r.number, r.tenantName, r.status);
            tile->setToolTip(tooltip);

            Room roomCopy = r;
            connect(tile, &QPushButton::clicked, this, [this, roomCopy]() {
                if (roomCopy.status == "occupied") {
                    UtilityDialog dlg(db, roomCopy, ownerId, this);
                    dlg.exec();
                } else {
                    RoomDialog dialog(this, roomCopy, db, ownerId);
                    if (dialog.exec() == QDialog::Accepted)
                        db->updateRoom(dialog.getRoom());
                }
                refreshDashboard();
            });

            roomGridLayout->addWidget(tile, row, col);
            if (++col >= columns) { col = 0; ++row; }
        }
    }

    // ---- Payment alerts: tenants who are due soon or already overdue ----
    alertsListWidget->clear();

    QDate today = QDate::currentDate();
    QString currentMonth = today.toString("yyyy-MM");
    QVector<Payment> payments = db->getPaymentsForMonth(currentMonth, ownerId);

    QMap<int, bool> paidByRoom;
    QMap<int, double> remainingByRoom;
    for (const Payment &p : payments) {
        paidByRoom[p.roomId] = p.paid;
        remainingByRoom[p.roomId] = qMax(0.0, p.amount - p.paidAmount);
    }

    struct AlertInfo { Room room; int days; bool overdue; double remaining; };
    QVector<AlertInfo> alerts;

    for (const Room &r : allRooms) {
        if (r.status != "occupied") continue;
        if (r.stayType != "monthly") continue; // short-term guests aren't billed on a due-day cycle
        if (paidByRoom.value(r.id, false)) continue; // already paid this month, no alert

        QDate reference = QDate::fromString(r.dueDate, "yyyy-MM-dd");

        QDate cycleDue;
        if (reference.isValid()) {
            // The move-in month itself isn't a due cycle - first rent is bundled with
            // moving in. The first tracked due date is one month later, then it repeats
            // every month after that (rolled forward to whichever cycle is "current now").
            cycleDue = reference.addMonths(1);
            while (cycleDue.addMonths(1) <= today)
                cycleDue = cycleDue.addMonths(1);
        } else {
            // Legacy fallback for rooms saved before the due-date picker existed -
            // only the day-of-month was stored, so reconstruct it against this month.
            int dueDayClamped = qMin(r.dueDay > 0 ? r.dueDay : 1, today.daysInMonth());
            cycleDue = QDate(today.year(), today.month(), dueDayClamped);
        }

        int diff = cycleDue.daysTo(today); // >0 = overdue by N days, <0 = N days until due

        if (diff > 0) {
            alerts.append({r, diff, true, remainingByRoom.value(r.id, 0)});
        } else if (diff >= -3) {
            alerts.append({r, -diff, false, remainingByRoom.value(r.id, 0)}); // due within the next 3 days
        }
    }

    if (alerts.isEmpty()) {
        QLabel *okLabel = new QLabel("\u2705  Everyone is paid up - no alerts right now.");
        okLabel->setStyleSheet("color: #64748b; font-size: 12px; padding: 10px 4px;");
        QListWidgetItem *okItem = new QListWidgetItem(alertsListWidget);
        okItem->setSizeHint(okLabel->sizeHint());
        alertsListWidget->addItem(okItem);
        alertsListWidget->setItemWidget(okItem, okLabel);
        return;
    }

    // Most overdue first, then soonest-due.
    std::sort(alerts.begin(), alerts.end(), [](const AlertInfo &a, const AlertInfo &b) {
        if (a.overdue != b.overdue) return a.overdue > b.overdue;
        return a.days > b.days;
    });

    for (const AlertInfo &a : alerts) {
        QFrame *card = new QFrame;
        card->setObjectName("rowCard");
        QHBoxLayout *rowLayout = new QHBoxLayout(card);
        rowLayout->setContentsMargins(14, 10, 14, 10);

        QVBoxLayout *textCol = new QVBoxLayout;
        textCol->setSpacing(2);
        QLabel *titleLbl = new QLabel(QString("Room %1  \u00B7  %2").arg(a.room.number, a.room.tenantName));
        titleLbl->setProperty("role", "rowTitle");
        QString timingText = a.overdue
            ? QString("%1 day(s) overdue").arg(a.days)
            : (a.days == 0 ? "Due today" : QString("Due in %1 day(s)").arg(a.days));
        QString balanceText = a.remaining > 0
            ? QString("  ·  $%1 remaining").arg(a.remaining, 0, 'f', 2)
            : QString();
        QLabel *subLbl = new QLabel(timingText + balanceText);
        subLbl->setProperty("role", "rowSubtitle");
        textCol->addWidget(titleLbl);
        textCol->addWidget(subLbl);

        QLabel *badge = new QLabel(a.overdue ? "OVERDUE" : "DUE SOON");
        badge->setProperty("status", a.overdue ? "unpaid" : "maintenance");

        QPushButton *remindBtn = new QPushButton("\U0001F4E9 Remind");
        remindBtn->setObjectName("miniButton");
        remindBtn->setCursor(Qt::PointingHandCursor);
        Room roomCopy = a.room;
        int daysCopy = a.days;
        bool overdueCopy = a.overdue;
        double remainingCopy = a.remaining;
        connect(remindBtn, &QPushButton::clicked, this, [this, roomCopy, daysCopy, overdueCopy, remainingCopy]() {
            sendTelegramReminder(roomCopy, daysCopy, overdueCopy, remainingCopy);
        });

        rowLayout->addLayout(textCol);
        rowLayout->addStretch();
        rowLayout->addWidget(remindBtn);
        rowLayout->addWidget(badge);

        QListWidgetItem *item = new QListWidgetItem(alertsListWidget);
        item->setSizeHint(card->sizeHint());
        alertsListWidget->addItem(item);
        alertsListWidget->setItemWidget(item, card);
    }
}

void MainWindow::refreshRoomList()
{
    roomListWidget->clear();

    QString filter = roomSearchEdit ? roomSearchEdit->text().trimmed().toLower() : QString();
    QString statusFilter = roomStatusFilterCombo ? roomStatusFilterCombo->currentData().toString() : QString();

    for (const Room &r : db->getAllRooms(ownerId)) {
        if (!filter.isEmpty()
            && !r.number.toLower().contains(filter)
            && !r.tenantName.toLower().contains(filter))
            continue;

        if (!statusFilter.isEmpty() && r.status != statusFilter)
            continue;

        QWidget *row = buildRoomRow(r);
        QListWidgetItem *item = new QListWidgetItem(roomListWidget);
        item->setData(Qt::UserRole, r.id);
        item->setSizeHint(row->sizeHint());
        roomListWidget->addItem(item);
        roomListWidget->setItemWidget(item, row);
    }
}

void MainWindow::refreshPaymentList()
{
    paymentListWidget->clear();

    QString actualCurrentMonth = QDate::currentDate().toString("yyyy-MM");
    QString selectedMonth = (paymentMonthFilterCombo && paymentMonthFilterCombo->currentIndex() >= 0)
        ? paymentMonthFilterCombo->currentData().toString()
        : actualCurrentMonth;

    // Only the real current month is allowed to auto-create missing payment rows
    // (for newly-occupied rooms). Past months are viewed read-only.
    QVector<Payment> payments = (selectedMonth == actualCurrentMonth)
        ? db->getPaymentsForMonth(selectedMonth, ownerId)
        : db->getPaymentsForMonthReadOnly(selectedMonth, ownerId);

    QString statusFilter = paymentStatusFilterCombo ? paymentStatusFilterCombo->currentData().toString() : QString();

    int visibleCount = 0;
    for (const Payment &p : payments) {
        QString status = p.paid ? "paid" : (p.paidAmount > 0 ? "partial" : "unpaid");
        if (!statusFilter.isEmpty() && status != statusFilter)
            continue;

        QWidget *row = buildPaymentRow(p);
        QListWidgetItem *item = new QListWidgetItem(paymentListWidget);
        item->setData(Qt::UserRole, p.id);
        item->setSizeHint(row->sizeHint());
        paymentListWidget->addItem(item);
        paymentListWidget->setItemWidget(item, row);
        ++visibleCount;
    }

    if (visibleCount == 0) {
        QString label = statusFilter == "paid" ? "No paid bills found for this month."
                      : statusFilter == "partial" ? "No partial bills found for this month."
                      : statusFilter == "unpaid" ? "No unpaid bills found for this month."
                      : "No bills found for this month.";
        QLabel *empty = new QLabel(label);
        empty->setStyleSheet("color: #64748b; font-size: 12px; padding: 12px;");
        QListWidgetItem *item = new QListWidgetItem(paymentListWidget);
        item->setSizeHint(empty->sizeHint());
        paymentListWidget->addItem(item);
        paymentListWidget->setItemWidget(item, empty);
    }
}

void MainWindow::refreshIncomePage()
{
    QVector<MonthlyIncomeSummary> summary = db->getMonthlyIncomeSummary(6, ownerId);
    incomeChartWidget->setData(summary);

    QString currentMonth = QDate::currentDate().toString("yyyy-MM");
    QVector<Payment> payments = db->getPaymentsForMonth(currentMonth, ownerId);
    QVector<Room> rooms = db->getAllRooms(ownerId);
    QMap<int, Room> roomById;
    for (const Room &r : rooms) roomById[r.id] = r;

    double expected = 0;
    double collected = 0;
    int partialCount = 0;
    int unpaidCount = 0;
    int overdueCount = 0;

    incomeOutstandingList->clear();

    QDate today = QDate::currentDate();
    for (const Payment &p : payments) {
        expected += p.amount;
        collected += p.paidAmount;

        if (p.paid) continue;
        if (p.paidAmount > 0) partialCount++;
        else unpaidCount++;

        if (!roomById.contains(p.roomId)) continue;
        const Room &room = roomById[p.roomId];
        QDate reference = QDate::fromString(room.dueDate, "yyyy-MM-dd");
        QDate cycleDue;
        if (reference.isValid()) {
            cycleDue = reference.addMonths(1);
            while (cycleDue.addMonths(1) <= today)
                cycleDue = cycleDue.addMonths(1);
        } else {
            int dueDay = qMin(room.dueDay > 0 ? room.dueDay : 1, today.daysInMonth());
            cycleDue = QDate(today.year(), today.month(), dueDay);
        }

        bool overdue = cycleDue < today;
        if (overdue) overdueCount++;

        double remaining = qMax(0.0, p.amount - p.paidAmount);
        QFrame *card = new QFrame;
        card->setObjectName("rowCard");
        QHBoxLayout *row = new QHBoxLayout(card);
        row->setContentsMargins(12, 9, 12, 9);

        QVBoxLayout *text = new QVBoxLayout;
        text->setSpacing(2);
        QLabel *name = new QLabel(QString("Room %1  ·  %2").arg(p.roomNumber, p.tenantName));
        name->setProperty("role", "rowTitle");

        QString detail;
        if (overdue) {
            int daysLate = cycleDue.daysTo(today);
            detail = QString("$%1 remaining  ·  %2 day(s) overdue")
                .arg(remaining, 0, 'f', 2).arg(daysLate);
        } else {
            detail = QString("$%1 remaining  ·  due %2")
                .arg(remaining, 0, 'f', 2).arg(cycleDue.toString("dd MMM"));
        }
        if (p.paidAmount > 0)
            detail += QString("  ·  $%1 already paid").arg(p.paidAmount, 0, 'f', 2);

        QLabel *sub = new QLabel(detail);
        sub->setProperty("role", "rowSubtitle");
        text->addWidget(name);
        text->addWidget(sub);
        row->addLayout(text);
        row->addStretch();

        QLabel *badge = new QLabel(overdue ? "OVERDUE" : (p.paidAmount > 0 ? "PARTIAL" : "UNPAID"));
        badge->setProperty("status", overdue ? "unpaid" : (p.paidAmount > 0 ? "partial" : "unpaid"));
        row->addWidget(badge);

        QPushButton *payBtn = new QPushButton("Record Payment");
        payBtn->setObjectName("miniButton");
        payBtn->setCursor(Qt::PointingHandCursor);
        connect(payBtn, &QPushButton::clicked, this, [this, p]() {
            double remainingNow = qMax(0.0, p.amount - p.paidAmount);
            bool ok = false;
            double amount = QInputDialog::getDouble(this, "Record Payment",
                QString("Amount received from %1\nRemaining: $%2")
                    .arg(p.tenantName).arg(remainingNow, 0, 'f', 2),
                remainingNow, 0.01, remainingNow, 2, &ok);
            if (!ok) return;
            if (db->recordPayment(p.id, amount)) {
                Payment updated = p;
                updated.paidAmount = qMin(p.amount, p.paidAmount + amount);
                updated.paid = updated.paidAmount >= updated.amount - 0.00001;
                offerPaymentReceipt(updated);
                refreshIncomePage();
                refreshPaymentList();
                refreshDashboard();
            } else {
                QMessageBox::warning(this, "Error", "Could not record the payment.");
            }
        });
        row->addWidget(payBtn);

        QPushButton *remindBtn = new QPushButton("Remind");
        remindBtn->setObjectName("miniButton");
        remindBtn->setCursor(Qt::PointingHandCursor);
        Room roomCopy = room;
        int days = overdue ? cycleDue.daysTo(today) : qMax(0, today.daysTo(cycleDue));
        connect(remindBtn, &QPushButton::clicked, this, [this, roomCopy, days, overdue, remaining]() {
            sendTelegramReminder(roomCopy, days, overdue, remaining);
        });
        row->addWidget(remindBtn);

        QListWidgetItem *item = new QListWidgetItem(incomeOutstandingList);
        item->setSizeHint(card->sizeHint());
        incomeOutstandingList->addItem(item);
        incomeOutstandingList->setItemWidget(item, card);
    }

    double outstanding = qMax(0.0, expected - collected);
    incomeExpectedLabel->setText(QString("$%1").arg(expected, 0, 'f', 2));
    incomeCollectedLabel->setText(QString("$%1").arg(collected, 0, 'f', 2));
    incomeOutstandingLabel->setText(QString("$%1").arg(outstanding, 0, 'f', 2));
    incomePartialLabel->setText(QString::number(partialCount));
    incomeUnpaidLabel->setText(QString::number(unpaidCount));
    incomeOverdueLabel->setText(QString::number(overdueCount));

    if (incomeOutstandingList->count() == 0) {
        QLabel *empty = new QLabel("✓ All tenants are fully paid this month.");
        empty->setStyleSheet("color: #15803d; font-size: 12px; padding: 12px;");
        QListWidgetItem *item = new QListWidgetItem(incomeOutstandingList);
        item->setSizeHint(empty->sizeHint());
        incomeOutstandingList->addItem(item);
        incomeOutstandingList->setItemWidget(item, empty);
    }
}

void MainWindow::refreshHistoryList()
{
    historyListWidget->clear();

    QString searchText = tenantSearchEdit ? tenantSearchEdit->text().trimmed() : QString();
    bool isSearching = !searchText.isEmpty();

    QVector<Payment> results = isSearching
        ? db->searchPaymentsByTenant(searchText, ownerId)
        : db->getPaymentHistory(ownerId);

    QString monthFilter = historyMonthFilterCombo ? historyMonthFilterCombo->currentData().toString() : QString();
    if (!monthFilter.isEmpty()) {
        QVector<Payment> filtered;
        for (const Payment &p : results)
            if (p.month == monthFilter)
                filtered.append(p);
        results = filtered;
    }

    if (results.isEmpty()) {
        QString message = isSearching
            ? QString("No payment records found for \"%1\".").arg(searchText)
            : "No payment records for this month.";
        QLabel *emptyLabel = new QLabel(message);
        emptyLabel->setStyleSheet("color: #64748b; font-size: 12px; padding: 10px 4px;");
        QListWidgetItem *item = new QListWidgetItem(historyListWidget);
        item->setSizeHint(emptyLabel->sizeHint());
        historyListWidget->addItem(item);
        historyListWidget->setItemWidget(item, emptyLabel);
        return;
    }

    for (const Payment &p : results) {
        QFrame *card = new QFrame;
        card->setObjectName("rowCard");
        QHBoxLayout *rowLayout = new QHBoxLayout(card);
        rowLayout->setContentsMargins(14, 10, 14, 10);

        QVBoxLayout *textCol = new QVBoxLayout;
        textCol->setSpacing(2);

        QString titleText = isSearching
            ? QString("Room %1  \u00B7  %2  \u00B7  %3").arg(p.roomNumber, p.month, p.tenantName)
            : QString("Room %1  \u00B7  %2").arg(p.roomNumber, p.month);
        QLabel *titleLbl = new QLabel(titleText);
        titleLbl->setProperty("role", "rowTitle");

        double remaining = qMax(0.0, p.amount - p.paidAmount);
        QString subText = p.paid
            ? QString("Bill $%1  ·  paid $%2 on %3").arg(p.amount, 0, 'f', 2).arg(p.paidAmount, 0, 'f', 2).arg(p.paidOn)
            : (p.paidAmount > 0
                ? QString("Bill $%1  ·  paid $%2  ·  remaining $%3")
                    .arg(p.amount, 0, 'f', 2).arg(p.paidAmount, 0, 'f', 2).arg(remaining, 0, 'f', 2)
                : QString("Bill $%1  ·  not paid yet").arg(p.amount, 0, 'f', 2));
        QLabel *subLbl = new QLabel(subText);
        subLbl->setProperty("role", "rowSubtitle");

        textCol->addWidget(titleLbl);
        textCol->addWidget(subLbl);
        rowLayout->addLayout(textCol);
        rowLayout->addStretch();

        if (isSearching) {
            QLabel *badge = new QLabel(p.paid ? "PAID" : (p.paidAmount > 0 ? "PARTIAL" : "UNPAID"));
            badge->setProperty("status", p.paid ? "paid" : (p.paidAmount > 0 ? "partial" : "unpaid"));
            rowLayout->addWidget(badge);
        }

        QListWidgetItem *item = new QListWidgetItem(historyListWidget);
        item->setSizeHint(card->sizeHint());
        historyListWidget->addItem(item);
        historyListWidget->setItemWidget(item, card);
    }
}

// ============ Helpers ============

int MainWindow::selectedRoomId() const
{
    QListWidgetItem *item = roomListWidget->currentItem();
    if (!item) return -1;
    return item->data(Qt::UserRole).toInt();
}

int MainWindow::selectedPaymentId() const
{
    QListWidgetItem *item = paymentListWidget->currentItem();
    if (!item) return -1;
    return item->data(Qt::UserRole).toInt();
}

// ============ Room actions ============

void MainWindow::onAddRoom()
{
    RoomDialog dialog(this, Room(), db, ownerId);
    if (dialog.exec() == QDialog::Accepted) {
        Room newRoom = dialog.getRoom();
        if (newRoom.number.isEmpty()) {
            QMessageBox::warning(this, "Missing info", "Room number is required.");
            return;
        }
        if (db->addRoom(newRoom, ownerId)) {
            refreshRoomList();
        } else {
            QMessageBox::warning(this, "Error", "Could not add room.");
        }
    }
}

void MainWindow::onEditRoom()
{
    int id = selectedRoomId();
    if (id == -1) {
        QMessageBox::information(this, "No selection", "Click a room in the list first.");
        return;
    }

    Room existing;
    for (const Room &r : db->getAllRooms(ownerId)) {
        if (r.id == id) { existing = r; break; }
    }

    RoomDialog dialog(this, existing, db, ownerId);
    if (dialog.exec() == QDialog::Accepted) {
        Room updated = dialog.getRoom();
        if (db->updateRoom(updated)) {
            refreshRoomList();
        } else {
            QMessageBox::warning(this, "Error", "Could not update room.");
        }
    }
}

void MainWindow::onDeleteRoom()
{
    int id = selectedRoomId();
    if (id == -1) {
        QMessageBox::information(this, "No selection", "Click a room in the list first.");
        return;
    }

    auto confirm = QMessageBox::question(this, "Delete room",
        "Are you sure? This also deletes its payment history.");
    if (confirm != QMessageBox::Yes) return;

    if (db->deleteRoom(id)) {
        refreshRoomList();
    } else {
        QMessageBox::warning(this, "Error", "Could not delete room.");
    }
}

// ============ Payment actions ============

Payment MainWindow::selectedPayment() const
{
    int id = selectedPaymentId();
    if (id == -1)
        return Payment();

    QString actualCurrentMonth = QDate::currentDate().toString("yyyy-MM");
    QString selectedMonth = (paymentMonthFilterCombo && paymentMonthFilterCombo->currentIndex() >= 0)
        ? paymentMonthFilterCombo->currentData().toString()
        : actualCurrentMonth;

    QVector<Payment> payments = (selectedMonth == actualCurrentMonth)
        ? db->getPaymentsForMonth(selectedMonth, ownerId)
        : db->getPaymentsForMonthReadOnly(selectedMonth, ownerId);

    for (const Payment &p : payments)
        if (p.id == id)
            return p;

    return Payment();
}

void MainWindow::onTogglePaid()
{
    onRecordPayment();
}

void MainWindow::onRecordPayment()
{
    Payment target = selectedPayment();
    if (target.id == -1) {
        QMessageBox::information(this, "No selection", "Click a payment in the list first.");
        return;
    }

    double remaining = qMax(0.0, target.amount - target.paidAmount);
    if (remaining <= 0.00001) {
        QMessageBox::information(this, "Already paid", "This bill is already fully paid.");
        return;
    }

    bool ok = false;
    double amount = QInputDialog::getDouble(this, "Record Payment",
        QString("Bill: $%1\nAlready paid: $%2\nRemaining: $%3\n\nEnter amount received:")
            .arg(target.amount, 0, 'f', 2)
            .arg(target.paidAmount, 0, 'f', 2)
            .arg(remaining, 0, 'f', 2),
        remaining, 0.01, remaining, 2, &ok);
    if (!ok) return;

    if (!db->recordPayment(target.id, amount)) {
        QMessageBox::warning(this, "Error", "Could not record the payment.");
        return;
    }

    Payment updated = target;
    updated.paidAmount = qMin(target.amount, target.paidAmount + amount);
    updated.paid = updated.paidAmount >= updated.amount - 0.00001;
    offerPaymentReceipt(updated);
    refreshPaymentList();
    refreshIncomePage();
    refreshDashboard();
}

// ============ Settings actions ============

void MainWindow::onSaveTelegramSettings()
{
    TelegramSettings settings;
    settings.botToken = botTokenEdit->text().trimmed();
    bool telegramOk = db->saveTelegramSettings(ownerId, settings);

    if (telegramOk) {
        QMessageBox::information(this, "Saved", "Settings saved.");
    } else {
        QMessageBox::warning(this, "Error", "Could not save settings.");
    }
}

void MainWindow::onUploadAbaQr()
{
    QString filePath = QFileDialog::getOpenFileName(
        this, "Select ABA KHQR image", QString(),
        "Images (*.png *.jpg *.jpeg *.bmp *.webp)");
    if (filePath.isEmpty())
        return;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Couldn't read file", "That image file couldn't be opened.");
        return;
    }
    QByteArray imageData = file.readAll();
    file.close();

    QPixmap check;
    if (imageData.isEmpty() || !check.loadFromData(imageData)) {
        QMessageBox::warning(this, "Invalid image", "That file doesn't look like a valid image.");
        return;
    }

    if (db->saveAbaQrImage(ownerId, imageData)) {
        refreshQrPreview();
        QMessageBox::information(this, "Saved", "Your ABA KHQR image has been saved.");
    } else {
        QMessageBox::warning(this, "Error", "Could not save the QR image.");
    }
}

void MainWindow::onRemoveAbaQr()
{
    if (QMessageBox::question(this, "Remove QR", "Remove your saved ABA KHQR image?",
                               QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    if (db->clearAbaQrImage(ownerId)) {
        refreshQrPreview();
    } else {
        QMessageBox::warning(this, "Error", "Could not remove the QR image.");
    }
}

void MainWindow::onTestTelegramConnection()
{
    QString token = botTokenEdit->text().trimmed();
    testConnectionStatusLabel->setText("Testing...");
    testConnectionStatusLabel->setStyleSheet("font-size: 12px; color: #64748b;");

    auto *telegram = new TelegramService(this);
    connect(telegram, &TelegramService::connectionTested, this,
        [this, telegram](bool success, const QString &botUsername, const QString &error) {
            telegram->deleteLater();
            if (success) {
                testConnectionStatusLabel->setText(QString("\u2705 Connected as @%1").arg(botUsername));
                testConnectionStatusLabel->setStyleSheet("font-size: 12px; color: #16a34a; font-weight: 600;");
            } else {
                testConnectionStatusLabel->setText(QString("\u274C %1").arg(error));
                testConnectionStatusLabel->setStyleSheet("font-size: 12px; color: #dc2626; font-weight: 600;");
            }
        });

    telegram->testConnection(token);
}

void MainWindow::sendTelegramReminder(const Room &room, int days, bool overdue, double remaining)
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

    QString tenantName = room.tenantName.isEmpty() ? "there" : room.tenantName;
    QString balanceText = remaining >= 0
        ? QString("\nOutstanding balance: $%1").arg(remaining, 0, 'f', 2)
        : QString();
    QString message = overdue
        ? QString("Hello, %1!\nThis is a friendly reminder that your rent for Room %2 "
                   "is now %3 day(s) overdue.%4\nPlease arrange payment when you can - thank you!")
              .arg(tenantName, room.number).arg(days).arg(balanceText)
        : QString("Hello, %1!\nJust a reminder that your rent for Room %2 is due in %3 day(s).%4 "
                   "Thanks for staying with us!")
              .arg(tenantName, room.number).arg(days).arg(balanceText);

    QByteArray qrImage = db->getAbaQrImage(ownerId);
    bool attachQr = false;
    if (!qrImage.isEmpty()) {
        attachQr = QMessageBox::question(this, "Attach ABA QR?",
            "Attach your ABA KHQR payment code to this reminder?",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes;
    }

    auto *telegram = new TelegramService(this);
    connect(telegram, &TelegramService::sendFinished, this,
        [this, telegram](bool success, const QString &error) {
            telegram->deleteLater();
            if (success)
                QMessageBox::information(this, "Sent", "The reminder was sent on Telegram.");
            else
                QMessageBox::warning(this, "Send failed", error);
        });

    if (attachQr)
        telegram->sendPhoto(settings.botToken, room.telegramChatId, qrImage, message);
    else
        telegram->sendMessage(settings.botToken, room.telegramChatId, message);
}

void MainWindow::offerPaymentReceipt(const Payment &payment)
{
    QVector<Room> rooms = db->getAllRooms(ownerId);
    Room room;
    bool found = false;
    for (const Room &r : rooms) {
        if (r.id == payment.roomId) { room = r; found = true; break; }
    }

    // Stay quiet if Telegram isn't set up for this tenant - no need to nag on
    // every single payment marked paid.
    if (!found || room.telegramChatId.trimmed().isEmpty())
        return;

    QString tenantName = payment.tenantName.isEmpty() ? "there" : payment.tenantName;

    auto reply = QMessageBox::question(this, "Send receipt?",
        QString("Send a payment confirmation to %1 on Telegram?").arg(tenantName),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (reply != QMessageBox::Yes)
        return;

    TelegramSettings settings = db->getTelegramSettings(ownerId);
    if (settings.botToken.trimmed().isEmpty()) {
        QMessageBox::warning(this, "Bot not set up",
            "Set up your Telegram Bot Token first, in the Settings page.");
        return;
    }

    double remaining = qMax(0.0, payment.amount - payment.paidAmount);
    QString message = payment.paid
        ? QString(
            "Hello, %1!\n"
            "Your payment for Room %2 - %3 is now fully paid.\n"
            "Total paid: $%4\n"
            "Paid on: %5\n\n"
            "Thank you! \u2705"
          )
          .arg(tenantName, payment.roomNumber, payment.month)
          .arg(payment.paidAmount, 0, 'f', 2)
          .arg(QDate::currentDate().toString("yyyy-MM-dd"))
        : QString(
            "Hello, %1!\n"
            "We've received your partial payment for Room %2 - %3.\n"
            "Amount received: $%4\n"
            "Remaining balance: $%5\n\n"
            "Thank you!"
          )
          .arg(tenantName, payment.roomNumber, payment.month)
          .arg(payment.paidAmount, 0, 'f', 2)
          .arg(remaining, 0, 'f', 2);

    auto *telegram = new TelegramService(this);
    connect(telegram, &TelegramService::sendFinished, this,
        [this, telegram](bool success, const QString &error) {
            telegram->deleteLater();
            if (success)
                QMessageBox::information(this, "Sent", "The receipt was sent on Telegram.");
            else
                QMessageBox::warning(this, "Send failed", error);
        });

    telegram->sendMessage(settings.botToken, room.telegramChatId, message);
}
