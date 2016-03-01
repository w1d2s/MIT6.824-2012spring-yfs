# Lab3 Detailed Guidance - MKDIR, UNLINK, and Locking  
## 简介
Lab3前半部分承接Lab2，实现以下操作：  
* MKDIR：创建目录  
* UNLINK：删除文件  

后半部分要使用Lab1实现的锁服务为Lab2和Lab3前半部分实现的这些操作加锁，保证并发操作的正确。    

本次Lab完成后整个系统架构图应当如下图所示：

![](https://github.com/w1d2s/MIT6.824-2012spring-yfs/blob/lab2/Detailed-Guidance/basic%20file%20system.jpg)  

## 实现
### 第一部分：MKDIR & UNLINK
第一部分要实现两个文件操作：创建目录(MKDIR)和删除文件(UNLINK)。
首先看创建目录(MKDIR)，根据Lab2中文件和目录的inum命名规则：文件inum的第31个bit为1，目录则为0.
而在创建目录时候，其实可以服用CREATE/MKNOD在yfs_client.cc中的代码，只需要修改inum的第31位就可以。对yfs_client.cc中的create()函数进行一点修改：
``` c++
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
```
同时记得把fuse.cc中CREATE/MKNOD对应的glue code作相应修改：
``` c++
yfs_client::status
fuseserver_createhelper(fuse_ino_t parent, const char *name,
     mode_t mode, struct fuse_entry_param *e){
    // In yfs, timeouts are always set to 0.0, and generations are always set to 0
    e->attr_timeout = 0.0;
    e->entry_timeout = 0.0;
    e->generation = 0;
    /* lab #2 */
    yfs_client::status ret;
    // get struct stat attr and ino(inum) for e
    yfs_client::inum ino, pa = parent;
    ret = yfs->create(pa, name, ino, true);
    if(ret != yfs_client::OK){
        return ret;
    }
    // after calling yfs->create() :
    e->ino = ino;
    ret = getattr(ino, e->attr);
    if(ret == yfs_client::OK){
        return ret;
    }
    // You fill this in for Lab 2
    return yfs_client::NOENT;
}
void
fuseserver_mkdir(fuse_req_t req, fuse_ino_t parent, const char *name,
     mode_t mode){
    struct fuse_entry_param e;
    // In yfs, timeouts are always set to 0.0, and generations are always set to 0
    e.attr_timeout = 0.0;
    e.entry_timeout = 0.0;
    e.generation = 0;
    /* lab #3 */
    yfs_client::inum pa = parent, ino;
    if(yfs->create(pa, name, ino, false) == yfs_client::OK){
        e.ino = ino;
        if(getattr(ino, e.attr) == yfs_client::OK){
            fuse_reply_entry(req, &e);
            return;
        }
    }
    fuse_reply_err(req, ENOSYS);
    // You fill this in for Lab 3
}
```
UNLINK的实现需要注意以下几点：
* 在调用extent_client的remove后，一定要把父目录索引中文件名字去掉
* 处理找不到文件的情况  

yfs_client中UNLINK逻辑如下：
```c++
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
```
这部分完成后应当能够通过test-lab-3-a.pl的测试。
### 第二部分：Locking
接下来要为文件操作加锁，保证并发时的正确性。首先要在yfs_client中实例化一个lock_client的对象。然后，模仿rpc/slock.h中的做法，在yfs_client.h中增加一个类：
```c++
class ScopedLockClient{
private:
    lock_client *lc;
    yfs_client::inum ino;
public:
    ScopedLockClient(lock_client *_lc, yfs_client::inum _ino){
        ino = _ino;
        lc = _lc;
        lc->acquire(ino);
    }
    ~ScopedLockClient(){
        lc->release(ino);
    }
};
```
这样在实例化一个ScopedLockClient对象时，他会自动acquire一个锁，在对象生命周期结束时，会调用析构函数自动release这个锁。
在yfs_client中，需要加锁的操作有：
* UNLINK
* WRITE
* SETATTR
* READDIR
* CREATE

注意不要给LOOKUP加锁，否则Lab4测试性能时，RPC数会暴涨。

第二部分完成后应当能通过test-lab-3-b.c和test-lab-3-c.c。注意测试test-lab-3-b时也要用两个同样的参数测试一下，例如
```
./test-lab-3-b ./yfs1 ./yfs1
```
