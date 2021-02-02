# MyMuduo－对Muduo库进行代码整理
ａ．删除命名空间保护让代码简洁易读

ｂ．统一更改变量命名，

类的成员变量均为m_开头．

变量命名采用首字符大写．

ｃ．添加充分的代码注释，功能注释

ｄ．重新组织结构，将几个Tcp相关文件提取到Tcp文件夹，其余统一放在Lib

ｅ．对Lib和Tcp分别添加

header.h--用于包含此模块所需各个头文件，

lib.h--用于统一导出模板需要提供给外部的头文件，这样外部模块要使用模块时，只需包含导入模块的lib.h一个头文件即可．

# Muduo功能架构总结

１．一个EventLoop的loop独占一个线程

EventLoop本身构成了此线程执行的语境

２．利用EventLoop内的epoll描述符，

事件循环可以对一个或多个描述符进行监听

监听的事件可以指定

事件循环还支持任务回调，其他可访问到EventLoop的线程，

可向其任务回调写入回调来提交任务．

３．每个描述符提供给epoll监听时允许绑定一个自定义数据

Muduo里每个描述符绑定一个指向Channel对象的指针，

此Channel对象构成了每个描述符的语境．

通过Channel语境可以提供描述符

当前与epoll的关系／要监听的事件／产生的事件／事件回调／．．．

４．一个TcpClient代表一个客户端

提供的接口connect/disconnect/stop允许触发

套接字创建／连接请求发起／连接关闭／．．．

５．Muduo的事件循环既监听多个描述符，

在事件发生时触发事件回调

又支持任务回调．

这样要求事件循环监听的每个描述符／处理的每个任务回调

必须在一个短的时间内完成事件处理／任务回调，

这样才能保证高响应性．

防止阻塞于一个处理，导致后续处理，延迟较大才响应情况．

为此，监听的描述符需要是非阻塞描述符

处理的任务回调需要是可以是快速处理完毕的任务回调

６．非阻塞客户端流程：

ａ．创建套接字＋发起连接请求

ｂ．创建套接字语境Channel＋事件循环监听套接字可读

ｃ．处理可读事件，

处理结果是产生一个TcpConnecton对象代表一个已经连接套接字的语境

ｄ．已经连接套接字，需要有其Channel语境＋事件循环监听套接字可读

对套接字可读的监听，在连接建立期间一直如此

仅在有要写到套接字的内容存储于缓冲区时，才需要监听其可写．

且在可写事件中，写入缓冲区数据后，若无数据等待写入，

则关闭对其可写监视

ｅ．连接关闭

主动发起时，

待收到对端的FIN，会触发读事件

读事件处理中，读０字节表示收到对端FIN，可关闭连接

执行清理－－删除在epoll中对此描述符监视／回收套接字的语境Channel

７．非阻塞服务端流程：

ａ．创建套接字＋开始监听

ｂ．创建监听套接字语境Channel＋事件循环监听套接字可读

ｃ．处理可读事件，

处理结果是产生一个TcpConnection对象代表一个已经连接套接字语境

ｄ．已经连接套接字，需要有其Channel语境＋事件循环监听套接字可读

对套接字可读的监听，在连接建立期间一直如此

仅在有要写到套接字的内容存储于缓冲区时，才需要监听其可写．

且在可写事件中，写入缓冲区数据后，若无数据等待写入，

则关闭对其可写监视

ｅ．连接关闭

主动发起时，

待收到对端的FIN，会触发读事件

读事件处理中，读０字节表示收到对端FIN，可关闭连接

执行清理－－删除在epoll中对此描述符监视／回收套接字的语境Channel

服务器对象可以保存所有存在的已经连接套接字及其他信息

实现管理

８．服务端的多线程模型

监听套接字依附的事件循环所在的线程，负责监听

服务器通过接口启动多个并行线程－－即线程池

监听套接字可读处理时，

产生的每个已经连接套接字按负载均衡方式分配到线程池中线程

９．已经连接套接字的应用层缓冲区

每个已经连接套接字在应用层分别有一个发送缓冲区和接收缓冲区

ａ．发送缓冲区用在要向套接字写入内容，

但套接字目前不可写，无法完成写入时，

这时，由于用了非阻塞套接字，让写入请求以轮询来等待是低效的．

Muduo的处理时，把无法立即写入的写入到发送缓冲区．

并开启对套接字可写的监听，

在可写事件处理中，进行发送缓冲区内容写入．

若发送缓冲区在可写事件处理中全部写入了，

关闭对套接字可写监听．

［要不然，会出现，没数据可写，而可写事件持续被触发情形］

