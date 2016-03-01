#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>



#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"
#include <pthread.h>

#include <set>
#include <map>


class lock_server_cache {
private:
    int nacquire;
public:
    /* lab #4 */
    enum lock_state{
        Free,
        Locked,
        Locked_and_Wait,
        Retrying
    };
    struct lock_info{
        lock_state state;
        std::string currentHolder;
        std::set<std::string> waitSet;
        lock_info(){
            state = Free;
            currentHolder = "NULL";
        }
    };

    std::map<lock_protocol::lockid_t, lock_info> lockMap;
    pthread_mutex_t mutex;
    /* lab #4 */
    lock_server_cache();
    lock_protocol::status stat(lock_protocol::lockid_t, int &);
    lock_protocol::status acquire(lock_protocol::lockid_t, std::string id, int &);
    lock_protocol::status release(lock_protocol::lockid_t, std::string id, int &);
};

#endif
