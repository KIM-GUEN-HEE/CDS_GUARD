#pragma once

#include <thread>
#include <mutex>
#include <shared_mutex>
#include <future>
#include <ranges>
#include <atomic>
#include <regex>
#include <span>
#include <unordered_set>
#include "asio.hpp"
#include "asio/ts/buffer.hpp"
#include "asio/ts/internet.hpp"
#include "nlohmann/json.hpp"
#include "DoubleArrayTrie.hpp"
#include "opencv2/opencv.hpp"
#include "opencv2/features2d.hpp"
#include "opencv2/img_hash.hpp"

std::vector<MatchedInfo> RegexSearch(const std::regex& _regex, std::string_view _search_string);


template <typename T>
concept ModifiableStringType = 
    std::same_as<std::remove_reference_t<T>, std::string> || 
    (std::convertible_to<std::remove_reference_t<T>, char*> && 
    !std::is_const_v<std::remove_pointer_t<std::remove_cvref_t<T>>>) || 
    std::same_as<std::remove_reference_t<T>, std::span<char>>;

class FilteringInterface
{
public:
    using modifiable_string_reference_type = std::variant<char*, std::reference_wrapper<std::string>, std::span<char>>;
    using string_censor_task_type = std::pair<modifiable_string_reference_type, std::promise<bool>>;
    using image_censor_task_type = std::pair<std::reference_wrapper<cv::Mat>, std::promise<bool>>;
    using log_callback_func_type = std::function<void(const std::string_view, const std::vector<MatchedInfo>&)>;

    enum class CdsRegex
    {
        kEmail = 0,
        kPhoneNumber = 1,
        kSSNRegex = 2,
        kIpWithPortRegex = 3,
        kUrlRegex = 4,
        kAllNumber = 5
    };

    static std::future<FilteringInterface&> instance();
    
    template <ModifiableStringType string_type>
    std::future<bool> enqueue_censor_string(string_type& _input_string);
    std::future<bool> enqueue_censor_image(cv::Mat& _image);
    std::future<size_t> enqueue_censor_document_file(std::span<std::uint8_t> _file_memory_buffer);
    template <ModifiableStringType string_type>
    std::vector<MatchedInfo> get_string_matched_info_by_rule(const string_type& _input_string) const;
    void deep_learning_enable();
    void deep_learning_disable();
    bool is_deep_learning_enabled() const;
    void set_server_endpoint(const asio::ip::tcp::endpoint& _server_endpoint);
    void set_server_endpoint(const std::array<std::uint8_t, 4>& _ip_address, std::uint16_t _port);
    void set_server_endpoint(const std::array<std::uint8_t, 16>& _ip_address, std::uint16_t _port);
    void set_server_endpoint(std::string_view ip_address_str, std::uint16_t port);

    template <ModifiableStringType string_type>
    std::vector<MatchedInfo> get_string_matched_info_by_dl(const string_type& _input_string) const;
    
    FilteringInterface(const FilteringInterface&) = delete;
    FilteringInterface& operator=(const FilteringInterface&) = delete;
    
private:
    friend class PolicyPage;
    friend class LoggingPage;
    
    FilteringInterface();
    ~FilteringInterface();
    
    template <std::ranges::input_range StringRangeType>
    requires std::convertible_to<std::ranges::range_value_t<StringRangeType>, DoubleArrayTrie::string_view_type>
    static void build_trie(const StringRangeType& _pattern_range);
    
    template <std::ranges::input_range CdsRegexRangeType>
    requires std::convertible_to<std::ranges::range_value_t<CdsRegexRangeType>, CdsRegex>
    static void build_regex(const CdsRegexRangeType& _regex_range);
    
    template <std::ranges::input_range ImageRangeType>
    requires std::convertible_to<std::ranges::range_value_t<ImageRangeType>, cv::Mat>
    static void build_image_hash_set(const ImageRangeType& _image_range);

    template <std::ranges::input_range StringRangeType, std::ranges::input_range CdsRegexRangeType, std::ranges::input_range ImageRangeType>
    requires std::convertible_to<std::ranges::range_value_t<StringRangeType>, DoubleArrayTrie::string_view_type> &&
             std::convertible_to<std::ranges::range_value_t<CdsRegexRangeType>, CdsRegex> &&
             std::convertible_to<std::ranges::range_value_t<ImageRangeType>, cv::Mat>
    static void build_all(const StringRangeType& _pattern_range, const CdsRegexRangeType& _regex_range, const ImageRangeType& _image_range);

