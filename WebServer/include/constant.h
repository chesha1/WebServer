#ifndef CONSTANT_H
#define CONSTANT_H
#include <cstddef>

namespace WebServer {

    constexpr unsigned int SOCKET_LISTEN_QUEUE_SIZE = 512;

    constexpr unsigned int MAX_BUFFER_RING_SIZE = 65536;

    constexpr size_t IO_URING_QUEUE_SIZE = 2048;

    constexpr unsigned int BUFFER_GROUP_ID = 0;

    constexpr unsigned int BUFFER_RING_SIZE = 4096;

    constexpr size_t BUFFER_SIZE = 1024;

}

#endif