#include "TimerWheel.h"
#include "TcpConnection.h"

#include <cassert>

Entry::~Entry(){
    TcpConnectionPtr conn = conn_.lock();
    // 若连接已关闭或过期，无需处理
    if (conn){
    conn->shutdown();
    }
}

//构造函数，初始化超时时间和时间轮
TimerWheel::TimerWheel(size_t idleSeconds)
    : idleSeconds_(idleSeconds)
{
    assert(idleSeconds_ > 0);
    //初始化时间轮，idleSeconds_表示时间轮的格数，每格代表1秒，并且每格都有一个空的Bucket
    wheel_.assign(idleSeconds_, Bucket());
}

TimerWheel::~TimerWheel() = default;

void TimerWheel::onTimerTick(){
    // 推进时间轮
    wheel_.push_back(Bucket());
    // move进行强制类型转换，wheel_.front()所有权由expired接管，不使用 "=" 避免深拷贝增加cpu开销
    Bucket expired = std::move(wheel_.front());
    wheel_.pop_front();
    // expired 容器析构时 Entry 会析构并检查连接自身状态
    // (void)expired 显式标记未使用，避免编译器警告
    (void)expired;
}

void TimerWheel::updateConnection(const TcpConnectionPtr& conn){
    //连接关闭直接返回
    if (!conn) return;

    int64_t now = time(nullptr); // 获取当前秒数

    // 屏蔽同一秒的重复入桶，复用已有的Entry对象，避免性能问题
    std::any& ctx = conn->getContext();
    if (auto* pWeakEntry = std::any_cast<std::weak_ptr<Entry>>(&ctx)) {
        if (auto pEntry = pWeakEntry->lock()){
            // 过滤同一秒的重复请求，避免频繁入桶导致性能问题
            if (pEntry->lastTickTime() == now) return; 

            // 如果是这一秒第一次见它，更新它的时间戳，然后复用它入桶
            pEntry->setLastTickTime(now);
            wheel_.back().insert(pEntry);
            return;
        }
    }

    /**
     * 下面是第一次建立连接时需要make_shared一个Entry对象入桶的情况
     */
    EntryPtr entry = std::make_shared<Entry>(conn);
    entry->setLastTickTime(now);
    // 保存到连接上下文
    std::weak_ptr<Entry> weakEntry(entry);
    conn->setContext(weakEntry);
    // 放到当前时间轮尾部的桶内，表示这个连接刚刚有活动，重置了超时时间
    wheel_.back().insert(entry);
}
