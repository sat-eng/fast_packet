#include "MainWindow.h"
#include "PacketSender.h"
#include "PacketReceiver.h"
#include "LossGraph.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QProgressBar>
#include <QNetworkInterface>
#include <QHostAddress>

// Maps priority combo index to QThread::Priority
static constexpr QThread::Priority kPriorities[] = {
    QThread::HighPriority,          // 0 – Above Normal
    QThread::LowPriority,           // 1 – Below Normal
    QThread::HighestPriority,       // 2 – Highest
    QThread::IdlePriority,          // 3 – Idle
    QThread::NormalPriority,        // 4 – Normal  (default)
    QThread::LowestPriority,        // 5 – Lowest
    QThread::TimeCriticalPriority,  // 6 – Time Critical
};
static constexpr int kPriorityCount = static_cast<int>(std::size(kPriorities));
static constexpr int kDefaultPriorityIdx = 4; // Normal

// ---------------------------------------------------------------------------

MainWindow::MainWindow(QWidget *parent) : QWidget(parent) {
    setWindowTitle("FastPacket 2.0");
    buildUi();
    populateInterfaces();

    m_sender   = new PacketSender(this);
    m_receiver = new PacketReceiver(this);

    connect(m_sender,   &PacketSender::dataRateUpdated, this, &MainWindow::onSenderDataRate);
    connect(m_sender,   &PacketSender::delayNsUpdated,  this, &MainWindow::onSenderDelayNs);
    connect(m_receiver, &PacketReceiver::statsUpdated,  this, &MainWindow::onReceiverStats);
}

MainWindow::~MainWindow() {
    if (m_transmitActive) { m_sender->requestStop();   m_sender->wait();   }
    if (m_listenActive)   { m_receiver->requestStop(); m_receiver->wait(); }
}

// ---------------------------------------------------------------------------

