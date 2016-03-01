// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"


lock_server_cache::lock_server_cache(){
    mutex = PTHREAD_MUTEX_INITIALIZER;
}

lock_protocol::status
lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, int &){
    /* lab #4 */
    //printf("client[%s] acuiqring lock[%d]\n", id.c_str(), lid);
    lock_protocol::status ret = lock_protocol::OK, ret_;
    int rr;
    bool revoke = false;
    pthread_mutex_lock(&mutex);
    if(lockMap.find(lid) == lockMap.end()){
        lockMap.insert(std::make_pair(lid, lock_info()));
    }
    if(lockMap[lid].state == Free){
        lockMap[lid].state = Locked;
        lockMap[lid].currentHolder = id;
    }else if(lockMap[lid].state == Locked){
        lockMap[lid].state = Locked_and_Wait;
        lockMap[lid].waitSet.insert(id);
        ret = lock_protocol::RETRY;
        revoke = true;
    }else if(lockMap[lid].state == Locked_and_Wait){
        lockMap[lid].waitSet.insert(id);
        ret = lock_protocol::RETRY;
    }else if(lockMap[lid].state == Retrying){
        if(lockMap[lid].waitSet.count(id)){
            lockMap[lid].state = Locked;
            lockMap[lid].currentHolder = id;
            lockMap[lid].waitSet.erase(id);
            if(!lockMap[lid].waitSet.empty()){
                lockMap[lid].state = Locked_and_Wait;
                revoke = true;
            }
        }else{
            lockMap[lid].waitSet.insert(id);
            ret = lock_protocol::RETRY;
        }
    }
    pthread_mutex_unlock(&mutex);
    if(revoke){
        ret_ = handle(lockMap[lid].currentHolder).safebind()->call(rlock_protocol::revoke, lid, rr);
    }
    return ret;
}

lock_protocol::status
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, int &r){
    /* lab #4 */
    //printf("client[%s] releasing lock[%d]\n", id.c_str(), lid);
    lock_protocol::status ret = lock_protocol::OK, ret_;
    int rr;
    std::string nextHolder;
    bool retry = false;
    pthread_mutex_lock(&mutex);
    if(lockMap.find(lid) == lockMap.end()){
        return lock_protocol::NOENT;
    }
    switch(lockMap[lid].state){
        case Free:
            ret = lock_protocol::IOERR;
            break;
        case Retrying:
            ret = lock_protocol::IOERR;
            break;
        case Locked:
            lockMap[lid].state = Free;
            lockMap[lid].currentHolder = "NULL";
        case Locked_and_Wait:
            lockMap[lid].state = Retrying;
            lockMap[lid].currentHolder = "NULL";
            nextHolder = *(lockMap[lid].waitSet.begin());
            retry = true;
        default:
            break;
    }

    //printf("	client[%s] released lock[%d] successfully\n", id.c_str(), lid);
    pthread_mutex_unlock(&mutex);
    if(retry){
        ret_ = handle(nextHolder).safebind()->call(rlock_protocol::retry, lid, rr);
    }
    return ret;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}
