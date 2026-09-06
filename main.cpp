#include <QApplication>
#include <QIcon>
#include <QFontDatabase>
#include <QFont>
#include <QStyleFactory>
#include <QPalette>
#include <QColor>
#include <QSettings>
#include <QEventLoop>
#include <QLineEdit>
#include <QAbstractSpinBox>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QEvent>
#include <QTimer>

class SelectAllOnFocusFilter : public QObject {
protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() == QEvent::FocusIn) {
            // Delay selection until Qt finishes its normal focus handling. This
            // prevents QLineEdit/QSpinBox from moving the cursor after selectAll().
            QTimer::singleShot(0, watched, [watched]() {
                if (auto *edit = qobject_cast<QLineEdit *>(watched)) {
                    edit->selectAll();
                } else if (auto *spin = qobject_cast<QAbstractSpinBox *>(watched)) {
                    if (auto *edit = spin->findChild<QLineEdit *>())
                        edit->selectAll();
                } else if (auto *combo = qobject_cast<QComboBox *>(watched)) {
                    if (combo->isEditable() && combo->lineEdit()) combo->lineEdit()->selectAll();
                } else if (auto *textEdit = qobject_cast<QTextEdit *>(watched)) {
                    textEdit->selectAll();
                } else if (auto *plainEdit = qobject_cast<QPlainTextEdit *>(watched)) {
                    plainEdit->selectAll();
                }
            });
        }
        return QObject::eventFilter(watched, event);
    }
};
#include "mainwindow.h"
#include "logindialog.h"
#include "database.h"
#include "splashscreen.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    SelectAllOnFocusFilter selectAllFilter;
    app.installEventFilter(&selectAllFilter);
    app.setQuitOnLastWindowClosed(false);
    app.setStyle(QStyleFactory::create("Fusion"));
    QPalette lightPalette;
    lightPalette.setColor(QPalette::Window,QColor("#f1f5f9")); lightPalette.setColor(QPalette::WindowText,QColor("#0f172a"));
    lightPalette.setColor(QPalette::Base,QColor("#ffffff")); lightPalette.setColor(QPalette::AlternateBase,QColor("#f8fafc"));
    lightPalette.setColor(QPalette::ToolTipBase,QColor("#ffffff")); lightPalette.setColor(QPalette::ToolTipText,QColor("#0f172a"));
    lightPalette.setColor(QPalette::Text,QColor("#0f172a")); lightPalette.setColor(QPalette::Button,QColor("#ffffff"));
    lightPalette.setColor(QPalette::ButtonText,QColor("#0f172a")); lightPalette.setColor(QPalette::Link,QColor("#4f5eff"));
    lightPalette.setColor(QPalette::Highlight,QColor("#4f5eff")); lightPalette.setColor(QPalette::HighlightedText,QColor("#ffffff"));
    lightPalette.setColor(QPalette::Disabled,QPalette::Text,QColor("#94a3b8")); lightPalette.setColor(QPalette::Disabled,QPalette::WindowText,QColor("#94a3b8"));
    app.setPalette(lightPalette);
    QFontDatabase::addApplicationFont(":/fonts/KantumruyPro-Regular.ttf"); QFontDatabase::addApplicationFont(":/fonts/KantumruyPro-Medium.ttf");
    QFontDatabase::addApplicationFont(":/fonts/KantumruyPro-SemiBold.ttf"); QFontDatabase::addApplicationFont(":/fonts/KantumruyPro-Bold.ttf");
    QFontDatabase::addApplicationFont(":/fonts/PlusJakartaSans-Bold.ttf"); QFontDatabase::addApplicationFont(":/fonts/PlusJakartaSans-ExtraBold.ttf");
    app.setFont(QFont("Kantumruy Pro",10));
    QIcon appIcon; for (int s : {16,24,32,48,64,128,256}) appIcon.addFile(QString(":/icons/app_icon_%1.png").arg(s),QSize(s,s));
    app.setWindowIcon(appIcon);
    Database db; if (!db.init()) return -1;
    QSettings session("StayHub","StayHub");
    int ownerId=session.value("ownerId",-1).toInt(); QString ownerUsername=session.value("username").toString();
    if (ownerId<0 || !db.getUserById(ownerId,ownerUsername)) { ownerId=-1; ownerUsername.clear(); session.remove("ownerId"); session.remove("username"); }
    SplashScreen splash(ownerId >= 0, ownerUsername); splash.exec();
    while (true) {
        if (ownerId<0) {
            LoginDialog loginDlg(&db);
            int loginResult = loginDlg.exec();
            if (loginResult == LoginDialog::AccountCreatedRestart)
                continue;
            if (loginResult != QDialog::Accepted)
                return 0;
            ownerId=loginDlg.loggedInUserId(); ownerUsername=loginDlg.loggedInUsername();
            session.setValue("ownerId",ownerId); session.setValue("username",ownerUsername); session.sync();
        }
        MainWindow window(&db,ownerId,ownerUsername); bool loggedOut=false; QEventLoop loop;
        QObject::connect(&window,&MainWindow::logoutRequested,[&](){ loggedOut=true; session.remove("ownerId"); session.remove("username"); session.sync(); loop.quit(); });
        QObject::connect(&window,&QWidget::destroyed,&loop,&QEventLoop::quit);
        window.show(); loop.exec();
        if (!loggedOut)
            return 0;

        ownerId = -1;
        ownerUsername.clear();
    }
}
