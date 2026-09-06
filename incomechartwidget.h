#pragma once
#include <QWidget>
#include <QVector>
#include <QVariantAnimation>
#include <QRectF>
#include "database.h"

class QPaintEvent;
class QMouseEvent;

// Custom-painted grouped bar chart comparing Expected vs Collected rent per
// month. Built with QPainter (not the Qt Charts module) so it never risks a
// "module not installed" build failure, and so every color/shape matches the
// rest of the app exactly. Bars grow in with a single eased animation on load.
class IncomeChartWidget : public QWidget {
    Q_OBJECT

public:
    explicit IncomeChartWidget(QWidget *parent = nullptr);

    void setData(const QVector<MonthlyIncomeSummary> &data);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QVector<MonthlyIncomeSummary> m_data;
    QVariantAnimation *m_animation;
    double m_animProgress = 1.0;

    struct BarHit { QRectF rect; QString tooltip; };
    QVector<BarHit> m_hitRects; // rebuilt every paint, used for hover tooltips
};
