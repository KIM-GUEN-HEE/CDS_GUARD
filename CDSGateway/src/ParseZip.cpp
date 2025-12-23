#include "ParseZip.hpp"
#include "MiniZipRAII.hpp"
#include <expected>

std::expected<ParseZip::RawExtractedDocContent, ParseZip::ExtractErrorInfo> ParseZip::extract_doc_content(const std::span<std::uint8_t> _input_zip_file_buffer)
{
    MzMemStreamRAII mz_mem_stream;
    mz_mem_stream.set_buffer(_input_zip_file_buffer);
    mz_mem_stream.open(MZ_OPEN_MODE_READ);
    if (mz_mem_stream.error_occured())
    {
        return std::unexpected(ParseZip::ExtractErrorInfo{ ParseZip::ExtractError::MinizipError, mz_mem_stream.get_last_error() });
    }

    return extract_doc_content(mz_mem_stream);
}



std::expected<ParseZip::RawExtractedDocContent, ParseZip::ExtractErrorInfo> ParseZip::extract_doc_content(MzMemStreamRAII& _input_mz_mem_stream)
{
    ParseZip::RawExtractedDocContent raw_doc_content;
    size_t mem_stream_offset = _input_mz_mem_stream.tell();
    if (_input_mz_mem_stream.error_occured())
    {
        return std::unexpected(ParseZip::ExtractErrorInfo{ ParseZip::ExtractError::MinizipError, _input_mz_mem_stream.get_last_error() });
    }

    constexpr int64_t kZipEntryFileMaxSize = 100 * 1024 * 1024;
    constexpr int64_t kMaxEntryNumber = 5000;
    constexpr static auto is_image_file = [](std::string_view path) -> bool 
    {
        constexpr std::array<std::string_view, 6> image_exts = { ".jpg", ".jpeg", ".png", ".gif", ".bmp", ".tiff" };

        auto pos = path.rfind('.');

        if (pos == std::string_view::npos) 
            return false;

        std::string_view ext = path.substr(pos);

        for (auto valid : image_exts) 
        {
            if (ext == valid) return true;
        }

        return false;
    };



    // 1. ZIP 메모리로부터 열기
    MzZipRAII input_zip;
    input_zip.open(_input_mz_mem_stream, MZ_OPEN_MODE_READ);
    if (input_zip.error_occured())
    {
        return std::unexpected(ParseZip::ExtractErrorInfo{ ParseZip::ExtractError::MinizipError, input_zip.get_last_error() });
    }

    if(input_zip.get_number_entry() > kMaxEntryNumber)
    {
        return std::unexpected(ParseZip::ExtractErrorInfo{ ParseZip::ExtractError::TooManyEntries });
    }
    
    
    
    // 2. 문서파일 타입 식별 및 entry 설정
    const char* docx_xml_path = "word/document.xml";
    const char* xlsx_xml_path = "xl/sharedStrings.xml";
    const char* hwpx_xml_path = "Contents/section0.xml";
    
    if (input_zip.locate_entry(docx_xml_path) == MZ_OK) // docx
    {
        raw_doc_content.doc_type_info.xml_text_start_tag = "<w:t>";
        raw_doc_content.doc_type_info.xml_text_end_tag = "</w:t>";
        raw_doc_content.doc_type_info.media_dir = "word/media/";
        raw_doc_content.doc_type = ParseZip::DocType::kDocx;
    }
    else if (input_zip.locate_entry(xlsx_xml_path) == MZ_OK) // xlsx
    {
        raw_doc_content.doc_type_info.xml_text_start_tag = "<t>";
        raw_doc_content.doc_type_info.xml_text_end_tag = "</t>";
        raw_doc_content.doc_type_info.media_dir = "x1/media";
        raw_doc_content.doc_type = ParseZip::DocType::kXlsx;
    } 
    else if (input_zip.locate_entry(hwpx_xml_path) == MZ_OK) // hwpx
    {
        raw_doc_content.doc_type_info.xml_text_start_tag = "<hp:t>";
        raw_doc_content.doc_type_info.xml_text_end_tag = "</hp:t>";
        raw_doc_content.doc_type_info.media_dir = "BinData";
        raw_doc_content.doc_type = ParseZip::DocType::kHwpx;
    }
    else
    {
        return std::unexpected(ParseZip::ExtractErrorInfo{ ParseZip::ExtractError::UnsupportedDocumentType });
    }



    // 3. XML 파일 및 offset 추출
    mz_zip_file* file_info = nullptr;

    file_info = input_zip.entry_get_info();
    if (input_zip.error_occured())
    {
        return std::unexpected(ParseZip::ExtractErrorInfo{ ParseZip::ExtractError::MinizipError, input_zip.get_last_error() });
    }
    else if (file_info->uncompressed_size > kZipEntryFileMaxSize)
    {
        return std::unexpected(ParseZip::ExtractErrorInfo{ ParseZip::ExtractError::XmlTooLarge });
    }

    raw_doc_content.xml_file_offset = input_zip.get_entry();
    input_zip.entry_read_open();
    raw_doc_content.xml_text = input_zip.entry_read<std::string>();
    if (input_zip.error_occured())
    {
        return std::unexpected(ParseZip::ExtractErrorInfo{ ParseZip::ExtractError::MinizipError, input_zip.get_last_error() });
    }

    input_zip.entry_close();
    if (input_zip.error_occured())
    {
        return std::unexpected(ParseZip::ExtractErrorInfo{ ParseZip::ExtractError::MinizipError, input_zip.get_last_error() });
    }
    


    // 4. 이미지 파일 및 offset 추출
    input_zip.goto_first_entry();
    if (input_zip.error_occured())
    {
        return std::unexpected(ParseZip::ExtractErrorInfo{ ParseZip::ExtractError::MinizipError, input_zip.get_last_error() });
    }
    std::vector<uint8_t> file_data;

    do
    {
        int64_t curr_file_offset = input_zip.get_entry();
        if (input_zip.error_occured())
        {
            return std::unexpected(ParseZip::ExtractErrorInfo{ ParseZip::ExtractError::MinizipError, input_zip.get_last_error() });
        }

        file_info = input_zip.entry_get_info();
        std::string_view file_name = file_info->filename;

        if (file_name.starts_with(raw_doc_content.doc_type_info.media_dir) && is_image_file(file_info->filename))
        {
            if (file_info->uncompressed_size > kZipEntryFileMaxSize)
            {
                return std::unexpected(ParseZip::ExtractErrorInfo{ ParseZip::ExtractError::ImageTooLarge });
            }
            
            uint64_t image_file_offset = input_zip.get_entry();
            input_zip.entry_read_open(false, nullptr);
            input_zip.entry_read(file_data);

            cv::Mat raw_data(1, static_cast<int>(file_data.size()), CV_8UC1, const_cast<uint8_t*>(file_data.data()));
            cv::Mat image = cv::imdecode(raw_data, cv::IMREAD_UNCHANGED);

            if (image.empty()) 
            {
                return std::unexpected(ParseZip::ExtractErrorInfo{ ParseZip::ExtractError::ImageDecodeFailed });
            }

            raw_doc_content.images.emplace_back(image_file_offset, std::move(image));
            input_zip.entry_close();
        }
    } while (input_zip.goto_next_entry() == MZ_OK);
    

    input_zip.close();
    _input_mz_mem_stream.seek(mem_stream_offset, MZ_SEEK_SET);
    if (input_zip.error_occured())
    {
        return std::unexpected(ParseZip::ExtractErrorInfo{ ParseZip::ExtractError::MinizipError, input_zip.get_last_error() });
    }

    return raw_doc_content;
}