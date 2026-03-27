#pragma once

#include <deque>
#include <unordered_set>
#include <memory>
#include <any>

class TcpConnection;
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;

class Entry : public std::enable_shared_from_this<Entry>{
public:
    explicit Entry(const TcpConnectionPtr& conn)
        : conn_(conn)
        , lastTickTime_(0)
    {}
    ~Entry();
    std::weak_ptr<TcpConnection> getConnection() const { return conn_; }

    //获取上次入桶时间
    int64_t lastTickTime() const { return lastTickTime_; }
    // 更新入桶时间
    void setLastTickTime(int64_t now) { lastTickTime_ = now; }
private:
    std::weak_ptr<TcpConnection> conn_;
    int64_t lastTickTime_;//记录上次更新时间，单位为秒
};

using EntryPtr = std::shared_ptr<Entry>;
using Bucket = std::unordered_set<EntryPtr>;
using Wheel = std::deque<Bucket>;

class TimerWheel {
public:
    explicit TimerWheel(size_t idleSeconds);
    ~TimerWheel();

    // 每秒调用一次，进轮一格
    void onTimerTick();

    // 连接有活动时，更新其Entry到尾桶
    void updateConnection(const TcpConnectionPtr& conn);

private:
    Wheel wheel_;
    size_t idleSeconds_;
};
