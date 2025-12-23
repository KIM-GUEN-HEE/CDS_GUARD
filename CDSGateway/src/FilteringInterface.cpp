#include <atomic>
#include <future>
#include <thread>
#include <mutex>
#include <variant>
#include <shared_mutex>
#include <cstring>
#include <array>
#include <asio/redirect_error.hpp>

#include "FilteringInterface.hpp"
#include "MiniZipRAII.hpp"
#include "ParseZip.hpp"

FilteringInterface::FilteringInterface() 
: k_num_threads_(std::thread::hardware_concurrency())
{
    worker_threads_.reserve(k_num_threads_);

    for (size_t i = 0; i < k_num_threads_; i++) 
    {
        worker_threads_.emplace_back([this](std::stop_token _stop_token){ worker_loop(_stop_token); });
    }

    icx_work_guard_.emplace(asio::make_work_guard(io_context_));
    unsigned const io_thread_count = 2; // 예시
    io_threads_.reserve(io_thread_count);
    for (unsigned i = 0; i < io_thread_count; ++i) {
        io_threads_.emplace_back([]{ io_context_.run(); });
    }
}

FilteringInterface::~FilteringInterface()
{
    icx_work_guard_.reset();
    io_context_.stop();
}


std::future<FilteringInterface&> FilteringInterface::instance()
{
    static FilteringInterface instance;

    auto wait_for_build_and_get =[&]()->FilteringInterface& 
    {
        while (!build_flag_.load(std::memory_order_acquire))
        {
            build_flag_.wait(false, std::memory_order_relaxed);
        }

        return instance;
    };

    return std::async(std::launch::async, wait_for_build_and_get);
}

void FilteringInterface::FilteringInterface::worker_loop(std::stop_token _stop_token) 
{
    int string_task_count = 0;

    while (true) 
    {
        string_censor_task_type task;

        std::unique_lock<std::mutex> lock(task_que_mutex_);
        task_cv_.wait(lock, _stop_token, []{ return !string_task_que_.empty() || !image_task_que_.empty(); });

        if (_stop_token.stop_requested())
            break;

        if (!string_task_que_.empty() && (string_task_count < 10 || image_task_que_.empty()))
        {
            task = std::move(string_task_que_.front());
            string_task_que_.pop();
            lock.unlock();

            auto& [variant_ref, result_promise] = task;

            auto visit_get_matched_info_lambda = [this](auto&& _ref)->std::vector<MatchedInfo>
            {
                using T = std::decay_t<decltype(_ref)>;

                if constexpr (std::is_same_v<T, std::reference_wrapper<std::string>>)
                {
                    std::vector<MatchedInfo> matched_info_vec = get_string_matched_info_by_rule(_ref.get());
                    if (is_deep_learning_enabled())
                    {
                        std::vector<MatchedInfo> dl_matched_info_vec = get_string_matched_info_by_dl(_ref.get());
                        matched_info_vec.insert(matched_info_vec.end(), std::make_move_iterator(dl_matched_info_vec.begin()), std::make_move_iterator(dl_matched_info_vec.end()));
                    }

                    return matched_info_vec;
                } 
                else 
                {
                    std::vector<MatchedInfo> matched_info_vec = get_string_matched_info_by_rule(_ref);
                    if (is_deep_learning_enabled())
                    {
                        std::vector<MatchedInfo> dl_matched_info_vec = get_string_matched_info_by_dl(_ref);
                        matched_info_vec.insert(matched_info_vec.end(), std::make_move_iterator(dl_matched_info_vec.begin()), std::make_move_iterator(dl_matched_info_vec.end()));
                    }
                    return matched_info_vec;
                    
                }
            };

            std::vector<MatchedInfo> matched_info_vec;

            try 
            {   
                matched_info_vec = std::visit(visit_get_matched_info_lambda, variant_ref);
            } 
            catch (...) 
            {
                result_promise.set_exception(std::current_exception());
            }

            bool censored_flag = !matched_info_vec.empty();
            
            auto visit_logging_callback_lambda = [this, &matched_info_vec](auto&& _ref)->void
            {
                using T = std::decay_t<decltype(_ref)>;
                
                if constexpr (std::is_same_v<T, std::reference_wrapper<std::string>>) 
                {
                    log_callback_func_(_ref.get(), matched_info_vec);
                }
                else if constexpr (std::is_same_v<T, char*>)
                {
                    log_callback_func_(_ref, matched_info_vec);
                }
                else if constexpr (std::is_same_v<T, std::span<char>>) 
                {
                    log_callback_func_(std::string_view(_ref.data(), _ref.size()), matched_info_vec);
                } 
            };

            auto visit_censor_lambda = [this, &matched_info_vec](auto&& _ref)->void
            {
                using T = std::decay_t<decltype(_ref)>;
                
                if constexpr (std::is_same_v<T, std::reference_wrapper<std::string>>) 
                {
                    censor(_ref.get(), matched_info_vec);
                }
                else if constexpr (std::is_same_v<T, char*>)
                {
                    censor(_ref, matched_info_vec);
                }
                else if constexpr (std::is_same_v<T, std::span<char>>) 
                {
                    censor(_ref, matched_info_vec);
                } 
            };
            
            if (censored_flag)
            {
                std::visit(visit_logging_callback_lambda, variant_ref);
                std::visit(visit_censor_lambda, variant_ref);
            }

            result_promise.set_value(censored_flag);
            string_task_count++;
        }
        else if (!image_task_que_.empty())
        {
            auto task = std::move(image_task_que_.front());
            image_task_que_.pop();
            lock.unlock();

            auto& [image_data, result_promise] = task;

            try 
            {
                bool in_set_flag = is_image_in_set(image_data);
                if (in_set_flag == true) censor(image_data);
                result_promise.set_value(in_set_flag);
            } 
            catch (const std::exception& e) 
            {
                std::cerr << "[censor thread exception] " << e.what() << std::endl;
                result_promise.set_exception(std::current_exception());
            } 
            catch (...) 
            {
                std::cerr << "[censor thread unknown exception]" << std::endl;
                result_promise.set_exception(std::current_exception());
            }

            string_task_count = 0;
        }
    }
}

