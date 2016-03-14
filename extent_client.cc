// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include <iostream>

void extent_client::cacheChecker(int opName, extent_protocol::extentid_t eid){
    std::string a[5] = {"", "get", "getattr", "put", "remove"};
    std::cout << " **** extent Op : " << a[opName] << " eid : " << eid << std::endl;
    if(fileCache[1].state == None){
        std::cout << "       fileCache[1] is None ! " << std::endl;
    }else if(fileCache[1].state == Removed){
        std::cout << "       fileCache[1] Removed ! " << std::endl;
    }else{
        std::cout << "       fileCache[1] : " << fileCache[1].data << std::endl;
        if(fileCache[1].data != ""){
            cacheRoot = fileCache[1].data;
        }
        if(cacheRoot != "" && fileCache[1].data == ""){
            std::cout << "       WRONG HERE! " << std::endl;
        }
    }
}

// The calls assume that the caller holds a lock on the extent

extent_client::extent_client(std::string dst){
    pthread_mutex_init(&clientMutex, NULL);
    sockaddr_in dstsock;
    make_sockaddr(dst.c_str(), &dstsock);
    cl = new rpcc(dstsock);
    if (cl->bind() != 0) {
        printf("extent_client: bind failed\n");
    }
}

/* lab #5 */
extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf){
    extent_protocol::status ret = extent_protocol::OK, ret2;
    ScopedLock ml(&clientMutex);
    std::map<extent_protocol::extentid_t, extentFile>::iterator it = fileCache.find(eid);
    if(it == fileCache.end()){
        extentFile tmpFile;
        ret = cl->call(extent_protocol::get, eid, tmpFile.data);
        if(ret != extent_protocol::OK){
            return extent_protocol::NOENT;
        }
        tmpFile.state = Clean;
        tmpFile.attr.atime = time(NULL);
        tmpFile.attr.size = tmpFile.data.size();
        fileCache[eid] = tmpFile;
        buf = tmpFile.data;
        return ret;
    }
    if(fileCache[eid].state == Removed){
        return extent_protocol::NOENT;
    }
    if(fileCache[eid].state == None){
        extentFile tmpFile;
        ret = cl->call(extent_protocol::get, eid, tmpFile.data);
        if(ret != extent_protocol::OK){
            return extent_protocol::NOENT;
        }
        tmpFile.state = Clean;
        tmpFile.attr.size = tmpFile.data.size();
        fileCache[eid] = tmpFile;
    }
    buf = fileCache[eid].data;
    fileCache[eid].attr.atime = time(NULL);
    return ret;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid,
		       extent_protocol::attr &attr){
    extent_protocol::status ret = extent_protocol::OK;
    extent_protocol::attr *a, tmpAttr;
    ScopedLock ml(&clientMutex);
    std::map<extent_protocol::extentid_t, extentFile>::iterator it = fileCache.find(eid);
    if(it == fileCache.end()){
        extentFile tmpFile;
        ret = cl->call(extent_protocol::getattr, eid, tmpFile.attr);
        if(ret != extent_protocol::OK){
            return extent_protocol::NOENT;
        }
        fileCache.insert(std::make_pair(eid, tmpFile));
        attr = tmpFile.attr;
        return ret;
    }
    if(fileCache[eid].state == Removed){
        return extent_protocol::NOENT;
    }
    a = &fileCache[eid].attr;
    if(!a->atime || !a->ctime || !a->mtime){
        ret = cl->call(extent_protocol::getattr, eid, tmpAttr);
        if(ret == extent_protocol::OK){
            if(!a->atime)
                a->atime = tmpAttr.atime;
            if(!a->ctime)
                a->ctime = tmpAttr.ctime;
            if(!a->mtime)
                a->mtime = tmpAttr.mtime;
            if(fileCache[eid].state == None)
                a->size = tmpAttr.size;
            else
                a->size = fileCache[eid].attr.size;
        }
    }
    attr = *a;
    return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf){
    extent_protocol::status ret = extent_protocol::OK;
    ScopedLock ml(&clientMutex);
    unsigned int curtime = time(NULL);
    std::map<extent_protocol::extentid_t, extentFile>::iterator it = fileCache.find(eid);
    if(it != fileCache.end()){
        if(fileCache[eid].state == Removed){
            return extent_protocol::NOENT;
        }
        fileCache[eid].state = Dirty;
        fileCache[eid].data = buf;
        fileCache[eid].attr.size = buf.size();
        fileCache[eid].attr.mtime = fileCache[eid].attr.ctime = curtime;
        //std::cout << " === put id : (Cached)" << eid << std::endl;
        //std::cout << "       data : " << fileCache[eid].data << std::endl;
        return ret;
    }
    extentFile tmpFile;
    tmpFile.state = Dirty;
    tmpFile.data = buf;
    tmpFile.attr.size = buf.size();
    tmpFile.attr.atime = tmpFile.attr.mtime = tmpFile.attr.ctime = curtime; // remember to set atime
    fileCache.insert(std::make_pair(eid, tmpFile));
    //std::cout << " === put id : (New)" << eid << std::endl;
    //std::cout << "       data : " << fileCache[eid].data << std::endl;
    return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid){
    extent_protocol::status ret = extent_protocol::OK;
    ScopedLock ml(&clientMutex);
    // think if you want to remove a file which is not locally cached
    fileCache[eid].state = Removed;
    return ret;
}

extent_protocol::status
extent_client::flush(extent_protocol::extentid_t eid){
    extent_protocol::status ret = extent_protocol::OK;
    ScopedLock ml(&clientMutex);
    std::map<extent_protocol::extentid_t, extentFile>::iterator it = fileCache.find(eid);
    if(it != fileCache.end()){
        int r;
        if(fileCache[eid].state == Dirty){
            ret = cl->call(extent_protocol::put, eid, fileCache[eid].data, r);
        }else if(fileCache[eid].state == Removed){
            ret = cl->call(extent_protocol::remove, eid, r);
        }
        fileCache.erase(eid);
        return ret;
    }
    return extent_protocol::NOENT;
}
