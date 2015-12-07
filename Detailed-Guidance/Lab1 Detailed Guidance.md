# Lab 1 Detailed Guidance - Lock Server
## 简介
yfs服务器需要锁服务来协调对文件系统的更新，在lab1当中我们要实现这个锁服务。
锁服务的核心逻辑由两个模块组成：lock_client和lock_server，两个模块通过RPC(Remote Procedure Call Protocol，远程过程调用协议)来通信。一个client在请求某个锁时，会给lock_server发送一个acquire的请求，收到请求后lock_server会在某时刻把锁交给client。在client使用完锁之后，会发送release请求给lock_server来请求释放这个锁，以便其他的client可以请求这个锁。  
除此了实现锁服务之外，我们还要对提供的RPC库进行修改，通过消除重复的RPC请求来保证at-most-once execution。RPC在面对有loss的网络状况时，必须重传某些请求来弥补原始RPC请求的丢包，因此会产生重复的RPC请求。  
重复的RPC请求需要妥善处理，否则会导致逻辑上的错误。例如某个client发送acquire请求得到了某个锁x，然后他释放了x。在这之后原始acquire请求的重复请求到达lock_server，如果lock_server又把锁x给了client，但是由于client不会再次释放这个锁，那么其他请求锁x的client都不会得到它。
## 实现
### 第一部分
实现能够在完美网络条件（无丢包）下正确工作的锁服务。**正确工作是指：在任何一个时间点，同一个锁至多被一个客户端所持有。**  
首先在lock_smain.cc中注册client的acquire和release调用在server对应的handler:
```c++
  /* server handles client's acquire/release by grant/release */
  server.reg(lock_protocol::acquire, &ls, &lock_server::grant);
  server.reg(lock_protocol::release, &ls, &lock_server::release);
```
每个RPC过程都有一个独一无二的过程号(procedure number)，client端acquire和release的过程号在lock_protocol.h中有定义，即lock_protocol::acquire和lock_protocol::release，在服务器端这两个调用的handler是lock_server的grant方法和release方法。  
在实现lock_server的这两个方法前，先在lock_protocol.h中定义锁：
```C++
class lock{
private:
	lock_protocol::lockid_t lid;
	int clt;
	enum st{
		locked,
		free
	}state;
public:
	pthread_cond_t cond_lock;
	lock(lock_protocol::lockid_t _lid){
		lid = _lid; //锁的id
		clt = 0;    //当前持有锁的client
		state = free;  //锁的状态
		cond_lock = PTHREAD_COND_INITIALIZER; //在锁locked时，用于阻塞其他请求该锁的进程。在锁free后唤醒它们
	}
	~lock(){}
	bool Is_Locked(){
		if(state == locked){
			return true;
		}
		return false;
	}
	int Get_Owner(){
		return clt;
	}
	bool Set_Locked(int _clt){
		if(state == free){
			state = locked;
			clt = _clt;
			return true;
		}
		else{
			printf("err: this lock is locked by %d \n", clt);
			return false;
		}
	}
	void Set_Free(){
		state = free;
		clt = 0;
	}
};
```
下面是lock_server在lock_server.h中的定义，添加一个互斥量mutex和一个map。map维护了所有锁的状态，mutex用于保护这个map的读写。其实应该每个锁由一个单独的mutex保护，然而课程的Detailed guidance里说只用一个粗粒度的mutex就行，可以简化代码。
```c++
class lock_server {
protected:
	int nacquire;
	pthread_mutex_t mutex;
	std::map<lock_protocol::lockid_t, lock *> mapLocks;
public:
	lock_server();
	~lock_server() {};
	lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);
	/* lab #1 */
	lock_protocol::status grant(int clt, lock_protocol::lockid_t lid, int &);
	lock_protocol::status release(int clt, lock_protocol::lockid_t lid, int &);
	/* lab #1 */
};
```
下面是lock_server::grant方法在lock_server.cc中的实现，这个方法接收的参数是lid、clt，主要的逻辑有：
* 检查锁lid是否存在，如果不存在则创建这个锁，并且把它准许给clt。
* 如果存在，且锁lid的状态是free，就把它准许给clt。
* 如果存在，但是状态是locked，将该进程阻塞。
注意，Detailed guidance中提到了，用条件变量阻塞时应当把pthread_cond_wait()放到一个循环中，防止它被虚假的唤醒（并没有理解是什么意思）。
```c++
lock_protocol::status
lock_server::grant(int clt, lock_protocol::lockid_t lid, int &r){
	pthread_mutex_lock(&mutex);
	if(mapLocks.find(lid) != mapLocks.end()){
		while(mapLocks[lid]->Is_Locked()){
			pthread_cond_wait(&(mapLocks[lid]->cond_lock), &mutex);
		}
		mapLocks[lid]->Set_Locked(clt);
	}
	else{
		lock * temp = new lock(lid);
		mapLocks.insert(std::make_pair(lid, temp));
		temp->Set_Locked(clt);
	}	
	pthread_mutex_unlock(&mutex);
	/* after granting the lock to clt */
	lock_protocol::status ret = lock_protocol::OK;
	r = nacquire;
	return ret;
}
```
注意这里要返回lock_protocol::OK，因为在之后会提到client端的acquire会根据这个返回值判断它是否得到了这个锁，**如果没有得到锁的话lock_client::acquire一定不能返回**。
下面是lock_server::release在lock_server.cc中的实现，同样接受lid，clt两个参数，用于某个客户端释放某个锁。
```c++
lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r){
	pthread_mutex_lock(&mutex);
	if(mapLocks[lid]->Is_Locked()){
		if(clt == mapLocks[lid]->Get_Owner()){
			mapLocks[lid]->Set_Free();
			pthread_cond_signal(&(mapLocks[lid]->cond_lock));
		}
		else{
			printf("err: client %d doesnt own this lock\n", clt);
		}
	}
	else{
		printf("err: this lock is already free now\n");
	}
	pthread_mutex_unlock(&mutex);
	lock_protocol::status ret = lock_protocol::OK;
	r = nacquire;
	return ret;
}
```
下面是lock_client.cc中client端acquire和release的实现，参考lock_demo.cc中stat调用的写法即可。
```c++
lock_protocol::status
lock_client::acquire(lock_protocol::lockid_t lid){
	int r;
	int ret = cl->call(lock_protocol::acquire, cl->id(), lid, r);
	assert(ret == lock_protocol::OK);
	return r;
}

lock_protocol::status
lock_client::release(lock_protocol::lockid_t lid){
	int r;
	int ret = cl->call(lock_protocol::release, cl->id(), lid, r);
	assert(ret == lock_protocol::OK);
	return r;
}
```
到此为止lab1的第一部分完成，可以通过lock_tester.cc的测试。

