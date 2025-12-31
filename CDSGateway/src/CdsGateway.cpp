#include "CdsGateway.hpp"
#include <list>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <iostream>
#include "CdsGatewayDiscovery.hpp"
#include "ProxyProtocol.hpp"
#include "FilteringInterface.hpp"
#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <arpa/inet.h>
#endif

inline std::list<ReceivedPayload> g_receive_task_list;
inline std::mutex g_task_mutex;
inline std::condition_variable_any g_task_cv;
inline std::atomic<size_t> g_active_connections = 0;

std::string ReceivedPayload::get_src_ip_to_string() const
{
    std::ostringstream oss;

    if (src_ip_ver == 4) 
    {
        oss << static_cast<int>(src_ip[0]) << "."
            << static_cast<int>(src_ip[1]) << "."
            << static_cast<int>(src_ip[2]) << "."
            << static_cast<int>(src_ip[3]);
    } 
    else if (src_ip_ver == 6) 
    {
        oss << std::hex;
        for (size_t i = 0; i < 16; i += 2) 
        {
            if (i > 0) oss << ":";
            uint16_t segment = (src_ip[i] << 8) | dest_ip[i + 1];
            oss << segment;
        }
    }

    return oss.str();
}

std::string ReceivedPayload::get_src_port_to_string() const
{
    return std::to_string(src_port);
}

std::string ReceivedPayload::get_dest_ip_to_string() const
{
    std::ostringstream oss;

    if (dest_ip_ver == 4) 
    {
        oss << static_cast<int>(dest_ip[0]) << "."
            << static_cast<int>(dest_ip[1]) << "."
            << static_cast<int>(dest_ip[2]) << "."
            << static_cast<int>(dest_ip[3]);
    } 
    else if (dest_ip_ver == 6) 
    {
        oss << std::hex;
        for (size_t i = 0; i < 16; i += 2) 
        {
            if (i > 0) oss << ":";
            uint16_t segment = (dest_ip[i] << 8) | dest_ip[i + 1];
            oss << segment;
        }
    }

    return oss.str();
}

std::string ReceivedPayload::get_dest_port_to_string() const
{
    return std::to_string(dest_port);
}

void FileSinkSession(asio::ip::tcp::socket sock) 
{
    try 
    {
        ReceivedPayload payload;
        payload.ts = std::chrono::system_clock::now();

        std::uint32_t magic;
        asio::read(sock, asio::buffer(&magic, 4));
        magic = ntohl(magic);
        if (magic != ProxyHeader::kMagic) 
            return;

        ProxyHeader ph{};

        asio::read(sock, asio::buffer(&ph.src_ip_ver, 1));
        asio::read(sock, asio::buffer(ph.src_ip.data(), 16));
        asio::read(sock, asio::buffer(&ph.src_port, 2));
        payload.src_ip_ver = ph.src_ip_ver;
        payload.src_ip_len = ph.src_ip_ver == 4 ? 4 : 16;
        payload.src_port = ntohs(ph.src_port);
        std::copy(ph.src_ip.begin(), ph.src_ip.begin() + payload.src_ip_len, payload.src_ip.begin());

        asio::read(sock, asio::buffer(&ph.dest_ip_ver, 1));
        asio::read(sock, asio::buffer(ph.dest_ip.data(), 16));
        asio::read(sock, asio::buffer(&ph.dest_port, 2));
        payload.dest_ip_ver = ph.dest_ip_ver;
        payload.dest_ip_len = ph.dest_ip_ver == 4 ? 4 : 16;
        payload.dest_port = ntohs(ph.dest_port);
        std::copy(ph.dest_ip.begin(), ph.dest_ip.begin() + payload.dest_ip_len, payload.dest_ip.begin());

        char tag[5];
        asio::read(sock, asio::buffer(tag, 5));
        

        if (std::memcmp(tag, "FILE\0", 5) == 0)
        {
            payload.is_file = true;
            uint32_t nameLen;
            asio::read(sock, asio::buffer(&nameLen, 4));
            payload.file_name.resize(nameLen);
            asio::read(sock, asio::buffer(payload.file_name.data(), nameLen));

            // --- .txt 파일 판별 로직 추가 ---
            auto pos = payload.file_name.rfind('.');
            if (pos != std::string::npos)
            {
                std::string ext = payload.file_name.substr(pos);
                // 대소문자 구분 없이 판별하려면 추가 처리가 필요할 수 있습니다.
                if (ext == ".txt")
                {
                    payload.is_txt_file = true;
                }
            }
            // ------------------------------

            uint64_t fileSize;
            asio::read(sock, asio::buffer(&fileSize, 8));
            payload.data.resize(fileSize);
            asio::read(sock, asio::buffer(payload.data.data(), fileSize));
        } 
        else  if (std::memcmp(tag, "TEXT\0", 5) == 0)
        {
            payload.is_file = false;
            payload.file_name = "";
            
            uint64_t text_size;
            asio::read(sock, asio::buffer(&text_size, 8));
            payload.data.resize(text_size);
            asio::read(sock, asio::buffer(payload.data.data(), text_size));
        }
        else
        {
            std::cerr << "[ERROR] Invalid tag received.\n";
            return;
        }

        {
            std::lock_guard lock(g_task_mutex);
            if (g_receive_task_list.size() < MAX_TASKS) 
            {
                g_receive_task_list.push_back(std::move(payload));
                g_task_cv.notify_all();
            }
        }

    } 
    catch (const std::exception& e) 
    {
        std::cerr << std::format("[ERROR] FileSinkSession failed : {}\n", e.what());
    }

    --g_active_connections;
}

