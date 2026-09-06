#include "logindialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QIcon>
#include <QButtonGroup>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QShowEvent>
#include <QRegularExpression>

LoginDialog::LoginDialog(Database *db, QWidget *parent)
    : QDialog(parent), db(db)
{
    setWindowTitle("StayHub - Sign In");
    setWindowIcon(QIcon(":/icons/app_icon_64.png"));
    setMinimumSize(760, 480);

    // ================= Left brand panel =================
    QFrame *brandPanel = new QFrame(this);
    brandPanel->setObjectName("brandPanel");
    brandPanel->setFixedWidth(280);

    QVBoxLayout *brandLayout = new QVBoxLayout(brandPanel);
    brandLayout->setContentsMargins(32, 40, 32, 32);
    brandLayout->addStretch();

    QLabel *brandIcon = new QLabel;
    brandIcon->setObjectName("brandLogoBadge");
    brandIcon->setFixedSize(56, 56);
    brandIcon->setAlignment(Qt::AlignCenter);
    QPixmap brandPixmap(":/icons/app_icon_64.png");
    brandIcon->setPixmap(brandPixmap.scaled(42, 42, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    brandLayout->addWidget(brandIcon);
    brandLayout->addSpacing(14);

    QLabel *brandTitle = new QLabel("StayHub");
    brandTitle->setObjectName("brandTitle");
    brandLayout->addWidget(brandTitle);

    QLabel *brandSubtitle = new QLabel("Rental Manager");
    brandSubtitle->setObjectName("brandSubtitle");
    brandLayout->addWidget(brandSubtitle);

    brandLayout->addSpacing(28);

    struct FeatureItem { QString iconPath; QString text; };
    const QVector<FeatureItem> features = {
        {":/icons/icon_key.png",       "Track every room & tenant"},
        {":/icons/icon_card.png",      "See who paid, who hasn't"},
        {":/icons/icon_dashboard.png", "One dashboard, full picture"}
    };

    for (const FeatureItem &f : features) {
        QFrame *chip = new QFrame;
        chip->setObjectName("featureChip");
        QHBoxLayout *rowLayout = new QHBoxLayout(chip);
        rowLayout->setContentsMargins(12, 10, 12, 10);
        rowLayout->setSpacing(10);

        QLabel *iconLabel = new QLabel;
        iconLabel->setFixedSize(16, 16);
        iconLabel->setPixmap(QPixmap(f.iconPath).scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        rowLayout->addWidget(iconLabel);

        QLabel *textLabel = new QLabel(f.text);
        textLabel->setObjectName("brandFeature");
        textLabel->setWordWrap(true);
        rowLayout->addWidget(textLabel, 1);

        brandLayout->addWidget(chip);
        brandLayout->addSpacing(8);
    }

    brandLayout->addStretch();

    // ================= Right form panel =================
    QWidget *formPanel = new QWidget(this);
    QVBoxLayout *formPanelLayout = new QVBoxLayout(formPanel);
    formPanelLayout->setContentsMargins(48, 56, 48, 56);

    // ---- Pill-shaped Log In / Create Account switcher ----
    QFrame *pillContainer = new QFrame;
    pillContainer->setObjectName("pillContainer");
    pillContainer->setFixedHeight(40);
    QHBoxLayout *pillLayout = new QHBoxLayout(pillContainer);
    pillLayout->setContentsMargins(4, 4, 4, 4);
    pillLayout->setSpacing(0);

    // Indicator is created first so it naturally sits BEHIND the buttons in
    // z-order, and isn't managed by pillLayout - its geometry is animated by hand.
    pillIndicator = new QFrame(pillContainer);
    pillIndicator->setObjectName("pillIndicator");

    loginTabBtn = new QPushButton("Log in");
    loginTabBtn->setObjectName("pillTab");
    loginTabBtn->setCheckable(true);
    loginTabBtn->setChecked(true);
    loginTabBtn->setCursor(Qt::PointingHandCursor);

    registerTabBtn = new QPushButton("Create account");
    registerTabBtn->setObjectName("pillTab");
    registerTabBtn->setCheckable(true);
    registerTabBtn->setCursor(Qt::PointingHandCursor);

    QButtonGroup *pillGroup = new QButtonGroup(this);
    pillGroup->setExclusive(true);
    pillGroup->addButton(loginTabBtn);
    pillGroup->addButton(registerTabBtn);

    pillLayout->addWidget(loginTabBtn);
    pillLayout->addWidget(registerTabBtn);

    formPanelLayout->addWidget(pillContainer, 0, Qt::AlignLeft);
    formPanelLayout->addSpacing(22);

    QLabel *formTitle = new QLabel("Welcome to StayHub");
    formTitle->setObjectName("formTitle");
    formPanelLayout->addWidget(formTitle);

    QLabel *formSubtitle = new QLabel("Manage rooms, tenants and payments with less work.");
    formSubtitle->setObjectName("formSubtitle");
    formSubtitle->setWordWrap(true);
    formPanelLayout->addWidget(formSubtitle);
    formPanelLayout->addSpacing(18);

    // ---- Stacked forms (crossfaded via stackOpacityEffect) ----
    formStack = new QStackedWidget;
    stackOpacityEffect = new QGraphicsOpacityEffect(formStack);
    stackOpacityEffect->setOpacity(1.0);
    formStack->setGraphicsEffect(stackOpacityEffect);

    // ---- Login form ----
    QWidget *loginTab = new QWidget;
    loginUserEdit = new QLineEdit;
    loginUserEdit->setPlaceholderText("your_username");
    loginUserEdit->addAction(QIcon(":/icons/icon_user_outline.png"), QLineEdit::LeadingPosition);
    loginPassEdit = new QLineEdit;
    loginPassEdit->setPlaceholderText("\u2022\u2022\u2022\u2022\u2022\u2022\u2022\u2022");
    loginPassEdit->setEchoMode(QLineEdit::Password);
    loginPassEdit->addAction(QIcon(":/icons/icon_lock_outline.png"), QLineEdit::LeadingPosition);

    QFormLayout *loginForm = new QFormLayout;
    loginForm->setVerticalSpacing(14);
    loginForm->addRow("Username", loginUserEdit);
    loginForm->addRow("Password", loginPassEdit);

    QPushButton *loginBtn = new QPushButton("Log in");
    loginBtn->setObjectName("primaryButton");
    loginBtn->setMinimumHeight(42);
    loginBtn->setCursor(Qt::PointingHandCursor);
    connect(loginBtn, &QPushButton::clicked, this, &LoginDialog::onLoginClicked);
    connect(loginPassEdit, &QLineEdit::returnPressed, this, &LoginDialog::onLoginClicked);

    QVBoxLayout *loginLayout = new QVBoxLayout(loginTab);
    loginLayout->setContentsMargins(4, 0, 4, 4);
    loginLayout->addLayout(loginForm);
    loginLayout->addSpacing(20);
    loginLayout->addWidget(loginBtn);
    loginLayout->addStretch();

    // ---- Register form ----
    QWidget *regTab = new QWidget;
    regUserEdit = new QLineEdit;
    regUserEdit->setPlaceholderText("pick a username");
    regUserEdit->addAction(QIcon(":/icons/icon_user_outline.png"), QLineEdit::LeadingPosition);
    regPassEdit = new QLineEdit;
    regPassEdit->setPlaceholderText("\u2022\u2022\u2022\u2022\u2022\u2022\u2022\u2022");
    regPassEdit->setEchoMode(QLineEdit::Password);
    regPassEdit->addAction(QIcon(":/icons/icon_lock_outline.png"), QLineEdit::LeadingPosition);
    regPassConfirmEdit = new QLineEdit;
    regPassConfirmEdit->setPlaceholderText("\u2022\u2022\u2022\u2022\u2022\u2022\u2022\u2022");
    regPassConfirmEdit->setEchoMode(QLineEdit::Password);
    regPassConfirmEdit->addAction(QIcon(":/icons/icon_lock_outline.png"), QLineEdit::LeadingPosition);

    QFormLayout *regForm = new QFormLayout;
    regForm->setVerticalSpacing(14);
    regForm->addRow("Username", regUserEdit);
    regForm->addRow("Password", regPassEdit);
    regForm->addRow("Confirm", regPassConfirmEdit);

    passwordChecklist = new QLabel;
    passwordChecklist->setObjectName("passwordChecklist");
    passwordChecklist->setTextFormat(Qt::RichText);

    QPushButton *regBtn = new QPushButton("Create account");
    regBtn->setObjectName("primaryButton");
    regBtn->setMinimumHeight(42);
    regBtn->setCursor(Qt::PointingHandCursor);
    connect(regBtn, &QPushButton::clicked, this, &LoginDialog::onRegisterClicked);

    QVBoxLayout *regLayout = new QVBoxLayout(regTab);
    regLayout->setContentsMargins(4, 0, 4, 4);
    regLayout->addLayout(regForm);
    regLayout->addSpacing(10);
    regLayout->addWidget(passwordChecklist);
    regLayout->addSpacing(14);
    regLayout->addWidget(regBtn);
    regLayout->addStretch();

    connect(regPassEdit, &QLineEdit::textChanged, this, &LoginDialog::updatePasswordChecklist);
    updatePasswordChecklist(QString()); // show the (all-grey) checklist right away

    formStack->addWidget(loginTab);
    formStack->addWidget(regTab);
    formPanelLayout->addWidget(formStack);

    connect(loginTabBtn, &QPushButton::clicked, this, [this]() {
        animateIndicatorTo(loginTabBtn);
        switchToForm(0);
    });
    connect(registerTabBtn, &QPushButton::clicked, this, [this]() {
        animateIndicatorTo(registerTabBtn);
        switchToForm(1);
    });

    // ================= Combine panels =================
    QHBoxLayout *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(brandPanel);
    root->addWidget(formPanel, 1);

    setStyleSheet(R"(
        QDialog {
            background-color: #ffffff;
        }
        QWidget {
            font-family: "Kantumruy Pro", "Segoe UI";
            color: #0f172a;
        }
        #brandPanel {
            background-color: #16213a;
        }
        #brandLogoBadge {
            background-color: #ffffff;
            border-radius: 20px;
        }
        #brandTitle {
            font-family: "Plus Jakarta Sans ExtraBold";
            color: #ffffff;
            font-size: 27px;
        }
        #brandSubtitle {
            color: #8896b3;
            font-size: 13px;
        }
        #featureChip {
            background-color: #1f2947;
            border-radius: 18px;
        }
        #brandFeature {
            color: #dfe3f0;
            font-size: 12px;
        }
        #pillContainer {
            background-color: #f1f0eb;
            border-radius: 999px;
        }
        #pillIndicator {
            background-color: #4f5eff;
            border-radius: 999px;
        }
        #pillTab {
            background-color: transparent;
            color: #64748b;
            border: none;
            border-radius: 999px;
            padding: 8px 20px;
            font-size: 13px;
            font-weight: 500;
        }
        #pillTab:checked {
            color: #ffffff;
            font-weight: 600;
        }
        QLabel {
            font-size: 13px;
        }
        #formTitle {
            font-family: "Plus Jakarta Sans ExtraBold";
            color: #0f172a;
            font-size: 25px;
            font-weight: 800;
        }
        #formSubtitle {
            color: #64748b;
            font-size: 12px;
            line-height: 1.4;
        }
        #passwordChecklist {
            font-size: 11px;
        }
        QLineEdit {
            border: 1px solid #e5e7eb;
            border-radius: 19px;
            padding: 9px 16px;
            font-size: 13px;
            background-color: #ffffff;
        }
        QLineEdit:focus {
            border: 1px solid #4f5eff;
        }
        #primaryButton {
            background-color: #4f5eff;
            color: white;
            border: none;
            border-radius: 21px;
            padding: 9px 16px;
            font-size: 14px;
            font-weight: 600;
        }
        #primaryButton:hover {
            background-color: #4048d8;
        }
        #primaryButton:pressed {
            background-color: #333ab0;
        }
    )");
}

void LoginDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    if (!indicatorPositioned) {
        // First paint: snap the indicator straight to the Log In button, no
        // animation - it should look correct on frame one, not slide in from nowhere.
        pillIndicator->setGeometry(loginTabBtn->geometry());
        pillIndicator->lower();
        indicatorPositioned = true;
    }
}

void LoginDialog::animateIndicatorTo(QPushButton *btn)
{
    auto *anim = new QPropertyAnimation(pillIndicator, "geometry", this);
    anim->setDuration(280);
    anim->setEasingCurve(QEasingCurve::OutBack); // tiny overshoot - reads as springy/flexible, not stiff
    anim->setStartValue(pillIndicator->geometry());
    anim->setEndValue(btn->geometry());
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void LoginDialog::switchToForm(int index)
{
    if (formStack->currentIndex() == index)
        return;

    // Fade the current form out, swap the page underneath once it's invisible,
    // then fade the new one in. Keeps the transition smooth in both directions.
    auto *fadeOut = new QPropertyAnimation(stackOpacityEffect, "opacity", this);
    fadeOut->setDuration(130);
    fadeOut->setStartValue(1.0);
    fadeOut->setEndValue(0.0);
    fadeOut->setEasingCurve(QEasingCurve::OutCubic);

    connect(fadeOut, &QPropertyAnimation::finished, this, [this, index]() {
        formStack->setCurrentIndex(index);

        auto *fadeIn = new QPropertyAnimation(stackOpacityEffect, "opacity", this);
        fadeIn->setDuration(220);
        fadeIn->setStartValue(0.0);
        fadeIn->setEndValue(1.0);
        fadeIn->setEasingCurve(QEasingCurve::OutCubic);
        fadeIn->start(QAbstractAnimation::DeleteWhenStopped);
    });

    fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
}

void LoginDialog::updatePasswordChecklist(const QString &password)
{
    bool hasLength = password.length() >= 8;
    bool hasUpper = password.contains(QRegularExpression("[A-Z]"));
    bool hasNumber = password.contains(QRegularExpression("[0-9]"));
    bool hasSpecial = password.contains(QRegularExpression("[^A-Za-z0-9]"));

    auto line = [](bool met, const QString &label) {
        QString color = met ? "#16a34a" : "#94a3b8";
        QString mark = met ? "\u2713" : "\u25CB"; // check / hollow circle
        return QString("<span style='color:%1;'>%2 %3</span>").arg(color, mark, label);
    };

    QString html = line(hasLength, "At least 8 characters") + "<br>"
                  + line(hasUpper, "One uppercase letter") + "<br>"
                  + line(hasNumber, "One number") + "<br>"
                  + line(hasSpecial, "One special character (!@#$...)");

    passwordChecklist->setText(html);
}

bool LoginDialog::isPasswordValid(const QString &password) const
{
    return password.length() >= 8
        && password.contains(QRegularExpression("[A-Z]"))
        && password.contains(QRegularExpression("[0-9]"))
        && password.contains(QRegularExpression("[^A-Za-z0-9]"));
}

void LoginDialog::onLoginClicked()
{
    QString user = loginUserEdit->text().trimmed();
    QString pass = loginPassEdit->text();

    if (user.isEmpty() || pass.isEmpty()) {
        QMessageBox::warning(this, "Missing info", "Enter username and password.");
        return;
    }

    int id = db->loginUser(user, pass);
    if (id == -1) {
        QMessageBox::warning(this, "Login failed", "Wrong username or password.");
        return;
    }

    userId = id;
    username = user;
    accept();
}

void LoginDialog::onRegisterClicked()
{
    QString user = regUserEdit->text().trimmed();
    QString pass = regPassEdit->text();
    QString confirm = regPassConfirmEdit->text();

    if (!isPasswordValid(pass)) {
        QMessageBox::warning(this, "Password too weak",
            "Your password needs to meet every rule shown under the password field "
            "(8+ characters, an uppercase letter, a number, and a special character).");
        return;
    }

    if (pass != confirm) {
        QMessageBox::warning(this, "Password mismatch", "Passwords do not match.");
        return;
    }

    QString error;
    if (!db->registerUser(user, pass, error)) {
        QMessageBox::warning(this, "Registration failed", error);
        return;
    }

    // No "you can log in now" popup - just close this dialog with a special code.
    // main() sees that code and opens a brand new LoginDialog, landing on the
    // Log In tab, which reads as "the app restarted and now wants you to log in".
    done(AccountCreatedRestart);
}
