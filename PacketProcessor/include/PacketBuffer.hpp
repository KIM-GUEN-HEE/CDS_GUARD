#pragma once

#include <vector>
#include <mutex>
#include <memory>
#include <cstdint>

class PacketBuffer {
public:
    // 패킷 데이터를 저장하는 구조체
    struct Packet {
        std::vector<uint8_t> data;  // 패킷 데이터 (헤더 포함)
        size_t size;               // 패킷 크기
    };

    PacketBuffer(size_t max_size = 1000);  // 기본 버퍼 크기 1000개 패킷
    ~PacketBuffer() = default;

    // 패킷 추가
    bool addPacket(const uint8_t* data, size_t size);
    
    // 패킷 읽기 (읽은 후 버퍼에서 제거)
    std::unique_ptr<Packet> readPacket();
    
    // 버퍼 비우기
    void clear();
    
    // 현재 버퍼에 저장된 패킷 수 반환
    size_t size() const;
    
    // 버퍼가 비어있는지 확인
    bool empty() const;
    
    // 버퍼가 가득 찼는지 확인
    bool full() const;

private:
    std::vector<std::unique_ptr<Packet>> buffer_;
    mutable std::mutex mutex_;
    size_t max_size_;
}; 