    static void safe_convert_for_hash(cv::Mat& _src_image);
    
    
    template <ModifiableStringType string_type>
    void censor(string_type& _input_string, const std::vector<MatchedInfo>& _matched_info_vec) const;
    
    static std::string_view cds_regex_enum_to_string(CdsRegex _enum_val); 
    void censor(cv::Mat& _image) const;
    static size_t compute_color_moment_hash(const cv::Mat& _image);
    bool is_image_in_set(const cv::Mat& _input_img);
    void add_image_variants(const cv::Mat& _image);
    void worker_loop(std::stop_token _stop_token);
    void set_log_callback_func(log_callback_func_type&& _callback_func);
    asio::awaitable<std::string> request_dl_analysis_coro(std::string_view request_str, asio::ip::tcp::endpoint _endpoint) const;

    static inline DoubleArrayTrie double_array_trie_;
    static inline std::atomic<bool> build_flag_ = false;
    static inline std::atomic<bool> deep_learning_enabled_ = false;
    static inline std::shared_mutex trie_mutex_;
    static inline std::mutex task_que_mutex_;
    static inline std::queue<string_censor_task_type> string_task_que_;
    static inline std::queue<image_censor_task_type> image_task_que_;
    static inline std::condition_variable_any task_cv_;
    static inline std::regex filtering_regex_;
    static inline log_callback_func_type log_callback_func_ = [](const std::string_view, const std::vector<MatchedInfo>&){;};
    static inline std::unordered_set<size_t> image_hash_set_;
    static inline asio::io_context io_context_;
    static inline asio::ip::tcp::endpoint server_endpoint_;
    static inline std::shared_mutex server_endpoint_mutex_;
    const size_t k_num_threads_;
    std::vector<std::jthread> worker_threads_;
    std::vector<std::jthread> io_threads_;
    using work_guard_type = asio::executor_work_guard<asio::io_context::executor_type>;
    std::optional<work_guard_type> icx_work_guard_;
};

template <std::ranges::input_range StringRangeType>
requires std::convertible_to<std::ranges::range_value_t<StringRangeType>, DoubleArrayTrie::string_view_type>
void FilteringInterface::build_trie(const StringRangeType& _pattern_range)
{
    build_flag_.store(false);

    {
        std::unique_lock lock(trie_mutex_);
        double_array_trie_.rebuild(_pattern_range);
    }

    build_flag_.store(true);
    build_flag_.notify_all();
}

template <std::ranges::input_range CdsRegexRangeType>
requires std::convertible_to<std::ranges::range_value_t<CdsRegexRangeType>, FilteringInterface::CdsRegex>
void FilteringInterface::build_regex(const CdsRegexRangeType& _regex_range)
{
    build_flag_.store(false);

    {
        std::unique_lock lock(trie_mutex_);

        std::string regex_string = "";

        for (const CdsRegex regex_enum : _regex_range)
        {
            regex_string += cds_regex_enum_to_string(regex_enum);
            regex_string += '|';
        }

        if (!regex_string.empty()) regex_string.pop_back();

        filtering_regex_ = std::regex(regex_string, std::regex_constants::optimize);
    }

    build_flag_.store(true);
    build_flag_.notify_all();
}

template <std::ranges::input_range ImageRangeType>
requires std::convertible_to<std::ranges::range_value_t<ImageRangeType>, cv::Mat>
void FilteringInterface::build_image_hash_set(const ImageRangeType& _image_range)
{
    image_hash_set_.clear();
    for (const cv::Mat& image : _image_range)
    {
        image_hash_set_.insert(compute_color_moment_hash(image));
    }
}


template <std::ranges::input_range StringRangeType, std::ranges::input_range CdsRegexRangeType, std::ranges::input_range ImageRangeType>
requires std::convertible_to<std::ranges::range_value_t<StringRangeType>, DoubleArrayTrie::string_view_type> &&
         std::convertible_to<std::ranges::range_value_t<CdsRegexRangeType>, FilteringInterface::CdsRegex> &&
         std::convertible_to<std::ranges::range_value_t<ImageRangeType>, cv::Mat>
