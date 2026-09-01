#include "LossGraph.h"
#include <QPainter>
#include <QPen>
#include <QBrush>

LossGraph::LossGraph(QWidget *parent) : QWidget(parent) {
    setMinimumSize(300, 180);
}

QSize LossGraph::sizeHint() const {
    return {420, 260};
}

void LossGraph::addSample(int lossPercent, quint64 lostThisInterval) {
    lossPercent = qBound(0, lossPercent, 100);
    if (m_loss.size() >= kMaxSamples) {
        m_loss.removeFirst();
        m_lostCounts.removeFirst();
    }
    m_loss.append(lossPercent);
    m_lostCounts.append(lostThisInterval);
    update();
}

void LossGraph::clear() {
    m_loss.clear();
    m_lostCounts.clear();
    update();
}

void LossGraph::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRect r = rect();
    const int marginL = 32, marginB = 20, marginT = 6, marginR = 6;
    const int gL = marginL;
    const int gR = r.width()  - marginR;
    const int gT = marginT;
    const int gB = r.height() - marginB;
    const int gW = gR - gL;
    const int gH = gB - gT;

    // Background
    p.fillRect(r, QColor(18, 18, 28));

    // Grid lines + Y-axis labels at 0 / 25 / 50 / 75 / 100 %
    p.setFont(QFont("monospace", 8));
    for (int pct : {0, 25, 50, 75, 100}) {
        int y = gB - pct * gH / 100;
        p.setPen(QPen(QColor(55, 55, 75), 1, Qt::DashLine));
        p.drawLine(gL, y, gR, y);
        p.setPen(QColor(140, 140, 155));
        p.drawText(0, y - 6, marginL - 2, 13, Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(pct));
    }

    // Axes
    p.setPen(QPen(QColor(160, 160, 180), 1));
    p.drawLine(gL, gT, gL, gB);
    p.drawLine(gL, gB, gR, gB);

    if (m_loss.isEmpty())
        return;

    const int n = m_loss.size();
    const double xStep = (n > 1) ? static_cast<double>(gW) / (n - 1) : gW;

    // Build polyline
    QPolygonF line;
    line.reserve(n);
    for (int i = 0; i < n; ++i)
        line << QPointF(gL + i * xStep, gB - m_loss[i] * gH / 100.0);

    // Filled area under the curve
    QPolygonF fill = line;
    fill.prepend({line.first().x(), static_cast<qreal>(gB)});
    fill.append({line.last().x(), static_cast<qreal>(gB)});
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(80, 210, 130, 45));
    p.drawPolygon(fill);

    // Loss line
    p.setPen(QPen(QColor(80, 210, 130), 2));
    p.setBrush(Qt::NoBrush);
    p.drawPolyline(line);

    // Highlight the last sample value
    if (!m_loss.isEmpty()) {
        int lastPct = m_loss.last();
        QColor dot = lastPct > 10 ? QColor(220, 80, 80) : QColor(80, 210, 130);
        p.setPen(Qt::NoPen);
        p.setBrush(dot);
        QPointF last = line.last();
        p.drawEllipse(last, 4.0, 4.0);
    }
}
