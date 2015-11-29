# MIT6.824-2012spring - Yet Another File System (yfs)
yfs是MIT6.824分布式系统的课程项目，根据论文[Frangipani: A Scalable Distributed File System](https://pdos.csail.mit.edu/archive/6.824-2012/papers/thekkath-frangipani.pdf)，通过一系列实验构建出一个分布式文件系统。
七个实验分别为：  

**Lab 1 - Lock Server**  
**Lab 2 - Basic File Server**  
**Lab 3 - MKDIR, UNLINK, and Locking**  
**Lab 4 - Caching Lock Server**  
**Lab 5 - Caching Extent Server + Consistency**  
**Lab 6 - Paxos**  
**Lab 7 - Replicated lock server**  

# Lab 1 - Lock Server
## 简介
yfs服务器需要锁服务来协调对文件系统的更新，在lab1当中我们要实现这个锁服务。
锁服务的核心逻辑由两个模块组成：lock_client和lock_server，两个模块通过RPC(Remote Procedure Call Protocol，远程过程调用协议)来通信。一个client在请求某个锁时，会给lock_server发送一个acquire的请求，收到请求后lock_server会在某时刻把锁交给client。在client使用完锁之后，会发送release请求给lock_server来请求释放这个锁，以便其他的client可以请求这个锁。  
除此了实现锁服务之外，我们还要对提供的RPC库进行修改，通过消除重复的RPC请求来保证at-most-once execution。RPC在面对有loss的网络状况时，必须重传某些请求来弥补原始RPC请求的丢包，因此会产生重复的RPC请求。  
重复的RPC请求需要妥善处理，否则会导致逻辑上的错误。例如某个client发送acquire请求得到了某个锁x，然后他释放了x。在这之后原始acquire请求的重复请求到达lock_server，如果lock_server又把锁x给了client，但是由于client不会再次释放这个锁，那么其他请求锁x的client都不会得到它。
## 实现
### 第一部分
实现能够在完美网络条件（无丢包）下



# Lab 2 - Basic File Server
# Lab 3 - MKDIR, UNLINK, and Locking
# Lab 4 - Caching Lock Server
# Lab 5 - Caching Extent Server + Consistency
# Lab 6 - Paxos
# Lab 7 - Replicated lock server
