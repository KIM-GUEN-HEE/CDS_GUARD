#include "GuardL2.hpp"
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <linux/if_packet.h>
#include <cstring>
#include <chrono>
#include <stdexcept>
#include <utility>

static uint64_t htonll(uint64_t x)
{
    if constexpr (std::endian::native == std::endian::little)
    {
        return (static_cast<uint64_t>(htonl(static_cast<uint32_t>(x & 0xFFFFFFFF))) << 32) | htonl(static_cast<uint32_t>(x >> 32));
    }
    else
    {
        return x;
    }
}

static uint64_t ntohll(uint64_t x)
{
    if constexpr (std::endian::native == std::endian::little)
    {
        return (static_cast<uint64_t>(ntohl(static_cast<uint32_t>(x & 0xFFFFFFFF))) << 32) | ntohl(static_cast<uint32_t>(x >> 32));
    }
    else
    {
        return x;
    }
}

constexpr static std::chrono::milliseconds PACKET_TIMEOUT(100); // 패킷 타임아웃 (0.5초)

// 간단한 CRC32 구현
constexpr uint32_t crc32_single(uint32_t i)
{
    uint32_t c = i;
    for (int j = 0; j < 8; ++j) c = (c & 1) ? (c >> 1) ^ 0xEDB88320 : (c >> 1);
    return c;
}

consteval std::array<uint32_t, 256UL> generate_crc32_table()
{
    std::array<uint32_t, 256> table{};
    for (uint32_t i = 0; i < 256; ++i)
        table[i] = crc32_single(i);
    return table;
}

static constexpr std::array<uint32_t, 256UL> kCrcTable = generate_crc32_table();

constexpr uint32_t compute_crc32(std::span<const uint8_t> data)
{
    uint32_t crc = 0xFFFFFFFF;
    for (uint8_t b : data)
        crc = (crc >> 8) ^ kCrcTable[(crc ^ b) & 0xFF];
    return ~crc;
}

GuardL2Sender::GuardL2Sender(const std::string &interface_name, const std::array<uint8_t, 6> &src_mac, const std::array<uint8_t, 6> &dst_mac)
: interface_name_(interface_name), src_mac_(src_mac), dst_mac_(dst_mac)
{
    session_id_ = std::chrono::system_clock::now().time_since_epoch().count();
    sock_fd_ = create_raw_socket(interface_name);

    if (sock_fd_ < 0)
    {
        throw std::runtime_error("Sender: Failed to create raw socket.");
    }

    GUARD_L2_DEBUG_LOG("Raw socket created successfully.\n");
}

GuardL2Sender::~GuardL2Sender()
{
    if (sock_fd_ >= 0)
    {
        close(sock_fd_);
        GUARD_L2_DEBUG_LOG("Raw socket closed.\n");
    }
}

void GuardL2Sender::ack_listener_thread(std::stop_token token)
{
    GUARD_L2_DEBUG_LOG("ACK listener thread started.\n");
    std::array<uint8_t, 1518> recv_buffer;
    uint64_t temp_total_data_size = 0;

    // 메인 스레드의 total_data_size_가 변경될 수 있으므로, 스레드 시작 시점의 값을 복사해서 사용
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        temp_total_data_size = this->total_data_size_;
    }

    while (!token.stop_requested())
    {
        struct timeval timeout = {0, 200000};
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sock_fd_, &read_fds);

        if (select(sock_fd_ + 1, &read_fds, nullptr, nullptr, &timeout) > 0)
        {
            ssize_t bytes = recv(sock_fd_, recv_buffer.data(), recv_buffer.size(), 0);
            if (bytes < sizeof(ether_header) + sizeof(GuardL2Header))
                continue;

            ether_header *eh = (ether_header *)recv_buffer.data();
            if (std::memcmp(eh->ether_dhost, src_mac_.data(), 6) != 0 || ntohs(eh->ether_type) != ETHERTYPE_GUARDL2)
                continue;

            GuardL2Header *gh = (GuardL2Header *)(recv_buffer.data() + sizeof(ether_header));
            if (ntohl(gh->session_id) != session_id_ || gh->type != GuardL2Header::FrameType::ACK)
                continue;

            uint32_t ack_seq = ntohl(gh->sequence_number);

            // 수신 윈도우 크기 읽기
            uint16_t advertised_window = ntohs(gh->receive_window);

            {
                std::lock_guard<std::mutex> lock(buffer_mutex_);
                if (send_buffer_.contains(ack_seq) && !send_buffer_[ack_seq].acked)
                {
                    send_buffer_[ack_seq].acked = true;
                    GUARD_L2_DEBUG_LOG("Received ACK for Seq:", ack_seq, "\n");
                    
                    // START(0)와 END(total_packets+1) ACK는 condition_variable을 깨우도록 명시적 처리
                    uint32_t total_packets = (total_data_size_ + 1400 - 1) / 1400;
                    
                    auto now   = std::chrono::steady_clock::now();
                    auto samp  = now - send_buffer_[ack_seq].time_sent;
                    update_rtt(samp);                 // RTT 갱신
                    on_ack_received(ack_seq, advertised_window);

                    ack_cv_.notify_all(); // 핸드셰이크 ACK는 CV를 깨움
                }
            }
        }
    }

    GUARD_L2_DEBUG_LOG("ACK listener thread stopping.\n");
}

