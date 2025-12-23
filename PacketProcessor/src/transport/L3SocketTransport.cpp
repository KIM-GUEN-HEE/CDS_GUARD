#include "transport/L3SocketTransport.h"
#include "common/Debug.h"
#include <vector>

// test code L3 send -> L2 receive
static std::vector<uint8_t> L3_BUFFER;

bool L3SocketTransport::send(const std::vector<uint8_t>& pkt) {
    DBG_PRINT("L3 send: %zu bytes", pkt.size());
    L3_BUFFER = pkt;  // buffer save
    return true;
}

std::vector<uint8_t> L3SocketTransport::receive() {
    DBG_PRINT("L3 receive: %zu bytes", L3_BUFFER.size());
    return L3_BUFFER; // return buffer
}
