// TcpClient
// 1.主动发起连接
// 2.连接建立后，
// ａ．接收输入
// ｂ．发送请求
// ｃ．接收应答
// ｄ．将应答作呈现
// ３．主动断开连接
// ４．被动断开连接
//
//
// TcpServer
// １．绑定通用端口并监听连接请求
// ２．执行连接返回一个额外的已经连接套接字
// ３．每个已经连接套接字采用独立线程进行
// ａ．接收请求
// ｂ．处理请求
// ｃ．发回响应
// ４．主动断开连接，终止线程
// ５．被动端口连接，终止线程
#ifndef NLIB_ACCEPTOR_H
#define NLIB_ACCEPTOR_H

#include "../Lib/lib.h"  
class EventLoop;
class InetAddress;
class Acceptor 
{
public:
    typedef std::function<void (
        int sockfd, 
        const InetAddress&)> NewConnectionCallback;
    Acceptor(
        EventLoop* loop, 
        const InetAddress& listenAddr, 
        bool reuseport);
    ~Acceptor();
    void setNewConnectionCallback(const NewConnectionCallback& cb)
    {
        m_nNewConnectionCallback = cb;
    }
    
    bool listenning() const 
    { 
        return m_bListenning; 
    }
    
    void listen();
    
private:
    void handleRead();
    EventLoop* m_pLoop;
    Socket m_nAcceptSocket;
    Channel m_nAcceptChannel;
    NewConnectionCallback m_nNewConnectionCallback;
    bool m_bListenning;
    int m_nIdleFd;
};

#endif