void GuardL2Sender::update_rtt(std::chrono::steady_clock::duration sample_rtt)
{
    using namespace std::chrono;
    const auto samp = duration_cast<microseconds>(sample_rtt);

    std::lock_guard lock(rtt_mutex_);

    if (srtt_.count() == 0) 
    {
        srtt_   = samp;
        rttvar_ = samp / 2;
    }
    else 
    {
        // a = 1/8, b = 1/4
        rttvar_ = microseconds((3 * rttvar_.count() + std::abs(srtt_.count() - samp.count())) / 4);
        srtt_   = microseconds((7 * srtt_.count() + samp.count()) / 8);
    }

    auto new_rto = srtt_ + rttvar_ * 4;
    constexpr microseconds RTO_MIN = 200ms;
    constexpr microseconds RTO_MAX = 3000ms;
    rto_ = std::clamp(new_rto, RTO_MIN, RTO_MAX);
}

std::chrono::milliseconds GuardL2Sender::get_rto()
{
    std::lock_guard lock(rtt_mutex_);
    return std::chrono::duration_cast<std::chrono::milliseconds>(rto_);
}

int GuardL2Sender::create_raw_socket(const std::string &if_name)
{
    int fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (fd < 0)
    {
        GUARD_L2_DEBUG_ERROR_LOG("[ERROR]", "socket creation failed\n");
        return -1;
    }

    struct ifreq ifr;
    std::memset(&ifr, 0, sizeof(ifr));
    std::strncpy(ifr.ifr_name, if_name.c_str(), IFNAMSIZ - 1);

    if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0)
    {
        GUARD_L2_DEBUG_ERROR_LOG("[ERROR]", "ioctl(SIOCGIFINDEX) failed\n");
        close(fd);
        return -1;
    }

    struct sockaddr_ll sll;
    std::memset(&sll, 0, sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_ifindex = ifr.ifr_ifindex;
    sll.sll_protocol = htons(ETH_P_ALL);

    if (bind(fd, (struct sockaddr *)&sll, sizeof(sll)) < 0)
    {
        GUARD_L2_DEBUG_ERROR_LOG("[ERROR]", "bind failed\n");
        close(fd);
        return -1;
    }
    return fd;
}

