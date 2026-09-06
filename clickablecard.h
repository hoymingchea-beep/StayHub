#pragma once
#include <QFrame>

class QMouseEvent;
class QEnterEvent;

// A QFrame that emits clicked() on left click and exposes a "hovered" dynamic
// property for QSS - lets dashboard stat cards look exactly like before but
// also be tappable, without QPushButton's text-only content limitation.
class ClickableCard : public QFrame {
    Q_OBJECT

public:
    explicit ClickableCard(QWidget *parent = nullptr);

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
};
