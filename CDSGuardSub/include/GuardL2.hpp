#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <array>
#include <cstdint>
#include <map>
#include <mutex>
#include <semaphore>
#include <chrono>
#include <span>
#include <condition_variable>
#include <thread>
#include <source_location>

#if __cplusplus >= 202302L
    #include <print>
    #define GUARD_L2_HAS_STD_PRINT 1
#else
    #define GUARD_L2_HAS_STD_PRINT 0
#endif

#ifdef GUARD_L2_ENABLE_DEBUG_LOG
    static constexpr bool kGuardL2EnableDebugLog = true;
#else
    static constexpr bool kGuardL2EnableDebugLog = false;
#endif

#define GUARD_L2_DEBUG_LOG(...)       GuardL2DebugLog(std::source_location::current(), __VA_ARGS__)
#define GUARD_L2_DEBUG_ERROR_LOG(...) GuardL2DebugErrorLog(std::source_location::current(), __VA_ARGS__)

template <typename... Args, typename Loc = std::source_location>
inline void GuardL2DebugLog(const std::source_location& loc, Args&&... args)
{
    if constexpr (kGuardL2EnableDebugLog) 
    {
        #if GUARD_L2_HAS_STD_PRINT
            std::print("[{}] ", loc.function_name());
            (std::print(std::forward<Args>(args)...));
        #else
            std::cout << "[" << loc.function_name() << "] ";
            const char* sep = "";
            (((std::cout << sep << std::forward<Args>(args)), sep = " "), ...);
        #endif
    }
}

template <typename... Args, typename Loc = std::source_location>
inline void GuardL2DebugErrorLog(const std::source_location& loc, Args&&... args)
{
    #if GUARD_L2_HAS_STD_PRINT
        std::print(stderr, "[{}] ", loc.function_name());
        (std::print(stderr, "{} ", std::forward<Args>(args)), ...);
    #else
        std::cout << "[" << loc.function_name() << "] ";
        const char* sep = "";
        (((std::cout << sep << std::forward<Args>(args)), sep = " "), ...);
    #endif
}

constexpr uint16_t ETHERTYPE_GUARDL2 = 0x88B5;

// L2 페이로드 앞에 붙을 커스텀 프로토콜 헤더
// __attribute__((packed))는 컴파일러가 패딩을 추가하지 않도록 하여
// 네트워크상에서 정확한 구조를 유지
struct GuardL2Header {
    /**
     * @brief 프로토콜에서 사용하는 패킷의 종류
     */
    enum class FrameType : uint8_t {
        START = 0x01,
        DATA  = 0x02,
        ACK   = 0x03,
        END   = 0x04,
    };

    FrameType type;
    uint32_t session_id;
    uint32_t sequence_number;
    uint64_t total_size;      // START 패킷에서만 사용됨
    uint16_t payload_length;
    uint16_t receive_window;
    uint32_t crc32;
} __attribute__((packed));


class GuardL2Sender {
public:
    GuardL2Sender(const std::string& interface_name, const std::array<uint8_t, 6>& src_mac, const std::array<uint8_t, 6>& dst_mac);
    ~GuardL2Sender();

    // 데이터를 안정적으로 전송하는 메인 함수
    bool send_reliable_data(std::span<const uint8_t> data);

private:
    struct SentPacketInfo 
    {
        std::vector<uint8_t> frame_data;
        std::chrono::steady_clock::time_point time_sent;
        bool acked = false;
    };

    int create_raw_socket(const std::string& interface_name);
    std::vector<uint8_t> build_frame(GuardL2Header::FrameType type, uint32_t seq_num, std::span<const uint8_t> payload);
    
    void send_raw_frame(const std::vector<uint8_t>& frame_data);

    // --- 동적 윈도우를 위한 함수 ---
    void on_ack_received(uint32_t ack_seq, uint16_t advertised_window);
    void on_packet_loss();
    
    // 특정 시퀀스 번호의 ACK를 기다리는 함수
    bool wait_for_ack(uint32_t expected_seq_num, uint32_t timeout_sec = 2);
    void ack_listener_thread(std::stop_token token);
    void update_rtt(std::chrono::steady_clock::duration sample_rtt);
    std::chrono::milliseconds get_rto();


    int sock_fd_ = -1;
    std::string interface_name_;
    std::array<uint8_t, 6> src_mac_;
    std::array<uint8_t, 6> dst_mac_;
    uint32_t session_id_;
    uint64_t total_data_size_ = 0;

    std::map<uint32_t, SentPacketInfo> send_buffer_; // Selective Repeat 상태 변수

    std::mutex buffer_mutex_; // send_buffer_ 보호용 뮤텍스
    std::jthread listener_thread_; // 소멸 시 자동 join되는 스레드
    std::condition_variable ack_cv_;

    // --- 동적 윈도우 멤버 변수 ---
    double cwnd_ = 1.0;                  // 혼잡 윈도우 크기 (Congestion Window)
    uint32_t ssthresh_ = 64;             // 느린 시작 임계값 (Slow Start Threshold)
    uint32_t ack_count_ = 0;             // 혼잡 회피 단계에서 cwnd 증가를 위한 카운터
    std::mutex cwnd_mutex_;              // cwnd_, ssthresh_ 보호용 뮤텍스

    uint32_t rwnd_ = 64;
    std::mutex rwnd_mutex_;

    // RTT 추정치
    std::chrono::microseconds srtt_{};
    std::chrono::microseconds rttvar_{};
    std::chrono::microseconds rto_{std::chrono::milliseconds(200)}; // 초기 200 ms
    std::mutex rtt_mutex_;
};

class GuardL2Receiver {
public:
    GuardL2Receiver(const std::string& interface_name, const std::array<uint8_t, 6>& my_mac);
    ~GuardL2Receiver();

    /**
     * @brief L2를 통해 데이터를 안정적으로 수신하고, 재조립된 전체 데이터를 반환
     * @return 수신 성공 시 데이터가 담긴 벡터, 실패 또는 타임아웃 시 빈 벡터
     */
    std::vector<uint8_t> receive_reliable_data();

private:
    int create_raw_socket(const std::string& interface_name);
    void send_ack(const std::array<uint8_t, 6>& dst_mac, uint32_t session_id, uint32_t seq_num);

    int sock_fd_ = -1;
    std::string interface_name_;
    std::array<uint8_t, 6> my_mac_;

    std::map<uint32_t, std::vector<uint8_t>> out_of_order_buffer_; // 순서가 맞지 않게 도착한 패킷의 '페이로드'를 임시 저장하는 버퍼 (시퀀스 번호 -> 데이터)
    constexpr static size_t RECEIVER_WINDOW_CAPACITY = 512; // 프레임 단위 버퍼 용량
};