void FilteringInterface::build_all(const StringRangeType& _pattern_range, const CdsRegexRangeType& _regex_range, const ImageRangeType& _image_range)
{
    build_flag_.store(false);

    {
        std::unique_lock lock(trie_mutex_);
        double_array_trie_.rebuild(_pattern_range);

        std::string regex_string = "";

        for (const CdsRegex regex_enum : _regex_range)
        {
            regex_string += cds_regex_enum_to_string(regex_enum);
            regex_string += '|';
        }

        if (!regex_string.empty()) regex_string.pop_back();

        filtering_regex_ = std::regex(regex_string, std::regex_constants::optimize);

        image_hash_set_.clear();
        for (const cv::Mat& image : _image_range)
        {
            image_hash_set_.insert(compute_color_moment_hash(image));
        }
    }

    build_flag_.store(true);
    build_flag_.notify_all();
}

template <ModifiableStringType string_type>
std::vector<MatchedInfo> FilteringInterface::get_string_matched_info_by_rule(const string_type& _input_string) const
{
    std::vector<MatchedInfo> matched_info_vec;
    std::vector<MatchedInfo> sub_matched_info_vec;
    std::string_view input_string_view;

    std::shared_lock lock(trie_mutex_);

    if constexpr (std::same_as<std::remove_cvref_t<string_type>, std::span<char>>)
    {
        input_string_view = std::string_view(_input_string.data(), _input_string.size());
    }
    else
    {
        input_string_view = _input_string;
    }

    sub_matched_info_vec = RegexSearch(filtering_regex_, input_string_view);
    matched_info_vec.insert(matched_info_vec.end(), std::make_move_iterator(sub_matched_info_vec.begin()), std::make_move_iterator(sub_matched_info_vec.end()));

    sub_matched_info_vec = double_array_trie_.search(input_string_view);
    matched_info_vec.insert(matched_info_vec.end(), std::make_move_iterator(sub_matched_info_vec.begin()), std::make_move_iterator(sub_matched_info_vec.end()));

    return matched_info_vec;
}

