// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <string>
#include <map>
#include <pthread.h>
#include "extent_protocol.h"

class extent_server {
/* lab #2 */
private:
    std::map<extent_protocol::extentid_t, std::string> mapContent;
    std::map<extent_protocol::extentid_t, extent_protocol::attr> mapAttr;
    pthread_mutex_t mutex;
public:
    extent_server();
    int put(extent_protocol::extentid_t id, std::string, int &);
    int get(extent_protocol::extentid_t id, std::string &);
    int getattr(extent_protocol::extentid_t id, extent_protocol::attr &);
    int remove(extent_protocol::extentid_t id, int &);
};

#endif