ｂ．接收缓冲区无条件用在套接字可读事件处理中

由于TCP是字节流的，

每次可读，读出的是无法保证的一定数量的字节流，

要向使用者提交读到的内容时，

要保证

必须读到完整的一条消息时才通知使用者．

所以需要将信息先存储到接收缓冲区，

再判断接收缓存区是否存在一条完整信息．

存在一条完整信息时，从接收缓冲区取出此信息，执行接收消息回调．

对于接收到的每条完整信息，

实际使用时，客户端／服务端还会先指定一个应用层协议．

双方依据协议来解释消息，确定消息边界．

# 事件处理＋非阻塞I/O下编程模式

一个完整的任务被拆分成多个事件

对每个事件处理时，可能需要结合语境信息作出相关处理，

然后，视情况决定是否触发下一阶段的事件监听

# Muduo所用的技术归纳

## 使用智能指针进行C++资源管理

１．使用智能指针－－shared_ptr<T>

每个shared_ptr<T>拥有一个其所管理的底层对象指针，

及该对象的引用计数．

在shared_ptr<T>对象拷贝构造／赋值时，

会对参与运算的对象的底层对象引用计数进行更新，

正确反映对其引用的shared_ptr<T>对象个数．

引用计数变为０时，底层对象被销毁．

注意点：

１．一旦使用shared_ptr<T>作为动态分配对象的生命期管理手段，

就应在new T()外的其他所有地方，

全部使用shared_ptr<T>作为对象访问的手段，不要使用对象原始指针．

２．在使用shared_ptr<T>调T的成员函数内部，

如有基于调用的shared_ptr<T>对象产生shared_ptr<T>的需求

应该：

ａ．T公共继承std::enable_shared_from_this<T>

ｂ．在需要基于调用shared_ptr<T>对象产生shared_ptr<T>处，

使用shared_from_this()

shared_from_this()将返回一个shared_ptr<T>对象，

该对象和调用基于的shared_ptr<T>

拥有相同的底层对象及引用计数对象．

２．使用智能指针－－weak_ptr<T>

一个weak_ptr<T>只能基于一个shared_ptr<T>对象来得到．

weak_ptr<T>对象拥有与基于的shared_ptr<T>对象

一样的底层对象及引用计数对象．

但特色是

ａ．weak_ptr<T>本身不能访问底层对象，

如要通过weak_ptr<T>访问底层对象，

先通过lock成员函数，返回shared_ptr<T>

在通过返回的shared_ptr<T>进行对底层对象的访问

ｂ．weak_ptr<T>虽然拥有与基于的shared_ptr<T>对象

一样的底层对象，引用计数对象．

但基于shared_ptr<T>得到weak_ptr<T>对象过程中，

不会导致引用计数对象＋１．


在一些你需要记录shared_ptr<T>拥有的底层对象，以便后续访问．

但同时，不想因此而导致底层对象因为你的记录，而无法销毁的场合，

weak_ptr<T>非常适合．

weak_ptr<T>后续使用时，可检查其包含的底层对象是否还有效，

有效，则访问．无效，则不能访问．

记录底层对象但不干涉对象的生命期管理是weak_ptr<T>存在的意义．

３．使用智能指针－－unique_ptr<T>

unique_ptr<T>存在的意义在于，

unique_ptr<T>所关联的底层对象只能被一个unique_ptr<T>对象所拥有．

不存在，

shared_ptr<T>的多个shared_ptr<T>对象共享同一底层对象，

及引用计数对象因此大于１的情况．


## 基于std::function和std::bind的回调

std::bind接收回调函数，允许预先设置回调参数．

返回std::function．

std::function的返回值和std::bind绑定的函数的返回值类型一致．

std::function的参数列表由std::bind绑定的函数的参数列表中

未预先设置的参数构成．


std::function对象可向普通的类对象一样

执行拷贝构造／拷贝赋值

若std::function通过std::bind得到

且std::bind绑定了实参，

实参会随着std::function的拷贝而被拷贝

## 多线程编程

－　线程特定数据

１．传统方式

pthread_once让一个回调仅在首次执行到pthread_once时被调用一次．

在回调中通过pthread_key_create得到一个供所有线程使用的key

每个线程通过此key结合pthread_getspecific／pthread_setspecific

来获取或设置线程特定数据．

２．新方式

__thread修饰全局变量．

被修饰的变量，每个线程有自己独立的一份．

每个线程通过同一变量名访问全局变量，

实际访问的是线程各自的版本，

多个线程间版本互不干扰．

__thread修饰对象要求

１．用于修饰POD类型，不能修饰class类型

２．可用于修饰全局变量，函数内静态变量，

