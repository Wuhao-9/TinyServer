## LT模式与ET模式
* **LT (Level-trigger)水平触发**
  * 只要事件**存在**，就会触发事件
  * 如：只要缓冲区有数据，就会触发EPOLLIN事件
  * 如：只要缓冲区可写(未满)，就会触发EPOLLOUT事件

* **ET (Eage-trigger)边缘触发**
  * 每当事件**发生**一次，则会触发一次
  * 如：假设此时server只连接了一个client，client向server发送了数据，服务端的epoll实例会监测到本次**读事件**，但无论服务端是否有将缓冲区中的数据读完，**读事件将不会被再次触发**(即使缓冲区中还有未读完的数据)，此时server会阻塞在epoll_wait中，直到client再次向server发送数据，epoll实例会检测到此次(即第二次)读事件，进而会被唤醒，继续读缓冲区的数据(且会从上次未读完的地方开始读)
  * 简而言之：client向server发送了100次数据，server能且只能检测到100次EPOLLIN事件
  * 故ET模式一般需要**循环读取缓冲区中的数据**，由于缓冲区没数据时，调用read/recv函数会被阻塞，故**需要设置文件描述符为非阻塞**，由返回值和errno来判断返回状态
  * 一般而言ET模式比LT模式更加高效：ET模式会减少系统调用epoll_wait函数的调用次数，进而能避免多次的内核态与用户态的上下文切换等事件

## EPOLL-events
* EPOLLRDHUP:
  
    **server对accept返回的client描述符设置EPOLLRDHUP事件，可检测对方是否关闭了tcp连接(用于通信的文件描述符)**
    当对端关闭FD时，会发送FIN/RST报文，server的epoll实例会检测到一次EPOLLIN事件
  
    设置EPOLLRDHUP事件可区分此次EPOLLIN事件是对方发送的数据还是对方断开连接的请求/通知

    **当对方断开连接，epoll_event.events == EPOLLIN | EPOLLRDHUP**

    故只需将EPOLLRDHUP的判断放置在EPOLLIN事件判断之上，则可进行区分(与Exception类处理情况用一思路)
```
  当服务端调用close关闭于客户端通信的fd时：
  1. 当服务端通信socket的读缓冲区没有数据，则向clinet发送FIN报文(执行正常的四次挥手过程)

       * 若此时客户端调用read读socket的缓冲区，读到FIN报文时，函数返回值为0->表示对端断开连接，此时客户端也应当调用close关闭对应socket
       
       * 若此时客户端(第一次)调用write向socket的缓冲区写入数据，会成功返回(注意：因为此时只是将数据写入了内核的socket缓冲区，write/send只负责将数据写入内核socket缓冲区，写入成功后则系统调用成功返回）
  
         等到tcp协议发送数据给对方时，对方会返回一个RST报文以表示socket读写端已关闭，此时客户端收到RST会将自己的socket缓冲区关闭
         若客户端再次(第二次)向socket缓冲区write，则会产生SIGPIPE信号！

  2. 当服务端通信socket的读缓冲区有数据，则丢弃这些数据，并向clinet发送RST报文
       * clinet端此时若调用read读其对应socket缓冲区，函数会返回-1，并设置erron
       perror的结果为：Connection reset by peer
```
* EPOLLET：
  
  设置**当前文件描述符**触发事件模式为ET模式

* EPOLLONESHOT:
  
  若当前FD设置了EPOLLONESHOT事件，则该FD当有**目标事件**被检测触发，并从epoll_wait系统调用返回且被写入接收事件中时(程序员在用户态下所准备的epoll_event结构体数组，内核将就绪的事件与FD拷贝至该数组，用于通知程序哪些事件已经就绪)
  
  **之后的事件监测都不会再去监测该文件描述符，即申请的事件只会被监听一次，事件就绪后将不再监听该文件描述符**

  换而言之：当前描述符只能被检测一次，事件成功发生就会被移出检测集合，若想保持始终检测该描述符，则需要在处理完事件时，重新设置文件描述符至epoll实例
  
  * **注意：重新加入检测需要调用** `epoll_ctl (_epfd, EPOLL_CTL_MOD, target_fd, &event)`

    `epoll_ctl (_epfd, EPOLL_CTL_ADD, target_fd, &event)`无效！






  