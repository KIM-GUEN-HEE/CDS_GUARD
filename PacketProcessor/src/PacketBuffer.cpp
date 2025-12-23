#include "PacketBuffer.hpp"

PacketBuffer::PacketBuffer(size_t max_size)
    : max_size_(max_size) {
    buffer_.reserve(max_size);
}

bool PacketBuffer::addPacket(const uint8_t* data, size_t size) {
    if (!data || size == 0) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    
    if (buffer_.size() >= max_size_) {
        return false;  // 버퍼가 가득 찼음
    }

    auto packet = std::make_unique<Packet>();
    packet->data.resize(size);
    std::copy(data, data + size, packet->data.begin());
    packet->size = size;

    buffer_.push_back(std::move(packet));
    return true;
}

std::unique_ptr<PacketBuffer::Packet> PacketBuffer::readPacket() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (buffer_.empty()) {
        return nullptr;
    }

    auto packet = std::move(buffer_.front());
    buffer_.erase(buffer_.begin());
    return packet;
}

void PacketBuffer::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    buffer_.clear();
}

size_t PacketBuffer::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return buffer_.size();
}

bool PacketBuffer::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return buffer_.empty();
}

bool PacketBuffer::full() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return buffer_.size() >= max_size_;
} 