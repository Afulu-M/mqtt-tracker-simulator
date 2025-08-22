#include "MainWindow.hpp"
#include "../../core/IClock.hpp"
#include "../../core/IRng.hpp"

#include <QApplication>
#include <QMenuBar>
#include <QStatusBar>
#include <QSplitter>
#include <QGridLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>

namespace tracker {
namespace qt {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_mqttClient(std::make_shared<PahoMqttClient>())
    , m_clock(std::make_shared<SystemClock>())
    , m_rng(std::make_shared<StandardRng>())
    , m_simulator(std::make_unique<Simulator>(m_mqttClient, m_clock, m_rng))
    , m_simulatorTimer(new QTimer(this))
{
    setupUI();
    loadConfiguration();
    
    connect(m_simulatorTimer, &QTimer::timeout, this, &MainWindow::onSimulatorTick);
    m_simulatorTimer->setInterval(1000); // 1 second
}

MainWindow::~MainWindow() {
    if (m_simulating) {
        m_simulator->stop();
    }
    saveConfiguration();
}

void MainWindow::setupUI() {
    setWindowTitle("MQTT Tracker Simulator");
    setMinimumSize(800, 600);
    
    m_tabWidget = new QTabWidget;
    setCentralWidget(m_tabWidget);
    
    // Connection Tab
    m_connectionTab = new QWidget;
    m_tabWidget->addTab(m_connectionTab, "Connection");
    
    auto* connLayout = new QFormLayout(m_connectionTab);
    
    m_hostEdit = new QLineEdit;
    m_hostEdit->setPlaceholderText("your-iot-hub.azure-devices.net");
    connLayout->addRow("IoT Hub Host:", m_hostEdit);
    
    m_deviceIdEdit = new QLineEdit;
    m_deviceIdEdit->setPlaceholderText("SIM-001");
    connLayout->addRow("Device ID:", m_deviceIdEdit);
    
    m_deviceKeyEdit = new QLineEdit;
    m_deviceKeyEdit->setEchoMode(QLineEdit::Password);
    m_deviceKeyEdit->setPlaceholderText("Base64 device key");
    connLayout->addRow("Device Key:", m_deviceKeyEdit);
    
    auto* connButtonLayout = new QHBoxLayout;
    m_connectButton = new QPushButton("Connect");
    m_disconnectButton = new QPushButton("Disconnect");
    m_disconnectButton->setEnabled(false);
    
    connButtonLayout->addWidget(m_connectButton);
    connButtonLayout->addWidget(m_disconnectButton);
    connButtonLayout->addStretch();
    
    connLayout->addRow(connButtonLayout);
    
    m_connectionStatus = new QLabel("Disconnected");
    m_connectionStatus->setStyleSheet("color: red; font-weight: bold;");
    connLayout->addRow("Status:", m_connectionStatus);
    
    connect(m_connectButton, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(m_disconnectButton, &QPushButton::clicked, this, &MainWindow::onDisconnectClicked);
    
    // Controls Tab
    m_controlsTab = new QWidget;
    m_tabWidget->addTab(m_controlsTab, "Controls");
    
    auto* controlsLayout = new QVBoxLayout(m_controlsTab);
    
    // Simulation control group
    auto* simGroup = new QGroupBox("Simulation Control");
    auto* simLayout = new QHBoxLayout(simGroup);
    
    m_startSimButton = new QPushButton("Start Simulation");
    m_stopSimButton = new QPushButton("Stop Simulation");
    m_stopSimButton->setEnabled(false);
    
    simLayout->addWidget(m_startSimButton);
    simLayout->addWidget(m_stopSimButton);
    simLayout->addStretch();
    
    controlsLayout->addWidget(simGroup);
    
    // Device control group
    auto* deviceGroup = new QGroupBox("Device Controls");
    auto* deviceLayout = new QFormLayout(deviceGroup);
    
    m_ignitionCheck = new QCheckBox("Ignition On");
    deviceLayout->addRow(m_ignitionCheck);
    
    m_speedSpin = new QDoubleSpinBox;
    m_speedSpin->setRange(0.0, 200.0);
    m_speedSpin->setSuffix(" km/h");
    deviceLayout->addRow("Speed:", m_speedSpin);
    
    m_batterySpin = new QSpinBox;
    m_batterySpin->setRange(0, 100);
    m_batterySpin->setValue(100);
    m_batterySpin->setSuffix("%");
    deviceLayout->addRow("Battery:", m_batterySpin);
    
    m_batteryProgress = new QProgressBar;
    m_batteryProgress->setRange(0, 100);
    m_batteryProgress->setValue(100);
    deviceLayout->addRow("Battery Level:", m_batteryProgress);
    
    controlsLayout->addWidget(deviceGroup);
    
    // Configuration group
    auto* configGroup = new QGroupBox("Configuration");
    auto* configLayout = new QFormLayout(configGroup);
    
    m_heartbeatSpin = new QSpinBox;
    m_heartbeatSpin->setRange(1, 3600);
    m_heartbeatSpin->setValue(60);
    m_heartbeatSpin->setSuffix("s");
    configLayout->addRow("Heartbeat Interval:", m_heartbeatSpin);
    
    m_speedLimitSpin = new QDoubleSpinBox;
    m_speedLimitSpin->setRange(10.0, 200.0);
    m_speedLimitSpin->setValue(90.0);
    m_speedLimitSpin->setSuffix(" km/h");
    configLayout->addRow("Speed Limit:", m_speedLimitSpin);
    
    controlsLayout->addWidget(configGroup);
    
    // Quick actions group
    auto* actionsGroup = new QGroupBox("Quick Actions");
    auto* actionsLayout = new QHBoxLayout(actionsGroup);
    
    m_driveButton = new QPushButton("Start Driving (10 min)");
    m_spikeButton = new QPushButton("Generate Spike (10 events)");
    
    actionsLayout->addWidget(m_driveButton);
    actionsLayout->addWidget(m_spikeButton);
    actionsLayout->addStretch();
    
    controlsLayout->addWidget(actionsGroup);
    
    m_simulationStatus = new QLabel("Simulation stopped");
    m_simulationStatus->setStyleSheet("color: red; font-weight: bold;");
    controlsLayout->addWidget(m_simulationStatus);
    
    controlsLayout->addStretch();
    
    // Connect control signals
    connect(m_startSimButton, &QPushButton::clicked, this, &MainWindow::onStartSimulationClicked);
    connect(m_stopSimButton, &QPushButton::clicked, this, &MainWindow::onStopSimulationClicked);
    connect(m_ignitionCheck, &QCheckBox::toggled, this, &MainWindow::onIgnitionToggled);
    connect(m_speedSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::onSpeedChanged);
    connect(m_batterySpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onBatteryChanged);
    connect(m_driveButton, &QPushButton::clicked, this, &MainWindow::onDriveClicked);
    connect(m_spikeButton, &QPushButton::clicked, this, &MainWindow::onSpikeClicked);
    
    // Events Tab
    m_eventsTab = new QWidget;
    m_tabWidget->addTab(m_eventsTab, "Events");
    
    auto* eventsLayout = new QVBoxLayout(m_eventsTab);
    
    auto* eventsSplitter = new QSplitter(Qt::Horizontal);
    
    m_eventLog = new QTextEdit;
    m_eventLog->setReadOnly(true);
    m_eventLog->setMaximumBlockCount(1000);
    eventsSplitter->addWidget(m_eventLog);
    
    m_jsonPreview = new QTextEdit;
    m_jsonPreview->setReadOnly(true);
    m_jsonPreview->setMaximumBlockCount(100);
    eventsSplitter->addWidget(m_jsonPreview);
    
    eventsSplitter->setSizes({400, 400});
    eventsLayout->addWidget(eventsSplitter);
    
    // Status bar
    statusBar()->showMessage("Ready");
}

void MainWindow::onConnectClicked() {
    if (m_hostEdit->text().isEmpty() || m_deviceIdEdit->text().isEmpty() || m_deviceKeyEdit->text().isEmpty()) {
        QMessageBox::warning(this, "Missing Information", "Please fill in all connection fields.");
        return;
    }
    
    SimulatorConfig config;
    config.iotHubHost = m_hostEdit->text().toStdString();
    config.deviceId = m_deviceIdEdit->text().toStdString();
    config.deviceKeyBase64 = m_deviceKeyEdit->text().toStdString();
    config.heartbeatSeconds = m_heartbeatSpin->value();
    config.speedLimitKph = m_speedLimitSpin->value();
    
    // Default route and geofences
    config.route = {
        {-26.2041, 28.0473},
        {-26.2000, 28.0500},
        {-26.1950, 28.0520},
        {-26.1920, 28.0480}
    };
    
    config.geofences = {
        {"office", -26.2041, 28.0473, 100.0},
        {"warehouse", -26.1920, 28.0480, 150.0}
    };
    
    m_simulator->configure(config);
    
    // Set up connection callback
    m_mqttClient->setConnectionCallback([this](bool connected, const std::string& reason) {
        QMetaObject::invokeMethod(this, [this, connected, reason]() {
            updateConnectionStatus(connected);
            if (connected) {
                appendEventLog("Connected to IoT Hub");
            } else {
                appendEventLog(QString("Disconnected: %1").arg(QString::fromStdString(reason)));
            }
        });
    });
    
    appendEventLog("Connecting to IoT Hub...");
    m_simulator->start();
}

void MainWindow::onDisconnectClicked() {
    m_simulator->stop();
    updateConnectionStatus(false);
    appendEventLog("Disconnected from IoT Hub");
}

void MainWindow::onStartSimulationClicked() {
    if (!m_connected) {
        QMessageBox::warning(this, "Not Connected", "Please connect to IoT Hub first.");
        return;
    }
    
    m_simulating = true;
    m_simulatorTimer->start();
    updateSimulationStatus();
    appendEventLog("Simulation started");
}

void MainWindow::onStopSimulationClicked() {
    m_simulating = false;
    m_simulatorTimer->stop();
    updateSimulationStatus();
    appendEventLog("Simulation stopped");
}

void MainWindow::onIgnitionToggled() {
    if (m_simulating) {
        m_simulator->setIgnition(m_ignitionCheck->isChecked());
        appendEventLog(QString("Ignition %1").arg(m_ignitionCheck->isChecked() ? "ON" : "OFF"));
    }
}

void MainWindow::onSpeedChanged() {
    if (m_simulating) {
        m_simulator->setSpeed(m_speedSpin->value());
        appendEventLog(QString("Speed set to %1 km/h").arg(m_speedSpin->value()));
    }
}

void MainWindow::onBatteryChanged() {
    if (m_simulating) {
        m_simulator->setBatteryPercentage(m_batterySpin->value());
        m_batteryProgress->setValue(m_batterySpin->value());
        appendEventLog(QString("Battery set to %1%").arg(m_batterySpin->value()));
    }
}

void MainWindow::onDriveClicked() {
    if (m_simulating) {
        m_simulator->startDriving(10.0);
        appendEventLog("Started driving simulation (10 minutes)");
    }
}

void MainWindow::onSpikeClicked() {
    if (m_simulating) {
        m_simulator->generateSpike(10);
        appendEventLog("Generated event spike (10 events)");
    }
}

void MainWindow::onSimulatorTick() {
    if (m_simulating) {
        m_simulator->tick();
    }
}

void MainWindow::updateConnectionStatus(bool connected) {
    m_connected = connected;
    m_connectionStatus->setText(connected ? "Connected" : "Disconnected");
    m_connectionStatus->setStyleSheet(connected ? "color: green; font-weight: bold;" : "color: red; font-weight: bold;");
    
    m_connectButton->setEnabled(!connected);
    m_disconnectButton->setEnabled(connected);
    
    statusBar()->showMessage(connected ? "Connected to IoT Hub" : "Disconnected from IoT Hub");
}

void MainWindow::updateSimulationStatus() {
    m_simulationStatus->setText(m_simulating ? "Simulation running" : "Simulation stopped");
    m_simulationStatus->setStyleSheet(m_simulating ? "color: green; font-weight: bold;" : "color: red; font-weight: bold;");
    
    m_startSimButton->setEnabled(!m_simulating);
    m_stopSimButton->setEnabled(m_simulating);
    
    // Enable/disable controls based on simulation state
    m_ignitionCheck->setEnabled(m_simulating);
    m_speedSpin->setEnabled(m_simulating);
    m_batterySpin->setEnabled(m_simulating);
    m_driveButton->setEnabled(m_simulating);
    m_spikeButton->setEnabled(m_simulating);
}

void MainWindow::appendEventLog(const QString& message) {
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    m_eventLog->append(QString("[%1] %2").arg(timestamp, message));
}

void MainWindow::loadConfiguration() {
    QSettings settings;
    
    m_hostEdit->setText(settings.value("connection/host").toString());
    m_deviceIdEdit->setText(settings.value("connection/deviceId", "SIM-001").toString());
    m_heartbeatSpin->setValue(settings.value("config/heartbeat", 60).toInt());
    m_speedLimitSpin->setValue(settings.value("config/speedLimit", 90.0).toDouble());
    
    restoreGeometry(settings.value("window/geometry").toByteArray());
    restoreState(settings.value("window/state").toByteArray());
}

void MainWindow::saveConfiguration() {
    QSettings settings;
    
    settings.setValue("connection/host", m_hostEdit->text());
    settings.setValue("connection/deviceId", m_deviceIdEdit->text());
    settings.setValue("config/heartbeat", m_heartbeatSpin->value());
    settings.setValue("config/speedLimit", m_speedLimitSpin->value());
    
    settings.setValue("window/geometry", saveGeometry());
    settings.setValue("window/state", saveState());
}

} // namespace qt
} // namespace tracker

#include "MainWindow.moc"