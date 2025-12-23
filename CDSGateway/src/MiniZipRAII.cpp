#include "MiniZipRAII.hpp"

MzMemStreamRAII::MzMemStreamRAII() 
: data_(static_cast<mz_stream*>(mz_stream_mem_create())), opened_(false), last_error_(MZ_OK)
{}

MzMemStreamRAII::MzMemStreamRAII(MzMemStreamRAII&& _other) noexcept 
: data_(_other.data_), opened_(_other.opened_), last_error_(_other.last_error_)
{ 
    _other.data_ = nullptr;
    _other.opened_ = false; 
}

MzMemStreamRAII::~MzMemStreamRAII() 
{ 
    if (data_ != nullptr) 
    {
        if (is_open()) close();
        mz_stream_mem_delete(reinterpret_cast<void**>(&data_));
    }
}

MzMemStreamRAII& MzMemStreamRAII::operator=(MzMemStreamRAII&& _other) noexcept 
{
    if (this != &_other) 
    {
        if (data_ != nullptr) 
        {
            if (is_open()) close();

            mz_stream_mem_delete(reinterpret_cast<void**>(&data_));
        }


        data_ = _other.data_;
        opened_ = _other.opened_;
        last_error_ = _other.last_error_;
        _other.data_ = nullptr;
        _other.opened_ = false;
    }
    
    return *this;
}

void MzMemStreamRAII::set_buffer(void* _external_buf, int32_t _size)
{
    mz_stream_mem_set_buffer(data_, _external_buf, _size);
}

void MzMemStreamRAII::grow_size(int32_t _size)
{
    mz_stream_mem_set_grow_size(data_, _size);
}

int32_t MzMemStreamRAII::open(int32_t _mode) 
{
    return last_error_ = open(nullptr, _mode);
}

int32_t MzMemStreamRAII::open(const char* _path, int32_t _mode) 
{
    last_error_ = mz_stream_open(data_, _path, _mode); 
    if (MZ_OK == last_error_) opened_ = true;
    return last_error_;
}

bool MzMemStreamRAII::is_open() const 
{
    return opened_;
}

int32_t MzMemStreamRAII::close() 
{
    last_error_ = mz_stream_close(data_);
    if (MZ_OK == last_error_) opened_ = false;
    return last_error_;
}

mz_stream* MzMemStreamRAII::get() const
{
    return data_;
}

MzMemStreamRAII::operator mz_stream*() const 
{
    return data_;
}

int32_t MzMemStreamRAII::get_last_error() const
{
    return last_error_;
}

bool MzMemStreamRAII::error_occured() const
{
    return last_error_ != MZ_OK;
}

int32_t MzMemStreamRAII::seek(int64_t _offset, int32_t _origin) 
{
    if (!opened_ || data_ == nullptr)
        return MZ_STREAM_ERROR;

    last_error_ = mz_stream_seek(data_, _offset, _origin);
    return last_error_;
}

int64_t MzMemStreamRAII::tell() const 
{
    if (!opened_ || data_ == nullptr)
        return -1;

    return mz_stream_tell(data_);
}

int32_t MzMemStreamRAII::get_buffer_length() const 
{
    int32_t length = 0;
    mz_stream_mem_get_buffer_length(data_, &length);
    return length;
}

void MzMemStreamRAII::set_buffer_limit(int32_t limit) 
{
    mz_stream_mem_set_buffer_limit(data_, limit);
}

MzZipRAII::MzZipRAII() 
: data_(static_cast<mz_stream*>(mz_zip_create())), zip_opened_(false), entry_opened_(false), last_error_(MZ_OK) 
{}

MzZipRAII::MzZipRAII(MzZipRAII&& _other) noexcept 
: data_(_other.data_), zip_opened_(_other.zip_opened_), entry_opened_(_other.entry_opened_), last_error_(_other.last_error_)
{
    _other.data_ = nullptr;
    _other.zip_opened_ = false;
    _other.entry_opened_ = false;
}

MzZipRAII& MzZipRAII::operator=(MzZipRAII&& _other) noexcept 
{
    if (this != &_other) 
    {
        if (data_ != nullptr) 
        {
            if (entry_is_open()) entry_close();
            if (is_open()) close();
            mz_zip_delete(reinterpret_cast<void**>(&data_));
        }

        data_ = _other.data_;
        zip_opened_ = _other.zip_opened_;
        entry_opened_ = _other.entry_opened_;
        last_error_ = _other.last_error_;
        _other.data_ = nullptr;
        _other.zip_opened_ = false;
        _other.entry_opened_ = false;
    }
    
    return *this;
}

MzZipRAII::~MzZipRAII()
{
    if (data_ != nullptr)
    {
        if (entry_is_open()) entry_close();
        if (is_open()) close();
        mz_zip_delete(reinterpret_cast<void**>(&data_)); 
    }
}

int32_t MzZipRAII::open(void* _stream, int32_t _mode)
{
    int32_t err = mz_zip_open(data_, _stream, _mode);
    if (err == MZ_OK) zip_opened_ = true;
    return last_error_ = err;
}

bool MzZipRAII::is_open()
{
    return zip_opened_;
}

int64_t MzZipRAII::get_number_entry() 
{
    uint64_t number_entry;
    int32_t err;
    
    err = mz_zip_get_number_entry(data_, &number_entry);
    return (err < 0)? (last_error_ = err) : number_entry;
}

bool MzZipRAII::entry_is_open() 
{
    return (last_error_ = mz_zip_entry_is_open(data_)) == MZ_OK;
}

bool MzZipRAII::entry_is_raw_open()
{
    return entry_is_open() && entry_opened_;
}

