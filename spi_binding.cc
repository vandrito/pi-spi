#include <node.h>
#include <node_version.h>
#include <node_buffer.h>

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cerrno>

#if __linux__
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#endif


using namespace v8;

uv_mutex_t spiAccess;


// HT http://kkaefer.github.io/node-cpp-modules/#calling-async
struct Baton {
    int err;
    
    int fd;
    uint32_t speed;
    uint8_t mode;
    uint8_t order;
    uint32_t readcount;
    uint32_t buflen;
    uint8_t buffer[0];      // allocated larger
};

Handle<Value> dataMode(const Arguments& args) {
#if NODE_VERSION_AT_LEAST(0, 11, 0)
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
#else
#define isolate
    HandleScope scope;
#endif
    int ret = 0;
    int fd = args[0]->ToInt32()->Value();
    uint8_t mode = args[1]->ToUint32()->Value();
    uv_mutex_lock(&spiAccess);
    ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
    uv_mutex_unlock(&spiAccess);
    return scope.Close(Undefined());
}

Handle<Value> bitOrder(const Arguments& args) {
#if NODE_VERSION_AT_LEAST(0, 11, 0)
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
#else
#define isolate
    HandleScope scope;
#endif    
    int ret = 0;
    int fd = args[0]->ToInt32()->Value();
    uint8_t bitOrder = args[1]->ToUint32()->Value();
    uv_mutex_lock(&spiAccess);
    ret = ioctl(fd, SPI_IOC_WR_LSB_FIRST, &bitOrder);
    uv_mutex_unlock(&spiAccess);
    return scope.Close(Undefined());
}

Handle<Value> Transfer(const Arguments& args) {
#if NODE_VERSION_AT_LEAST(0, 11, 0)
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
#else
#define isolate
    HandleScope scope;
#endif
    
    // (fd, speed, mode, order, writebuf, readcount, cb)
    /* perfs issues !!!
    assert(args.Length() == 6);
    assert(args[0]->IsNumber());
    assert(args[1]->IsNumber());
    assert(args[2]->IsNumber());
    assert(args[3]->IsNumber());
    assert(args[4]->IsNull() || node::Buffer::HasInstance(args[4]));
    assert(args[5]->IsNumber());*/
    
    uint32_t readcount = args[5]->ToUint32()->Value();
    
    size_t writelen;
    char* writedata;
    if (args[4]->IsObject()) {
        Local<Object> writebuf = args[4]->ToObject();
        writelen = node::Buffer::Length(writebuf);
        // assert(writelen <= 0xffffffff /*std::numeric_limits<T>::max()*/);
        writedata = node::Buffer::Data(writebuf);
    } else {
        writelen = 0;
        writedata = NULL;
    }
    
    uint32_t buflen = (readcount > writelen) ? readcount : writelen /* std::max(readcount,writelen) */;
    
    Baton* baton = (Baton*)new uint8_t[sizeof(Baton)+buflen];
    baton->fd = args[0]->ToInt32()->Value();
    baton->speed = args[1]->ToUint32()->Value();
    baton->mode = args[2]->ToUint32()->Value();
    baton->order = args[3]->ToUint32()->Value();
    baton->readcount = readcount;
    baton->buflen = buflen;
    if (writelen) memcpy(baton->buffer, writedata, writelen);
    if (readcount > writelen) memset(baton->buffer+writelen, 0, readcount-writelen);
 
    int ret = 0;
#ifdef SPI_IOC_MESSAGE
    uv_mutex_lock(&spiAccess);
/*    ret = ioctl(baton->fd, SPI_IOC_WR_MODE, &baton->mode);
    if (ret != -1) {
        ret = ioctl(baton->fd, SPI_IOC_WR_LSB_FIRST, &baton->order);
        if (ret != -1) {*/
            struct spi_ioc_transfer msg = {
                /*.tx_buf = */ (uintptr_t)baton->buffer,
                /*.rx_buf = */ (uintptr_t)baton->buffer,
                /*.len = */ baton->buflen,
                /*.speed_hz = */ baton->speed,
                
                // avoid "missing initializer" warnings…
                /*.delay_usecs = */ 0,
                /*.bits_per_word = */ 0,
                /*.cs_change = */ 0,
                /*.pad = */ 0,
            };
            ret = ioctl(baton->fd, SPI_IOC_MESSAGE(1), &msg);
       /* }
    }*/
    uv_mutex_unlock(&spiAccess);
#else
#warning "Building without SPI support"
    ret = -1;
    errno = ENOSYS;
#endif
    baton->err = (ret == -1) ? errno : 0;
    
    Local<Value> d;
    if (!baton->err && baton->readcount) {
        node::Buffer* b = node::Buffer::New((char*)baton->buffer, baton->readcount);     
        Local<Object> globalObj = Context::GetCurrent()->Global();
        Local<Function> bufferConstructor = Local<Function>::Cast(globalObj->Get(String::New("Buffer")));  
 
        Handle<Value> v[] = {b->handle_, Integer::New(baton->readcount), Integer::New(0)};
        d = bufferConstructor->NewInstance(3, v);
    } else {
        ThrowException(Exception::TypeError(String::New("Empty buffer or error")));
        delete[] baton;
        return scope.Close(Undefined());
    }
    
    delete[] baton;
    
    return scope.Close(d);
}

void init(Handle<Object> exports) {
    uv_mutex_init(&spiAccess); 
    exports->Set(
        String::NewSymbol("transfer"),
        FunctionTemplate::New(Transfer)->GetFunction()
    );
}

NODE_MODULE(spi_binding, init)