不能修饰函数的局部变量或class的普通成员变量

３．__thread变量的初始化只能用编译期常量

－　获得线程id

#include <sys/syscall.h>

#define gettid() syscall(__NR_gettid)

或

pid_t gettid()

{

return static_cast<pid_t>(::syscall(SYS_gettid));

}



主线程id和进程id一致．

多个线程id彼此互不相同．

－　线程间互斥

通过线程间共享的互斥锁对象

每个线程对共享数据访问时，

对共享的互斥锁对象上锁

访问结束，

释放锁

可保证多线程间对共享数据的有序访问

－　线程间同步

互斥锁＋条件变量

通过多个线程共享的一个互斥锁＋条件变量对象．

每个线程对共享数据访问按以下模式



加锁

while(操作条件检测不满足)

{

  释放锁＋阻塞等待在条件变量

}


操作

if(操作导致某些条件得到满足)

{
  
  给条件关联的条件变量发信号

}


释放锁



１．阻塞在条件变量等待的线程，收到信号时，

重新加锁，再返回．

若收到信号时，无法立即加锁，阻塞等待，加锁成功，再返回．

２．给条件变量发信号时，

没有线程阻塞等待在此条件变量，

发的信号被系统忽略．不会被记录供后续等待使用．

## Linux下NonBlock I/O+I/O Multiplex

－　新一代的I/O Multiplex机制：使用epoll替代select/poll

１．相对于select和poll来说，epoll更加灵活，没有描述符限制。

epoll使用一个文件描述符管理多个描述符．

２．epoll的wait直接返回发生的事件集合

避免select/poll下对值－结果参数的低效轮询．

３．epoll在注册描述符时，允许绑定自定义数据．

在返回事件时，自定义数据也返回．

给予事件处理提供语境支持，

利用语境，可增强事件处理功能／灵活性


int epoll_create(int size);

int epoll_ctl(
	int epfd, 
	int op, 
	int fd, 
	struct epoll_event *event);

int epoll_wait(
	int epfd, 
	struct epoll_event * events, 
	int maxevents, int timeout);
  
对epoll_ctl

第一个参数是epoll_create()的返回值，

第二个参数表示动作，

用三个宏来表示：

EPOLL_CTL_ADD：注册新的fd到epfd中；

EPOLL_CTL_MOD：修改已经注册的fd的监听事件；

EPOLL_CTL_DEL：从epfd中删除一个fd；

第三个参数是需要监听的fd，

第四个参数是告诉内核需要监听什么事，


struct epoll_event 

{

  __uint32_t events;  /* Epoll events */

  epoll_data_t data;  /* User data variable */

};


events可以是以下几个宏的集合：

EPOLLIN ：

表示对应的文件描述符可以读；

EPOLLOUT：

表示对应的文件描述符可以写；

EPOLLPRI：

表示对应的文件描述符有紧急的数据可读

EPOLLERR：

表示对应的文件描述符发生错误；

EPOLLHUP：

表示对应的文件描述符被挂断；

EPOLLET：

将EPOLL设为边缘触发(Edge Triggered)模式，

这是相对于水平触发(Level Triggered)来说的。

LT模式是默认模式，

LT下触发条件满足，每次执行epoll_wait都会得到事件通知

ET下，触发条件由不满足变为满足后，

首次执行epoll_wait会得到事件通知，

此后再次执行epoll_wait若上次通知未处理或未处理完，

导致上次触发条件在处理后仍然满足，

但此后的epoll_wait不会得到此事件的通知．

此模式下，通知的次数少了，

但每次通知下，必须处理干净．

且必须搭配非阻塞套接字使用．

EPOLLONESHOT：

只监听一次事件，当监听完这次事件之后，

如果还需要继续监听这个socket的话，

需要再次把这个socket加入到EPOLL队列里．

否则，就是注册一次描述符持续有效．

后续可以对已经注册的描述符执行修改／删除，但无法执行添加．

对epoll_wait

参数events用来从内核得到事件的集合，

maxevents告之内核这个events有多大，

参数timeout是超时时间

（毫秒，0会立即返回，-1将不确定，也有说法说是永久阻塞）。

该函数返回需要处理的事件数目，如返回0表示已超时。

－　Muduo对epoll的恰到好处的使用

１．epoll中注册的描述符都是非阻塞的．

这样有效避免，

虚假唤醒下，事件处理时，阻塞在一处．

而导致后续事件处理／事件监听因此而迟迟无法响应问题．

通过搭配非阻塞描述符，

可以始终保证每个事件的处理，在一个极短时间内完成．

从而保持epoll高响应时效性．

２．多线程环境下，

