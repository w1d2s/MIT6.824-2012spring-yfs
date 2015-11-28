// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server():
	nacquire (0){
	mutex = PTHREAD_MUTEX_INITIALIZER;
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
	lock_protocol::status ret = lock_protocol::OK;
	printf("stat request from clt %d\n", clt);
	r = nacquire;
	return ret;
}
/* lab #1 */
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
/* lab #1 */