void FilteringInterface::censor(cv::Mat& _image) const
{
    int mosaic_block_size = 10;

    cv::Size small_size(_image.cols / mosaic_block_size, _image.rows / mosaic_block_size);
    if (small_size.width == 0 || small_size.height == 0) return;

    cv::Mat temp;
    cv::resize(_image, temp, small_size, 0, 0, cv::INTER_LINEAR);
    cv::resize(temp, _image, _image.size(), 0, 0, cv::INTER_NEAREST);
}

std::string_view FilteringInterface::cds_regex_enum_to_string(CdsRegex _enum_val)
{
    // static const std::string kEmailRegexString(R"(([\w\.-]+)@([\w\.-]+)\.([a-zA-Z]{2,}))");
    // static const std::string kPhoneNumberRegexString = R"((01[0|1|6|7|8|9])\s?-\s?([0-9]{3,4})\s?-\s?([0-9]{4}))";
    // static const std::string kSSNRegexString(R"((\d{6})\s?-\s?(\d{7}))");
    // static const std::string kIpWithPortRegexString(R"(((\d{1,3}\.){3}\d{1,3})(:\d{1,5})?)");
    // static const std::string kUrlRegex(R"(https?:\/\/(?:[a-zA-Z0-9-]+\.)+[a-zA-Z]{2,}(?::\d{1,5})?(?:\/[^\s]*)?)");
    // static const std::string kAllNumbersRegexString(R"([-+]?(?:\d{1,3}(?:,\d{3})*|\d+)(?:\.\d+)?)");

    static const std::array<std::string_view, 6> regex_string_arr
    {
        R"(([\w\.-]+)@([\w\.-]+)\.([a-zA-Z]{2,}))",
        R"((01[0|1|6|7|8|9])\s?-\s?([0-9]{3,4})\s?-\s?([0-9]{4}))",
        R"((\d{6})\s?-\s?(\d{7}))",
        R"((((25[0-5]|2[0-4]\d|1\d{2}|[1-9]?\d)\.){3}(25[0-5]|2[0-4]\d|1\d{2}|[1-9]?\d)(:\d{1,5})?))",
        R"((https?:\/\/(?:[a-zA-Z0-9-]+\.)+[a-zA-Z]{2,}(?::\d{1,5})?(?:\/[^\s]*)?))",
        R"(([-+]?(?:\d{1,3}(?:,\d{3})*|\d+)(?:\.\d+)?))"
    };

    return regex_string_arr[static_cast<size_t>(_enum_val)];
}