std::vector<uint8_t> GuardL2Sender::build_frame(GuardL2Header::FrameType type, uint32_t seq_num, std::span<const uint8_t> payload)
{
    size_t payload_size = payload.size();
    const size_t frame_size = sizeof(ether_header) + sizeof(GuardL2Header) + payload_size;
    std::vector<uint8_t> frame_buffer(frame_size);

    uint8_t *eth_header_ptr = frame_buffer.data();
    uint8_t *guard_header_ptr = eth_header_ptr + sizeof(ether_header);
    uint8_t *payload_ptr = guard_header_ptr + sizeof(GuardL2Header);

    ether_header *eh = (ether_header *)eth_header_ptr;
    std::memcpy(eh->ether_shost, src_mac_.data(), 6);
    std::memcpy(eh->ether_dhost, dst_mac_.data(), 6);
    eh->ether_type = htons(ETHERTYPE_GUARDL2);

    if (!payload.empty())
    {
        std::memcpy(payload_ptr, payload.data(), payload_size);
    }

    GuardL2Header *gh = (GuardL2Header *)guard_header_ptr;
    gh->type = type;
    gh->session_id = htonl(session_id_);
    gh->sequence_number = htonl(seq_num);
    gh->total_size = htonll(total_data_size_);
    gh->payload_length = htons(payload_size);
    gh->receive_window = 0; // ACK가 아니므로 0으로 설정
    gh->crc32 = 0;

    uint32_t crc = compute_crc32(std::span<const uint8_t>{guard_header_ptr, sizeof(GuardL2Header) + payload_size});
    gh->crc32 = htonl(crc);

    return frame_buffer;
}

void GuardL2Sender::send_raw_frame(const std::vector<uint8_t> &frame_data)
{
    if (sock_fd_ < 0)
    {
        GUARD_L2_DEBUG_ERROR_LOG("[ERROR]", "Socket is not open. Cannot send frame.\n");
        return;
    }

    // send() 시스템 콜을 사용하여 데이터 전송
    ssize_t sent_bytes = send(sock_fd_, frame_data.data(), frame_data.size(), 0);

    if (sent_bytes < 0)
    {
        GUARD_L2_DEBUG_ERROR_LOG("[ERROR]", "Frame send failed\n");
    }
    else if (static_cast<size_t>(sent_bytes) != frame_data.size())
    {
        GUARD_L2_DEBUG_ERROR_LOG("[WARN]", "Incomplete frame sent. Sent ", sent_bytes, " of ", frame_data.size(), " bytes.\n");
    }
}

// GuardL2.cpp 에 추가

void GuardL2Sender::on_ack_received(uint32_t ack_seq, uint16_t advertised_window)
{
    {
        std::lock_guard<std::mutex> lock(rwnd_mutex_);
        rwnd_ = advertised_window;
    }

    std::lock_guard<std::mutex> lock(cwnd_mutex_);

    // START(0), END ACK는 윈도우 계산에 포함하지 않음
    if (ack_seq == 0 || ack_seq > (total_data_size_ + 1400 - 1) / 1400) 
    {
        return;
    }

    if (cwnd_ < ssthresh_) 
    {
        // 느린 시작 (Slow Start): cwnd를 지수적으로 증가
        cwnd_ += 1.0;
        GUARD_L2_DEBUG_LOG("Slow Start: cwnd increased to ", cwnd_, "\n");
    } 
    else
    {
        // 혼잡 회피 (Congestion Avoidance): cwnd를 선형적으로 증가
        // 매 RTT마다 약 1씩 증가하도록 구현
        ack_count_++;
        if (ack_count_ >= static_cast<uint32_t>(cwnd_)) 
        {
            cwnd_ += 1.0;
            ack_count_ = 0;
            GUARD_L2_DEBUG_LOG("Congestion Avoidance: cwnd increased to ", cwnd_, "\n");
        }
    }
}

void GuardL2Sender::on_packet_loss() 
{
    std::lock_guard<std::mutex> lock(cwnd_mutex_);
    
    // 타임아웃 발생 시
    ssthresh_ = std::max(2u, static_cast<uint32_t>(cwnd_ / 2.0)); // ssthresh를 cwnd의 절반으로 줄임 (최소 2)
    cwnd_ = 1.0;                                                 // cwnd를 1로 리셋 (느린 시작 재시작)
    ack_count_ = 0;                                              // ack_count_ 리셋

    GUARD_L2_DEBUG_ERROR_LOG("[CONGESTION] Packet loss detected. ssthresh: ", ssthresh_, ", cwnd reset to: ", cwnd_, "\n");
}

