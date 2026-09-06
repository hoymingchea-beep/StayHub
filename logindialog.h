#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QFrame>
#include "database.h"

class QGraphicsOpacityEffect;
class QShowEvent;
class QLabel;

// Shown once at app startup, before MainWindow.
// Has a pill-shaped Log In / Create Account switcher (animated sliding highlight)
// over two stacked forms (animated crossfade between them).
class LoginDialog : public QDialog {
    Q_OBJECT

public:
    // exec() returns this instead of QDialog::Accepted when a new account was just
    // created. main() should respond by opening a brand new LoginDialog (landing on
    // the Log In tab) so it feels like the app "restarted" into the login screen.
    static constexpr int AccountCreatedRestart = 2;

    LoginDialog(Database *db, QWidget *parent = nullptr);

    int loggedInUserId() const { return userId; }
    QString loggedInUsername() const { return username; }

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void onLoginClicked();
    void onRegisterClicked();

private:
    Database *db;
    int userId = -1;
    QString username;

    QPushButton *loginTabBtn;
    QPushButton *registerTabBtn;
    QFrame *pillIndicator;
    bool indicatorPositioned = false;

    QStackedWidget *formStack;
    QGraphicsOpacityEffect *stackOpacityEffect;

    QLineEdit *loginUserEdit;
    QLineEdit *loginPassEdit;

    QLineEdit *regUserEdit;
    QLineEdit *regPassEdit;
    QLineEdit *regPassConfirmEdit;
    QLabel *passwordChecklist;

    // Slides the pill highlight behind whichever tab button was just clicked.
    void animateIndicatorTo(QPushButton *btn);
    // Crossfades formStack from its current page to a new one.
    void switchToForm(int index);
    // Rebuilds the live checklist under the Create Account password field.
    void updatePasswordChecklist(const QString &password);
    // True only when password meets every rule shown in the checklist.
    bool isPasswordValid(const QString &password) const;
};
