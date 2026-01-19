#define RPCMANAGER_STD_STRING
#include "../shared/simplerpc/simplerpc.h"
#include "../shared/rpcinterface.h"

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QSerialPortInfo>
#include <QThread>

using namespace simplerpc;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , serialPort(nullptr)
    , rpcManager(nullptr)
    , pico2PcRpcServer(nullptr)
    , pc2PicoRpcClient(nullptr) {
    ui->setupUi(this);
    
    // List available ports
    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
        qDebug() << "Name : " << info.portName();
        qDebug() << "Description : " << info.description();
        qDebug() << "Manufacturer: " << info.manufacturer();
    }

    // Create and configure serial port
    serialPort = new QSerialPort("COM6", this);
    serialPort->setBaudRate(QSerialPort::Baud115200);
    serialPort->setDataBits(QSerialPort::Data8);
    serialPort->setParity(QSerialPort::NoParity);
    serialPort->setStopBits(QSerialPort::OneStop);
    serialPort->setFlowControl(QSerialPort::NoFlowControl);
    
    if(!serialPort->open(QIODevice::ReadWrite)){
        qInfo() << "Cannot open the port:" << serialPort->errorString();
        exit(1);
        return;
    }
    // Set DTR and RTS signals to signal device we're ready
    serialPort->setDataTerminalReady(true);
    serialPort->setRequestToSend(true);
    qInfo() << "Port opened successfully on COM3 at 115200 baud";

    // Connect the readyRead signal to our slot
    connect(serialPort, &QSerialPort::readyRead, this, &MainWindow::readSerialData);

    // Initialize RPC system
    initRpcSystem();

    // Connect button signals to slots
    connect(ui->pingButton, &QPushButton::clicked, this, &MainWindow::onPingButtonClicked);

    logMessage("Application started");
}

MainWindow::~MainWindow() {
    // Cleanup RPC
    if (rpcManager) {
        rpcManager->deregisterServer<Pico2Pc>();
        delete rpcManager;
    }
    
    if (pico2PcRpcServer) {
        delete pico2PcRpcServer;
    }
    
    if (pc2PicoRpcClient) {
        delete pc2PicoRpcClient;
    }
    
    if (serialPort && serialPort->isOpen()) {
        serialPort->close();
    }
    delete ui;
}

void MainWindow::initRpcSystem() {
    qInfo() << "Initializing RPC system...";
    
    // Create RPC manager
    rpcManager = new RpcManager<>();
    
    // Add filters to RPC manager (handles packet assembly/disassembly)
    rpcManager->addInputFilter(new StreamFramerInputFilter());
    rpcManager->addOutputFilter(new StreamFramerOutputFilter());
    
    // Set global timeout for all RPC calls (2 seconds)
    rpcManager->setDefaultTimeout(5000);
    
    // Setup transport layer: send framed data via Serial in 64-byte chunks
    rpcManager->setOnSendCallback([this](const char* data, int len) {
        if (data && len > 0 && serialPort && serialPort->isOpen()) {
            const int chunkSize = 64;
            int offset = 0;
            while (offset < len) {
                int bytesToSend = std::min(chunkSize, len - offset);
                serialPort->write(data + offset, bytesToSend);
                serialPort->flush();
                offset += bytesToSend;
            }
        } else {
            qWarning() << "Serial port not open, cannot send data";
        }
    });
    
    // Setup error callback
    rpcManager->onError([](int code, const char* msg) {
        qWarning() << "[PC RPC ERROR" << code << "]" << (msg ? msg : "");
    });
    
    // ---------------------------------------------
    // Register SERVER: Pico2Pc methods that Pico can call
    // ---------------------------------------------
    pico2PcRpcServer = new Pico2Pc();
    pico2PcRpcServer->ping = [](int val) -> int {
        qInfo() << "[PC SERVER] ping(" << val << ") called from Pico";
        return val;
    };
    
    pico2PcRpcServer->debugPrint = [this](std::string value) {
        qInfo() << "[PICO LOG]" << QString::fromStdString(value);
        logMessage(QString("[PICO] ") + QString::fromStdString(value));
    };
    
    rpcManager->registerServer(*pico2PcRpcServer);
    qInfo() << "Pico2Pc server registered";
    
    // ---------------------------------------------
    // Create CLIENT: Pc2Pico to call Pico's methods
    // ---------------------------------------------
    pc2PicoRpcClient = new Pc2Pico();
    *pc2PicoRpcClient = rpcManager->createClient<Pc2Pico>();
    qInfo() << "Pc2Pico client created";
    
    qInfo() << "RPC system initialized successfully!";
}

void MainWindow::printByteBuffer(QByteArray&data) {
    QString str = "";
    for (int i=0; i<data.size(); i++) {
        unsigned char byte = static_cast<unsigned char>(data.at(i));
        str += QString("%1 ").arg(byte, 2, 16, QChar('0')).toUpper();
    }
    logMessage("message received: " + str);
}

void MainWindow::readSerialData() {
    if (!serialPort)
        return;
    
    // Read all available data and forward to RPC manager
    QByteArray data = serialPort->readAll();
    if (data.size() > 0 && rpcManager) {
        rpcManager->processInput(data.constData(), data.size());
    }
}

void MainWindow::logMessage(const QString& message) {
    ui->logTextEdit->append(message);
}

void MainWindow::onPingButtonClicked() {
    if (!rpcManager || !pc2PicoRpcClient) {
        logMessage("[ERROR] RPC manager not initialized");
        return;
    }
    
    // Run RPC call in separate thread to avoid blocking UI thread
    QThread* thread = QThread::create([this]() {
        try {
            logMessage("[PC] Sending ping to Pico...");
            int response = pc2PicoRpcClient->ping(42);
            logMessage(QString("[PC] Received pong from Pico: %1 (expected 42)").arg(response));
            
            if (response != 42) {
                logMessage(QString("[PC ERROR] Expected 42 but got %1").arg(response));
            }
        } catch (const std::exception& e) {
            logMessage(QString("[ERROR] Ping failed: %1").arg(e.what()));
        }
    });
    
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}


