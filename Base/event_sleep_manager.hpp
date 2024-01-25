#ifndef EVENT_SLEEP_MANAGER_HPP
#define EVENT_SLEEP_MANAGER_HPP

#include <QObject>
#include <QTimer>


// TODO: 问题
// 此模块存在一个问题，每个代码块并非是同步延时的。例如，在 funcExample0() 中，第二个块的延时并非为
// 第一个块执行完成后的再延时，而是第一个块开始时的再延时，因此，第二个块的延时时间必须大于或者应该大幅大于
// 第一个块的执行时间。目前没有很好的办法，待解决。


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