void ControlSession(asio::io_context& ctx, asio::ip::tcp::socket control_sock)
{
    try 
    {
        asio::ip::tcp::acceptor dynamic_acceptor(ctx, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 0));
        std::uint16_t assigned_port = dynamic_acceptor.local_endpoint().port();

        asio::write(control_sock, asio::buffer(&assigned_port, sizeof(assigned_port)));

        asio::ip::tcp::socket file_sock(ctx);
        dynamic_acceptor.accept(file_sock);

        FileSinkSession(std::move(file_sock));
    } 
    catch (const std::exception& e) 
    {
        CDS_GATEWAY_DEBUG_ERROR_LOG("[ERROR]", "ControlSession error:", e.what(), "\n");
    }
}

std::jthread CdsGatewayStartDiscoveryResponder(asio::io_context& ctx, std::uint16_t controlPort)
{
    asio::ip::udp::socket sock(ctx, asio::ip::udp::endpoint(asio::ip::udp::v4(), 15050));
    sock.set_option(asio::ip::udp::socket::reuse_address(true));

    return std::jthread([sock = std::move(sock), controlPort](std::stop_token stoken) mutable 
    {
        std::array<char, 64> buf;
        asio::ip::udp::endpoint from;

        while (!stoken.stop_requested())
        {
            std::size_t n = sock.receive_from(asio::buffer(buf), from, 0);
            if (n < 4 || std::memcmp(buf.data(), "PING", 4) != 0) continue;

            std::vector<std::uint8_t> verify(buf.begin(), buf.begin() + 4 + 12);
            if (hmacPsk(verify) != *reinterpret_cast<std::array<std::uint8_t, 32>*>(buf.data() + 16))
                continue;

            std::vector<std::uint8_t> pong = { 'P','O','N','G' };
            pong.insert(pong.end(), reinterpret_cast<const uint8_t*>(&controlPort), reinterpret_cast<const uint8_t*>(&controlPort) + 2);
            pong.insert(pong.end(), buf.begin() + 4, buf.begin() + 16);
            auto mac = hmacPsk(pong);
            pong.insert(pong.end(), mac.begin(), mac.end());

            sock.send_to(asio::buffer(pong), asio::ip::udp::endpoint(from.address(), 15051));
        }
    });
}

