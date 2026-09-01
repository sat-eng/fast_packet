#pragma once
#include <QWidget>
#include <QThread>

class QComboBox;
class QLineEdit;
class QLabel;
class QProgressBar;
class QPushButton;
class QCheckBox;
class QSpinBox;
class QDoubleSpinBox;
class PacketSender;
class PacketReceiver;
class LossGraph;

class MainWindow : public QWidget {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onTransmit();
    void onListen();
    void onStop();
    void onToggleGraph();
    void onPriorityChanged(int index);
    void onSenderDataRate(double mbps);
    void onSenderDelayNs(quint64 ns);
    void onReceiverStats(quint64 count, quint64 lost, const QString &fromAddr, double mbps);

private:
    void buildUi();
    void populateInterfaces();
    void setSessionActive(bool active);

    // Network / packet settings
    QComboBox      *m_ifaceCombo{};
    QLineEdit      *m_destEdit{};
    QSpinBox       *m_portSpin{};
    QSpinBox       *m_pktSizeSpin{};
    QDoubleSpinBox *m_dataRateSpin{};
    QCheckBox      *m_randomCheck{};

    // Status / stats
    QLabel       *m_statusLabel{};
    QLabel       *m_measRateLabel{};
    QLabel       *m_countCaptionLabel{};
    QLabel       *m_countLabel{};
    QLabel       *m_lostLabel{};
    QLabel       *m_fromLabel{};
    QProgressBar *m_lossBar{};

    // Controls
    QComboBox  *m_priorityCombo{};
    QPushButton *m_transmitBtn{};
    QPushButton *m_listenBtn{};
    QPushButton *m_stopBtn{};
    QPushButton *m_graphBtn{};

    LossGraph       *m_graph{};
    PacketSender    *m_sender{};
    PacketReceiver  *m_receiver{};

    bool    m_transmitActive{false};
    bool    m_listenActive{false};
    bool    m_graphShown{false};
    quint64 m_lastCount{0};
    quint64 m_lastLost{0};
};
