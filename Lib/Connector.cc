#include "Connector.h"
#include "Logging.h"
#include "Channel.h"
#include "EventLoop.h"
#include "SocketOps.h"

const int Connector::s_nMaxRetryDelayMs;
Connector::Connector(
    EventLoop* loop, 
    const InetAddress& serverAddr)
  : m_pLoop(loop),
    m_nServerAddr(serverAddr),
    m_bConnect(false),
    m_nState(s_nDisconnected),
    m_nRetryDelayMs(s_nInitRetryDelayMs)
{
  LOG_DEBUG 
      << "ctor[" 
      << this 
      << "]";
}

Connector::~Connector()
{
  LOG_DEBUG 
      << "dtor[" 
      << this << "]";
  assert(!m_pChannel);
}

void Connector::start()
{
  m_bConnect = true;
  m_pLoop->runInLoop(
          std::bind(
            &Connector::startInLoop, 
            this)); 
}

void Connector::startInLoop()
{
  m_pLoop->assertInLoopThread();
  assert(m_nState == s_nDisconnected);
  if (m_bConnect)
  {
    connect();
  }
  else
  {
    LOG_DEBUG 
        << "do not connect";
  }
}


void Connector::connect()
{
  int sockfd = createNonblockingOrDie(
          m_nServerAddr.family());
  int ret = ::connect(
          sockfd, 
          m_nServerAddr.getSockAddr());
  int savedErrno = (ret == 0) ? 0 : errno;
  switch (savedErrno)
  {
    case 0:
    case EINPROGRESS:
    case EINTR:
    case EISCONN:
      connecting(sockfd);
      break;

    case EAGAIN:
    case EADDRINUSE:
    case EADDRNOTAVAIL:
    case ECONNREFUSED:
    case ENETUNREACH:
      retry(sockfd);
      break;

    case EACCES:
    case EPERM:
    case EAFNOSUPPORT:
    case EALREADY:
    case EBADF:
    case EFAULT:
    case ENOTSOCK:
      LOG_SYSERR 
          << "connect error in Connector::startInLoop " 
          << savedErrno;
      close(sockfd);
      break;

    default:
      LOG_SYSERR 
          << "Unexpected error in Connector::startInLoop " 
          << savedErrno;
      close(sockfd);
      break;
  }
}

// 连接请求发出后，要做的是：
// 在事件循环中等待描述符可写
// 可写时执行回调
void Connector::connecting(int sockfd)
{
  setState(s_nConnecting);
  assert(!m_pChannel);
  // unique_ptr这里起到动态资源管理的作用
  // 只在发出连接后需要等待描述符可写事件这段事件需要一个Channel
  // 负责上述任务
  //
  // 在连接建立后／失败时／停止时，
  // 不需要执行上述任务的Channel对象
  // 通过reset，让之前存在的Channel对象自动销毁
  m_pChannel.reset(
    new Channel(
        m_pLoop, 
        sockfd));

  m_pChannel->setWriteCallback(
      std::bind(
          &Connector::handleWrite, 
          this)); 
  m_pChannel->setErrorCallback(
      std::bind(
          &Connector::handleError, 
          this)); 
  
  m_pChannel->enableWriting();
}

void Connector::handleWrite()
{
  LOG_INFO 
      << "Connector::handleWrite " 
      << m_nState;

  if (m_nState == s_nConnecting)
  {
    int sockfd = removeAndResetChannel();
    int err = getSocketError(sockfd);
    if (err)
    {
      LOG_WARN 
          << "Connector::handleWrite - SO_ERROR = "
          << err 
          << " " 
          << strerror_tl(err);
      retry(sockfd);
    }
    else if (isSelfConnect(sockfd))
    {
      LOG_WARN 
          << "Connector::handleWrite - Self connect";
      retry(sockfd);
    }
    else
    {
      setState(s_nConnected);
      if (m_bConnect)
      {
        m_nNewConnectionCallback(sockfd);
      }
      else
      {
        close(sockfd);
      }
    }
  }
  else
  {
    assert(m_nState == s_nDisconnected);
  }
}

void Connector::handleError()
{
  LOG_ERROR 
      << "Connector::handleError state=" 
      << m_nState;
  if (m_nState == s_nConnecting)
  {
    int sockfd = removeAndResetChannel();
    int err = getSocketError(sockfd);
    LOG_INFO 
        << "SO_ERROR = " 
        << err 
        << " " 
        << strerror_tl(err);
    retry(sockfd);
  }
}

void Connector::stop()
{
  m_bConnect = false;
  m_pLoop->queueInLoop(
    std::bind(
        &Connector::stopInLoop, 
        this));
}

void Connector::stopInLoop()
{
  m_pLoop->assertInLoopThread();
  if (m_nState == s_nConnecting)
  {
    setState(s_nDisconnected);
    int sockfd = removeAndResetChannel();
    retry(sockfd);
  }
}

int Connector::removeAndResetChannel()
{
  m_pChannel->disableAll();
  m_pChannel->remove();

  int sockfd = m_pChannel->fd();
  m_pLoop->queueInLoop(
    std::bind(
        &Connector::resetChannel, 
        this)); 
  return sockfd;
}

void Connector::resetChannel()
{
  m_pChannel.reset();
}

void Connector::restart()
{
  m_pLoop->assertInLoopThread();
  setState(s_nDisconnected);
  m_nRetryDelayMs = s_nInitRetryDelayMs;
  m_bConnect = true;
  startInLoop();
}

void Connector::retry(int sockfd)
{
  close(sockfd);
  setState(s_nDisconnected);
  if (m_bConnect)
  {
    LOG_INFO 
        << "Connector::retry - Retry connecting to " 
        << m_nServerAddr.toIpPort()   
        << " in " 
        << m_nRetryDelayMs 
        << " milliseconds. ";
    m_pLoop->runAfter(
        m_nRetryDelayMs/1000.0,
        std::bind(
            &Connector::startInLoop, 
            shared_from_this()));
    m_nRetryDelayMs = std::min(
        m_nRetryDelayMs * 2, s_nMaxRetryDelayMs);
  }
  else
  {
    LOG_DEBUG 
        << "do not connect";
  }
}

