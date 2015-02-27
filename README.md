# my-unp-exercies 


"unix网络编程 vol-1" 中一些知识点已经过时(在当前的linux-kernal-3.2及之后的版本中), 所以在此做记录.

1). accept不会再因为信号中断

2). listen系统调用中backlog在目前的linux中指的是完全建立tcp连接的最大队列大小, 当队列满时, 其他外来的客户连接将被忽略. 而不发RST.

3). accpet在linux中不需要上锁.

4). 当前并发服务器, 进程池并发服务器比迭代更普遍.

5). sctp 并没有那么大的使用范围, 可以忽略.

6). streams 已经明确在APUE第3版中废弃了, 所以streams可以忽略.

7). linux目前更流行的多路转接(select/poll) I/O模型是 epoll
