#pragma once

#include "ISocketTransport.h"

class L2SocketTransport : public ISocketTransport {
public:
    bool send(const std::vector<uint8_t>& pkt) override;
    std::vector<uint8_t> receive() override;
};