bool GuardL2Sender::wait_for_ack(uint32_t expected_seq_num, uint32_t timeout_sec)
{
    GUARD_L2_DEBUG_LOG("Waiting for ACK for Seq: ", expected_seq_num, "...\n");

    fd_set read_fds;
    struct timeval timeout;

    while (true)
    {
        FD_ZERO(&read_fds);
        FD_SET(sock_fd_, &read_fds);

        timeout.tv_sec = timeout_sec;
        timeout.tv_usec = 0;

        int ret = select(sock_fd_ + 1, &read_fds, nullptr, nullptr, &timeout);

        if (ret < 0)
        {
            GUARD_L2_DEBUG_ERROR_LOG("[ERROR]", "select() failed\n");
            return false;
        }
        if (ret == 0)
        {
            GUARD_L2_DEBUG_ERROR_LOG("[WARN]", "ACK wait timed out for Seq: ", expected_seq_num, "\n");
            return false;
        }

        if (FD_ISSET(sock_fd_, &read_fds))
        {
            std::array<uint8_t, 1518> recv_buffer; // Max Ethernet frame size
            ssize_t bytes_received = recv(sock_fd_, recv_buffer.data(), recv_buffer.size(), 0);

            if (bytes_received < sizeof(ether_header) + sizeof(GuardL2Header))
                continue;

            ether_header *eh = (ether_header *)recv_buffer.data();
            if (ntohs(eh->ether_type) != ETHERTYPE_GUARDL2)
                continue;

            // 우리에게 온 패킷이 맞는지 MAC 주소 확인
            if (std::memcmp(eh->ether_dhost, src_mac_.data(), 6) != 0)
                continue;

            GuardL2Header *gh = (GuardL2Header *)(recv_buffer.data() + sizeof(ether_header));

            if (ntohl(gh->session_id) != session_id_)
                continue;

            if (gh->type == GuardL2Header::FrameType::ACK && ntohl(gh->sequence_number) == expected_seq_num)
            {
                GUARD_L2_DEBUG_LOG("Correct ACK received for Seq: ", expected_seq_num, "\n");
                return true;
            }
            else
            {
                GUARD_L2_DEBUG_LOG("Ignored packet -> Type: ", static_cast<int>(gh->type), ", Seq: ", ntohl(gh->sequence_number), "\n");
            }
        }
    }
}

constexpr static size_t INITIAL_WINDOW_SIZE = 64; // 초기 윈도우 크기 (예: 64 프레임)

