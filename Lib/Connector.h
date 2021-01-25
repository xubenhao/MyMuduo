#ifndef NLIB_CONNECTOR_H
#define NLIB_CONNECTOR_H

#include "InetAddress.h"
class Channel;
class EventLoop;

// 负责处理发起连接及关联处理
// 连接由客户端发起：
// １．创建套接字＋发出Connect请求
// ２．依赖于事件循环的Loop监听套接字可写
// ３．套接字可写时执行回调＆取消事件循环的Loop对套接字的可写监听
//
//
// 传递给Connector对象的事件循环意义是
// 连接的发起＋套接字事件的监听＋回调执行全部在事件循环线程执行
// 创建Connector对象线程可提交请求，请求在传入的事件循环线程被执行
// 多线程共享同一Connector对象
// 通过std::bind绑定，各个线程执行回调时，获得this，
// 可在回调时，拥有所需的对象信息．
//
//
// Connector对象至少在创建此对象线程和传入事件循环线程两个线程内
// 均是可访问的．
// Connector对象本身对数据成员执行访问时并没有进行互斥锁保护
// 问：Connector对象各个数据成员如何做到多线程下安全＆有序？
// 答案：
// １．Connector的三个public函数可有创建／任何可访问此对象线程调用，
// 而此三个函数涉及的m_bConnect/m_nState/m_nRetryDelayMs
// 严格来说是可是施加多线程互斥保护的．
// ２．对于实际功能的执行及剩下的成员
// 由于通过m_pLoop->queueInLoop
// 可以使得要执行的功能调用转移到m_pLoop所在线程中进行
// 所有本对象的所有private成员函数，及相关成员
// 实际只在事件循环所在的单线程环境下执行，
// 这样，对这些函数及成员不需要施加多线程互斥／同步保护．
//
//
// Muduo的一个亮点：
// 一个事件循环拥有一个Poll对象
// 多个线程可借助此Poll对象进行描述符监听
// 但每个线程对此Poll的描述符监听集合变更／删除监听／Poll类成员的修改
// 通过m_pLoop->queueInLoop
// 将所有请求转移到拥有Poll的事件循环所在的线程执行．
//
//
// 这样使得多个线程使用Poll，
// 对Poll对象的实际操作全部集中在一个线程中进行．
// 有效避免了多线程并发使用同一Poll对象
// 造成的各种潜在问题．
class Connector : NonCopyable,
    public std::enable_shared_from_this<Connector>
{
public:
    enum States
    {
        s_nDisconnected,
        s_nConnecting,
        s_nConnected
    };

    static const int s_nMaxRetryDelayMs = 30*1000;
    static const int s_nInitRetryDelayMs = 500;

public:
    typedef std::function<void (int sockfd)> NewConnectionCallback;
    Connector(
        EventLoop* loop, 
        const InetAddress& serverAddr);
    ~Connector();
    
    void setNewConnectionCallback(
        const NewConnectionCallback& cb)
    {
        m_nNewConnectionCallback = cb;
    }

    const InetAddress& serverAddress() const
    {
        return m_nServerAddr;
    }

    void start();
    void restart();
    void stop();
        
private:
    void setState(States s)
    {
        m_nState = s;
    }
    
    void startInLoop();
    void stopInLoop();
    void connect();
    void connecting(int sockfd);
    void handleWrite();
    void handleError();
    void retry(int sockfd);
    int removeAndResetChannel();
    void resetChannel();

private:
    EventLoop* m_pLoop;
    InetAddress m_nServerAddr;
    bool m_bConnect;
    States m_nState;

    std::unique_ptr<Channel> m_pChannel;
    NewConnectionCallback m_nNewConnectionCallback;
    int m_nRetryDelayMs;
};

#endif

