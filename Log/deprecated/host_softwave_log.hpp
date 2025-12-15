// .pro: QT += concurrent

#ifndef HOST_SOFTWAVE_LOG_HPP
#define HOST_SOFTWAVE_LOG_HPP

#include <QMutex>
#include <QMutexLocker>
#include <QThread>
#include <QtConcurrent>
#include <QFile>

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <deque>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <ctime>


#define LOG_LEVEL_ERROR "ERROR"
#define LOG_LEVEL_WARN "WARN"
#define LOG_LEVEL_INFO "INFO"
#define LOG_LEVEL_DEBUG "DEBUG"

// 级别：1 - "ERROR", 2 - "WARN", 3 - "INFO", 4 - "DEBUG"
enum class LogEnableLevel {
    None,
    Error,
    Warning,
    Info,
    Debug
};

#define LogInstance ProgramLog::getInstance()

#define Log_Info(msg) if ((int) LogEnableLevel::Info <= LogInstance.getEnableLevel()) LogInstance.log(msg, "default", LOG_LEVEL_INFO, __FILE__, __LINE__, __FUNCTION__, QThread::currentThreadId())
#define Log_Debug(msg) if ((int) LogEnableLevel::Debug <= LogInstance.getEnableLevel()) LogInstance.log(msg, "default", LOG_LEVEL_DEBUG, __FILE__, __LINE__, __FUNCTION__, QThread::currentThreadId())
#define Log_Warning(msg) if ((int) LogEnableLevel::Warning <= LogInstance.getEnableLevel()) LogInstance.log(msg, "default", LOG_LEVEL_WARN, __FILE__, __LINE__, __FUNCTION__, QThread::currentThreadId())
#define Log_Error(msg) if ((int) LogEnableLevel::Error <= LogInstance.getEnableLevel()) LogInstance.log(msg, "default", LOG_LEVEL_ERROR, __FILE__, __LINE__, __FUNCTION__, QThread::currentThreadId())

#define Log_Info_M(msg, module) if ((int) LogEnableLevel::Info <= LogInstance.getEnableLevel()) LogInstance.log(msg, module, LOG_LEVEL_INFO, __FILE__, __LINE__, __FUNCTION__, QThread::currentThreadId())
#define Log_Debug_M(msg, module) if ((int) LogEnableLevel::Debug <= LogInstance.getEnableLevel()) LogInstance.log(msg, module, LOG_LEVEL_DEBUG, __FILE__, __LINE__, __FUNCTION__, QThread::currentThreadId())
#define Log_Warning_M(msg, module) if ((int) LogEnableLevel::Warning <= LogInstance.getEnableLevel()) LogInstance.log(msg, module, LOG_LEVEL_WARN, __FILE__, __LINE__, __FUNCTION__, QThread::currentThreadId())
#define Log_Error_M(msg, module) if ((int) LogEnableLevel::Error <= LogInstance.getEnableLevel()) LogInstance.log(msg, module, LOG_LEVEL_ERROR, __FILE__, __LINE__, __FUNCTION__, QThread::currentThreadId())


class ProgramLog {
private:
    ProgramLog() = default;

    ~ProgramLog() {
        stopOutput();
    }

public:
    static ProgramLog &getInstance() {
        static ProgramLog instance;  // 局部静态变量，注意生命周期

        return instance;
    }

    void setEnableLevel(LogEnableLevel level) {
        m_enable_level = (int) level;
    }

    int getEnableLevel() const {
        return m_enable_level;
    }