bool GuardL2Sender::send_reliable_data(std::span<const uint8_t> data)
{
    total_data_size_ = data.size();
    
    // 이전에 남아있을 수 있는 버퍼를 정리
    send_buffer_.clear();
    {
        std::lock_guard<std::mutex> lock(cwnd_mutex_);
        cwnd_ = 1.0;
        ssthresh_ = INITIAL_WINDOW_SIZE; // 초기 ssthresh를 이전의 고정 윈도우 크기로 설정
        ack_count_ = 0;
    }
    
    {
        std::lock_guard<std::mutex> lock(rwnd_mutex_);
        rwnd_ = INITIAL_WINDOW_SIZE;
    }
    
    // 0. ACK 리스너 jthread 시작
    listener_thread_ = std::jthread(&GuardL2Sender::ack_listener_thread, this);
    
    // --- 1. START 핸드셰이크 (Stop-and-Wait) ---
    uint32_t start_seq = 0;
    // START 패킷은 페이로드가 없으므로 {} 또는 std::span<const uint8_t>{}를 전달
    auto start_frame = build_frame(GuardL2Header::FrameType::START, start_seq, {});
    {
        std::lock_guard<std::mutex> buffer_lock(buffer_mutex_);
        send_buffer_[start_seq] = {std::move(start_frame), std::chrono::steady_clock::now(), false};
    }
    
    bool start_acked = false;
    for (int i = 0; i < 5; ++i) // 5번 재시도
    {
        send_raw_frame(send_buffer_.at(start_seq).frame_data);
        std::unique_lock<std::mutex> buffer_lock(buffer_mutex_);
        if (ack_cv_.wait_for(buffer_lock, get_rto(), [&]
        { return send_buffer_.at(start_seq).acked; }))
        {
            start_acked = true;
            break;
        }
        
        GUARD_L2_DEBUG_ERROR_LOG("[WARN]", "Timeout for START ACK. Retrying...\n");
    }
    
    if (!start_acked)
    {
        GUARD_L2_DEBUG_ERROR_LOG("[ERROR]", "START handshake failed.\n");
        listener_thread_.request_stop(); // 스레드 중단 요청
        return false;
    }
    
    // --- 2. 데이터 전송 (Sliding Window) ---
    uint32_t send_window_base = 1;
    uint32_t next_seq_num = 1;
    const size_t max_payload_size = 1400;
    uint32_t total_packets = (total_data_size_ + max_payload_size - 1) / max_payload_size;
    
    // 모든 패킷이 ACK될 때까지 루프 실행
    while (send_window_base <= total_packets)
    {
        std::vector<std::pair<uint32_t, std::vector<uint8_t>>> frames_to_send;
        {
            uint32_t current_cwnd;
            {
                // cwnd_ 값을 읽어오기 위해 짧게 잠금
                std::lock_guard<std::mutex> cwnd_lock(cwnd_mutex_);
                current_cwnd = static_cast<uint32_t>(cwnd_);
            } // 여기서 cwnd_mutex_ 잠금 해제
            
            uint32_t current_rwnd;
            {
                // rwnd_ 값을 읽어오기 위해 짧게 잠금
                std::lock_guard<std::mutex> rwnd_lock(rwnd_mutex_);
                current_rwnd = rwnd_;
            } // 여기서 rwnd_mutex_ 잠금 해제

            uint32_t effective_window = std::min(current_cwnd, current_rwnd);
            if (effective_window == 0) 
            {
                // 수신자 버퍼가 꽉 참. Zero Window Probe를 단순화하여 잠시 대기
                GUARD_L2_DEBUG_LOG("Effective window is 0. Pausing transmission.\n");
            }

            for(uint32_t seq = next_seq_num; seq < send_window_base + effective_window && seq <= total_packets; ++seq)
            {
                size_t offset = (seq - 1) * max_payload_size;
                size_t chunk_size = std::min(max_payload_size, data.size() - offset);
                frames_to_send.emplace_back(seq, build_frame(GuardL2Header::FrameType::DATA, seq, data.subspan(offset, chunk_size)));
            }
        }

        if (!frames_to_send.empty()) 
        {
            std::lock_guard<std::mutex> lock(buffer_mutex_);

            for (const auto& pair : frames_to_send) 
            {
                const uint32_t seq = pair.first;
                const auto& frame = pair.second;
                
                send_buffer_[seq] = {frame, std::chrono::steady_clock::now(), false};
                send_raw_frame(frame);
                GUARD_L2_DEBUG_LOG("Sent DATA Seq:", seq, "\n");

                if (seq >= next_seq_num) 
                {
                    next_seq_num = seq + 1;
                }
            }
        }

        // 단계 B: 다음 이벤트(ACK 수신 or 타임아웃)까지 대기
        std::unique_lock<std::mutex> buffer_lock(buffer_mutex_);

        // 가장 빠른 타임아웃 시간 계산
        auto next_timeout = std::chrono::steady_clock::time_point::max();
        bool is_waiting_for_ack = false;
        for (uint32_t i = send_window_base; i < next_seq_num; i++) 
        {
            if (send_buffer_.contains(i) && !send_buffer_[i].acked) 
            {
                next_timeout = std::min(next_timeout, send_buffer_[i].time_sent + get_rto());
                is_waiting_for_ack = true;
            }
        }

        if (is_waiting_for_ack) 
        {
            ack_cv_.wait_until(buffer_lock, next_timeout); // notify가 오거나 타임아웃이 될 때까지 대기
        }
        else if (next_seq_num > total_packets) 
        {
            // 모든 패킷 전송 및 ACK 완료. 윈도우 슬라이딩 후 루프 종료.
        } 
        else 
        {
            // Zero window probe: 윈도우가 0일 때 상대방이 윈도우를 열어줄 때까지 대기
            GUARD_L2_DEBUG_LOG("Effective window is 0. Probing...\n");
            ack_cv_.wait_for(buffer_lock, get_rto()); // RTO만큼 대기 후 다시 윈도우 체크
        }

        // 단계 C: 전송된 패킷들의 타임아웃을 체크하고 필요 시 재전송
        bool timeout_occurred = false;
        const auto now = std::chrono::steady_clock::now();
        std::vector<uint32_t> retransmit_seqs; // 재전송 목록
        for (uint32_t i = send_window_base; i < next_seq_num; ++i) 
        {
            if (send_buffer_.count(i) && !send_buffer_.at(i).acked && now >= send_buffer_.at(i).time_sent + get_rto()) 
            {
                retransmit_seqs.push_back(i);
                timeout_occurred = true;
            }
        }


        if (timeout_occurred) 
        {
            on_packet_loss(); // 타임아웃 발생 시 혼잡 감지 처리

            for (uint32_t seq : retransmit_seqs) 
            {
                 GUARD_L2_DEBUG_ERROR_LOG("[WARN] Timeout for DATA Seq: ", seq, ". Retransmitting...\n");
                 send_raw_frame(send_buffer_.at(seq).frame_data);
                 send_buffer_.at(seq).time_sent = std::chrono::steady_clock::now();
            }
        }

        // 단계 D: ACK된 패킷들을 처리하며 윈도우를 앞으로 슬라이딩
        while (send_buffer_.count(send_window_base) && send_buffer_.at(send_window_base).acked) 
        {
            send_buffer_.erase(send_window_base);
            send_window_base++;
        }
    }

    // --- 3. END 핸드셰이크 (Stop-and-Wait) ---
    uint32_t end_seq = total_packets + 1;
    auto end_frame = build_frame(GuardL2Header::FrameType::END, end_seq, {});
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        send_buffer_[end_seq] = {std::move(end_frame), std::chrono::steady_clock::now(), false};
    }

    bool end_acked = false;
    for (int i = 0; i < 5; ++i)
    {
        send_raw_frame(send_buffer_.at(end_seq).frame_data);
        std::unique_lock<std::mutex> lock(buffer_mutex_);
        if (ack_cv_.wait_for(lock, get_rto(), [&] { return send_buffer_.at(end_seq).acked; }))
        {
            end_acked = true;
            break;
        }
        GUARD_L2_DEBUG_ERROR_LOG("[WARN]", "Timeout for END ACK. Retrying...\n");
    }

    if (!end_acked)
    {
        GUARD_L2_DEBUG_ERROR_LOG("[ERROR]", "END handshake failed.\n");
        listener_thread_.request_stop();
        return false;
    }

    GUARD_L2_DEBUG_LOG("Transfer completed successfully.\n");
    listener_thread_.request_stop(); // ACK 리스너 스레드에 중단 요청
    return true;
}

