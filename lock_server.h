// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <map>
#include <pthread.h>
#include <string>
#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc.h"

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

#endif 