template <ModifiableStringType string_type>
std::vector<MatchedInfo> FilteringInterface::get_string_matched_info_by_dl(const string_type& _input_string) const
{
    const auto static get_utf8_char_byte_length = 
        [](uint8_t _byte)
        {
            if ((_byte & 0b10000000) == 0) return 1; // 0xxxxxxx
            else if ((_byte & 0b11100000) == 0b11000000) return 2; // 110xxxxx
            else if ((_byte & 0b11110000) == 0b11100000) return 3; // 1110xxxx
            else if ((_byte & 0b11111000) == 0b11110000) return 4; // 11110xxx
            else return 0; // 잘못된 경우 0byte를 넘김
        };

    const auto static build_char_to_byte_map = 
        [](std::string_view _string_view) -> std::unordered_map<size_t, size_t>
        {
            std::unordered_map<size_t, size_t> char_to_byte_map;
            size_t char_count = 0;
            for (size_t i = 0; i < _string_view.length(); ) 
            {
                char_to_byte_map[char_count] = i;
                i += get_utf8_char_byte_length(_string_view[i]);
                char_count++;
            }
            char_to_byte_map[char_count] = _string_view.length();
            return char_to_byte_map;
        };

    const auto static convert_char_to_byte_indices = 
        [](std::string_view utf8_string_view, const std::vector<MatchedInfo>& char_results) -> std::vector<MatchedInfo>
        {
            std::vector<MatchedInfo> byte_results;
            if (char_results.empty()) return byte_results;

            auto char_map = build_char_to_byte_map(utf8_string_view);

            for (const auto& char_info : char_results) 
            {
                size_t char_start = char_info.index_;
                size_t char_end = char_info.index_ + char_info.size_;

                auto it_start = char_map.find(char_start);
                auto it_end = char_map.find(char_end);

                if (it_start != char_map.end() && it_end != char_map.end()) 
                {
                    size_t byte_start = it_start->second;
                    size_t byte_length = it_end->second - byte_start;
                    byte_results.push_back({byte_start, byte_length});
                }
            }
            return byte_results;
        };

    std::vector<MatchedInfo> matched_info_vec;
    std::string_view input_string_view;

    if constexpr (std::same_as<std::remove_cvref_t<string_type>, std::span<char>>)
    {
        input_string_view = std::string_view(_input_string.data(), _input_string.size());
    }
    else
    {
        input_string_view = _input_string;
    }

    try 
    {
        asio::ip::tcp::endpoint endpoint_to_use;
        {
            std::shared_lock lock(server_endpoint_mutex_);
            endpoint_to_use = server_endpoint_;
        }

        // --- 1. HTTP 요청 메시지 생성 ---
        nlohmann::json request_body_json = {{"text", input_string_view}};
        std::string request_body = request_body_json.dump();
         std::string request_str =
            "POST /analyze_text HTTP/1.1\r\n"
            "Host: " + endpoint_to_use.address().to_string() + "\r\n"
            "Accept: application/json\r\n"
            "Content-Type: application/json; charset=utf-8\r\n"
            "Content-Length: " + std::to_string(request_body.length()) + "\r\n"
            "Connection: close\r\n\r\n" + request_body;
        
        // --- 2. 코루틴을 실행하고, 그 결과를 받을 std::future를 직접 생성 ---
        // asio::co_spawn은 이제 promise 대신 future를 직접 반환합니다.
        std::future<std::string> response_future = asio::co_spawn(
            io_context_, // 어느 실행 컨텍스트에서?
            request_dl_analysis_coro(request_str, endpoint_to_use), // 어떤 코루틴을?
            asio::use_future // 결과를 어떻게 받을까? -> future로!
        );

        // --- 3. 동기적으로 결과 대기 ---
        std::string json_response_body = response_future.get();
        if (json_response_body.empty()) return {};
        
        // 임시 저장소로 사용할 벡터를 생성합니다. 이 벡터는 '문자 기준'의 MatchedInfo를 잠시 담는 용도로 재활용됩니다.
        std::vector<MatchedInfo> char_results;

        // 2. nlohmann/json으로 JSON 문자열을 파싱. `parse`의 세 번째 인자(false)는 파싱 실패 시 예외를 던지지 않게 합니다.
        nlohmann::json response_json = nlohmann::json::parse(json_response_body, nullptr, false);

        // 파싱이 실패했거나, 결과가 배열 형태가 아니면 즉시 종료
        if (response_json.is_discarded() || !response_json.is_array()) 
        {
            std::cerr << "DL Response is not a valid JSON array." << std::endl;
            return {};
        }

        // JSON 배열의 각 항목을 순회합니다.
        for (const auto& item : response_json) 
        {
            // item이 객체가 아니거나, 필요한 키가 없는 경우를 대비하여 안전하게 접근
            if (!item.is_object()) continue;

            if (item.value("score", 0.0) < 0.50) 
            {
                continue;
            }

            // start, end 키가 모두 존재하는지 확인합니다.
            if (!item.contains("start") || !item.contains("end")) 
            {
                continue;
            }

            // 값을 추출하여 '문자 기준'의 MatchedInfo를 만듭니다.
            size_t start = item.value("start", (size_t)0);
            size_t end = item.value("end", (size_t)0);
            
            if (start < end) 
            {
                 char_results.push_back({start, end - start});
            }
        }
        
        return convert_char_to_byte_indices(input_string_view, char_results);

    } catch (const std::exception& e) {
        std::cerr << "DL Request Exception: " << e.what() << std::endl;
        return {};
    }
}


template <ModifiableStringType string_type>
void FilteringInterface::censor(string_type& _input_string, const std::vector<MatchedInfo>& _matched_info_vec) const
{
    for (auto [index, size] : _matched_info_vec)
    {
        for (size_t i = 0; i < size; i++)
        {
            _input_string[index + i] = '*';
        }
    }
}

template <ModifiableStringType string_type>
std::future<bool> FilteringInterface::enqueue_censor_string(string_type& _input_string) 
{
    while (!build_flag_.load(std::memory_order_acquire)) 
    {
        build_flag_.wait(false, std::memory_order_relaxed);
    }

    std::promise<bool> promise;
    std::future<bool> future = promise.get_future();

    modifiable_string_reference_type ref_variant = [&]() -> modifiable_string_reference_type 
    {
        if constexpr (std::is_same_v<std::remove_reference_t<string_type>, std::string>) 
        {
            return std::ref(_input_string);
        } 
        else 
        {
            return _input_string;
        }
    }();

    {
        std::lock_guard<std::mutex> lock(task_que_mutex_);
        string_task_que_.emplace(std::move(ref_variant), std::move(promise));
    }

    task_cv_.notify_one();
    return future;
}