对epoll的使用全部集中在一个线程．

有效避免了，多线程同时使用epoll各种可能的混乱．

这需要利用

ａ．std::bind,std::function回调函数及回调参数保存机制．

ｂ．在C++成员函数调C的API

调用时，this指向对象构成调用时语境．

为了实现高实时性的响应，利用了

ａ．epoll处理的均是非阻塞描述符

ｂ．epoll监听的描述符中有一个专门用于及时唤醒

会在epoll所在this对象待处理回调集合非空时，被设置为可读．

从而导致，若epoll本来处于wait时，立即由于事件存在而返回．

Muduo如何实现这点：

ａ．一个事件循环对象ａ启动事件循环便独占一个线程ｔ．

循环执行：

epoll事件等待

事件处理

回调函数集合处理

ｂ．多个线程可获得事件循环ａ的指针

获得ａ指针的线程可以修改ａ的回调函数集合

这样在ａ的事件循环执行回调函数集合时，

通过在线程ｔ执行回调

而从而去与epoll交互

ｃ．在事件处理中，由于epoll事件允许绑定一个用户自定义数据

利用自定义数据也可实现，

事件处理时，与epoll的交互．

上述实现多线程下，对指定事件循环ａ的epoll始终是ａ所在的线程

在与其交互．

－　在C++成员函数中执行C的API调用的优势

成员函数总是可以获得this

this指向一个对象

对象本身可存储成员变量

对象及其成员变量构成了我们调C的API时候的一个语境．

这样在API返回，或调用前传参的时候，

我们可以基于调用发生时候的语境［this对象内容］

来获取我们需要的关联信息，作对应处理．

－　eventfd

eventfd包含一个由内核维护的64位无符号整型计数器，

创建eventfd时会返回一个文件描述符，

进程可对这个文件描述符进行read/write来读取/改变计数器的值，	

从而实现进程间通信。


int eventfd(unsigned int, int);

参数１：创建eventfd时它所对应的64位计数器的初始值；

参数２：eventfd文件描述符的标志

EFD_CLOEXEC／EFD_NONBLOCK／EFD_SEMAPHORE。


EFD_CLOEXEC

表示返回的eventfd文件描述符在fork后exec其他程序时

会自动关闭

EFD_NONBLOCK

设置返回的eventfd非阻塞；

EFD_SEMAPHORE

表示将eventfd作为一个信号量来使用。

１．读eventfd

ａ．read函数会从eventfd对应的64位计数器中

读取一个8字节的整型变量；

ｂ．read函数设置的接收buf的大小不能低于8个字节，

否则read函数会出错，errno为EINVAL;

ｃ．read函数返回的值是按小端字节序的；

ｄ．

如果eventfd设置了EFD_SEMAPHORE，

那么每次read就会返回1，并且让eventfd对应的计数器减一；

如果eventfd没有设置EFD_SEMAPHORE，

那么每次read就会直接返回计数器中的数值，read之后计数器就会置0。

不管是哪一种，当计数器为0时，

如果继续read，那么read就会阻塞

（如果eventfd没有设置EFD_NONBLOCK）

或者返回EAGAIN错误（如果eventfd设置了EFD_NONBLOCK）。

２．写eventfd

ａ．在没有设置EFD_SEMAPHORE的情况下，

write函数会将发送buf中的数据写入到eventfd对应的计数器中，

最大只能写入0xffffffffffffffff，否则返回EINVAL错误；

ｂ．

在设置了EFD_SEMAPHORE的情况下，

write函数相当于是向计数器中进行“添加”，

比如说计数器中的值原本是2，

如果write了一个3，那么计数器中的值就变成了5。

如果某一次write后，计数器中的值超过了0xfffffffffffffffe

那么write就会阻塞

直到另一个进程/线程从eventfd中进行了read

（如果write没有设置EFD_NONBLOCK），

或者返回EAGAIN错误（如果write设置了EFD_NONBLOCK）。

## 万能对象

－　boost::any

使用举例

#include <iostream>

#include <list>

#include <boost/any.hpp>

typedef std::list<boost::any> list_any; 

void fill_list(list_any& la) 

{
  la.push_back(10); 
 
  la.push_back(std::string("glemontree"));
}


void show_list(list_any& la) 

{
  
  list_any::iterator it;
  
  boost::any anyone;
  
  for (it = la.begin; it!= la.end(); ++it) 
  
  {
  
    anyone = *it;
    
    // C++中，
    
    // typeid用于返回指针或引用所指对象的实际类型，typeid是操作符，不是函数
    
    if (anyone.type() == typeid(int)) 
    
    {
      
      std::cout 
      
      	<< boost::any_cast<int>(*it) 
      	
	<< std::endl;
    
    } 
    
    else if (anyone.type() == typeid(std::string)) 
    
    {
    
    	std::cout 
      	
	  << boost::any_cast<std::string>(*it).c_str() 
      	
  	  << std::endl;
    
    }
  
  }

}
  