void CdsGatewayConnectionAcceptLoop(std::stop_token stoken, asio::io_context& ctx, asio::ip::tcp::acceptor& acceptor)
{
    while (!stoken.stop_requested()) 
    {
        asio::ip::tcp::socket control_sock(ctx);
        acceptor.accept(control_sock);

        {
            std::unique_lock lock(g_task_mutex);
            if (g_active_connections >= MAX_CONNECTIONS || g_receive_task_list.size() >= MAX_TASKS) 
            {
                CDS_GATEWAY_DEBUG_ERROR_LOG("[WARN]", "Too many connections or tasks\n");
                continue;
            }
            ++g_active_connections;
        }

        std::thread(ControlSession, std::ref(ctx), std::move(control_sock)).detach();
    }
}

void CdsGatewayFilteringProcessingLoop(std::stop_token stoken, std::function<void(ReceivedPayload&&)> callback_func)
{
    FilteringInterface& filtering_interface = FilteringInterface::instance().get();
    std::list<ReceivedPayload> censor_task_list;
    std::list<std::variant<std::future<bool>, std::future<size_t>>> censor_future_result_list;

    while (!stoken.stop_requested()) 
    {
        std::unique_lock lock(g_task_mutex);
        g_task_cv.wait(lock, [&] { return !g_receive_task_list.empty() || !censor_future_result_list.empty() || stoken.stop_requested(); });

        auto receive_task_it = g_receive_task_list.begin();
        while (receive_task_it != g_receive_task_list.end()) 
        {
            censor_task_list.push_back(std::move(*receive_task_it));
            receive_task_it = g_receive_task_list.erase(receive_task_it);
            
            lock.unlock();

            // if (censor_task_list.back().is_file == true)
            // {
            //     std::span<std::uint8_t> file_memory_buffer{censor_task_list.back().data.data(), censor_task_list.back().data.size()};
            //     censor_future_result_list.push_back(filtering_interface.enqueue_censor_document_file(file_memory_buffer));
            // }
            // else if(censor_task_list.back().is_file == true && censor_task_list.back().is_txt_file == true)
            // {
            //     std::span<char> string_span{reinterpret_cast<char*>(censor_task_list.back().data.data()), censor_task_list.back().data.size()};
            //     censor_future_result_list.push_back(filtering_interface.enqueue_censor_string(string_span));
            // }
            // else
            // {
            //     std::span<char> string_span{reinterpret_cast<char*>(censor_task_list.back().data.data()), censor_task_list.back().data.size()};
            //     censor_future_result_list.push_back(filtering_interface.enqueue_censor_string(string_span));
            // }


            if (censor_task_list.back().is_txt_file == true) // .txt 파일인 경우 우선 처리
            {
                // 내용은 텍스트로 검열하지만
                std::span string_span{reinterpret_cast<char *>(censor_task_list.back().data.data()), censor_task_list.back().data.size()};
                censor_future_result_list.push_back(filtering_interface.enqueue_censor_string(string_span));

                // 이 시점에서 is_file은 반드시 true여야 합니다.
                censor_task_list.back().is_file = true;
            }
            else if (censor_task_list.back().is_file == true) // 일반 문서 파일(.docx, .hwpx 등)
            {
                std::span file_memory_buffer{censor_task_list.back().data.data(), censor_task_list.back().data.size()};
                censor_future_result_list.push_back(filtering_interface.enqueue_censor_document_file(file_memory_buffer));
            }
            else // 순수 텍스트 메시지
            {
                std::span string_span{reinterpret_cast<char *>(censor_task_list.back().data.data()), censor_task_list.back().data.size()};
                censor_future_result_list.push_back(filtering_interface.enqueue_censor_string(string_span));
            }

            lock.lock();
        }

        lock.unlock();


        auto censor_task_it = censor_task_list.begin();
        auto censor_future_it = censor_future_result_list.begin();
        while (censor_task_it != censor_task_list.end()) 
        {
            auto visit_lambda = [&](auto&& future) 
            {
                using FutureType = std::decay_t<decltype(future)>;

                if constexpr (std::is_same_v<FutureType, std::future<bool>>) 
                {
                    if (future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
                    {
                        callback_func(std::move(*censor_task_it));
                        censor_task_it = censor_task_list.erase(censor_task_it);
                        censor_future_it = censor_future_result_list.erase(censor_future_it);

                        return;
                    }

                } 
                else if constexpr (std::is_same_v<FutureType, std::future<size_t>>) 
                {
                    if (future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
                    {
                        size_t censored_size = future.get();
                        if (censored_size > 0) 
                        {
                            censor_task_it->data.resize(censored_size);
                        }
                        else 
                        {
                            CDS_GATEWAY_DEBUG_ERROR_LOG("[ERROR]", "Censoring document file failed.\n");
                        }
                        callback_func(std::move(*censor_task_it));
                        censor_task_it = censor_task_list.erase(censor_task_it);
                        censor_future_it = censor_future_result_list.erase(censor_future_it);

                        return;
                    }
                }

                ++censor_task_it;
                ++censor_future_it;
            };

            std::visit(visit_lambda, *censor_future_it);
        }
    }
}

static std::optional<asio::ip::tcp::endpoint> g_guard_endpoint;
static std::shared_mutex g_guard_mutex;

void InvalidateGuardEndpoint(const asio::ip::tcp::endpoint& failed_endpoint)
{
    std::unique_lock lock(g_guard_mutex);

    if (g_guard_endpoint.has_value() && g_guard_endpoint.value() == failed_endpoint) 
    {
        CDS_GATEWAY_DEBUG_ERROR_LOG("[WARN]", "Invalidating stale guard endpoint:", failed_endpoint.address().to_string(), ":", failed_endpoint.port(), "\n");
        g_guard_endpoint.reset();
    }
}

asio::ip::tcp::endpoint GetCurrentGuardEndpoint() 
{
    // 1. 읽기 잠금 확인 (대부분의 경우)
    {
        std::shared_lock lock(g_guard_mutex);
        if (g_guard_endpoint.has_value()) 
        {
            return *g_guard_endpoint;
        }
    }

    // 2. 값이 없다면(초기 상태 또는 통신 실패 후), 쓰기 잠금(unique_lock)으로 전환
    std::unique_lock lock(g_guard_mutex);
    
    // 3. Double-Check: 다른 스레드가 그새 값을 채웠을 수 있으므로 다시 확인
    if (g_guard_endpoint.has_value()) 
    {
        return *g_guard_endpoint;
    }

    // 4. 이제 정말로 값을 찾아야 함. 성공할 때까지 여기서 블락됨.
    CDS_GATEWAY_DEBUG_LOG("[INFO]", "Guard Endpoint not available. Initiating discovery...\n");
    asio::io_context discovery_io;
    
    std::optional<asio::ip::tcp::endpoint> discovered_endpoint = CdsGatewayDiscorverGuard(discovery_io);

    while (!discovered_endpoint.has_value())
    {
        discovered_endpoint = CdsGatewayDiscorverGuard(discovery_io);
    }
    g_guard_endpoint = discovered_endpoint; // 찾은 값을 공유 변수에 저장

    return *g_guard_endpoint;
}

std::optional<asio::ip::tcp::endpoint> CdsGatewayDiscorverGuard(asio::io_context& io)
{
    try
    {
        std::array<uint8_t, 12UL> nonce = randomNonce();
        asio::ip::udp::socket send_socket(io, asio::ip::udp::endpoint(asio::ip::udp::v4(), 0));
        asio::ip::udp::socket recv_socket(io, asio::ip::udp::endpoint(asio::ip::udp::v4(), 15053));
        send_socket.set_option(asio::socket_base::reuse_address(true));
        send_socket.set_option(asio::socket_base::broadcast(true));

        std::vector<uint8_t> packet;
        packet.reserve(4 + 12 + 32); // "GUAR" + nonce + HMAC
        packet.insert(packet.end(), {'G', 'U', 'A', 'R'});
        packet.insert(packet.end(), nonce.begin(), nonce.end());

        std::array<uint8_t, 32UL> mac = hmacPsk({ packet.begin(), packet.end() });
        packet.insert(packet.end(), mac.begin(), mac.end());

        send_socket.send_to(asio::buffer(packet), asio::ip::udp::endpoint(asio::ip::address_v4::broadcast(), 15052));

        std::array<uint8_t, 128> recv_buf;
        asio::ip::udp::endpoint guard_endpoint;

        asio::steady_timer timer(io);
        timer.expires_after(std::chrono::seconds(2)); // 2초 타임아웃

        std::optional<asio::ip::tcp::endpoint> result_endpoint;

        timer.async_wait([&recv_socket](const asio::error_code& ec) 
        {
            // 타이머가 다른 이유(예: cancel)로 중단된게 아니라면 소켓을 닫아버린다.
            if (!ec) 
            {
                CDS_GATEWAY_DEBUG_ERROR_LOG("[WARN]", "Guard discovery timed out.\n");
                recv_socket.cancel(); // recv_socket의 비동기 작업을 취소시킨다.
            }
        });

        recv_socket.async_receive_from
        (
            asio::buffer(recv_buf), 
            guard_endpoint,
            [&](const asio::error_code& ec, std::size_t length) 
            {
                const size_t expected_len = 4 + 2 + 12 + 32;
                timer.cancel();

                if (ec == asio::error::operation_aborted) 
                {
                    return; 
                }
                else if (ec) 
                {
                    CDS_GATEWAY_DEBUG_ERROR_LOG("[ERROR]", "receive error:", ec.message(), "\n");
                    return;
                }
                else if (length != expected_len || std::memcmp(recv_buf.data(), "RAUG", 4) != 0) 
                {
                    CDS_GATEWAY_DEBUG_ERROR_LOG("[WARN]", "received invalid packet.\n");
                    return;
                }
                else if (std::memcmp(recv_buf.data() + 4 + 2, nonce.data(), 12) != 0) 
                {
                    CDS_GATEWAY_DEBUG_ERROR_LOG("[WARN]", "nonce mismatch.\n");
                    return;
                }
                else
                {
                    std::vector<uint8_t> verify_data(recv_buf.data(), recv_buf.data() + 4 + 2 + 12);
                    auto received_mac = hmacPsk(verify_data);
                    if (std::memcmp(received_mac.data(), recv_buf.data() + 4 + 2 + 12, 32) != 0) 
                    {
                        CDS_GATEWAY_DEBUG_ERROR_LOG("[WARN]", "HMAC verification failed.\n");
                        return;
                    }
                }
                

                uint16_t control_port = (static_cast<uint8_t>(recv_buf[4]) << 8) | recv_buf[5];
                control_port = ntohs(control_port);
                
                result_endpoint.emplace(guard_endpoint.address(), control_port);
            }
        );
            
        io.run();
        io.restart();

        return result_endpoint;
    }
    catch(const std::exception& e)
    {
        CDS_GATEWAY_DEBUG_ERROR_LOG("[ERROR]", e.what(), "\n");
        return std::nullopt;
    }
}

void CdsGatewayWakeupFilteringProcessingLoop()
{
    g_task_cv.notify_all();
}

void CdsGatewayWakeupDiscovery(uint16_t port) 
{
    asio::io_context ctx;
    asio::ip::udp::socket sock(ctx);
    sock.open(asio::ip::udp::v4());
    std::string dummy = "PING1234567890123456";
    sock.send_to(asio::buffer(dummy), asio::ip::udp::endpoint(asio::ip::address_v4::loopback(), port));
}

void CdsGatewayWakeupConnectionAcceptor(uint16_t port) 
{
    asio::io_context ctx;
    asio::ip::tcp::socket s(ctx);
    s.connect(asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), port));
}