#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include "../shared/rpcinterface.h"
#include "../shared/simplerpc/simplerpc.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

struct Pico2Pc;
struct Pc2Pico;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void readSerialData();
    void onPingButtonClicked();

private:
    void initRpcSystem();
    void logMessage(const QString& message);
    void printByteBuffer(QByteArray&data);
    
    Ui::MainWindow *ui;
    QSerialPort *serialPort;
    
    // RPC system
    simplerpc::RpcManager<>* rpcManager;
    Pico2Pc* pico2PcRpcServer;
    Pc2Pico* pc2PicoRpcClient;
};
#endif // MAINWINDOW_H
