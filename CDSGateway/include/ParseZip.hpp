#pragma once

#include <string>
#include <vector>
#include <span>
#include <utility>
#include <expected>
#include <string_view>
#include "MiniZipRAII.hpp"
#include "opencv2/opencv.hpp"


namespace ParseZip
{
    template <typename info_type = std::string_view> requires std::convertible_to<info_type, std::string_view>
    struct DocTypeInfo
    {
        info_type xml_text_start_tag;
        info_type xml_text_end_tag;
        info_type media_dir;
    };
    
    enum class ExtractError 
    {
        MinizipError,          // 실제 코드 값은 따로 저장
        XmlMissing,
        XmlTooLarge,
        TooManyEntries,
        UnsupportedDocumentType,
        ImageTooLarge,
        ImageDecodeFailed,
        Unknown
    };

    enum class DocType
    {
        kHwpx,
        kDocx,
        kXlsx,
        Unsupported
    };
    
    struct ExtractErrorInfo 
    {
        ExtractError code;
        int minizip_code = 0;
    };
    
    struct RawExtractedDocContent 
    {
        DocTypeInfo<std::string_view> doc_type_info;
        std::string xml_text;
        uint64_t xml_file_offset = 0;
        std::vector<std::pair<uint64_t, cv::Mat>> images; // first: file offset, second: image data
        DocType doc_type = DocType::Unsupported;
    };


    std::expected<RawExtractedDocContent, ExtractErrorInfo> extract_doc_content(const std::span<std::uint8_t> _input_zip_file_buffer);
    std::expected<RawExtractedDocContent, ExtractErrorInfo> extract_doc_content(MzMemStreamRAII& _input_mz_mem_stream);
}