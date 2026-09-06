#include "incomechartwidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QToolTip>
#include <QDate>
#include <QtMath>

IncomeChartWidget::IncomeChartWidget(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(260);
    setMouseTracking(true);

    m_animation = new QVariantAnimation(this);
    m_animation->setStartValue(0.0);
    m_animation->setEndValue(1.0);
    m_animation->setDuration(750);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_animProgress = value.toDouble();
        update();
    });
}

void IncomeChartWidget::setData(const QVector<MonthlyIncomeSummary> &data)
{
    m_data = data;
    m_animProgress = 0.0;
    m_animation->stop();
    m_animation->start();
}

void IncomeChartWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRectF full(0, 0, width(), height());

    // ---- Card background ----
    QPainterPath cardPath;
    cardPath.addRoundedRect(full.adjusted(0, 0, -1, -1), 12, 12);
    painter.fillPath(cardPath, QColor("#ffffff"));
    painter.setPen(QPen(QColor("#e8e2d5"), 1));
    painter.drawPath(cardPath);

    m_hitRects.clear();

    if (m_data.isEmpty()) {
        painter.setPen(QColor("#94a3b8"));
        painter.drawText(full, Qt::AlignCenter, "No income data yet");
        return;
    }

    // ---- Legend (top-right) ----
    const int legendY = 20;
    int legendX = width() - 20;

    auto drawLegendItem = [&](const QString &label, const QColor &color) {
        QFontMetrics fm(painter.font());
        int textW = fm.horizontalAdvance(label);
        legendX -= textW;
        painter.setPen(QColor("#475569"));
        painter.drawText(QRectF(legendX, legendY - 6, textW, 16), Qt::AlignVCenter, label);
        legendX -= 18;
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawRoundedRect(QRectF(legendX, legendY - 5, 12, 12), 3, 3);
        legendX -= 18;
    };
    drawLegendItem("Collected", QColor("#22c55e"));
    drawLegendItem("Expected", QColor("#4f5eff"));

    // ---- Plot area ----
    const double leftPad = 54, rightPad = 16, topPad = 40, bottomPad = 30;
    QRectF plot(leftPad, topPad, width() - leftPad - rightPad, height() - topPad - bottomPad);

    double maxVal = 1.0;
    for (const auto &m : m_data)
        maxVal = qMax(maxVal, qMax(m.expected, m.collected));
    maxVal *= 1.15;

    // ---- Gridlines + $ axis labels ----
    painter.setPen(QColor("#94a3b8"));
    QFont smallFont = painter.font();
    smallFont.setPointSizeF(smallFont.pointSizeF() - 1);
    painter.setFont(smallFont);

    for (int i = 0; i <= 3; ++i) {
        double frac = i / 3.0;
        double y = plot.bottom() - frac * plot.height();
        painter.setPen(QColor("#f1efe9"));
        painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));

        double value = frac * maxVal;
        painter.setPen(QColor("#94a3b8"));
        QString label = "$" + QString::number(qRound(value));
        painter.drawText(QRectF(0, y - 8, leftPad - 8, 16), Qt::AlignRight | Qt::AlignVCenter, label);
    }

    // ---- Bars ----
    int n = m_data.size();
    double slotWidth = plot.width() / n;
    double barWidth = qMin(22.0, slotWidth * 0.30);
    double barGap = 6;

    for (int i = 0; i < n; ++i) {
        const MonthlyIncomeSummary &m = m_data[i];
        double slotCenter = plot.left() + slotWidth * (i + 0.5);

        double expH = (m.expected / maxVal) * plot.height() * m_animProgress;
        double colH = (m.collected / maxVal) * plot.height() * m_animProgress;

        QRectF expRect(slotCenter - barWidth - barGap / 2, plot.bottom() - expH, barWidth, expH);
        QRectF colRect(slotCenter + barGap / 2, plot.bottom() - colH, barWidth, colH);

        auto drawBar = [&](const QRectF &rect, const QColor &color, const QString &tooltipLabel, double value) {
            if (rect.height() < 1) return;
            QPainterPath path;
            path.addRoundedRect(rect, 4, 4);
            painter.fillPath(path, color);
            m_hitRects.append({rect, QString("%1: $%2").arg(tooltipLabel).arg(value, 0, 'f', 2)});
        };

        drawBar(expRect, QColor("#4f5eff"), "Expected", m.expected);
        drawBar(colRect, QColor("#22c55e"), "Collected", m.collected);

        QDate d = QDate::fromString(m.month + "-01", "yyyy-MM-dd");
        QString monthLabel = d.isValid() ? d.toString("MMM") : m.month;
        painter.setPen(QColor("#64748b"));
        painter.drawText(QRectF(plot.left() + slotWidth * i, plot.bottom() + 8, slotWidth, 18),
                          Qt::AlignCenter, monthLabel);
    }
}

void IncomeChartWidget::mouseMoveEvent(QMouseEvent *event)
{
    QPointF pos = event->position();
    for (const BarHit &hit : m_hitRects) {
        if (hit.rect.contains(pos)) {
            QToolTip::showText(event->globalPosition().toPoint(), hit.tooltip, this);
            return;
        }
    }
    QToolTip::hideText();
}

void IncomeChartWidget::leaveEvent(QEvent *)
{
    QToolTip::hideText();
}
