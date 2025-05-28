// .pro: QT += serialport

#ifndef SERIAL_PORT_HPP
#define SERIAL_PORT_HPP

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QMutex>
#include <QThread>

#include <iostream>
#include <string>


class SerialPort;


class SerialPortSendThread : public QThread {
    Q_OBJECT

public:
    SerialPortSendThread(SerialPort *serial_port, QString data, bool data_type,
                         unsigned int intervals, unsigned int times,
                         bool &thread_flag, QObject *parent = nullptr);

    ~SerialPortSendThread() = default;

protected:
    void run() override {
        if (m_times == 0) {
            while (m_thread_flag) {
                QThread::msleep(m_intervals);

                emit signalSendData(m_data);
            }
        } else {  // m_times != 0
            for (unsigned int i = 0; i < m_times; i++) {
                if (!m_thread_flag) break;

                QThread::msleep(m_intervals);

                emit signalSendData(m_data);
            }
        }
    }

signals:
    void signalSendData(const QString &data);

private:
    QString m_data;
    unsigned int m_intervals;
    unsigned int m_times;

    bool &m_thread_flag;
};


class SerialPortSendMultiThread : public QThread {
    Q_OBJECT

public:
    SerialPortSendMultiThread(SerialPort *serial_port, QStringList data_list, bool data_type,
                         unsigned int intervals, unsigned int times,
                         bool &thread_flag, QObject *parent = nullptr);

    ~SerialPortSendMultiThread() = default;

protected:
    void run() override {
        if (m_times == 0) {
            while (m_thread_flag) {
                QThread::msleep(m_intervals);

                if ((m_data_no += 1) == m_data_list.size()) m_data_no = 0;

                emit signalSendData(m_data_list.at(m_data_no));
            }
        } else {  // m_times != 0
            for (unsigned int i = 0; i < m_times; i++) {
                if (!m_thread_flag) break;

                QThread::msleep(m_intervals);

                if ((m_data_no += 1) == m_data_list.size()) m_data_no = 0;

                emit signalSendData(m_data_list.at(m_data_no));
            }
        }
    }

signals:
    void signalSendData(const QString &data);

private:
    QStringList m_data_list;
    int m_data_no;
    unsigned int m_intervals;
    unsigned int m_times;

    bool &m_thread_flag;
};


class SerialPort : public QObject {
    Q_OBJECT

public:
    explicit SerialPort(QObject *parent = nullptr)
        : QObject(parent),
          m_send_thread_flag(true) {}

    SerialPort(const QString &port_name,
               QSerialPort::BaudRate baud_rate = QSerialPort::Baud115200,
               QSerialPort::Parity parity = QSerialPort::NoParity,
               QSerialPort::DataBits data_bits = QSerialPort::Data8,
               QSerialPort::StopBits stop_bits = QSerialPort::OneStop,
               QSerialPort::FlowControl flow_control = QSerialPort::NoFlowControl,
               QObject *parent = nullptr)
            : QObject(parent),
              m_send_thread_flag(true) {
        open(port_name, baud_rate, parity, data_bits, stop_bits, flow_control);
    }

    ~SerialPort() {
        close();
    }

public slots:
    void sendAsciiDataQ(const QString &ascii_data) {
        sendAsciiData(ascii_data.toStdString());
    }

    void sendHexDataQ(const QString &hex_data) {
        sendHexData(hex_data.toStdString());
    }

    void sendContinueAsciiDataQ(const QString &ascii_data, unsigned int intervals, unsigned int times = 0) {
        sendContinueAsciiData(ascii_data.toStdString(), intervals, times);
    }

    void sendContinueHexDataQ(const QString &hex_data, unsigned int intervals, unsigned int times = 0) {
        sendContinueHexData(hex_data.toStdString(), intervals, times);
    }

    void sendContinueMultiAsciiDataQ(const QStringList &ascii_data_list, unsigned int intervals, unsigned int times = 0) {
        if (!m_serial.isOpen()) return;

        if (ascii_data_list.size() == 0) return;

        m_send_thread_flag = true;

        SerialPortSendMultiThread *thread = new SerialPortSendMultiThread(this, ascii_data_list, false, intervals, times, m_send_thread_flag);

        connect(thread, &QThread::finished, thread, &QObject::deleteLater);

        thread->start();
    }

