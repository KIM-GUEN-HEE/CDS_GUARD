#pragma once

#include <span>
#include <string_view>
#include "mz.h"
#include "mz_zip.h"
#include "mz_strm.h"
#include "mz_strm_mem.h"
#include "mz_zip_rw.h"

template <typename T>
concept ResizableContiguousByteContainer = requires(T t, size_t n) 
{
    typename T::size_type;
    typename T::value_type;
    requires (sizeof(typename T::value_type) == 1);
    requires std::ranges::contiguous_range<T>;
    { t.resize(n) } -> std::same_as<void>;
    { t.size() } -> std::convertible_to<typename T::size_type>;
    { t.data() } -> std::convertible_to<typename T::value_type*>;
};

class MzMemStreamRAII
{
public:
    MzMemStreamRAII();
    MzMemStreamRAII(MzMemStreamRAII&& _other) noexcept;
    MzMemStreamRAII(const MzMemStreamRAII&) = delete;
    ~MzMemStreamRAII();

    MzMemStreamRAII& operator=(const MzMemStreamRAII&) = delete;
    MzMemStreamRAII& operator=(MzMemStreamRAII&& _other) noexcept;

    template <typename T> requires (sizeof(T) == 1) void set_buffer(std::span<T> _span_container);
    void set_buffer(void* _external_buf, int32_t _size);
    void grow_size(int32_t _size);
    int32_t open(int32_t _mode);
    int32_t open(const char* _path, int32_t _mode);
    bool is_open() const;
    int32_t close();
    int32_t get_last_error() const;
    bool error_occured() const;
    mz_stream* get() const;
    int32_t seek(int64_t _offset, int32_t _origin);
    int64_t tell() const;
    int32_t get_buffer_length() const;
    void set_buffer_limit(int32_t limit);
    operator mz_stream*() const;

private:
    mz_stream* data_;
    int32_t last_error_;
    bool opened_;
};

template <typename T>
requires (sizeof(T) == 1)
void MzMemStreamRAII::set_buffer(std::span<T> _span_container)
{
    set_buffer(_span_container.data(), static_cast<int32_t>(_span_container.size_bytes()));
}

class MzZipRAII
{
public:
    MzZipRAII();
    MzZipRAII(MzZipRAII&& _other) noexcept;
    MzZipRAII(const MzZipRAII&) = delete;
    ~MzZipRAII();

    MzZipRAII& operator=(const MzZipRAII&) = delete;
    MzZipRAII& operator=(MzZipRAII&& _other) noexcept;

    int32_t open(void* _stream, int32_t _mode);
    bool is_open();
    int64_t get_number_entry();
    bool entry_is_open();
    bool entry_is_raw_open();
    int32_t entry_read_open(bool _raw = false, const char* _password = nullptr);
    int32_t entry_read_close(uint32_t* _crc32 = nullptr, int64_t* _compressed_size = nullptr, int64_t* _uncompressed_size = nullptr);
    int32_t entry_read(void* _buf, int32_t _len);
    template <typename T> requires (sizeof(T) == 1) int32_t entry_read(std::span<T> _buf);
    template <ResizableContiguousByteContainer Container> int32_t entry_read(Container& _container);
    template <ResizableContiguousByteContainer Container> Container entry_read();
    int32_t entry_write_open(const mz_zip_file* _file_info, int16_t _compress_level = MZ_COMPRESS_LEVEL_DEFAULT, bool _raw = false, const char* _password = nullptr);
    int32_t entry_write_open(const char* _file_path);
    template<typename T> int32_t entry_write(std::span<T> _span_container);
    int32_t entry_write(void* _buf, int32_t _len);
    int32_t entry_write_close(uint32_t _crc_32, int64_t _compressed_size, int64_t _uncompressed_size);
    int32_t entry_close_raw(int64_t _uncompressed_size, uint32_t _crc_32);
    int32_t entry_close();
    bool entry_is_dir();
    bool entry_is_symlink();
    mz_zip_file* entry_get_info();
    mz_zip_file* entry_get_local_info();
    uint32_t entry_get_crc();
    int64_t entry_get_uncompressed_size();
    int64_t entry_get_compressed_size();
    std::string_view entry_get_file_name();
    int64_t get_entry();
    int32_t goto_entry(int64_t _cd_pos);
    int32_t goto_first_entry();
    int32_t goto_next_entry();
    int32_t locate_entry(const char* _file_name, bool _ignore_case = false);
    int32_t close();
    int32_t get_last_error() const;
    bool error_occured() const;
    void* get() const;
    operator void*() const;

private:
    void* data_;
    bool zip_opened_;
    bool entry_opened_;
    bool entry_raw_opened_;
    int32_t last_error_;
};

template <typename T>
requires (sizeof(T) == 1)
int32_t MzZipRAII::entry_read(std::span<T> _buf)
{
    return entry_read(_buf.data(), static_cast<int32_t>(_buf.size_bytes()));
}

template <ResizableContiguousByteContainer Container>
int32_t MzZipRAII::entry_read(Container& _container)
{
    size_t need_read_size = entry_raw_opened_ ? entry_get_uncompressed_size() : entry_get_compressed_size();
    if (error_occured() || need_read_size == 0) need_read_size = 1024;
    _container.resize(need_read_size);

    for (size_t free_size = _container.size(), readed_size = 0;;)
    {
        int32_t curr_read_size = entry_read(_container.data() + readed_size,free_size);

        if (error_occured()) // error occured
        {
            _container.clear();
            return 0;
        }
        else if (curr_read_size > 0)
        {
            free_size -= curr_read_size;
            readed_size += curr_read_size;

            if (free_size == 0)
            {
                free_size = _container.size();
                _container.resize(free_size * 2);
            }
        }
        else
        {
            _container.resize(readed_size);
            break;
        }
    }

    last_error_ = MZ_OK;
    return _container.size();
}

template <ResizableContiguousByteContainer Container>
Container MzZipRAII::entry_read()
{
    Container container;
    size_t need_read_size = entry_raw_opened_ ? entry_get_uncompressed_size() : entry_get_compressed_size();
    if (error_occured() || need_read_size == 0) need_read_size = 1024;
    container.resize(need_read_size);

    for (size_t free_size = container.size(), readed_size = 0;;)
    {
        int32_t curr_read_size = entry_read(container.data() + readed_size, free_size);

        if (error_occured()) // error occured
        {
            container.clear();
            return container;
        }
        else if (curr_read_size > 0)
        {
            free_size -= curr_read_size;
            readed_size += curr_read_size;

            if (free_size == 0)
            {
                free_size = container.size();
                container.resize(free_size * 2);
            }
        }
        else
        {
            container.resize(readed_size);
            break;
        }
    }

    last_error_ = MZ_OK;
    return container;
}

template<typename T> int32_t MzZipRAII::entry_write(std::span<T> _span_container)
{
    return entry_write(_span_container.data(), _span_container.size());
}