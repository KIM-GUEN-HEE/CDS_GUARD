#include "transport/L2SocketTransport.h"
#include "transport/L3SocketTransport.h"
#include "common/Debug.h"
#include <vector>

// test code
bool L2SocketTransport::send(const std::vector<uint8_t>& pkt) {
    DBG_PRINT("L2 send: %zu bytes", pkt.size());
    return true;
}

std::vector<uint8_t> L2SocketTransport::receive() {
    // l3 buffer 
    auto data = L3SocketTransport().receive();
    DBG_PRINT("L2 receive: %zu bytes", data.size());
    return data;
}