std::future<size_t> FilteringInterface::enqueue_censor_document_file(std::span<std::uint8_t> _input_zip_file_buffer)
{
    return std::async(std::launch::async, [this, _input_zip_file_buffer]() -> size_t 
    {

        // 1. ZIP을 열고 XML 파일과 이미지 파일을 추출
        MzMemStreamRAII mem_stream;
        mem_stream.set_buffer(_input_zip_file_buffer);
        mem_stream.open(MZ_OPEN_MODE_READWRITE);
        if (mem_stream.error_occured())
        {
            std::cerr << std::format("mz_stream_open (read) error : {}", mem_stream.get_last_error()) << std::endl;
            return 0;
        }
        
        std::expected<ParseZip::RawExtractedDocContent, ParseZip::ExtractErrorInfo> expected_raw_doc_content = ParseZip::extract_doc_content(mem_stream);
        ParseZip::RawExtractedDocContent raw_doc_content = std::move(expected_raw_doc_content.value());

        if (!expected_raw_doc_content.has_value())
        {
            std::cerr << std::format("ParseZip::extract_doc_content error: {}", static_cast<int64_t>(expected_raw_doc_content.error().code)) << std::endl;
            return 0;
        }

        MzZipRAII input_zip;
        input_zip.open(mem_stream, MZ_OPEN_MODE_READ);
        if (input_zip.error_occured())
        {
            std::cerr << std::format("mz_zip_open error : {}", input_zip.get_last_error()) << std::endl;
            return 0;
        }
        


        // 2. XML 파일 검열 queue에 삽입
        std::vector<std::future<bool>> string_future_result_vec;
        size_t xml_string_curr_pos = 0;
        size_t xml_text_start_tag_size = raw_doc_content.doc_type_info.xml_text_start_tag.size();
        size_t xml_text_end_tag_size = raw_doc_content.doc_type_info.xml_text_end_tag.size();
        
        while ((xml_string_curr_pos = raw_doc_content.xml_text.find(raw_doc_content.doc_type_info.xml_text_start_tag, xml_string_curr_pos)) != std::string::npos) 
        {
            size_t xml_string_next_pos = raw_doc_content.xml_text.find(raw_doc_content.doc_type_info.xml_text_end_tag, xml_string_curr_pos);
            if (xml_string_next_pos == std::string::npos) 
            break;
            
            // 태그는 서로 분리되어있으므로 모두 다른 위치를 가리키며, 
            // 검열 문자의 byte 수(문자열 길이가 아니라 byte 길이)만큼 대체하여 길이가 안 변함 
            // -> 멀티스레드 환경에서도 데이터 레이스 발생 X
            std::span<char> text(
                raw_doc_content.xml_text.data() + xml_text_start_tag_size + xml_string_curr_pos,
                raw_doc_content.xml_text.data() + xml_string_next_pos
            );
            
            if (raw_doc_content.doc_type == ParseZip::DocType::kHwpx)
            {
                size_t base_offset = text.data() - raw_doc_content.xml_text.data();

                size_t local_pos = 0;
                while (local_pos < text.size()) 
                {
                    const char* data = text.data();
                    size_t abs_pos = base_offset + local_pos;

                    size_t hp_pos = raw_doc_content.xml_text.find("<hp:", abs_pos);
                    if (hp_pos == std::string::npos || hp_pos >= base_offset + text.size())
                    {
                        // 남은 전체 span
                        std::span<char> span(raw_doc_content.xml_text.data() + abs_pos, base_offset + text.size() - abs_pos);
                        string_future_result_vec.emplace_back(enqueue_censor_string(span));
                        break;
                    }

                    // span 처리 전
                    if (hp_pos > abs_pos) 
                    {
                        std::span<char> span(raw_doc_content.xml_text.data() + abs_pos, hp_pos - abs_pos);
                        string_future_result_vec.emplace_back(enqueue_censor_string(span));
                    }

                    size_t tag_close = raw_doc_content.xml_text.find('>', hp_pos);
                    if (tag_close == std::string::npos || tag_close >= base_offset + text.size())
                        break;  // malformed

                    local_pos = (tag_close + 1) - base_offset;  // span 내부 상대 위치로 복귀
                }
            }
            else
            {
                string_future_result_vec.emplace_back(enqueue_censor_string(text));
            }

            xml_string_curr_pos = xml_string_next_pos + xml_text_end_tag_size;
        }
        
        
        
        // 3. 이미지 파일 검열 queue에 삽입
        std::vector<std::future<bool>> image_future_result_vec;
        
        for (size_t i = 0; i < raw_doc_content.images.size(); i++)
        {
            if (raw_doc_content.images[i].second.empty())
            {
                std::cerr << "Failed to decode image from memory." << std::endl;
                return 0;
            }
            
            switch (raw_doc_content.images[i].second.type()) 
            {
                case CV_8UC1:
                case CV_8UC3:
                case CV_8UC4:
                    break;
                default:
                    safe_convert_for_hash(raw_doc_content.images[i].second);
                    break;
            }
            
            image_future_result_vec.emplace_back(enqueue_censor_image(raw_doc_content.images[i].second));
        }
    


        // 4. 검열 대상인 XML 파일과 이미지 파일을 제외한 나머지 파일들을 새 ZIP 파일에 복사
        MzMemStreamRAII temp_mem_stream;
        temp_mem_stream.grow_size(_input_zip_file_buffer.size());
        temp_mem_stream.open(MZ_OPEN_MODE_CREATE);
        if (temp_mem_stream.error_occured())
        {
            std::cerr << std::format("make temp buf error : {}", temp_mem_stream.get_last_error());
            return 0;
        }
        
        MzZipRAII output_zip;
        output_zip.open(temp_mem_stream, MZ_OPEN_MODE_CREATE | MZ_OPEN_MODE_WRITE);
        if (output_zip.error_occured())
        {
            std::cerr << std::format("mz_zip_open (write) error: {}", output_zip.get_last_error()) << std::endl;
            return 0;
        }
        
        input_zip.goto_first_entry();
        if (input_zip.error_occured())
        {
            std::cerr << std::format("Could not go first entry: {}", input_zip.get_last_error()) << std::endl;
            return 0;
        }    
        
        std::unordered_set<uint64_t> censor_file_offset_set;
        censor_file_offset_set.reserve(1 + raw_doc_content.images.size());

        censor_file_offset_set.insert(raw_doc_content.xml_file_offset);

        for (const auto& [offset, _] : raw_doc_content.images)
        {
            censor_file_offset_set.insert(offset);
        }


        std::vector<uint8_t> file_data;
        do
        {
            uint64_t curr_file_offset = input_zip.get_entry();
            if (input_zip.error_occured())
            {
                std::cerr << std::format("could not get file offset: {}", input_zip.get_last_error()) << std::endl;
                return 0;
            }

            if (!censor_file_offset_set.contains(curr_file_offset))
            {
                mz_zip_file* file_info = input_zip.entry_get_info();
                std::string_view file_name = file_info->filename;
    
                input_zip.entry_read_open(true, nullptr);
                output_zip.entry_write_open(file_info, file_info->compression_method, true, nullptr);
                
                input_zip.entry_read(file_data);
                if (input_zip.error_occured())
                {
                    std::cerr << std::format("error occured when file was being copyed (read): {}", input_zip.get_last_error()) << std::endl;
                    return 0;
                }
    
                output_zip.entry_write(std::span{file_data});
                if (output_zip.error_occured())
                {
                    std::cerr << std::format("error occured when file was being copyed (write): {}", output_zip.get_last_error()) << std::endl;
                    return 0;
                }
    
                input_zip.entry_close();
                output_zip.entry_close_raw(file_info->uncompressed_size, file_info->crc);
            }

        } while (input_zip.goto_next_entry() == MZ_OK);



        // 5. XML 파일 데이터 검열 종료를 기다림
        for (auto& future : string_future_result_vec) 
        {
            future.wait();
        }



        // 6. 검열 된 XML 파일을 zip 파일에 씀
        input_zip.goto_entry(raw_doc_content.xml_file_offset);
        if (input_zip.error_occured())
        {
            std::cerr << std::format("Could not open xml file in write mode: {}", input_zip.get_last_error()) << std::endl;
            return 0;
        }

        mz_zip_file xml_file_info;
        mz_zip_file* file_info;
        file_info = input_zip.entry_get_info();
        xml_file_info = *file_info;
        xml_file_info.uncompressed_size = raw_doc_content.xml_text.size();
        xml_file_info.compressed_size = 0;
        output_zip.entry_write_open(&xml_file_info, xml_file_info.compression_method, false, nullptr);
        output_zip.entry_write(std::span{raw_doc_content.xml_text});
        if (output_zip.error_occured())
        {
            std::cerr << std::format("Could not write xml file: {}", output_zip.get_last_error()) << std::endl;
            return 0;
        }

        output_zip.entry_close();
        if (output_zip.error_occured())
        {
            std::cerr << std::format("Could not close xml file in xml file: {}", output_zip.get_last_error()) << std::endl;
            return 0;
        }
        
        
        
        // 7. 검열 된 image 파일을 zip 파일에 write
        for (size_t i = 0; i < raw_doc_content.images.size(); i++)
        {
            bool was_censored = image_future_result_vec[i].get();
            int64_t offset = raw_doc_content.images[i].first;

            input_zip.goto_entry(offset);
            if (input_zip.error_occured()) 
            {
                std::cerr << std::format("Failed to go to entry at offset : {}", input_zip.get_last_error()) << std::endl;
                return 0;
            }

            mz_zip_file* image_file_info = input_zip.entry_get_info();

            if (was_censored)
            {
                mz_zip_file* file_info = input_zip.entry_get_info();
                std::string_view path = file_info->filename;
                std::string format;

                if (size_t dot_pos = path.rfind('.'); dot_pos != std::string_view::npos)
                    format = std::string(path.substr(dot_pos));
                
                cv::Mat image = std::move(raw_doc_content.images[i].second);
                std::vector<uint8_t> encoded;
                if (!cv::imencode(format, image, encoded))
                {
                    std::cerr << std::format("Failed to encode censored image at index {}", i) << std::endl;
                    return 0;
                }
                
                mz_zip_file new_info = *file_info;
                new_info.uncompressed_size = encoded.size();
                new_info.compressed_size = 0;


                output_zip.entry_write_open(&new_info, file_info->compression_method, false, nullptr);
                output_zip.entry_write(std::span{encoded});
                output_zip.entry_close();
            }
            else
            {
                input_zip.entry_read_open(true, nullptr);
                output_zip.entry_write_open(image_file_info, image_file_info->compression_method, true, nullptr);

                std::vector<uint8_t> file_data;
                input_zip.entry_read(file_data);

                output_zip.entry_write(std::span{file_data});

                input_zip.entry_close();
                output_zip.entry_close_raw(image_file_info->uncompressed_size, image_file_info->crc);
            }

            if (output_zip.error_occured()) 
            {
                std::cerr << std::format("Failed to write image index {}: {}", i, output_zip.get_last_error()) << std::endl;
                return 0;
            }
        }


        
        // 8. input_zip과 output_zip을 닫고, output_zip 데이터를 복사 후, 검열된 문서파일의 크기를 반환
        input_zip.close();
        output_zip.close();

        if (input_zip.error_occured() || output_zip.error_occured())
        {
            std::cerr << std::format("Could not close zip file: input_zip({}) output_zip({})", input_zip.get_last_error(), output_zip.get_last_error());
            return 0;
        }

        int32_t new_zip_size;
        mem_stream.seek(0, MZ_SEEK_SET);
        temp_mem_stream.seek(0, MZ_SEEK_SET);

        int32_t errco = mz_stream_copy_to_end(mem_stream, temp_mem_stream);
        if (errco != MZ_OK)
        {
            std::cerr << std::format("mz_stream_copy_to_end error: {}", errco) << std::endl;
            return 0;
        }

        new_zip_size = temp_mem_stream.get_buffer_length();
        mem_stream.set_buffer_limit(temp_mem_stream.get_buffer_length());

        return new_zip_size;
    });
}