int32_t MzZipRAII::entry_read_open(bool _raw, const char* _password)
{
    last_error_ = mz_zip_entry_read_open(data_, _raw, _password);
    if (last_error_ == MZ_OK) entry_opened_ = true, entry_raw_opened_ = true;
    return last_error_;
}

int32_t MzZipRAII::entry_read_close(uint32_t* _crc32, int64_t* _compressed_size, int64_t* _uncompressed_size)
{
    last_error_ = mz_zip_entry_read_close(data_, _crc32, _compressed_size, _uncompressed_size);
    if (last_error_ == MZ_OK) entry_opened_ = false, entry_raw_opened_ = false;
    return last_error_;
}

int32_t MzZipRAII::entry_read(void* _buf, int32_t _len)
{
    int32_t bytes_readed = mz_zip_entry_read(data_, _buf, _len);
    if (bytes_readed < 0) last_error_ = bytes_readed;
    return bytes_readed;
}

int32_t MzZipRAII::entry_write_open(const mz_zip_file* _file_info, int16_t _compress_level, bool _raw, const char* _password)
{
    last_error_ = mz_zip_entry_write_open(data_, _file_info, _compress_level, _raw, _password);
    if (last_error_ == MZ_OK) entry_opened_ = true, entry_raw_opened_ = _raw;
    return last_error_;
}

int32_t MzZipRAII::entry_write_open(const char* _file_path)
{
    mz_zip_file file_info = {};
    file_info.filename = _file_path;
    return entry_write_open(&file_info);
}

int32_t MzZipRAII::entry_write(void* _buf, int32_t _len)
{
    int32_t bytes_written = mz_zip_entry_write(data_, _buf, _len);
    if (bytes_written < 0) last_error_ = bytes_written;
    return bytes_written;
}

int32_t MzZipRAII::entry_write_close(uint32_t _crc_32, int64_t _compressed_size, int64_t _uncompressed_size)
{
    last_error_ = mz_zip_entry_write_close(data_, _crc_32, _compressed_size, _uncompressed_size);
    if (last_error_ == MZ_OK) entry_opened_ = false, entry_raw_opened_ = false;
    return last_error_;
}

int32_t MzZipRAII::entry_close_raw(int64_t _uncompressed_size, uint32_t _crc_32)
{
    last_error_ = mz_zip_entry_close_raw(data_, _uncompressed_size, _crc_32);
    if (last_error_ == MZ_OK) entry_opened_ = false, entry_raw_opened_ = false;
    return last_error_;
}

int32_t MzZipRAII::entry_close()
{
    last_error_ = mz_zip_entry_close(data_);
    if (last_error_ == MZ_OK) entry_opened_ = false;
    return last_error_;
}

bool MzZipRAII::entry_is_dir()
{
    return (last_error_ = mz_zip_entry_is_dir(data_)) == MZ_OK;
}

bool MzZipRAII::entry_is_symlink()
{
    return (last_error_ = mz_zip_entry_is_symlink(data_)) == MZ_OK;
}

mz_zip_file* MzZipRAII::entry_get_info()
{
    mz_zip_file* file_info;
    last_error_ = mz_zip_entry_get_info(data_, &file_info);
    return last_error_ == MZ_OK ? file_info : nullptr;
}

mz_zip_file* MzZipRAII::entry_get_local_info()
{
    mz_zip_file* local_file_info;
    last_error_ = mz_zip_entry_get_info(data_, &local_file_info);
    return last_error_ == MZ_OK ? local_file_info : nullptr;
}

uint32_t MzZipRAII::entry_get_crc()
{
    mz_zip_file* file_info = entry_get_info();
    return error_occured() ? 0 : file_info->crc;
}

int64_t MzZipRAII::entry_get_uncompressed_size()
{
    mz_zip_file* file_info = entry_get_info();
    return error_occured() ? static_cast<int64_t>(get_last_error()) : file_info->uncompressed_size;
}

int64_t MzZipRAII::entry_get_compressed_size()
{
    mz_zip_file* file_info = entry_get_info();
    return error_occured() ? static_cast<int64_t>(get_last_error()) : file_info->compressed_size;
}

std::string_view MzZipRAII::entry_get_file_name()
{
    mz_zip_file* file_info = entry_get_info();
    return error_occured() ? std::string_view{} : file_info->filename;
}

int64_t MzZipRAII::get_entry()
{
    int64_t pos = mz_zip_get_entry(data_);
    return pos < 0 ? (last_error_ = pos) : pos;
}

int32_t MzZipRAII::goto_entry(int64_t _cd_pos)
{
    return last_error_ = mz_zip_goto_entry(data_, _cd_pos);
}

int32_t MzZipRAII::goto_first_entry()
{
    return last_error_ = mz_zip_goto_first_entry(data_);
}
int32_t MzZipRAII::goto_next_entry()
{
    return last_error_ = mz_zip_goto_next_entry(data_);
}

int32_t MzZipRAII::locate_entry(const char* _file_name, bool _ignore_case)
{
    return last_error_ = mz_zip_locate_entry(data_, _file_name, _ignore_case);
}

int32_t MzZipRAII::close()
{
    last_error_ = mz_zip_close(data_);
    if (last_error_ == MZ_OK) zip_opened_ = false;
    return last_error_;
}

void* MzZipRAII::get() const
{
    return data_;
}

MzZipRAII::operator void*() const
{
    return data_;
}

int32_t MzZipRAII::get_last_error() const
{
    return last_error_;
}

bool MzZipRAII::error_occured() const
{
    return last_error_ != MZ_OK;
}