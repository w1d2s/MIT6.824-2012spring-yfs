#ifndef yfs_client_h
#define yfs_client_h

#include <string>
//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>

#include "lock_protocol.h"
#include "lock_client.h"

class yfs_client {
    extent_client *ec;
    public:

        typedef unsigned long long inum;
        enum xxstatus { OK, RPCERR, NOENT, IOERR, EXIST };
        typedef int status;

        struct fileinfo {
            unsigned long long size;
            unsigned long atime;
            unsigned long mtime;
            unsigned long ctime;
        };
        struct dirinfo {
            unsigned long atime;
            unsigned long mtime;
            unsigned long ctime;
        };
        struct dirent {
            std::string name;
            yfs_client::inum inum;
        };

    private:
        static std::string filename(inum);
        static inum n2i(std::string);
    public:

        yfs_client(std::string, std::string);

        bool isfile(inum);
        bool isdir(inum);

        int getfile(inum, fileinfo &);
        int getdir(inum, dirinfo &);
        /* lab #2 */
        inum random_inum(bool isfile);
        status dirparser(std::string raw, const char * name, inum & ino);
        status create(inum parent, const char * name, inum & ino);
        status lookup(inum parent, const char * name, inum & ino);
        status readdir(inum ino, std::vector<dirent> & files);

        status setattr(inum ino, struct stat * attr);
        status read(inum ino, size_t & size, off_t offset, std::string & buf);
        status write(inum ino, size_t & size, off_t offset, const char * buf);
        /* lab #2 */
};

#endif
