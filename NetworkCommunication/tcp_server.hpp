// .pro: QT += network

#ifndef TCP_SERVER_HPP
#define TCP_SERVER_HPP

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>

#include <iostream>
#include <string>

#define clientPeerAddressString() client_socket->peerAddress().toString().toStdString()


class TcpServer : public QObject {
    Q_OBJECT

public:
    TcpServer(unsigned short port, const QHostAddress &address = QHostAddress::Any, QObject *parent = nullptr)
            : QObject(parent) {
        if (!m_server.listen(address, port)) {
            std::cout << "The tcp server failed to start! "
                      << "Port: " << m_server.serverPort() << std::endl;
            return;
        }
        std::cout << "The tcp server started successfully! "
                  << "Port: " << m_server.serverPort() << std::endl;

        connect(&m_server, &QTcpServer::newConnection, this, &TcpServer::onNewConnection);
    }

    ~TcpServer() = default;

public slots:
    void sendAsciiData(const QByteArray &ascii_data, const QHostAddress &address = QHostAddress::Null) {
        if (address.isNull()) {
            if (m_clients.size() == 1) {
                sendAsciiData(ascii_data, m_clients[0]);  // 单连接默认
            } else {
                return;
            }
        }

        QTcpSocket *client = findClientByAddress(address);
        sendAsciiData(ascii_data, client);
    }

    void sendAsciiData(const QByteArray &ascii_data, QTcpSocket *client_socket) {
        if (client_socket) {
            client_socket->write(ascii_data);

            std::cout << "Tcp send [ " << clientPeerAddressString() << " ] : "
                      << ascii_data.constData() << std::endl;
        }
    }

    void sendHexData(const QByteArray &hex_data, const QHostAddress &address = QHostAddress::Null) {
        if (address.isNull()) {
            if (m_clients.size() == 1) {
                sendHexData(hex_data, m_clients[0]);  // 单连接默认
            } else {
                return;
            }
        }

        QTcpSocket *client = findClientByAddress(address);
        sendHexData(hex_data, client);
    }

    void sendHexData(const QByteArray &hex_data, QTcpSocket *client_socket) {
        if (client_socket) {
            QByteArray _data = QByteArray::fromHex(hex_data);
            client_socket->write(_data);

            std::cout << "Tcp send [ " << clientPeerAddressString() << " ] : "
                      << hex_data.constData() << std::endl;
        }
    }

    void disconnectClient(const QHostAddress &address = QHostAddress::Null) {
        if (address.isNull()) {
            if (m_clients.size() == 1) {
                disconnectClient(m_clients[0]);  // 单连接默认
            } else {
                return;
            }
        }

        QTcpSocket *client = findClientByAddress(address);
        disconnectClient(client);
    }

    void disconnectClient(QTcpSocket *client_socket) {
        if (client_socket) {
            std::cout << "Disconnect tcp client: " << clientPeerAddressString() << std::endl;

            m_clients.removeOne(client_socket);
            client_socket->disconnect();
            client_socket->deleteLater();
        }
    }

public:
    QTcpSocket *findClientByAddress(const QHostAddress &address) {
        for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
            auto *client_socket = *it;
            if (client_socket->peerAddress().toIPv4Address() == address.toIPv4Address()) {
                return client_socket;
            }
        }

        return nullptr;
    }

signals:
    void signalReceiveAsciiData(const std::string &client_ip, const std::string &data);

    void signalReceiveHexData(const std::string &client_ip, const std::string &data);

private slots:
    void onNewConnection() {
        QTcpSocket *client_socket = m_server.nextPendingConnection();
        connectClientSignals(client_socket);
        m_clients.append(client_socket);

        std::cout << "New tcp connection from: " << clientPeerAddressString() << std::endl;
    }

    void onReadyRead() {
        auto *client_socket = qobject_cast<QTcpSocket *>(sender());  // sender(): 获取发送信号的对象
        if (!client_socket) return;

        QByteArray _data = client_socket->readAll();

        // Hex Data
        std::string hex_data = _data.toHex(' ').toUpper().toStdString();
        std::cout << "Tcp receive [ " << clientPeerAddressString() << " ] : "
                  << hex_data << std::endl;
        emit signalReceiveHexData(getIPv4FromPeerAddress(client_socket->peerAddress()), hex_data);

        // Ascii Data
        std::string ascii_data = _data.toStdString();
        std::cout << "Tcp receive [ " << clientPeerAddressString() << " ] : "
                  << ascii_data << std::endl;
        emit signalReceiveAsciiData(getIPv4FromPeerAddress(client_socket->peerAddress()), ascii_data);
    }

    void onClientDisconnected() {
        auto *client_socket = qobject_cast<QTcpSocket *>(sender());
        if (!client_socket) return;

        std::cout << "Disconnected from tcp client: " << clientPeerAddressString() << std::endl;

        m_clients.removeOne(client_socket);
        client_socket->disconnect();
        client_socket->deleteLater();
    }

private:
    void connectClientSignals(QTcpSocket *client_socket) {
        connect(client_socket, &QTcpSocket::readyRead, this, &TcpServer::onReadyRead);
        connect(client_socket, &QTcpSocket::disconnected, this, &TcpServer::onClientDisconnected);
    }

    static std::string getIPv4FromPeerAddress(const QHostAddress &peer_address) {
        // peerAddress(): return "IPv4-mapped IPv6 address" like "::ffff:192.168.1.102"
        // this method: return "IPv4 address"(std::string) like "192.168.1.102"
        return peer_address.toString().split(":").last().toStdString();
    }

private:
    QTcpServer m_server;
    QList<QTcpSocket *> m_clients;
};


#endif // TCP_SERVER_HPP
