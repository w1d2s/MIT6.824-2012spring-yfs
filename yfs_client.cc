// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include "lock_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


yfs_client::yfs_client(std::string extent_dst, std::string lock_dst){
    ec = new extent_client(extent_dst);
    lc = new lock_client(lock_dst);
}

yfs_client::inum
yfs_client::n2i(std::string n){
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
yfs_client::filename(inum inum){
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

bool
yfs_client::isfile(inum inum){
    if(inum & 0x80000000)
        return true;
    return false;
}

bool
yfs_client::isdir(inum inum){
    return ! isfile(inum);
}

int
yfs_client::getfile(inum inum, fileinfo &fin){
    int r = OK;
    /* lab #3 */
    ScopedLockClient slc(lc, inum);
    // You modify this function for Lab 3
    // - hold and release the file lock

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

    release:

    return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din){
    int r = OK;
    /* lab #3 */
    ScopedLockClient slc(lc, inum);
    // You modify this function for Lab 3
    // - hold and release the directory lock

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

    release:
    return r;
}

/* lab #2 */
yfs_client::inum
yfs_client::random_inum(bool isfile){
    inum ret = (unsigned long long)((rand() & 0x7fffffff) | (isfile << 31));
    ret = 0xffffffff & ret;
    return ret;
}

yfs_client::status
yfs_client::dirparser(std::string raw, const char* name, inum & ino){
	std::string strIno = "", filename = std::string(name) + "][";
    int pos = raw.find(filename);
    if(pos == std::string::npos){
        return NOENT;
    }
    pos +=  filename.length();
	for(int i = pos; ; i++){
		if((raw[i] == ']') && (raw[i+1] == '}'))
			break;
		strIno += raw[i];
	}
    std::cout << "strIno : " << strIno << std::endl;
	ino = n2i(strIno);
    return OK;
}


yfs_client::status
yfs_client::create(inum parent, const char * name, inum & ino, bool isFile){
    std::string dirRaw = "", fileRaw = "", tmpBuf;
    std::string fileName = "{[" + std::string(name) + "][";
    do{  //eliminate collision
        ino = random_inum(isFile);
    }while(ec->get(ino, tmpBuf) != extent_protocol::NOENT);
    ScopedLockClient slc(lc, parent);
    if(ec->get(parent, dirRaw) == extent_protocol::OK){
        //directory 'parent' already exists
        if(dirRaw.find(name) != std::string::npos){
            // return yfs_client::EXIST if file 'name' already exist
            return yfs_client::EXIST;
        }
        if(ec->put(ino, fileRaw) == extent_protocol::OK){
            // directory format: {[filename][inum]}
            dirRaw.append(fileName + filename(ino) + "]}");
            ec->put(parent, dirRaw);
            return yfs_client::OK;
        }
    }
    return yfs_client::IOERR;
}

yfs_client::status
yfs_client::lookup(inum parent, const char * name, inum & ino){
    ScopedLockClient slc(lc, parent);
    std::string dirRaw = "";
    if(ec->get(parent, dirRaw) == extent_protocol::OK){
        if(dirparser(dirRaw, name, ino) == OK){
            return yfs_client::OK;
        }
    }
    return yfs_client::NOENT;
}

yfs_client::status
yfs_client::readdir(inum ino, std::vector<yfs_client::dirent> & files){
    ScopedLockClient slc(lc, ino);
    std::string dirRaw = "";
    yfs_client::dirent tempDir;
    if(ec->get(ino, dirRaw) == extent_protocol::OK){
        int pos = 0, head = 0, tail = 0;
	    while(pos + 1 < dirRaw.length()){
	        head = dirRaw.find("{[", pos) + 2;
		    tail = dirRaw.find("][", pos);
		    tempDir.name = dirRaw.substr(head, tail - head);
		    head = tail + 2;
		    tail = dirRaw.find("]}", pos);
		    tempDir.inum = n2i(dirRaw.substr(head, tail - head));
            files.push_back(tempDir);
		    //std::cout << "file name : " << name << std::endl;
		    //std::cout << "inum : " << inum << std::endl;
		    pos = tail + 2;
	    }
        return yfs_client::OK;
    }
    return yfs_client::NOENT;
}

yfs_client::status
yfs_client::setattr(inum ino, struct stat * attr){
    ScopedLockClient slc(lc, ino);
    std::string data;
    size_t size = attr->st_size;
    if(ec->get(ino, data) == extent_protocol::OK){
        data.resize(size, '\0');
        if(ec->put(ino, data) != extent_protocol::OK){
            return yfs_client::IOERR;
        }
        return yfs_client::OK;
    }
    return yfs_client::NOENT;
}

yfs_client::status
yfs_client::read(inum ino, size_t size, off_t offset, std::string & buf){
    std::string data;
    if(ec->get(ino, data) == extent_protocol::OK){
        if((offset < 0) || (offset > data.size() - 1)){
            buf = std::string();
        }else{
            buf = data.substr(offset, size);
        }
        return yfs_client::OK;
    }
    return yfs_client::NOENT;
}

yfs_client::status
yfs_client::write(inum ino, size_t & size, off_t offset, const char * buf){
    ScopedLockClient slc(lc, ino);
    std::string data;
    if(ec->get(ino, data) == extent_protocol::OK){
        if(offset < 0){  // note : append a str : offset = data.size()
        }else{
            if(offset + size > data.size()){
                data.resize(size + offset, '\0');
            }
            data.replace(offset, size, buf, size);
            if(ec->put(ino, data) != extent_protocol::OK){
                return yfs_client::IOERR;
            }
        }
        return yfs_client::OK;
    }
    return yfs_client::NOENT;
}
/* lab #2 */
/* lab #3 */
yfs_client::status
yfs_client::unlink(inum parent, const char * name){
    ScopedLockClient slc(lc, parent);
    inum ino;
    std::string dirRaw = "";
    if(ec->get(parent, dirRaw) == extent_protocol::OK){
        if(dirparser(dirRaw, name, ino) == OK){
            ScopedLockClient slc(lc, ino);
            if(ec->remove(ino) == extent_protocol::OK){
                std::string filename = "{[" + std::string(name) + "]";
		        if(dirRaw.find(filename) == std::string::npos){
                    return yfs_client::NOENT;
		        }
                int head = dirRaw.find(filename);
                int tail = dirRaw.find("]}", head) + 2;
                dirRaw.erase(head, tail - head);
                if(ec->put(parent, dirRaw) == extent_protocol::OK){
                    return yfs_client::OK;
                }
            }
        }
    }
    return yfs_client::NOENT;
}
/* lab #3 */
