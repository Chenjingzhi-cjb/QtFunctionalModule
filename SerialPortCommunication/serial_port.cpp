#include "serial_port.hpp"


SerialPortSendThread::SerialPortSendThread(SerialPort *serial_port, QString data, bool data_type,
                                           unsigned int intervals, unsigned int times,
                                           bool &thread_flag, QObject *parent)
        : QThread(parent),
          m_data(data),
          m_intervals(intervals),
          m_times(times),
          m_thread_flag(thread_flag) {
    if (data_type) {  // true - hex
        connect(this, &SerialPortSendThread::signalSendData, serial_port, &SerialPort::sendHexDataQ, Qt::QueuedConnection);
    } else {  // !data_type, false - ascii
        connect(this, &SerialPortSendThread::signalSendData, serial_port, &SerialPort::sendAsciiDataQ, Qt::QueuedConnection);
    }
}

SerialPortSendMultiThread::SerialPortSendMultiThread(SerialPort *serial_port, QStringList data_list, bool data_type,
                                           unsigned int intervals, unsigned int times,
                                           bool &thread_flag, QObject *parent)
        : QThread(parent),
          m_data_list(data_list),
          m_data_no(-1),
          m_intervals(intervals),
          m_times(times * data_list.size()),
          m_thread_flag(thread_flag) {
    if (data_type) {  // true - hex
        connect(this, &SerialPortSendMultiThread::signalSendData, serial_port, &SerialPort::sendHexDataQ, Qt::QueuedConnection);
    } else {  // !data_type, false - ascii
        connect(this, &SerialPortSendMultiThread::signalSendData, serial_port, &SerialPort::sendAsciiDataQ, Qt::QueuedConnection);
    }
}
