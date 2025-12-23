#pragma once

#include <iostream>
#include <vector>
#include <stack>
#include <queue>
#include <unordered_map>
#include <cstdint>
#include <algorithm>

struct MatchedInfo
{
    size_t index_;
    size_t size_;
};

enum class LanguageType
{
    KAsciiWithoutAlphabet 	= 0b00000001,
    kUpperAlphabet          = 0b00000010,
    kLowerAlphabet			= 0b00000100,
    kAlphabet 				= 0b00000110,
    kAscii 					= 0b00000111,
	kHangulJamo 	 		= 0b00100000,
	kHangulSyllable 		= 0b01000000,
    kHangul 				= 0b01100000,
    kUndefined 				= 0b10000000
};

inline LanguageType operator|(LanguageType lhs, LanguageType rhs)
{
    using T = std::underlying_type_t<LanguageType>;
    return static_cast<LanguageType>(static_cast<T>(lhs) | static_cast<T>(rhs));
}

inline LanguageType operator&(LanguageType lhs, LanguageType rhs)
{
    using T = std::underlying_type_t<LanguageType>;
    return static_cast<LanguageType>(static_cast<T>(lhs) & static_cast<T>(rhs));
}

std::uint32_t get_utf8_char_length(unsigned char byte);

inline char32_t decode_utf8_2b(unsigned char byte1, unsigned char byte2)
{
    return ((byte1 & 0b00011111) << 6) | (byte2 & 0b00111111);
}

inline char32_t decode_utf8_3b(unsigned char byte1, unsigned char byte2, unsigned char byte3)
{
    return ((byte1 & 0b00001111) << 12) | ((byte2 & 0b00111111) << 6) | (byte3 & 0b00111111);
}

inline char32_t decode_utf8_4b(unsigned char byte1, unsigned char byte2, unsigned char byte3, unsigned char byte4)
{
    return ((byte1 & 0b00000111) << 18) | ((byte2 & 0b00111111) << 12) | ((byte3 & 0b00111111) << 6) | (byte4 & 0b00111111);
}

template<std::forward_iterator Iter> 
requires(std::convertible_to<typename std::iterator_traits<Iter>::value_type, const char>) 
std::pair<std::uint32_t, char32_t> decode_utf8(const Iter _it, const Iter _end)
{
    if ((*_it & 0b10000000) == 0 && _it < _end)
        return std::pair<std::uint32_t, char32_t>(1, *_it);
    else if ((*_it & 0b11100000) == 0b11000000 && (_it + 1) < _end)
        return std::pair<std::uint32_t, char32_t>(2, decode_utf8_2b(*_it, *(_it + 1)));
    else if ((*_it & 0b11110000) == 0b11100000 && (_it + 2) < _end)
        return std::pair<std::uint32_t, char32_t>(3, decode_utf8_3b(*_it, *(_it + 1), *(_it + 2)));
    else if ((*_it & 0b11111000) == 0b11110000 && (_it + 3) < _end)
        return std::pair<std::uint32_t, char32_t>(4, decode_utf8_4b(*_it, *(_it + 1), *(_it + 2), *(_it + 3)));
    else
        return std::pair<std::uint32_t, char32_t>(0, static_cast<char32_t>(U'\0')); // 잘못된 경우 0byte를 넘김
}

class DoubleArrayTrie
{
public:
    using index_type = std::uint32_t;
    using s_index_type = std::int32_t;
    using size_type = size_t;
    using string_type = std::string;
    using string_view_type = std::string_view;
    using c_string_type = const char*;

    DoubleArrayTrie();

    template <std::ranges::input_range RangeType>
    requires std::convertible_to<std::ranges::range_value_t<RangeType>, string_view_type>
    DoubleArrayTrie(const RangeType& pattern_range);

    template <std::ranges::input_range RangeType>
    requires std::convertible_to<std::ranges::range_value_t<RangeType>, string_view_type>
    DoubleArrayTrie(const RangeType& pattern_range, bool _ignore_case_flag);

    template <std::ranges::input_range RangeType>
    requires std::convertible_to<std::ranges::range_value_t<RangeType>, string_view_type>
    void rebuild(const RangeType& pattern_range, bool _ignore_case_flag = false);

    std::vector<MatchedInfo> search(const string_view_type& _search_string) const;

private:
    bool empty_node (index_type _node_index) const;
    bool exist_node(index_type _index) const;
    bool root_index(index_type _index) const;
    bool sparse_block(index_type _node_index) const;
    void increase_block_fail_count(index_type _node_index);
    void increase_block_used_count(index_type _node_index);
    void reserve_trie();
    index_type char_code(char32_t _ch) const;
    bool is_korean_char(char32_t _ch) const;
    bool is_english_char(char32_t _ch) const;
    LanguageType get_char_language_type(char32_t _ch) const;
    s_index_type get_proper_base(const std::vector<index_type>& _char_code_vec);
    index_type get_block_index(index_type _node_index) const;
    void reset_block_fail_count(index_type _node_index);
    void erase_all();
    
