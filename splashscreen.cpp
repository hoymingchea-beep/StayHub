#include "splashscreen.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QTimer>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QSoundEffect>
#include <QUrl>

SplashScreen::SplashScreen(bool returningUser, const QString &username, QWidget *parent) : QDialog(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(520, 320);

    QWidget *card = new QWidget(this);
    card->setGeometry(rect());
    card->setStyleSheet("QWidget { background: #16213a; border-radius: 26px; } QLabel { color: white; }");

    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(40, 34, 40, 30);
    layout->setAlignment(Qt::AlignCenter);

    QLabel *logo = new QLabel;
    logo->setFixedSize(92, 92);
    logo->setAlignment(Qt::AlignCenter);
    logo->setStyleSheet("background: white; border-radius: 26px;");
    logo->setPixmap(QPixmap(":/icons/app_icon_128.png").scaled(68, 68, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    layout->addWidget(logo, 0, Qt::AlignCenter);

    QLabel *title = new QLabel("StayHub");
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-family: 'Plus Jakarta Sans ExtraBold'; font-size: 34px; font-weight: 800;");
    layout->addWidget(title);

    QLabel *subtitle = new QLabel;
    subtitle->setAlignment(Qt::AlignCenter);
    if (returningUser && !username.isEmpty()) {
        subtitle->setText(QString("Welcome back, %1 👋").arg(username));
        subtitle->setStyleSheet("color: #dce4ff; font-size: 14px; font-weight: 600;");
    } else {
        subtitle->setText("Rental Manager");
        subtitle->setStyleSheet("color: #aeb9d3; font-size: 14px;");
    }
    layout->addWidget(subtitle);

    QLabel *loading = new QLabel(returningUser ? "Opening your workspace..." : "Starting StayHub...");
    loading->setAlignment(Qt::AlignCenter);
    loading->setStyleSheet("color: #7f8fb1; font-size: 11px;");
    layout->addSpacing(12);
    layout->addWidget(loading);

    auto *effect = new QGraphicsOpacityEffect(this);
    loading->setGraphicsEffect(effect);
    auto *pulse = new QPropertyAnimation(effect, "opacity", this);
    pulse->setDuration(550);
    pulse->setStartValue(0.35);
    pulse->setEndValue(1.0);
    pulse->setLoopCount(2);
    pulse->start(QAbstractAnimation::DeleteWhenStopped);

    auto *sound = new QSoundEffect(this);
    sound->setSource(QUrl("qrc:/sounds/stayhub_start.wav"));
    sound->setVolume(0.28);
    sound->play();

    QTimer::singleShot(returningUser ? 950 : 1150, this, &QDialog::accept);
}
