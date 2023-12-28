// .pro: QT += network

#ifndef TCP_CLIENT_HPP
#define TCP_CLIENT_HPP

#include <QObject>
#include <QTcpSocket>
#include <QHostAddress>

#include <iostream>
#include <string>


class TcpClient : public QObject {
    Q_OBJECT

public:
    TcpClient(QObject *parent = nullptr)
            : QObject(parent) {
        connect(&m_client, &QTcpSocket::connected, this, &TcpClient::onConnected);
        connect(&m_client, &QTcpSocket::readyRead, this, &TcpClient::onReadyRead);
        connect(&m_client, &QTcpSocket::disconnected, this, &TcpClient::onDisconnected);
        connect(&m_client, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
                this, &TcpClient::onSocketError);
    }

    ~TcpClient() = default;

public slots:
    void connectToServer(const QString &server_name, unsigned short port) {
        m_client.connectToHost(server_name, port);
        m_server_info = server_name.toStdString() + ":" + std::to_string(port);

        std::cout << "Connecting to tcp server: " << m_server_info << std::endl;
    }

    void connectToServer(const QHostAddress &server_address, unsigned short port) {
        m_client.connectToHost(server_address, port);
        m_server_info = server_address.toString().toStdString() + ":" + std::to_string(port);

        std::cout << "Connecting to tcp server: " << m_server_info << std::endl;
    }

    void sendAsciiData(const QByteArray &ascii_data) {
        m_client.write(ascii_data);

        std::cout << "Tcp send [ " << m_server_info << " ] : " << ascii_data.constData() << std::endl;
    }

    void sendHexData(const QByteArray &hex_data) {
        QByteArray _data = QByteArray::fromHex(hex_data);
        m_client.write(_data);

        std::cout << "Tcp send [ " << m_server_info << " ] : " << hex_data.constData() << std::endl;
    }

    void disconnectFromServer() {
        m_client.disconnectFromHost();
    }

signals:
    void signalReceiveAsciiData(const std::string &data);

    void signalReceiveHexData(const std::string &data);

private slots:
    void onConnected() {
        std::cout << "Connected to tcp server: " << m_server_info << std::endl;
    }

    void onReadyRead() {
        QByteArray _data = m_client.readAll();

        // Hex Data
        std::string hex_data = _data.toHex(' ').toUpper().toStdString();
        std::cout << "Tcp receive [ " << m_server_info << " ] : " << hex_data << std::endl;
        emit signalReceiveHexData(hex_data);

        // Ascii Data
        std::string ascii_data = _data.toStdString();
        std::cout << "Tcp receive [ " << m_server_info << " ] : " << ascii_data << std::endl;
        emit signalReceiveAsciiData(ascii_data);
    }

    void onDisconnected() {
        std::cout << "Disconnected from tcp server: " << m_server_info << std::endl;

        m_client.disconnect();
    }

    void onSocketError(QAbstractSocket::SocketError socketError) {
        std::cout << "Tcp socket error: " << socketError << std::endl;
    }

private:
    QTcpSocket m_client;
    std::string m_server_info;
};


#endif // TCP_CLIENT_HPP
