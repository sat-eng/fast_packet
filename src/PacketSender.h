#pragma once
#include <QThread>
#include <QHostAddress>
#include <atomic>

// Sends UDP packets at a target data rate on a background thread.
// Timing uses a chrono spin-wait (same principle as the original QueryPerformanceCounter
// approach) so the inter-packet gap is as accurate as the OS allows without sleep jitter.
class PacketSender : public QThread {
    Q_OBJECT
public:
    explicit PacketSender(QObject *parent = nullptr);

    void configure(const QHostAddress &dest, quint16 port,
                   const QHostAddress &iface, double dataRateMbps,
                   int pktSize, bool randomPayload);
    void requestStop();

signals:
    void dataRateUpdated(double mbps);      // measured Mbit/s
    void delayNsUpdated(quint64 ns);        // current inter-packet delay

protected:
    void run() override;

private:
    std::atomic<bool> m_stop{false};
    QHostAddress m_dest;
    quint16      m_port{105};
    QHostAddress m_iface;
    double       m_dataRateMbps{2.0};
    int          m_pktSize{1427};
    bool         m_randomPayload{false};
};