    template <std::ranges::input_range RangeType>
    requires std::convertible_to<std::ranges::range_value_t<RangeType>, string_view_type>
    void build(const RangeType& _pattern_range);

    constexpr static index_type k_max_val_of_char_code_ = 0x02DA3;
    constexpr static size_type k_block_size_ = 1024;
    constexpr static size_type k_block_threshold_size_ = k_block_size_ / 2;
    constexpr static size_type k_block_threshold_fail_conut_ = k_block_size_ / 2;
    constexpr static size_type k_map_increase_size_ = 4;
    constexpr static index_type k_root_index_ = 1;
    constexpr static index_type k_unused_sentinal_val_ = 0;
	bool ignore_case_flag_;
    std::vector<s_index_type> base_vec_;
    std::vector<index_type> check_vec_;
    std::vector<index_type> fail_vec_;
    std::vector<index_type> output_size_vec_;
    std::queue<index_type> chain_queue_;
    std::queue<index_type> dense_chain_queue_;
    std::vector<index_type> block_used_size_vec_;
    std::vector<index_type> block_fail_count_vec_;
    size_type map_size_;
    size_type element_capacity_;
    size_type max_chain_node_index;
};

template <std::ranges::input_range RangeType>
requires std::convertible_to<std::ranges::range_value_t<RangeType>, DoubleArrayTrie::string_view_type>
DoubleArrayTrie::DoubleArrayTrie(const RangeType& _pattern_range)
: DoubleArrayTrie(_pattern_range, true)
{}

template <std::ranges::input_range RangeType>
requires std::convertible_to<std::ranges::range_value_t<RangeType>, DoubleArrayTrie::string_view_type>
DoubleArrayTrie::DoubleArrayTrie(const RangeType& _pattern_range, bool _ignore_case_flag)
: map_size_(0), element_capacity_(0), max_chain_node_index(0), ignore_case_flag_(_ignore_case_flag)
{
    reserve_trie();
    chain_queue_.pop(); // for unused_sentinal_val;
    build(_pattern_range);
}