GuardL2Receiver::GuardL2Receiver(const std::string &interface_name, const std::array<uint8_t, 6> &my_mac)
    : interface_name_(interface_name), my_mac_(my_mac)
{
    sock_fd_ = create_raw_socket(interface_name);
    if (sock_fd_ < 0)
    {
        throw std::runtime_error("Receiver: Failed to create raw socket.");
    }

    GUARD_L2_DEBUG_LOG("Raw socket created successfully for interface ", interface_name, ".\n");
}

GuardL2Receiver::~GuardL2Receiver()
{
    if (sock_fd_ >= 0)
    {
        close(sock_fd_);
        GUARD_L2_DEBUG_LOG("Raw socket closed.\n");
    }
}

int GuardL2Receiver::create_raw_socket(const std::string &if_name)
{
    int fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (fd < 0)
    {
        GUARD_L2_DEBUG_ERROR_LOG("[ERROR]", " socket creation failed\n");
        return -1;
    }

    struct ifreq ifr;
    std::memset(&ifr, 0, sizeof(ifr));
    std::strncpy(ifr.ifr_name, if_name.c_str(), IFNAMSIZ - 1);

    if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0)
    {
        GUARD_L2_DEBUG_ERROR_LOG("[ERROR]", " ioctl(SIOCGIFINDEX) failed\n");
        close(fd);
        return -1;
    }

    struct sockaddr_ll sll;
    std::memset(&sll, 0, sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_ifindex = ifr.ifr_ifindex;
    sll.sll_protocol = htons(ETH_P_ALL);

    if (bind(fd, (struct sockaddr *)&sll, sizeof(sll)) < 0)
    {
        GUARD_L2_DEBUG_ERROR_LOG("[ERROR]", " bind failed\n");
        close(fd);
        return -1;
    }
    return fd;
}

