// extent client interface.

#ifndef extent_client_h
#define extent_client_h

#include <string>
#include <pthread.h>
#include "extent_protocol.h"
#include "rpc.h"

enum fileState{
    None,
    Clean,
    Dirty,
    Removed
};

struct extentFile{
    fileState state;
    std::string data;
    extent_protocol::attr attr;
    extentFile() : state(None){}
};

class extent_client {
private:
    rpcc *cl;
    std::map<extent_protocol::extentid_t, extentFile> fileCache;
    pthread_mutex_t clientMutex;
    std::string cacheRoot;
public:
    extent_client(std::string dst);

    extent_protocol::status get(extent_protocol::extentid_t eid,
			      std::string &buf);
    extent_protocol::status getattr(extent_protocol::extentid_t eid,
				  extent_protocol::attr &a);
    extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
    extent_protocol::status remove(extent_protocol::extentid_t eid);
    /* lab #5 */
    extent_protocol::status flush(extent_protocol::extentid_t eid);

    void cacheChecker(int opName, extent_protocol::extentid_t eid);
};

#endif
