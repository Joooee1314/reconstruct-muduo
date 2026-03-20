#include "Buffer.h"

#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

/**
 * 从 fd上读取数据 Poller工作在LT模式
 * buffer缓冲区是有大小的，但是从fd上读取流式数据的时候，不知道tcp数据最终大小
 * readv系统调用可以把数据读取在，多个不连续的内存缓冲区
 * 栈上内存效率快，且会随着栈函数调用结束自动释放内存
 */
ssize_t Buffer::readFd(int fd, int* saveErrno){
    char extrabuf[65536]={0}; //栈上的内存
    struct iovec vec[2];
    const size_t writable = writableBytes(); //这是buffer底层缓冲区剩余可写空间
    vec[0].iov_base = begin()+writerIndex_;
    vec[0].iov_len = writable;

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);

    const int iovcnt=(writable < sizeof(extrabuf)) ? 2: 1;
    const ssize_t n = readv(fd,vec,iovcnt);
    if(n<0){
        *saveErrno = errno;
    }
    else if(n<=writable){
        writerIndex_+=n;
    }
    else{ //extrabuf里面也写入了数据
        writerIndex_ = buffer_.size();
        append(extrabuf , n-writable); // writerIndex_开始写 n - writable大小的数据
    }

    return n;
}

ssize_t Buffer::writeFd(int fd, int* saveErrno){
    ssize_t n = write(fd,peek(),readableBytes());
    if(n<0){
        *saveErrno = errno;
    }
    return n;
}