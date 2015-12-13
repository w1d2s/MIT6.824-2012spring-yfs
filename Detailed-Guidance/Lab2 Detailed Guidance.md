# Lab2 Detailed Guidance - Basic File Server  
## 简介
Lab2要实现基本的文件系统，即实现下列基本操作：    
* CREATE/MKNOD, LOOKUP, READDIR  
* SETATTR, READ, WRITE    

回想YFS的架构：  
![](https://github.com/w1d2s/MIT6.824-2012spring-yfs/blob/lab2/Detailed-Guidance/yfs.jpg)  

yfs模块负责实现主要的文件操作逻辑（读，写，查找，创建文件等），由两部分构成：  
*   The FUSE interface, in fuse.cc：他将FUSE kernel module中的FUSE操作翻译成yfs client中的调用。 已经提供了必要的代码框架，需要做的是修改fuse.cc中的部分函数来调用yfs_client中的文件操作函数，并将结果通过fuse.cc返回给FUSE kernel。  
*  The YFS client, in yfs_client.{cc,h}：主要文件系统的逻辑在YFS client中实现，基于lab1中实现的lock server和本次lab中的extent server。由于他和lock_server与extent_server间的关系，称其为一个yfs_client。


而extent server存储着文件系统的所有数据，它的角色像是普通文件系统中的硬盘一样。在之后的lab中，你将会在不同的主机上运行多个yfs客户端，所有yfs客户端与同一个extent server通信，他们从不同的主机上看到的文件系统是一眼的。不同的yfs分享数据的唯一渠道就是extent server。他也由两部分构成：  
*  Extent client, in extent_client.{cc,h}：是一个封装类，yfs client通过这个类使用RPC与extent server通信。  
* Extent server, in extent_server.{cc,h} and extent_smain.cc：extent server负责简单的key-value存储，key的类型是extent_protocol::extentid_t，一个整数，而存储的值是std::string，一个字符串。他只负责存储，而不负责任何解释数据内容的工作。对每一个key/value pair，还需在extent server中维护一个attribute，表明这个pair的属性。extent server提供四种针对key/value pair的基本操作，即put(key, value)，get(key)，getattr(key)，remove(key)，他们都可以通过RPC被extent client调用。

下面这张架构图基本概括了上面这段话：  
![](https://github.com/w1d2s/MIT6.824-2012spring-yfs/blob/lab2/Detailed-Guidance/basic%20file%20system.jpg)  

## 实现
### 第一部分：CREATE/MKNOD, LOOKUP, and READDIR  
第一部分要实现三种基本文件操作：创建文件(create/mknod)，查找文件(lookup)，读取目录中的文件列表(readdir)。而这些操作是基于extent server支持的四种方法的，所以先实现extent server的四种方法。   
先在extent_server.h中添加key-value存储结构：
``` c++
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
```
可以定义一个类把value和attribute打包起来用一个map，不过我认为由于extent server面向extent client时的接口只有上述四种，形成了良好的封装效果，用两个map问题也不大。mutex用于保证四种基本操作间的互斥。
下面是extent_server.cc中实现的四种基本操作，**不涉及任何对存储内容的解释性操作**。有几个点需要注意：
attribute由文件大小size和三个时间构成：the last content modification time (mtime)，the last attribute change time (ctime)，and last content access time (atime)。在put()中要修改ctime和mtime为当前时间，如果是新文件还要修改atime；在get中要修改atime()为当前时间。
``` c++
int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &){
// You fill this in for Lab 2.
    pthread_mutex_lock(&mutex);
    unsigned int curtime = time(NULL);
    if(mapContent.find(id) == mapContent.end()){
        mapAttr[id].atime = curtime; // note: set atime if it is a new file
    }
    mapContent[id] = buf;
    mapAttr[id].size = buf.size();
    mapAttr[id].mtime = curtime;
    mapAttr[id].ctime = curtime;
    pthread_mutex_unlock(&mutex);
    return extent_protocol::OK;
/* lab #2 */
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf){
// You fill this in for Lab 2.
    pthread_mutex_lock(&mutex);
    if(mapContent.find(id) != mapContent.end()){
        buf = mapContent[id];
        mapAttr[id].atime = time(NULL);
        pthread_mutex_unlock(&mutex);
        return extent_protocol::OK;
    }
    pthread_mutex_unlock(&mutex);
/* lab #2 */
    return extent_protocol::NOENT;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a){
    // You fill this in for Lab 2.
    // You replace this with a real implementation. We send a phony response
    // for now because it's difficult to get FUSE to do anything (including
    // unmount) if getattr fails.
    pthread_mutex_lock(&mutex);
    if(mapAttr.find(id) != mapAttr.end()){
        a.size = mapAttr[id].size;
        a.atime = mapAttr[id].atime;
        a.mtime = mapAttr[id].mtime;
        a.ctime = mapAttr[id].ctime;
        pthread_mutex_unlock(&mutex);
        return extent_protocol::OK;
    }
    pthread_mutex_unlock(&mutex);
    /* lab #2 */
    return extent_protocol::NOENT;
}

int extent_server::remove(extent_protocol::extentid_t id, int &){
    // You fill this in for Lab 2.
    pthread_mutex_lock(&mutex);
    if(mapContent.find(id) != mapContent.end()){
        mapContent.erase(id);
        mapAttr.erase(id);
        pthread_mutex_unlock(&mutex);
        return extent_protocol::OK;
    }
    /* lab #2 */
    pthread_mutex_unlock(&mutex);
    return extent_protocol::NOENT;
}
```  
另外FUSE需要有一个根目录文件，名为root，inumber为1，可以再extent_server构造函数中建立。  
> Though you are free to choose any inum identifier you like for newly created files, FUSE assumes that the inum for the root directory is 0x00000001. Thus, you'll need to ensure that when yfs_client starts, it is ready to export an empty directory stored under that inum.  

``` c++
extent_server::extent_server(){
    pthread_mutex_init(&mutex, NULL);
    int ret;
    put(1, "", ret); // create root
}
```  
接下来就可以实现yfs_client中的文件操作逻辑了，在此之前需要澄清一些约定：  
* 每个文件或目录都要有一个独一无二的identifier，在fuse中用的是一个32-bits的整数（像是unix系统中的inode号）；在yfs_client，以及向上的extent_server中我们使用一个64-bits的整数，高32位置零，第32位与fuse的identifier相同，我们将其称作inum，在yfs_client.h中有相应定义。并且为了yfs能够分辨一个inum代表的是文件还是目录，规定文件的inum第31位为1，目录的inum第31位为0.  
* 目录内容的存储：用一个格式化的字符串存储目录中的文件，即其文件名与inum的一一对应：{[filename1][inum1]}{[filename2][inum2]}....你也可以自己决定用什么格式。但是extent_server只负责对这个字符串基本的存储操作，至于如何在添加文件后修改目录内容，如何将目录内容parse为文件名列表，应当交给yfs_client来做。  
> You should store a directory's entire content in a single entry in the extent server, with the directory's inum as the key. LOOKUP and READDIR read directory contents, and CREATE/MKNOD modify directory contents.  

* 对于fuse和yfs_client的关系，看上面那张架构图。值得提醒的一点是，如果你不知道fuse.cc中的函数应该取回什么内容来返回给fuse kernel，请参阅 [fuse_lowlevel.h](http://fuse.sourceforge.net/doxygen/fuse__lowlevel_8h.html)。  

接下来首先通过研究fuse.cc中的getattr()函数来了解一个fuse handler是怎么调用yfs_client中的方法从extent_server取回内容，又是怎样将其返回给fuse kernel的。他只是一层glue code，核心逻辑还是在yfs_client的各种方法中。明白了之后就可以用类似的方式实现其他的fuse handler和yfs_client中的方法了。   
下面是yfs_client.cc中create()，lookup()，readdir()的具体实现。在这个文件中已经提供了很多好用的函数，比如n2i(std::string n)和filename(inum inum)用于string和inum的转换，在修改和读取目录内容时可能有用；再比如isfile(inum inum)和isdir(inum inum)可以判断一个inum代表的是文件还是目录。  
create()生成inum是随机选择的，简单的用了一个循环消除碰撞。如果目录下已经存在同名文件，返回EEXIST。  
readdir()会解析目录的字符串，将一个个filename/inum pair放进yfs_client.h定义的结构体dirent中。  
fuse.cc中的glue code我就不放上来了。
``` c++
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
yfs_client::create(inum parent, const char * name, inum & ino){
    std::string dirRaw = "", fileRaw = "", tmpBuf;
    std::string fileName = "{[" + std::string(name) + "][";
    do{  //eliminate collision
        ino = random_inum(true);
    }while(ec->get(ino, tmpBuf) != extent_protocol::NOENT);
    if(ec->get(parent, dirRaw) == extent_protocol::OK){
        //directory 'parent' already exists
        if(dirRaw.find(name) != std::string::npos){
            // return yfs_client::EXIST if file 'name' already exist
            return yfs_client::EXIST;
        }
    }
    if(ec->put(ino, fileRaw) == extent_protocol::OK){
        // directory format: {[filename][inum]}
        dirRaw.append(fileName + filename(ino) + "]}");
        ec->put(parent, dirRaw);
        return yfs_client::OK;
    }
    return yfs_client::IOERR;
}

yfs_client::status
yfs_client::lookup(inum parent, const char * name, inum & ino){
    std::string dirRaw = "";
    if(ec->get(parent, dirRaw) == extent_protocol::OK){
        if(dirparser(dirRaw, name, ino) == OK){
            return yfs_client::OK;
        }
        return yfs_client::NOENT;
    }
    return yfs_client::NOENT;
}

yfs_client::status
yfs_client::readdir(inum ino, std::vector<yfs_client::dirent> & files){
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

```
完成后用以下命令测试，如果lab1消除rpc duplicate的实现正确，在RPC_LOSSY为5时应该依然可以通过。
```
./start.sh
/test-lab-2-a.pl ./yfs1
./stop.sh
```  

### 第二部分：SETATTR, READ, WRITE  
第二部分实现设置文件属性（只设置size），读和写。在不考虑多个yfs client并发的情况下，一个yfs写入的文件可以被另一个读取。在这里放出yfs_client.cc中的实现，fuse.cc中的handler实现就不放了。  
在这里有几个坑要注意下：  
* 如果setattr时resize的大小比原来大，要用'\0'补齐长度
* read时如果offset为负或者超出了字符串长度，返回'\0'
* write时如果offset超出了字符串长度，要用'\0'补到相应长度，再从offset处开始写；如果offset+size超出了原来的长度，也要补齐相应长度再写入。  

``` c++
yfs_client::status
yfs_client::setattr(inum ino, struct stat * attr){
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
yfs_client::read(inum ino, size_t & size, off_t offset, std::string & buf){
    std::string data;
    if(ec->get(ino, data) == extent_protocol::OK){
        if((offset < 0) || (offset > data.size() - 1)){
            buf = "";
            size = 0;
        }else{
            buf = data.substr(offset, size);
            size = buf.size();
        }
        return yfs_client::OK;
    }
    return yfs_client::NOENT;
}

yfs_client::status
yfs_client::write(inum ino, size_t & size, off_t offset, const char * buf){
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
```
