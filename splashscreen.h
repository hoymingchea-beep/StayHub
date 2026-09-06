#pragma once
#include <QDialog>

class SplashScreen : public QDialog {
    Q_OBJECT
public:
    explicit SplashScreen(bool returningUser = false, const QString &username = QString(), QWidget *parent = nullptr);
};
