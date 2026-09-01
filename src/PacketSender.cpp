#include "PacketSender.h"
#include <QUdpSocket>
#include <QNetworkInterface>
#include <QRandomGenerator>
#include <QVariant>
#ifndef Q_OS_WIN
#include <sys/socket.h>
#endif
#include <chrono>
#include <cmath>

using Clock = std::chrono::steady_clock;
using Ns    = std::chrono::nanoseconds;

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

PacketSender::PacketSender(QObject *parent) : QThread(parent) {}

void PacketSender::configure(const QHostAddress &dest, quint16 port,
                              const QHostAddress &iface, double dataRateMbps,
                              int pktSize, bool randomPayload) {
    m_dest          = dest;
    m_port          = port;
    m_iface         = iface;
    m_dataRateMbps  = dataRateMbps;
    m_pktSize       = pktSize;
    m_randomPayload = randomPayload;
}

void PacketSender::requestStop() {
    m_stop = true;
}

void PacketSender::run() {
    m_stop = false;

    QUdpSocket socket;

    // Bind to the selected local interface (or any if 0.0.0.0)
    bool anyIface = m_iface.isNull() || m_iface == QHostAddress(QHostAddress::AnyIPv4);
    socket.bind(anyIface ? QHostAddress(QHostAddress::AnyIPv4) : m_iface, 0);

    // Set multicast outbound interface and disable loopback
    if (!anyIface) {
        QNetworkInterface ni = interfaceForAddress(m_iface);
        if (ni.isValid())
            socket.setMulticastInterface(ni);
    }
    socket.setSocketOption(QAbstractSocket::MulticastLoopbackOption, QVariant(0));

    // Maximise send buffer (best-effort; OS may cap lower)
    {
        int bufSize = 8 * 1024 * 1024;
        qintptr fd = socket.socketDescriptor();
        ::setsockopt(static_cast<int>(fd), SOL_SOCKET, SO_SNDBUF,
                     reinterpret_cast<const char*>(&bufSize), sizeof(bufSize));
    }

    // Build packet buffer: first 4 bytes are the uint32 sequence number
    QByteArray buffer(m_pktSize, 0);
    if (m_randomPayload) {
        for (int i = 4; i < m_pktSize; ++i)
            buffer[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    }
    auto *seqPtr = reinterpret_cast<quint32*>(buffer.data());
    *seqPtr = 0;

    // Target inter-packet gap in nanoseconds: t = (pktBits / dataRateBps)
    const double bitsPerPkt  = m_pktSize * 8.0;
    const double dataRateBps = m_dataRateMbps * 1e6;
    qint64 targetNs = static_cast<qint64>((bitsPerPkt / dataRateBps) * 1e9);

    constexpr int kBatch = 50;
    double measuredRate = m_dataRateMbps;

    auto batchStart = Clock::now();
    auto lastEmit   = Clock::now();

    while (!m_stop) {
        for (int i = 0; i < kBatch && !m_stop; ++i) {
            auto pktStart = Clock::now();

            socket.writeDatagram(buffer, m_dest, m_port);
            ++(*seqPtr);

            // Spin-wait for the inter-packet interval
            if (targetNs > 0) {
                auto deadline = pktStart + Ns(targetNs);
                while (Clock::now() < deadline && !m_stop) { /* spin */ }
            }
        }

        // Measure actual throughput over the batch
        auto batchEnd = Clock::now();
        double elapsed = std::chrono::duration<double>(batchEnd - batchStart).count();
        if (elapsed > 0.0) {
            double newRate = (static_cast<double>(kBatch) * m_pktSize * 8.0) / (elapsed * 1e6);
            measuredRate = (measuredRate + newRate) * 0.5;
        }
        batchStart = batchEnd;

        // Adjust delay: if measured > required, increase gap (slow down) and vice-versa
        double diff = std::abs(m_dataRateMbps - measuredRate);
        if (diff > 0.05 * m_dataRateMbps && measuredRate > 0.0) {
            // targetNs scales inversely with rate; multiply by (measured/required)
            targetNs = static_cast<qint64>(static_cast<double>(targetNs)
                                           * (measuredRate / m_dataRateMbps));
            if (targetNs < 0) targetNs = 0;
        }

        // Rate-limit signal emissions to ~5 Hz to avoid flooding the UI
        auto now = Clock::now();
        if (std::chrono::duration<double>(now - lastEmit).count() >= 0.2) {
            emit dataRateUpdated(measuredRate);
            emit delayNsUpdated(static_cast<quint64>(targetNs));
            lastEmit = now;
        }
    }

    socket.close();
}