－　shared_ptr<void>

一个shared_ptr<void>可以基于任意T下的shared_ptr<T>对象，

执行拷贝构造，拷贝赋值．

拷贝构造／拷贝赋值发生时，依然可以

正确处理引用计数更新，指向同一底层对象

shared_ptr<void>析构时，

可以正确处理引用计数更新，

及引用计数为０时，对底层对象的析构处理．
  
## 用描述符事件模型处理定时器

timerfd是Linux为用户程序提供的一个定时器接口。

这个接口基于文件描述符，

通过文件描述符的可读事件进行超时通知，

因此可以配合select/poll/epoll等使用。


int timerfd_create(int clockid, int flags);

clockid：

CLOCK_REALTIME:

系统实时时间,随系统实时时间改变而改变,

即从UTC1970-1-1 0:0:0开始计时,

中间时刻如果系统时间被用户改成其他,

则对应的时间相应改变

CLOCK_MONOTONIC:

从系统启动这一刻起开始计时,不受系统时间被用户改变的影响

flags：

TFD_NONBLOCK(非阻塞模式)

TFD_CLOEXEC

（表示当程序执行exec函数时本fd将被系统自动关闭,表示不传递）

struct timespec 

{

	time_t tv_sec;

	long   tv_nsec;

};

struct itimerspec 

{

	struct timespec it_interval;

	struct timespec it_value;

};

int timerfd_settime(
	
	int fd, 
	
	int flags, 
	
	const struct itimerspec *new_value, 
	
	struct itimerspec *old_value);
  

设置新的超时时间，并开始计时,

能够启动和停止定时器;

fd: 

参数fd是timerfd_create函数返回的文件句柄

flags：

为1代表设置的是绝对时间

（TFD_TIMER_ABSTIME 表示绝对定时器）；

为0代表相对时间。

new_value: 

参数new_value指定定时器的超时时间以及超时间隔时间

old_value: 

如果old_value不为NULL, 

old_vlaue返回之前定时器设置的超时时间，


it_interval不为0则表示是周期性定时器。

it_value和it_interval都为0表示停止定时器

int timerfd_gettime(
	
	int fd, 
	
	struct itimerspec *curr_value);
  

timerfd_gettime()函数获取距离下次超时剩余的时间

curr_value.it_value 字段表示距离下次超时的时间，

如果该值为0，表示计时器已经解除

该字段表示的值永远是一个相对值，

无论TFD_TIMER_ABSTIME是否被设置

curr_value.it_interval 定时器间隔时间


uint64_t exp = 0;

read(fd, &exp, sizeof(uint64_t)); 


可以用read函数读取计时器的超时次数，

该值是一个8字节无符号的长整型

# 库的使用
接口与责任：

TcpClient:

１．设置客户端属性，如是否支持失败重连，重连次数，．．．

２．设置收到来自套接字数据的回调

－－典型使用

对数据进行分析，

若数据包含一个完整消息，则取出消息处理

若数据不包含一个完整消息，则忽略，等待后续达到一个消息时再在回调时进行处理

３．设置客户端连接建立／连接撤销回调

－－典型使用

连接建立时，存储作为参数传入的shared_ptr

连接销毁时，删除存储的shared_ptr

TcpServer：

１．设置服务端属性，如是否支持线程池，线程池数目，．．．．

２．设置收到来自负责的客户套接字数据的回调

－－典型使用

对数据进行分析，

若数据包含一个完整消息，则取出消息处理

若数据不包含一个完整消息，则忽略，等待后续达到一个消息时再在回调时进行处理

３．设置接收客户端连接／断开客户端连接回调

－－典型使用

连接建立时，存储作为参数传入的shared_ptr

连接销毁时，删除存储的shared_ptr

这里与TcpClient的３区别是

一个TcpClient其至多维护一个shared_ptr

一个TcpServer对与其连接的每个客户维护一个shared_ptr

TcpServer为了标识每个与其连接的TcpConnection

且实现标识与TcpConnection间快速的双向定位，

典型做法就是：

为每个TcpConnection分配id，以<id, shared_ptr>形式用红黑树[map]存储shared_ptr

对每个TcpConnection将其id设置为对象的语境

TcpConnection：

１．设置连接属性

２．通过接口给套接字发送数据

３．进行连接控制，如关闭连接／．．．
  
