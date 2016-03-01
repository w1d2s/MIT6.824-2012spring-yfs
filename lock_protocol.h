// lock protocol

#ifndef lock_protocol_h
#define lock_protocol_h

#include <pthread.h>
#include <stdio.h>
#include "rpc.h"

class lock_protocol {
public:
	enum xxstatus { OK, RETRY, RPCERR, NOENT, IOERR };
	typedef int status;
	typedef unsigned long long lockid_t;
	enum rpc_numbers {
		acquire = 0x7001,
    	release,
    	subscribe,	// for lab 5
    	stat
	};
};

class rlock_protocol {
public:
	enum xxstatus { OK, RPCERR };
  	typedef int status;
  	enum rpc_numbers {
    	revoke = 0x8001,
    	retry = 0x8002
  	};
};

/* lab #1 */
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
		lid = _lid;
		clt = 0;
		state = free;
		cond_lock = PTHREAD_COND_INITIALIZER;
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
/* lab #1 */

#endif