std::vector<MatchedInfo> RegexSearch(const std::regex& _regex, std::string_view _search_string)
{
    std::vector<MatchedInfo> result;

    if (_regex.mark_count())
    {
        auto begin = _search_string.begin();
        auto end = _search_string.end();

        using Iterator = decltype(begin);

        while (true)
        {
            std::match_results<Iterator> regex_results;

            if (!std::regex_search(begin, end, regex_results, _regex))
                break;

            result.emplace_back(MatchedInfo{static_cast<size_t>(regex_results.position() + std::distance(_search_string.begin(), begin)), static_cast<size_t>(regex_results.length())});

            begin += regex_results.position() + regex_results.length();
        }
    }

    return result;
}

void FilteringInterface::set_log_callback_func(log_callback_func_type&& _callback_func)
{
    log_callback_func_ = std::move(_callback_func);
}

size_t FilteringInterface::compute_color_moment_hash(const cv::Mat& _input_image) 
{
    cv::Mat hash;
    cv::Ptr<cv::img_hash::ColorMomentHash> hasher = cv::img_hash::ColorMomentHash::create();
    hasher->compute(_input_image, hash);

    return std::hash<std::string_view>{}(std::string_view(reinterpret_cast<char*>(hash.data), hash.total() * hash.elemSize()));
}

