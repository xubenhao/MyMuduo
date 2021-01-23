#include "Acceptor.h"
#include "Logging.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "SocketOps.h"

Acceptor::Acceptor(
    EventLoop* loop, 
    const InetAddress& listenAddr,
    bool reuseport)
  : m_pLoop(loop),
    m_nAcceptSocket(
        createNonblockingOrDie(listenAddr.family())),
    m_nAcceptChannel(
        m_pLoop, 
        m_nAcceptSocket.fd()),
    m_bListenning(false),
    m_nIdleFd(::open("/dev/null", O_RDONLY | O_CLOEXEC))
{
  assert(m_nIdleFd >= 0);
  m_nAcceptSocket.setReuseAddr(true);
  m_nAcceptSocket.setReusePort(reuseport);
  m_nAcceptSocket.bindAddress(listenAddr);
  
  m_nAcceptChannel.setReadCallback(
      std::bind(
          &Acceptor::handleRead, 
          this));
}

Acceptor::~Acceptor()
{
  m_nAcceptChannel.disableAll();
  m_nAcceptChannel.remove();
  close(m_nIdleFd);
}

void Acceptor::listen()
{
  m_pLoop->assertInLoopThread();
  m_bListenning = true;
  m_nAcceptSocket.listen();
  m_nAcceptChannel.enableReading();
}

void Acceptor::handleRead()
{
  m_pLoop->assertInLoopThread();
  InetAddress peerAddr;
  int connfd = m_nAcceptSocket.accept(&peerAddr);
  if (connfd >= 0)
  {
    if (m_nNewConnectionCallback)
    {
      m_nNewConnectionCallback(
        connfd, 
        peerAddr);
    }
    else
    {
      close(connfd);
    }
  }
  else
  {
    LOG_SYSERR 
        << "in Acceptor::handleRead";
    // 连接失败：打开描述符个数过多
    if (errno == EMFILE)
    {
      ::close(m_nIdleFd);
      // 通过accept消除监听套接字的此刻的可读条件
      // 但拒绝连接建立．
      // 取出已经连接套接字－－对其执行close
      m_nIdleFd = ::accept(m_nAcceptSocket.fd(), NULL, NULL);
      ::close(m_nIdleFd);
      m_nIdleFd = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
    }
  }
}

