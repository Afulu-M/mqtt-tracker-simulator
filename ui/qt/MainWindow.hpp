#pragma once

#include <QMainWindow>
#include <QTimer>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QProgressBar>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QCheckBox>

#include "../../core/Simulator.hpp"
#include "../../net/mqtt/PahoMqttClient.hpp"
#include <memory>

namespace tracker {
namespace qt {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onStartSimulationClicked();
    void onStopSimulationClicked();
    void onIgnitionToggled();
    void onSpeedChanged();
    void onBatteryChanged();
    void onDriveClicked();
    void onSpikeClicked();
    void onSimulatorTick();
    
private:
    void setupUI();
    void updateConnectionStatus(bool connected);
    void updateSimulationStatus();
    void appendEventLog(const QString& message);
    void loadConfiguration();
    void saveConfiguration();
    
    // UI Components
    QTabWidget* m_tabWidget;
    
    // Connection tab
    QWidget* m_connectionTab;
    QLineEdit* m_hostEdit;
    QLineEdit* m_deviceIdEdit;
    QLineEdit* m_deviceKeyEdit;
    QPushButton* m_connectButton;
    QPushButton* m_disconnectButton;
    QLabel* m_connectionStatus;
    
    // Controls tab
    QWidget* m_controlsTab;
    QPushButton* m_startSimButton;
    QPushButton* m_stopSimButton;
    QCheckBox* m_ignitionCheck;
    QDoubleSpinBox* m_speedSpin;
    QSpinBox* m_batterySpin;
    QSpinBox* m_heartbeatSpin;
    QDoubleSpinBox* m_speedLimitSpin;
    QPushButton* m_driveButton;
    QPushButton* m_spikeButton;
    QProgressBar* m_batteryProgress;
    QLabel* m_simulationStatus;
    
    // Events tab
    QWidget* m_eventsTab;
    QTextEdit* m_eventLog;
    QTextEdit* m_jsonPreview;
    
    // Simulator components
    std::shared_ptr<IMqttClient> m_mqttClient;
    std::shared_ptr<IClock> m_clock;
    std::shared_ptr<IRng> m_rng;
    std::unique_ptr<Simulator> m_simulator;
    
    QTimer* m_simulatorTimer;
    bool m_connected = false;
    bool m_simulating = false;
};

} // namespace qt
} // namespace tracker