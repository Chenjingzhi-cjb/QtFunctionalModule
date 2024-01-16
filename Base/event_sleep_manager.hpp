#ifndef EVENT_SLEEP_MANAGER_HPP
#define EVENT_SLEEP_MANAGER_HPP

#include <QObject>
#include <QTimer>


/* example

void funcExample0() {
    // ... 最开始的、不需要延时的操作

    // 构造该对象后，除了 最开始的、不需要延时的操作 外，
    // 该函数内后续的所有操作均需要使用 exec() 执行，否则会时序错乱
    EventLoopSleepManager manager(this);

    // ... 最开始的、不需要延时的操作

    // 延时 10s 执行
    manager.exec(10000, [=]() {
        std::cout << "111" << std::endl;
    });

    // 再延时 10s 执行
    manager.exec(10000, [=]() {
        std::cout << "222" << std::endl;
    });

    // 再延时 0s 执行，这种情况建议写到上一个 exec() 中
    manager.exec(0, [=]() {
        std::cout << "333" << std::endl;
    });
}

*/


class EventLoopSleepManager : public QObject {
    Q_OBJECT

public:
    explicit EventLoopSleepManager(QObject *parent)
        : m_parent(parent),
          m_sleep_time(0) {}

    ~EventLoopSleepManager() = default;

public:
    template <typename Func>
    void exec(int sleep_ms, Func func) {
        m_sleep_time += sleep_ms;
        QTimer::singleShot(m_sleep_time, m_parent, func);
    }

private:
    QObject *m_parent;
    int m_sleep_time;
};


#endif // EVENT_SLEEP_MANAGER_HPP