void FilteringInterface::add_image_variants(const cv::Mat& _iput_image) 
{
    image_hash_set_.insert(compute_color_moment_hash(_iput_image));

    cv::Mat flipped_lr;
    cv::flip(_iput_image, flipped_lr, 1);
    image_hash_set_.insert(compute_color_moment_hash(flipped_lr));

    cv::Mat flipped_ud;
    cv::flip(_iput_image, flipped_ud, 0);
    image_hash_set_.insert(compute_color_moment_hash(flipped_ud));

    cv::Mat flipped_both;
    cv::flip(_iput_image, flipped_both, -1);
    image_hash_set_.insert(compute_color_moment_hash(flipped_both));

    cv::Mat inverted = cv::Scalar::all(255) - _iput_image;
    image_hash_set_.insert(compute_color_moment_hash(inverted));
}

bool FilteringInterface::is_image_in_set(const cv::Mat& _image) 
{
    size_t hash_value = compute_color_moment_hash(_image);
    return image_hash_set_.contains(hash_value);
}

std::future<bool> FilteringInterface::enqueue_censor_image(cv::Mat& _image)
{
    while (!build_flag_.load(std::memory_order_acquire)) 
    {
        build_flag_.wait(false, std::memory_order_relaxed);
    }

    std::promise<bool> promise;
    std::future<bool> future = promise.get_future();

    {
        std::lock_guard<std::mutex> lock(task_que_mutex_);
        image_task_que_.emplace(std::ref(_image), std::move(promise));
    }

    task_cv_.notify_one();
    return future;
}