void MainWindow::buildUi() {
    // ── Left panel ───────────────────────────────────────────────────────────
    auto *left = new QWidget;
    auto *lv   = new QVBoxLayout(left);
    lv->setSpacing(8);

    // Network group
    {
        auto *grp  = new QGroupBox("Network");
        auto *grid = new QGridLayout(grp);

        grid->addWidget(new QLabel("Interface:"),   0, 0);
        m_ifaceCombo = new QComboBox;
        grid->addWidget(m_ifaceCombo, 0, 1);

        grid->addWidget(new QLabel("Destination:"), 1, 0);
        m_destEdit = new QLineEdit("225.10.100.100");
        grid->addWidget(m_destEdit, 1, 1);

        grid->addWidget(new QLabel("Port:"),        2, 0);
        m_portSpin = new QSpinBox;
        m_portSpin->setRange(1, 65535);
        m_portSpin->setValue(105);
        grid->addWidget(m_portSpin, 2, 1);

        lv->addWidget(grp);
    }

    // Packet group
    {
        auto *grp  = new QGroupBox("Packet");
        auto *grid = new QGridLayout(grp);

        grid->addWidget(new QLabel("Size (bytes):"),      0, 0);
        m_pktSizeSpin = new QSpinBox;
        m_pktSizeSpin->setRange(64, 65507);
        m_pktSizeSpin->setValue(1427);
        grid->addWidget(m_pktSizeSpin, 0, 1);

        grid->addWidget(new QLabel("Data Rate (Mbit/s):"), 1, 0);
        m_dataRateSpin = new QDoubleSpinBox;
        m_dataRateSpin->setRange(0.001, 10000.0);
        m_dataRateSpin->setDecimals(3);
        m_dataRateSpin->setValue(2.0);
        grid->addWidget(m_dataRateSpin, 1, 1);

        m_randomCheck = new QCheckBox("Random payload");
        grid->addWidget(m_randomCheck, 2, 0, 1, 2);

        lv->addWidget(grp);
    }

    // Action buttons
    {
        auto *row = new QHBoxLayout;
        m_transmitBtn = new QPushButton("Transmit");
        m_listenBtn   = new QPushButton("Listen");
        m_stopBtn     = new QPushButton("Stop");
        m_stopBtn->setEnabled(false);
        row->addWidget(m_transmitBtn);
        row->addWidget(m_listenBtn);
        row->addWidget(m_stopBtn);
        lv->addLayout(row);
    }

    // Status / stats group
    {
        auto *grp  = new QGroupBox("Status");
        auto *grid = new QGridLayout(grp);

        grid->addWidget(new QLabel("State:"),      0, 0);
        m_statusLabel = new QLabel("Ready");
        m_statusLabel->setStyleSheet("font-weight: bold;");
        grid->addWidget(m_statusLabel, 0, 1);

        grid->addWidget(new QLabel("Data Rate:"),  1, 0);
        m_measRateLabel = new QLabel("—");
        grid->addWidget(m_measRateLabel, 1, 1);

        m_countCaptionLabel = new QLabel("Packets:");
        grid->addWidget(m_countCaptionLabel, 2, 0);
        m_countLabel = new QLabel("—");
        grid->addWidget(m_countLabel, 2, 1);

        grid->addWidget(new QLabel("Lost:"),       3, 0);
        m_lostLabel = new QLabel("—");
        grid->addWidget(m_lostLabel, 3, 1);

        grid->addWidget(new QLabel("From:"),       4, 0);
        m_fromLabel = new QLabel("—");
        grid->addWidget(m_fromLabel, 4, 1);

        grid->addWidget(new QLabel("Loss %:"),     5, 0);
        m_lossBar = new QProgressBar;
        m_lossBar->setRange(0, 100);
        m_lossBar->setValue(0);
        grid->addWidget(m_lossBar, 5, 1);

        lv->addWidget(grp);
    }

    // Bottom row: priority + graph toggle
    {
        auto *row = new QHBoxLayout;
        row->addWidget(new QLabel("Thread Priority:"));

        m_priorityCombo = new QComboBox;
        m_priorityCombo->addItem("Above Normal");
        m_priorityCombo->addItem("Below Normal");
        m_priorityCombo->addItem("Highest");
        m_priorityCombo->addItem("Idle");
        m_priorityCombo->addItem("Normal");
        m_priorityCombo->addItem("Lowest");
        m_priorityCombo->addItem("Time Critical");
        m_priorityCombo->setCurrentIndex(kDefaultPriorityIdx);
        row->addWidget(m_priorityCombo);

        row->addStretch();

        m_graphBtn = new QPushButton(">>");
        m_graphBtn->setFixedWidth(36);
        row->addWidget(m_graphBtn);

        lv->addLayout(row);
    }

    // ── Graph panel (hidden until toggled) ───────────────────────────────────
    m_graph = new LossGraph;
    m_graph->setVisible(false);

    // ── Top-level layout ─────────────────────────────────────────────────────
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->addWidget(left);
    root->addWidget(m_graph);

    // Connections
    connect(m_transmitBtn,  &QPushButton::clicked, this, &MainWindow::onTransmit);
    connect(m_listenBtn,    &QPushButton::clicked, this, &MainWindow::onListen);
    connect(m_stopBtn,      &QPushButton::clicked, this, &MainWindow::onStop);
    connect(m_graphBtn,     &QPushButton::clicked, this, &MainWindow::onToggleGraph);
    connect(m_priorityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onPriorityChanged);

    adjustSize();
}

void MainWindow::populateInterfaces() {
    m_ifaceCombo->clear();
    m_ifaceCombo->addItem("0.0.0.0 (any)",
                          QVariant::fromValue(QHostAddress(QHostAddress::AnyIPv4)));
    int defaultIdx = 0;

    for (const QHostAddress &addr : QNetworkInterface::allAddresses()) {
        if (addr.protocol() != QAbstractSocket::IPv4Protocol) continue;
        if (addr.isLoopback()) continue;

        QString s = addr.toString();
        m_ifaceCombo->addItem(s, QVariant::fromValue(addr));

        // Prefer addresses that look like satellite/headend network ranges
        if (s.startsWith("10.9.") || s.startsWith("10.40.2."))
            defaultIdx = m_ifaceCombo->count() - 1;
    }
    m_ifaceCombo->setCurrentIndex(defaultIdx);
}

