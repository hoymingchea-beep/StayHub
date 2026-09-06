#include "clickablecard.h"
#include <QMouseEvent>
#include <QStyle>

ClickableCard::ClickableCard(QWidget *parent) : QFrame(parent)
{
    setCursor(Qt::PointingHandCursor);
    setProperty("hovered", false);
}

void ClickableCard::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        emit clicked();
    QFrame::mousePressEvent(event);
}

void ClickableCard::enterEvent(QEnterEvent *event)
{
    setProperty("hovered", true);
    style()->unpolish(this);
    style()->polish(this);
    QFrame::enterEvent(event);
}

void ClickableCard::leaveEvent(QEvent *event)
{
    setProperty("hovered", false);
    style()->unpolish(this);
    style()->polish(this);
    QFrame::leaveEvent(event);
}