void FilteringInterface::safe_convert_for_hash(cv::Mat& _src_image)
{
    cv::Mat converted = _src_image;

    if (converted.depth() != CV_8U) {
        double minVal, maxVal;
        cv::minMaxLoc(converted, &minVal, &maxVal);  // 값 범위 구함
        if (minVal == maxVal) maxVal = minVal + 1;   // 나눗셈 0 방지
        converted.convertTo(converted, CV_8U, 255.0 / (maxVal - minVal), -minVal * 255.0 / (maxVal - minVal));
    }

    if (converted.channels() == 1) 
    {
        cv::cvtColor(converted, converted, cv::COLOR_GRAY2BGR);
    } 
    else if (converted.channels() == 2) 
    {
        std::vector<cv::Mat> chs;
        cv::split(converted, chs);
        chs.resize(3, cv::Mat::zeros(converted.size(), CV_8U));
        cv::merge(chs, converted);
    } 
    else if (converted.channels() > 4) 
    {
        std::vector<cv::Mat> chs;
        cv::split(converted, chs);
        chs.resize(3);
        cv::merge(chs, converted);
    }

    _src_image = converted;
};

void FilteringInterface::deep_learning_enable()
{
    deep_learning_enabled_.store(true, std::memory_order_release);
}

void FilteringInterface::deep_learning_disable()
{
    deep_learning_enabled_.store(false, std::memory_order_release);
}

