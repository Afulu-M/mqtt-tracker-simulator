#include <QApplication>
#include "MainWindow.hpp"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    app.setApplicationName("MQTT Tracker Simulator");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("Tracker Simulator");
    
    tracker::qt::MainWindow window;
    window.show();
    
    return app.exec();
}