void GuardL2Receiver::send_ack(const std::array<uint8_t, 6> &dst_mac, uint32_t session_id, uint32_t seq_num)
{
    const size_t frame_size = sizeof(ether_header) + sizeof(GuardL2Header);
    std::vector<uint8_t> frame_buffer(frame_size);

    ether_header *eh = (ether_header *)frame_buffer.data();
    std::memcpy(eh->ether_shost, my_mac_.data(), 6);
    std::memcpy(eh->ether_dhost, dst_mac.data(), 6);
    eh->ether_type = htons(ETHERTYPE_GUARDL2);

    GuardL2Header *gh = (GuardL2Header *)(frame_buffer.data() + sizeof(ether_header));
    gh->type = GuardL2Header::FrameType::ACK;
    gh->session_id = htonl(session_id);
    gh->sequence_number = htonl(seq_num);
    gh->total_size = 0;
    gh->payload_length = 0;

    // 가용 윈도우 크기 계산
    size_t used_buffer_slots = out_of_order_buffer_.size();
    uint16_t available_window = (RECEIVER_WINDOW_CAPACITY > used_buffer_slots) ? (RECEIVER_WINDOW_CAPACITY - used_buffer_slots) : 0;
    gh->receive_window = htons(available_window);

    gh->crc32 = 0; // CRC 계산 전 0으로 설정
    uint32_t crc = compute_crc32(std::span<const uint8_t>{(uint8_t *)gh, sizeof(GuardL2Header)});
    gh->crc32 = htonl(crc);

    if (send(sock_fd_, frame_buffer.data(), frame_size, 0) < 0)
    {
        GUARD_L2_DEBUG_ERROR_LOG("[ERROR]", "ACK send failed\n");
    }
    else
    {
        GUARD_L2_DEBUG_LOG("Sent ACK for Seq: ", seq_num, "\n");
    }
}

