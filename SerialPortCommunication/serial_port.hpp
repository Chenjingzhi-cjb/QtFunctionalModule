// .pro: QT += serialport

#ifndef SERIAL_PORT_HPP
#define SERIAL_PORT_HPP

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QMutex>

#include <iostream>
#include <string>


class SerialPort : public QObject {
    Q_OBJECT

public:
    SerialPort(const QString &port_name,
               QSerialPort::BaudRate baud_rate = QSerialPort::Baud115200,
               QSerialPort::Parity parity = QSerialPort::NoParity,
               QSerialPort::DataBits data_bits = QSerialPort::Data8,
               QSerialPort::StopBits stop_bits = QSerialPort::OneStop,
               QSerialPort::FlowControl flow_control = QSerialPort::NoFlowControl,
               QObject *parent = nullptr)
            : QObject(parent) {
        m_serial.setPortName(port_name);
        if (m_serial.open(QIODevice::ReadWrite)) {
            std::cout << "The serial port opened successfully! "
                      << "Port: " << port_name.toStdString() << std::endl;

            m_serial.setBaudRate(baud_rate);
            m_serial.setParity(parity);
            m_serial.setDataBits(data_bits);
            m_serial.setStopBits(stop_bits);
            m_serial.setFlowControl(flow_control);

            connect(&m_serial, &QSerialPort::readyRead, this, &SerialPort::onReadyRead);
        } else {
            std::cout << "The serial port failed to open! "
                      << "Port: " << port_name.toStdString() << std::endl;
        }
    }

    ~SerialPort() {
        if (m_serial.isOpen()) {
            m_serial.close();
        }
    }

public:
    bool isOpen() {
        return m_serial.isOpen();
    };

    void sendAsciiData(const std::string &ascii_data) {
        QMutexLocker locker(&m_serial_mutex);

        if (m_serial.isOpen()) {
            QByteArray _data = QString::fromStdString(ascii_data).toUtf8();
            m_serial.write(_data);

            std::cout << "Serial port send : " << ascii_data << std::endl;
        }
    }

    void sendHexData(const std::string &hex_data) {
        QMutexLocker locker(&m_serial_mutex);

        if (m_serial.isOpen()) {
            QStringList hex_byte_list = QString::fromStdString(hex_data).split(" ");
            QByteArray _data;
            for (const QString &hex_byte : hex_byte_list) {
                char byte = static_cast<char>(hex_byte.toInt(nullptr, 16));
                _data.append(byte);
            }
            m_serial.write(_data);

            std::cout << "Serial port send : " << hex_data << std::endl;
        }
    }

private slots:
    void onReadyRead() {
        QByteArray _data = m_serial.readAll();

        // Hex Data
        std::string hex_data = _data.toHex(' ').toUpper().toStdString();
        std::cout << "Serial port receive : " << hex_data << std::endl;
        emit signalReceiveHexData(hex_data);

        // Ascii Data
        std::string ascii_data = _data.toStdString();
        std::cout << "Serial port receive : " << ascii_data << std::endl;
        emit signalReceiveAsciiData(ascii_data);
    }

signals:
    void signalReceiveAsciiData(const std::string &data);

    void signalReceiveHexData(const std::string &data);

private:
    QSerialPort m_serial;
    QMutex m_serial_mutex;
};


#endif // SERIAL_PORT_HPP