    void sendContinueMultiHexDataQ(const QStringList &hex_data_list, unsigned int intervals, unsigned int times = 0) {
        if (!m_serial.isOpen()) return;

        if (hex_data_list.size() == 0) return;

        m_send_thread_flag = true;

        SerialPortSendMultiThread *thread = new SerialPortSendMultiThread(this, hex_data_list, true, intervals, times, m_send_thread_flag);

        connect(thread, &QThread::finished, thread, &QObject::deleteLater);

        thread->start();
    }

    void stopSendContinue() {
        m_send_thread_flag = false;
    }

public:
    void open(const QString &port_name,
              QSerialPort::BaudRate baud_rate = QSerialPort::Baud115200,
              QSerialPort::Parity parity = QSerialPort::NoParity,
              QSerialPort::DataBits data_bits = QSerialPort::Data8,
              QSerialPort::StopBits stop_bits = QSerialPort::OneStop,
              QSerialPort::FlowControl flow_control = QSerialPort::NoFlowControl) {
        if (m_serial.isOpen()) {
            m_serial.close();
        }

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

    void close() {
        if (m_serial.isOpen()) {
            stopSendContinue();
            m_serial.close();
        }
    }

    bool isOpen() {
        return m_serial.isOpen();
    }

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

    void sendContinueAsciiData(const std::string &ascii_data, unsigned int intervals, unsigned int times = 0) {
        if (!m_serial.isOpen()) return;

        m_send_thread_flag = true;

        SerialPortSendThread *thread = new SerialPortSendThread(this, QString::fromStdString(ascii_data), false, intervals, times, m_send_thread_flag);

        connect(thread, &QThread::finished, thread, &QObject::deleteLater);

        thread->start();
    }

    void sendContinueHexData(const std::string &hex_data, unsigned int intervals, unsigned int times = 0) {
        if (!m_serial.isOpen()) return;

        m_send_thread_flag = true;

        SerialPortSendThread *thread = new SerialPortSendThread(this, QString::fromStdString(hex_data), true, intervals, times, m_send_thread_flag);

        connect(thread, &QThread::finished, thread, &QObject::deleteLater);

        thread->start();
    }

    void setReceiveEndCharCompleteCheck(QByteArray end_char) {
        m_end_char = end_char;
        m_use_end_char_flag = true;
    }

    void setReceiveNoneCompleteCheck() {
        m_use_end_char_flag = false;
    }

private slots:
    void onReadyRead() {
        QByteArray _data = m_serial.readAll();

        if (m_use_end_char_flag) {
            m_data_buffer.append(_data);
            while (existCompleteData()) {
                transferCompleteData();
            }
        } else {
            transferReceiveData(_data);
        }
    }

signals:
    void signalReceiveAsciiData(const std::string &data);

    void signalReceiveHexData(const std::string &data);

private:
    void transferReceiveData(const QByteArray &data) {
        // Hex Data
        std::string hex_data = data.toHex(' ').toUpper().toStdString();
        std::cout << "Serial port receive : " << hex_data << std::endl;
        emit signalReceiveHexData(hex_data);

        // Ascii Data
        std::string ascii_data = data.toStdString();
        std::cout << "Serial port receive : " << ascii_data << std::endl;
        emit signalReceiveAsciiData(ascii_data);
    }

    bool existCompleteData() {
        return m_data_buffer.contains(QByteArray::fromHex(m_end_char));
    }

    void transferCompleteData() {
        int end_index = m_data_buffer.indexOf(QByteArray::fromHex(m_end_char)) + m_end_char.size();
        QByteArray complete_data = m_data_buffer.left(end_index);

        m_data_buffer.remove(0, end_index);

        transferReceiveData(complete_data);
    }

private:
    QSerialPort m_serial;
    QMutex m_serial_mutex;

    bool m_send_thread_flag;

    bool m_use_end_char_flag;
    QByteArray m_data_buffer;
    QByteArray m_end_char;
};


#endif // SERIAL_PORT_HPP