    void startOutput(std::string output_path, int flush_interval_sec = 60) {
        QMutexLocker locker(&m_output_status_mutex);

        if (m_output_status == 0) {
            // 检查文件路径有效性
            QFile file(QString::fromStdString(output_path));
            if (!file.open(QIODevice::WriteOnly | QIODevice::Append)) {
                std::cerr << "ProgramLog::startOutput(): Cannot open or create file for writing - " << output_path << std::endl;
                return;
            }
            if (!file.permissions().testFlag(QFileDevice::WriteUser)) {
                std::cerr << "ProgramLog::startOutput(): Log file is not writable - " << output_path << std::endl;
                file.close();
                return;
            }
            file.close();

            m_log_output_cache.clear();
            m_output_status = 1;
            m_output_path = output_path;
            m_flush_interval_sec = flush_interval_sec;
            startFlushTimer();
        } else {
            std::cerr << "ProgramLog::startOutput(): A log output thread is already working!" << std::endl;
        }
    }

    void stopOutput() {
        QMutexLocker locker(&m_output_status_mutex);

        if (m_output_status == 1) {
            m_output_status = 2;

            {
                QMutexLocker locker(&m_flush_mutex);
                m_flush_wait_condition.wakeAll();  // 唤醒等待的线程
            }
        }
    }

    void log(const std::string &message, const std::string &module, const std::string &level, const char *file, int line, const char *func, Qt::HANDLE thread_id) {
        auto task = [this, message, module, level, file, line, func, thread_id]() {
            logTask(message, module, level, file, line, func, thread_id);
        };
        QtConcurrent::run(task);
    }

private:
    std::string getTime() {
        // 获取当前时间点
        auto now = std::chrono::system_clock::now();

        // 将时间点转换为 time_t 以便进行时间格式化
        std::time_t time_t_seconds = std::chrono::system_clock::to_time_t(now);

        // 将 time_t 转换为本地时间
        std::tm tm_buffer;
        localtime_s(&tm_buffer, &time_t_seconds);  // 线程安全

        // 使用 ostringstream 来构建字符串
        std::ostringstream oss;
        oss << std::put_time(&tm_buffer, "%a %b %d %H:%M:%S %Y");

        // 返回格式化的时间字符串
        return oss.str();
    }

    void logTask(const std::string &message, const std::string &module, const std::string &level, const char *file, int line, const char *func, Qt::HANDLE thread_id) {
        std::ostringstream oss;
        oss << getTime()
            << " [" << level << "]"
            << " [" << module << "]"
            << " [" << (unsigned int) thread_id << "]"
            << " [" << file << ":" << line << "]"
            << " [" << func << "] "
            << message;
        std::string log_string = oss.str();

        std::cout << log_string << std::endl;

        if (m_output_status == 1) {
            QMutexLocker locker(&m_log_cache_mutex);
            m_log_output_cache.emplace_back(log_string);
        }
    }

    void flush() {
        if (m_log_output_cache.empty()) return;

        QMutexLocker locker(&m_log_cache_mutex);

        std::ofstream log_file(m_output_path, std::ios_base::app);
        if (!log_file.is_open()) return;

        while (!m_log_output_cache.empty()) {
            log_file << m_log_output_cache.front() << std::endl;
            m_log_output_cache.pop_front();
        }

        log_file.close();
    }

    void startFlushTimer() {
        QtConcurrent::run([this]() {
            while (true) {
                {
                    QMutexLocker locker(&m_flush_mutex);
                    if (m_output_status == 1) {
                        m_flush_wait_condition.wait(&m_flush_mutex, m_flush_interval_sec * 1000);
                    }
                }

                if (m_output_status == 2) {
                    m_output_status = 0;
                    flush();
                    return;
                } else {  // actually m_output_status == 1
                    flush();
                }
            }
        });
    }

private:
    int m_enable_level = (int) LogEnableLevel::Debug;

    QMutex m_log_cache_mutex;
    std::deque<std::string> m_log_output_cache;  // 日志缓存

    QMutex m_flush_mutex;
    QWaitCondition m_flush_wait_condition;

    QMutex m_output_status_mutex;
    int m_output_status = 0;  // 0 - 未启动/停止, 1 - 启动, 2 - 停止中

    std::string m_output_path;
    int m_flush_interval_sec;  // 定时刷新间隔
};


#endif // HOST_SOFTWAVE_LOG_HPP