bool FilteringInterface::is_deep_learning_enabled() const
{
    return deep_learning_enabled_.load(std::memory_order_acquire);
}

void FilteringInterface::set_server_endpoint(const asio::ip::tcp::endpoint& _server_endpoint)
{
    std::unique_lock lock(server_endpoint_mutex_);
    server_endpoint_ = _server_endpoint;
}

void FilteringInterface::set_server_endpoint(const std::array<std::uint8_t, 4>& _ip_address, std::uint16_t _port)
{
    asio::ip::address_v4 ip_v4(_ip_address);
    asio::ip::tcp::endpoint new_endpoint(ip_v4, _port);
    set_server_endpoint(new_endpoint);
}

void FilteringInterface::set_server_endpoint(const std::array<std::uint8_t, 16>& _ip_address, std::uint16_t _port)
{
    asio::ip::address_v6 ip_v6(_ip_address);
    asio::ip::tcp::endpoint new_endpoint(ip_v6, _port);
    set_server_endpoint(new_endpoint);
}

void FilteringInterface::set_server_endpoint(std::string_view ip_address_str, std::uint16_t port)
{
    asio::error_code ec;
    // 1. 문자열을 asio::ip::address 객체로 변환합니다.
    asio::ip::address ip = asio::ip::make_address(ip_address_str, ec);

    if (ec) {
        // 잘못된 IP 주소 형식에 대한 오류 처리
        std::cerr << "Invalid IP address format: " << ip_address_str << std::endl;
        return;
    }

    // 2. 생성된 IP 주소와 포트로 엔드포인트를 만듭니다.
    asio::ip::tcp::endpoint new_endpoint(ip, port);
    
    // 3. 첫 번째 함수를 호출하여 값을 설정합니다.
    set_server_endpoint(new_endpoint);
}

asio::awaitable<std::string> FilteringInterface::request_dl_analysis_coro(std::string_view request_str, asio::ip::tcp::endpoint _endpoint) const
{
    try {
        // 이 함수 안에서는 모든 것이 순차적으로 보입니다.
        asio::ip::tcp::socket socket(co_await asio::this_coro::executor);

        // 1. 비동기 연결 후 대기
        co_await socket.async_connect(_endpoint, asio::use_awaitable);
        
        // 2. 비동기 쓰기 후 대기
        co_await asio::async_write(socket, asio::buffer(request_str), asio::use_awaitable);
        
        // 3. 비동기 읽기 후 대기
        asio::streambuf response_buffer;
        asio::error_code ec;
        co_await asio::async_read(socket, response_buffer, asio::redirect_error(asio::use_awaitable, ec));
        if (ec && ec != asio::error::eof) 
        {
            throw std::system_error(ec);
        }

        // 4. 응답 처리 및 반환
        std::string http_body(std::istreambuf_iterator<char>(&response_buffer), {});
        size_t body_start = http_body.find("\r\n\r\n");
        if (body_start != std::string::npos) {
            co_return http_body.substr(body_start + 4);
        }

    } catch (std::exception& e) {
        std::cerr << "Coroutine network error: " << e.what() << std::endl;
    }
    
    // 오류가 발생했거나 본문이 없으면 빈 문자열 반환
    co_return "";
}