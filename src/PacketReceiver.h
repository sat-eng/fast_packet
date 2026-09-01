#pragma once
#include <QThread>
#include <QHostAddress>
#include <atomic>

// Receives UDP packets on a background thread, tracks sequence-number gaps
// to count lost packets, and measures throughput every 100 received packets.
// Shutdown: call requestStop(); the 100 ms waitForReadyRead timeout lets the
// thread exit cleanly without needing to close the socket from outside.
class PacketReceiver : public QThread {
    Q_OBJECT
public:
    explicit PacketReceiver(QObject *parent = nullptr);

    void configure(const QHostAddress &group, quint16 port, const QHostAddress &iface);
    void requestStop();

signals:
    // Emitted every 100 packets (or up to every 200 ms at low rates).
    // count = packets received since session start, lost = cumulative lost.
    void statsUpdated(quint64 count, quint64 lost, const QString &fromAddr, double mbps);

protected:
    void run() override;

private:
    std::atomic<bool> m_stop{false};
    QHostAddress m_group;
    quint16      m_port{105};
    QHostAddress m_iface;
};
