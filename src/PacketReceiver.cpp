#include "PacketReceiver.h"
#include <QUdpSocket>
#include <QNetworkInterface>
#ifndef Q_OS_WIN
#include <sys/socket.h>
#endif
#include <chrono>

using Clock = std::chrono::steady_clock;

static QNetworkInterface interfaceForAddress(const QHostAddress &addr) {
    const auto ifaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : ifaces) {
        const auto entries = iface.addressEntries();
        for (const QNetworkAddressEntry &entry : entries) {
            if (entry.ip() == addr)
                return iface;
        }
    }
    return QNetworkInterface();
}

PacketReceiver::PacketReceiver(QObject *parent) : QThread(parent) {}

void PacketReceiver::configure(const QHostAddress &group, quint16 port, const QHostAddress &iface) {
    m_group = group;
    m_port  = port;
    m_iface = iface;
}

void PacketReceiver::requestStop() {
    m_stop = true;
}

void PacketReceiver::run() {
    m_stop = false;

    QUdpSocket socket;

    // Bind to any address on the port so multicast datagrams are delivered
    if (!socket.bind(QHostAddress(QHostAddress::AnyIPv4), m_port,
                     QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        return;
    }

    // Maximise receive buffer
    {
        int bufSize = 8 * 1024 * 1024;
        qintptr fd = socket.socketDescriptor();
        ::setsockopt(static_cast<int>(fd), SOL_SOCKET, SO_RCVBUF,
                     reinterpret_cast<const char*>(&bufSize), sizeof(bufSize));
    }

    // Join multicast group if the destination is a multicast address
    bool anyIface  = m_iface.isNull() || m_iface == QHostAddress(QHostAddress::AnyIPv4);
    QNetworkInterface ni;
    if (!anyIface)
        ni = interfaceForAddress(m_iface);

    bool isMulticast = m_group.isMulticast();
    if (isMulticast) {
        if (ni.isValid())
            socket.joinMulticastGroup(m_group, ni);
        else
            socket.joinMulticastGroup(m_group);
    }

    QByteArray  buf(65536, 0);
    quint64     lastSeq    = 0;
    quint64     firstSeq   = 0;
    quint64     lostTotal  = 0;
    bool        isFirst    = true;

    quint64 recvCount  = 0;   // packets since session start (no gaps)
    quint64 sampleN    = 0;   // counter for rate measurement window
    int     lastSize   = 0;
    double  measRate   = 0.0;
    QString fromAddr;

    auto prevTime  = Clock::now();
    auto lastEmit  = Clock::now();

    while (!m_stop) {
        if (!socket.waitForReadyRead(100))  // 100 ms timeout → check m_stop
            goto maybeEmit;

        while (socket.hasPendingDatagrams() && !m_stop) {
            QHostAddress sender;
            quint16 senderPort;
            qint64 sz = socket.readDatagram(buf.data(), buf.size(), &sender, &senderPort);

            if (sz < static_cast<qint64>(sizeof(quint32)))
                continue;

            fromAddr = sender.toString();
            lastSize = static_cast<int>(sz);

            quint32 seq = *reinterpret_cast<const quint32*>(buf.constData());

            if (isFirst) {
                isFirst   = false;
                firstSeq  = seq;
                lastSeq   = seq;
                lostTotal = 0;
                recvCount = 0;
            } else if (seq > lastSeq) {
                lostTotal += (seq - lastSeq) - 1;
                lastSeq    = seq;
                ++recvCount;
            } else {
                // Sequence went backwards → new sender session; reset tracking
                isFirst = true;
                continue;
            }

            ++sampleN;

            if (sampleN % 100 == 0) {
                auto now     = Clock::now();
                double elaps = std::chrono::duration<double>(now - prevTime).count();
                if (elaps > 0.0) {
                    double newR = (100.0 * lastSize * 8.0) / (elaps * 1e6);
                    measRate    = (measRate + newR) * 0.5;
                }
                prevTime = now;
                emit statsUpdated(recvCount, lostTotal, fromAddr, measRate);
                lastEmit = now;
                continue;
            }
        }

    maybeEmit:
        // Emit at least every 200 ms so the UI doesn't freeze at low rates
        {
            auto now = Clock::now();
            if (!isFirst &&
                std::chrono::duration<double>(now - lastEmit).count() >= 0.2) {
                emit statsUpdated(recvCount, lostTotal, fromAddr, measRate);
                lastEmit = now;
            }
        }
    }

    if (isMulticast) {
        if (ni.isValid())
            socket.leaveMulticastGroup(m_group, ni);
        else
            socket.leaveMulticastGroup(m_group);
    }
    socket.close();
}