template <std::ranges::input_range RangeType>
requires std::convertible_to<std::ranges::range_value_t<RangeType>, DoubleArrayTrie::string_view_type>
void DoubleArrayTrie::build(const RangeType& _pattern_range)
{
    struct Trie
    {
        index_type ch_code_;
        index_type matched_size_;
        index_type dat_index_;
        Trie* fail_node_;
        std::unordered_map<index_type, Trie*> child_node_map_;

        ~Trie()
        {
            for (auto [ch, child_node] : child_node_map_)
            {
                delete child_node;
            }
        }
    };

    Trie* root = new Trie{static_cast<index_type>(U'\0'), 0, 0, nullptr};

    for (const std::string_view& pattern_string : _pattern_range)
    {
        Trie* curr_node = root;

        for (size_t ch_i = 0; ch_i < pattern_string.size();)
        {
            size_t utf8_char_length = get_utf8_char_length(pattern_string[ch_i]);
            char32_t utf32_char;
            index_type next_node_ch_code;

            if (ch_i + utf8_char_length > pattern_string.size())
            {
                throw std::runtime_error("invalid pattern string");
            }

            switch (utf8_char_length)
            {
                case 1:
                    utf32_char = pattern_string[ch_i];
                    break;
                case 2:
                    utf32_char = decode_utf8_2b(pattern_string[ch_i], pattern_string[ch_i + 1]);
                    break;
                case 3:
                    utf32_char = decode_utf8_3b(pattern_string[ch_i], pattern_string[ch_i + 1], pattern_string[ch_i + 2]);
                    break;
                case 4:
                    utf32_char = decode_utf8_4b(pattern_string[ch_i], pattern_string[ch_i + 1], pattern_string[ch_i + 2], pattern_string[ch_i + 3]);
                    break;
                default:
                    ch_i++;
                    continue;
            }

            next_node_ch_code = char_code(utf32_char);
            if (next_node_ch_code == static_cast<index_type>(U'\0'))
            {
                throw std::runtime_error("not support unicode char included");
            }

            Trie*& next_node = curr_node->child_node_map_[next_node_ch_code];

            if (next_node == nullptr)
            {
                next_node = new Trie{next_node_ch_code, 0, 0, nullptr};
            }

            curr_node = next_node;
            
            ch_i += utf8_char_length;
        }

        curr_node->matched_size_ = pattern_string.size(); // 매칭 길이가 이상하면 문제가 될 수도?
    }

    std::queue<Trie*> que;
    for (auto [ch, child_node] : root->child_node_map_)
    {
        child_node->fail_node_ = root;
        que.push(child_node);
    }

    while (!que.empty())
    {
        Trie* curr_node = que.front();
        que.pop();

        for (auto [next_ch_code, next_node] : curr_node->child_node_map_)
        {
            Trie* dest_node = curr_node->fail_node_;

            while (dest_node != root && !dest_node->child_node_map_.contains(next_ch_code))
            {
                dest_node = dest_node->fail_node_;
            }

            if (dest_node->child_node_map_.contains(next_ch_code))
            {
                dest_node = dest_node->child_node_map_.at(next_ch_code);
            }

            next_node->fail_node_ = dest_node;
            next_node->matched_size_ = std::max(next_node->matched_size_, next_node->fail_node_->matched_size_);
            
            que.push(next_node);
        }
    }
    
    struct SetIndexEntry
    {
        Trie* node_;
        index_type child_index_;
        std::vector<std::pair<index_type, Trie*>> child_vec_;
    };
    
    std::stack<SetIndexEntry> set_index_entry_stk;
    root->dat_index_ = 1;
    check_vec_[k_root_index_] = 1;
    fail_vec_[k_root_index_] = k_unused_sentinal_val_;
    block_used_size_vec_[0]++;
    set_index_entry_stk.push(SetIndexEntry{root, 0});

    while (!set_index_entry_stk.empty())
    {
        auto& [curr_node_addr, curr_child_index, curr_child_vec] = set_index_entry_stk.top();
        index_type curr_node_index = curr_node_addr->dat_index_;

        if (curr_child_index != curr_node_addr->child_node_map_.size())
        {
            // std::println("depth : {}  / curr_index : {} / child_size : {} / cuur_code : {}", set_index_entry_stk.size(), curr_node_index, curr_node_addr->child_node_map_.size(), curr_node_addr->ch_code_);
            //curr_child_it == curr_node_addr->child_node_map_.begin()
            if (base_vec_[curr_node_index] == 0)
            {
                std::vector<index_type> next_node_ch_code_vec;
                
                curr_child_vec.reserve(curr_node_addr->child_node_map_.size());
                next_node_ch_code_vec.reserve(curr_node_addr->child_node_map_.size());

                for (auto [next_node_ch_code, next_node_addr] : curr_node_addr->child_node_map_)
                {
                    curr_child_vec.push_back({next_node_ch_code, next_node_addr});
                }

                std::sort(curr_child_vec.begin(), curr_child_vec.end(), [](const std::pair<index_type, Trie*>& p1, const std::pair<index_type, Trie*>& p2)->bool{ return p1.first < p2.first; });
                for (auto [next_node_ch_code, next_node_addr] : curr_child_vec)
                {
                    next_node_ch_code_vec.push_back(next_node_ch_code);
                }

                base_vec_[curr_node_index] = get_proper_base(next_node_ch_code_vec);
                
                for (size_t i = 0; i < curr_child_vec.size(); i++)
                {
                    index_type next_node_index = base_vec_[curr_node_index] + curr_child_vec[i].first;
                    Trie* next_node_addr = curr_child_vec[i].second;
                    
                    check_vec_[next_node_index] = curr_node_index;
                    output_size_vec_[next_node_index] = next_node_addr->matched_size_;
                    next_node_addr->dat_index_ = next_node_index;
                    increase_block_used_count(next_node_index);
                }
            }

            index_type next_node_ch_code = curr_child_vec[curr_child_index].first;
            Trie* next_node_addr = curr_child_vec[curr_child_index].second;
            index_type next_node_index = base_vec_[curr_node_index] + next_node_ch_code;

            curr_child_index++;
            set_index_entry_stk.push(SetIndexEntry{next_node_addr, 0});
        }
            
        else
        {
            set_index_entry_stk.pop();
        }
    }

    chain_queue_ = std::queue<index_type>();
    dense_chain_queue_ = std::queue<index_type>();

    struct SetFailEntry
    {
        Trie* node_;
        index_type index_;
    };

    std::queue<SetFailEntry> entry_que;
    entry_que.push({root, k_root_index_});
    
    while (!entry_que.empty())
    {
        auto [curr_node_address, curr_node_index] = entry_que.front();
        entry_que.pop();
        
        for (auto [next_node_ch_code, next_node_addr] : curr_node_address->child_node_map_)
        {
            index_type next_node_index = next_node_addr->dat_index_;
            fail_vec_[next_node_index] = next_node_addr->fail_node_->dat_index_;

            entry_que.push({next_node_addr, next_node_index});
        }
    }

    delete root;
}

template <std::ranges::input_range RangeType>
requires std::convertible_to<std::ranges::range_value_t<RangeType>, DoubleArrayTrie::string_view_type>
void DoubleArrayTrie::rebuild(const RangeType& _pattern_range, bool _ignore_case_flag)
{
    erase_all();
    ignore_case_flag_ = _ignore_case_flag;
    build(_pattern_range);
}