void MainWindow::setSessionActive(bool active) {
    m_ifaceCombo->setEnabled(!active);
    m_destEdit->setEnabled(!active);
    m_portSpin->setEnabled(!active);
    m_pktSizeSpin->setEnabled(!active);
    m_dataRateSpin->setEnabled(!active);
    m_randomCheck->setEnabled(!active);
    m_transmitBtn->setEnabled(!active);
    m_listenBtn->setEnabled(!active);
    m_stopBtn->setEnabled(active);
}

// ---------------------------------------------------------------------------

void MainWindow::onTransmit() {
    QHostAddress dest  = QHostAddress(m_destEdit->text().trimmed());
    QHostAddress iface = m_ifaceCombo->currentData().value<QHostAddress>();

    m_sender->configure(dest, static_cast<quint16>(m_portSpin->value()),
                        iface, m_dataRateSpin->value(),
                        m_pktSizeSpin->value(), m_randomCheck->isChecked());

    m_transmitActive = true;
    m_graph->clear();
    m_statusLabel->setText("Sending");
    m_countCaptionLabel->setText("Delay (ns):");
    m_lostLabel->setText("—");
    m_fromLabel->setText("—");
    setSessionActive(true);

    m_sender->start(kPriorities[m_priorityCombo->currentIndex()]);
}

void MainWindow::onListen() {
    QHostAddress group = QHostAddress(m_destEdit->text().trimmed());
    QHostAddress iface = m_ifaceCombo->currentData().value<QHostAddress>();

    m_receiver->configure(group, static_cast<quint16>(m_portSpin->value()), iface);

    m_listenActive = true;
    m_lastCount    = 0;
    m_lastLost     = 0;
    m_graph->clear();
    m_statusLabel->setText("Listening");
    m_countCaptionLabel->setText("Packets:");
    setSessionActive(true);

    m_receiver->start(kPriorities[m_priorityCombo->currentIndex()]);
}

void MainWindow::onStop() {
    if (m_transmitActive) {
        m_sender->requestStop();
        m_sender->wait();
        m_transmitActive = false;
    }
    if (m_listenActive) {
        m_receiver->requestStop();
        m_receiver->wait();
        m_listenActive = false;
    }
    m_statusLabel->setText("Ready");
    setSessionActive(false);
}

void MainWindow::onToggleGraph() {
    m_graphShown = !m_graphShown;
    m_graph->setVisible(m_graphShown);
    m_graphBtn->setText(m_graphShown ? "<<" : ">>");
    adjustSize();
}

void MainWindow::onPriorityChanged(int index) {
    if (index < 0 || index >= kPriorityCount) return;
    QThread::Priority prio = kPriorities[index];
    if (m_transmitActive && m_sender->isRunning())
        m_sender->setPriority(prio);
    else if (m_listenActive && m_receiver->isRunning())
        m_receiver->setPriority(prio);
}

// ---------------------------------------------------------------------------

void MainWindow::onSenderDataRate(double mbps) {
    m_measRateLabel->setText(QString::number(mbps, 'f', 2) + " Mbit/s");
}

void MainWindow::onSenderDelayNs(quint64 ns) {
    m_countLabel->setText(QString::number(ns) + " ns");
}

void MainWindow::onReceiverStats(quint64 count, quint64 lost,
                                  const QString &fromAddr, double mbps) {
    m_countLabel->setText(QString::number(count));
    m_lostLabel->setText(QString::number(lost));
    m_fromLabel->setText(fromAddr);
    m_measRateLabel->setText(QString::number(mbps, 'f', 2) + " Mbit/s");

    // Delta loss % for this interval → progress bar + graph sample
    quint64 deltaCount = count - m_lastCount;
    quint64 deltaLost  = lost  - m_lastLost;
    m_lastCount = count;
    m_lastLost  = lost;

    int lossPercent = 0;
    if (deltaCount > 0)
        lossPercent = static_cast<int>((deltaLost * 100) / deltaCount);

    m_lossBar->setValue(lossPercent);
    m_graph->addSample(lossPercent, deltaLost);
}
