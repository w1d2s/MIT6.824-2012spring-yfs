// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"
#include <map>


int lock_client_cache::last_port = 0;

lock_client_cache::lock_client_cache(std::string xdst,
				     class lock_user *_lu)
  : lock_client(xdst), lu(_lu)
{
  srand(time(NULL)^last_port);
  rlock_port = ((rand()%32000) | (0x1 << 10));
  const char *hname;
  // VERIFY(gethostname(hname, 100) == 0);
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlock_port;
  id = host.str();
  last_port = rlock_port;
  rpcs *rlsrpc = new rpcs(rlock_port);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);
  mutex = PTHREAD_MUTEX_INITIALIZER;
}

/* lab #5 */
void lock_user::dorelease(lock_protocol::lockid_t lid){
	if(ec->flush(lid) == extent_protocol::OK){
		return;
	}
	//printf("	--- flush failed! \n");
}

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid){
	/* lab #4 */
	int ret = lock_protocol::OK, r;
	std::map<lock_protocol::lockid_t, lock_info> iter;
	pthread_mutex_lock(&mutex);
	if(lockCache.find(lid) == lockCache.end()){
		lockCache.insert(std::make_pair(lid, lock_info()));
	}
	while(true){
		switch(lockCache[lid].state){
			case None:
				//printf("acquiring lock %d via RPC!(1)\n", &lid);
				lockCache[lid].getRetry = false;
				lockCache[lid].state = Acquiring;
				pthread_mutex_unlock(&mutex);
				ret = cl->call(lock_protocol::acquire, lid, id, r);
				pthread_mutex_lock(&mutex);
				if(ret == lock_protocol::OK){
					lockCache[lid].state = Locked;
					pthread_mutex_unlock(&mutex);
					return ret;
				}else if(ret == lock_protocol::RETRY){
					if(!lockCache[lid].getRetry){
						pthread_cond_wait(&lockCache[lid].retryQueue, &mutex);
					}
				}
				break;
			case Free:
				//printf("	acquiring lock %d via Cache!\n", &lid);
				lockCache[lid].state = Locked;
				pthread_mutex_unlock(&mutex);
				return lock_protocol::OK;
			case Locked:
				pthread_cond_wait(&lockCache[lid].waitQueue, &mutex);
				break;
			case Acquiring:
				if(!lockCache[lid].getRetry){
					pthread_cond_wait(&lockCache[lid].waitQueue, &mutex);
				}else{
					//printf("acquiring lock %d via RPC!(2)\n", &lid);
					lockCache[lid].getRetry = false;
					lockCache[lid].state = Acquiring;
					pthread_mutex_unlock(&mutex);
					ret = cl->call(lock_protocol::acquire, lid, id, r);
					pthread_mutex_lock(&mutex);
					if(ret == lock_protocol::OK){
						lockCache[lid].state = Locked;
						pthread_mutex_unlock(&mutex);
						return ret;
					}else if(ret == lock_protocol::RETRY){
						if(!lockCache[lid].getRetry){
							pthread_cond_wait(&lockCache[lid].retryQueue, &mutex);
						}
					}
				}
				break;
			case Releasing:
				pthread_cond_wait(&lockCache[lid].releaseQueue, &mutex);
				break;
		}
	}
	return lock_protocol::OK;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid){
	/* lab #4 */
	int ret = lock_protocol::OK;
	lock_protocol::status r;
	pthread_mutex_lock(&mutex);
	if(lockCache.find(lid) == lockCache.end()){
		return lock_protocol::NOENT;
	}
	if(lockCache[lid].getRevoke){
		lockCache[lid].state = Releasing;
		lockCache[lid].getRevoke = false;
		/* lab #5 */
		lu->dorelease(lid);
		pthread_mutex_unlock(&mutex);
		ret = cl->call(lock_protocol::release, lid, id, r);
		pthread_mutex_lock(&mutex);
		lockCache[lid].state = None;
		pthread_cond_broadcast(&(lockCache[lid].releaseQueue));
		pthread_mutex_unlock(&mutex);
		return ret;
	}else{
		lockCache[lid].state = Free;
		pthread_cond_signal(&(lockCache[lid].waitQueue));
		pthread_mutex_unlock(&mutex);
	  	return lock_protocol::OK;
	}
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, int &){
	/* lab #4 */
	int ret = lock_protocol::OK, ret_;
	lock_protocol::status r;
	pthread_mutex_lock(&mutex);
	if(lockCache.find(lid) == lockCache.end()){
		ret = lock_protocol::NOENT;
	}else{
		if(lockCache[lid].state == Free){
			lockCache[lid].state = Releasing;
			/* lab #5 */
			lu->dorelease(lid);
			pthread_mutex_unlock(&mutex);
			int ret_ = cl->call(lock_protocol::release, lid, id, r);
			pthread_mutex_lock(&mutex);
			lockCache[lid].state = None;
			pthread_cond_broadcast(&lockCache[lid].releaseQueue);
		}else{
			lockCache[lid].getRevoke = true;
		}
	}
	pthread_mutex_unlock(&mutex);
  	return ret;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, int &){
	/* lab #4 */
	int ret;
	pthread_mutex_lock(&mutex);
	if(lockCache.find(lid) == lockCache.end()){
		ret = lock_protocol::NOENT;
	}else{
		lockCache[lid].getRetry = true;
		pthread_cond_signal(&(lockCache[lid].retryQueue));
		ret = lock_protocol::OK;
	}
	pthread_mutex_unlock(&mutex);
  	return ret;
}
