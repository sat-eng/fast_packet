#pragma once
#include <QWidget>
#include <QVector>

// Scrolling line graph of packet-loss percentage (0–100 %) over time.
// Call addSample() from any thread; it is connected via queued signal in practice.
class LossGraph : public QWidget {
    Q_OBJECT
public:
    explicit LossGraph(QWidget *parent = nullptr);
    QSize sizeHint() const override;

public slots:
    void addSample(int lossPercent, quint64 lostThisInterval);
    void clear();

protected:
    void paintEvent(QPaintEvent *) override;

private:
    static constexpr int kMaxSamples = 200;
    QVector<int>     m_loss;
    QVector<quint64> m_lostCounts;
};