std::vector<uint8_t> GuardL2Receiver::receive_reliable_data()
{
    GUARD_L2_DEBUG_LOG("\n[*] Waiting for new transmission session...\n");
    std::array<uint8_t, 2048> recv_buffer;
    std::vector<uint8_t> reassembled_data;

    // 세션 상태 변수 초기화화
    uint32_t current_session_id = 0;
    bool session_active = false;
    uint64_t total_data_size = 0;
    uint32_t total_packets = 0;
    uint32_t receive_window_base = 0;
    bool end_packet_received = false; // END 패킷 수신 여부

    while (true)
    {
        struct timeval timeout = {30, 0}; // 30초 타임아웃
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sock_fd_, &read_fds);

        int ret = select(sock_fd_ + 1, &read_fds, nullptr, nullptr, &timeout);
        if (ret <= 0)
        {
            if (ret == 0)
            {
                GUARD_L2_DEBUG_ERROR_LOG("[ERROR]", "Session timed out.\n");
            }
            else
            {
                GUARD_L2_DEBUG_ERROR_LOG("[ERROR]", " select() failed\n");
            }

            return {};
        }

        ssize_t bytes_received = recv(sock_fd_, recv_buffer.data(), recv_buffer.size(), 0);

        // 기본적인 패킷 유효성 검사 (길이, MAC 주소, EtherType)
        if (bytes_received < sizeof(ether_header) + sizeof(GuardL2Header))
            continue;

        ether_header *eh = (ether_header *)recv_buffer.data();
        if (std::memcmp(eh->ether_dhost, my_mac_.data(), 6) != 0)
            continue;
        if (ntohs(eh->ether_type) != ETHERTYPE_GUARDL2)
            continue;

        uint8_t *guard_header_ptr = recv_buffer.data() + sizeof(ether_header);
        GuardL2Header *gh = (GuardL2Header *)guard_header_ptr;
        uint16_t payload_len = ntohs(gh->payload_length);

        if (bytes_received < sizeof(ether_header) + sizeof(GuardL2Header) + payload_len)
        {
            GUARD_L2_DEBUG_ERROR_LOG("[WARN]", "Truncated packet received. Dropped.\n");
            continue;
        }

        // CRC 검증증
        uint32_t received_crc = ntohl(gh->crc32);
        gh->crc32 = 0;
        uint32_t calculated_crc = compute_crc32(std::span<const uint8_t>{guard_header_ptr, sizeof(GuardL2Header) + payload_len});
        gh->crc32 = htonl(received_crc);

        if (received_crc != calculated_crc)
        {
            GUARD_L2_DEBUG_ERROR_LOG("[WARN]", "CRC mismatch. Expected: ", calculated_crc, ", Received: ", received_crc, "Packet dropped.", "\n");
            continue;
        }

        // 패킷 유형에 따라 처리
        uint32_t session_id = ntohl(gh->session_id);
        uint32_t seq_num = ntohl(gh->sequence_number);
        const uint8_t *payload = guard_header_ptr + sizeof(GuardL2Header);
        std::array<uint8_t, 6> sender_mac;
        std::memcpy(sender_mac.data(), eh->ether_shost, 6);

        switch (gh->type)
        {
        case GuardL2Header::FrameType::START:
            if (seq_num == 0)
            {
                GUARD_L2_DEBUG_LOG("New session started. ID: ", session_id, "\n");
                session_active = true;
                current_session_id = session_id;
                receive_window_base = 1;
                total_data_size = ntohll(gh->total_size);
                total_packets = (total_data_size == 0) ? 0 : (total_data_size + 1400 - 1) / 1400;
                reassembled_data.clear();
                reassembled_data.reserve(total_data_size);
                out_of_order_buffer_.clear();
                end_packet_received = false; // Reset for the new session

                send_ack(sender_mac, current_session_id, seq_num);
            }
            break;

        case GuardL2Header::FrameType::DATA:
            if (!session_active || current_session_id != session_id)
                continue;

            if (seq_num >= receive_window_base && seq_num < receive_window_base + RECEIVER_WINDOW_CAPACITY)
            {
                send_ack(sender_mac, current_session_id, seq_num);

                if (seq_num == receive_window_base)
                {
                    reassembled_data.insert(reassembled_data.end(), payload, payload + payload_len);
                    receive_window_base++;

                    while (out_of_order_buffer_.contains(receive_window_base))
                    {
                        auto &buffered_data = out_of_order_buffer_.at(receive_window_base);
                        reassembled_data.insert(reassembled_data.end(), buffered_data.begin(), buffered_data.end());
                        out_of_order_buffer_.erase(receive_window_base);
                        receive_window_base++;
                    }
                }
                else
                {
                    if (!out_of_order_buffer_.contains(seq_num))
                    {
                        out_of_order_buffer_[seq_num] = std::vector<uint8_t>(payload, payload + payload_len);
                    }
                }
            }
            else if (seq_num < receive_window_base)
            {
                send_ack(sender_mac, current_session_id, seq_num);
            }

            break;

        case GuardL2Header::FrameType::END:
        {
            uint32_t end_seq_num = total_packets + 1;
            if (session_active && current_session_id == session_id && seq_num == end_seq_num)
            {
                send_ack(sender_mac, current_session_id, seq_num);
                end_packet_received = true; // END 패킷을 받앗음을 표시
            }
        }
        break;

        default:
            break;
        }

        // 각 패킷 처리 후 세션 종료 조건을 검사
        if (session_active && end_packet_received)
        {
            uint32_t end_seq_num = total_packets + 1;
            if (receive_window_base == end_seq_num)
            {
                if (reassembled_data.size() == total_data_size)
                {
                    GUARD_L2_DEBUG_LOG("Transfer complete. Total received: ", reassembled_data.size(), " bytes.\n");
                    return reassembled_data;
                }
                else
                {
                    GUARD_L2_DEBUG_LOG("[ERROR]", "Received END packet but data size mismatch! Expected: ", total_data_size, ", Got: ", reassembled_data.size(), "\n");
                    return {};
                }
            }
        }
    }
    return {}; // 실패 시 빈 벡터 반환
}