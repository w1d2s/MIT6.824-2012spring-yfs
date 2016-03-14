// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#include <iostream>

extent_server::extent_server(){
    pthread_mutex_init(&mutex, NULL);
    int ret;
    put(1, "", ret); // create root
}


int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &){
// You fill this in for Lab 2.
    //pthread_mutex_lock(&mutex);
    ScopedLock ml(&mutex);
    unsigned int curtime = time(NULL);
    if(mapContent.find(id) == mapContent.end()){
        mapAttr[id].atime = curtime; // note: set atime if it is a new file
    }
    mapContent[id] = buf;
    mapAttr[id].size = buf.size();
    mapAttr[id].mtime = curtime;
    mapAttr[id].ctime = curtime;
    //std::cout << "put id : " << id << std::endl;
    //std::cout << "  data : " << buf << std::endl;
    //pthread_mutex_unlock(&mutex);
    return extent_protocol::OK;
/* lab #2 */
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf){
// You fill this in for Lab 2.
    //pthread_mutex_lock(&mutex);
    ScopedLock ml(&mutex);
    if(mapContent.find(id) != mapContent.end()){
        buf = mapContent[id];
        mapAttr[id].atime = time(NULL);
        //std::cout << "get id : " << id << std::endl;
        //std::cout << "  data : " << buf << std::endl;
        //pthread_mutex_unlock(&mutex);
        return extent_protocol::OK;
    }
    //pthread_mutex_unlock(&mutex);
/* lab #2 */
    return extent_protocol::NOENT;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a){
    // You fill this in for Lab 2.
    // You replace this with a real implementation. We send a phony response
    // for now because it's difficult to get FUSE to do anything (including
    // unmount) if getattr fails.
    //pthread_mutex_lock(&mutex);
    ScopedLock ml(&mutex);
    if(mapAttr.find(id) != mapAttr.end()){
        a.size = mapAttr[id].size;
        a.atime = mapAttr[id].atime;
        a.mtime = mapAttr[id].mtime;
        a.ctime = mapAttr[id].ctime;
        //std::cout << "getattr id : " << id << std::endl;
        //pthread_mutex_unlock(&mutex);
        return extent_protocol::OK;
    }
    //pthread_mutex_unlock(&mutex);
    /* lab #2 */
    return extent_protocol::NOENT;
}

int extent_server::remove(extent_protocol::extentid_t id, int &){
    // You fill this in for Lab 2.
    //pthread_mutex_lock(&mutex);
    ScopedLock ml(&mutex);
    if(mapContent.find(id) != mapContent.end()){
        mapContent.erase(id);
        mapAttr.erase(id);
        //std::cout << "remove id : " << id << std::endl;
        //pthread_mutex_unlock(&mutex);
        return extent_protocol::OK;
    }
    /* lab #2 */
    //pthread_mutex_unlock(&mutex);
    return extent_protocol::NOENT;
}
