#include "DoubleArrayTrie.hpp"



inline std::uint32_t get_utf8_char_length(unsigned char byte)
{
    if ((byte & 0b10000000) == 0) return 1; // 0xxxxxxx
    else if ((byte & 0b11100000) == 0b11000000) return 2; // 110xxxxx
    else if ((byte & 0b11110000) == 0b11100000) return 3; // 1110xxxx
    else if ((byte & 0b11111000) == 0b11110000) return 4; // 11110xxx
    else return 0; // 잘못된 경우 0byte를 넘김
}

DoubleArrayTrie::DoubleArrayTrie() 
: DoubleArrayTrie(std::initializer_list{""})
{}

std::vector<MatchedInfo> DoubleArrayTrie::search(const string_view_type& _search_string) const
{
    std::vector<MatchedInfo> matched_info_vec;

    index_type curr_node_index = k_root_index_;

    for (size_t search_ch_i = 0; search_ch_i < _search_string.size();)
    {
        size_t utf8_search_char_length = get_utf8_char_length(_search_string[search_ch_i]);
        char32_t utf32_search_ch;
        index_type search_ch_code;

        if (search_ch_i + utf8_search_char_length > _search_string.size())
        {
            search_ch_i++;
            continue;
        }

        switch (utf8_search_char_length)
        {
            case 1:
                utf32_search_ch = _search_string[search_ch_i];
                break;
            case 2:
                utf32_search_ch = decode_utf8_2b(_search_string[search_ch_i], _search_string[search_ch_i + 1]);
                break;
            case 3:
                utf32_search_ch = decode_utf8_3b(_search_string[search_ch_i], _search_string[search_ch_i + 1], _search_string[search_ch_i + 2]);
                break;
            case 4:
                utf32_search_ch = decode_utf8_4b(_search_string[search_ch_i], _search_string[search_ch_i + 1], _search_string[search_ch_i + 2], _search_string[search_ch_i + 3]);
                break;
            default:
                search_ch_i++;
                continue;
        }

        search_ch_code = char_code(utf32_search_ch);
        index_type next_node_index = base_vec_[curr_node_index] + search_ch_code;

        while (!root_index(curr_node_index) && (next_node_index >= check_vec_.size() || check_vec_[next_node_index] != curr_node_index))
        {
            curr_node_index = fail_vec_[curr_node_index];
            next_node_index = base_vec_[curr_node_index] + search_ch_code;
        }

        if (next_node_index < check_vec_.size() && check_vec_[next_node_index] == curr_node_index)
        {
            curr_node_index = next_node_index;
        }

        if (output_size_vec_[curr_node_index] != 0)
        {
            const size_t new_matched_string_size = output_size_vec_[curr_node_index];
            const size_t new_matched_string_start_index = search_ch_i + (utf8_search_char_length - 1) - (new_matched_string_size - 1);
            const size_t new_matched_string_end_index = search_ch_i;

            size_t front_matched_string_size;
            size_t front_matched_string_start_index;
            size_t front_matched_string_end_index;

            bool replace_flag = false;

            if (!matched_info_vec.empty())
            {
                front_matched_string_size = matched_info_vec.back().size_;
                front_matched_string_start_index = matched_info_vec.back().index_;
                front_matched_string_end_index = front_matched_string_start_index + (front_matched_string_size - 1);

                if (new_matched_string_start_index <= front_matched_string_start_index && new_matched_string_end_index >= front_matched_string_end_index)
                {
                    replace_flag = true;
                }
            }

            if (replace_flag)
            {
                matched_info_vec.back().index_ = new_matched_string_start_index;
                matched_info_vec.back().size_ = new_matched_string_size;
            }
            else
            {
                matched_info_vec.push_back(MatchedInfo{new_matched_string_start_index, new_matched_string_size});
            }
        }

        search_ch_i += utf8_search_char_length;
    }

    return matched_info_vec;
}

bool DoubleArrayTrie::empty_node(index_type _index) const
{
    return check_vec_.at(_index) == k_unused_sentinal_val_;
}

bool DoubleArrayTrie::exist_node(index_type _index) const
{
    return !empty_node(_index);
}

bool DoubleArrayTrie::root_index(index_type _index) const
{
    return _index == k_root_index_;
}

bool DoubleArrayTrie::sparse_block(index_type _node_index) const
{
    index_type block_index = get_block_index(_node_index);
    return (block_used_size_vec_[block_index] < k_block_threshold_size_) && (block_fail_count_vec_[block_index] < k_block_threshold_fail_conut_);
}

void DoubleArrayTrie::increase_block_used_count(index_type _node_index)
{
    index_type block_index = get_block_index(_node_index);
    block_used_size_vec_[block_index]++;
}

void DoubleArrayTrie::increase_block_fail_count(index_type _node_index)
{
    index_type block_index = get_block_index(_node_index);
    block_fail_count_vec_[block_index] = block_fail_count_vec_[block_index] < k_block_threshold_fail_conut_ ? block_fail_count_vec_[block_index] + 1 : block_fail_count_vec_[block_index];
}

void DoubleArrayTrie::reset_block_fail_count(index_type _node_index)
{
    index_type block_index = get_block_index(_node_index);
    block_fail_count_vec_[block_index] = 0;
}

typename DoubleArrayTrie::index_type DoubleArrayTrie::get_block_index(index_type _node_index) const
{
    return _node_index / k_block_size_;
}

typename DoubleArrayTrie::index_type DoubleArrayTrie::char_code(char32_t _ch) const
{
    LanguageType ch_language_type = get_char_language_type(_ch);

    if (static_cast<bool>(ch_language_type & LanguageType::kAscii)) // Ascii assigned 0x0000 to 0x00FF
    {
        if (ignore_case_flag_ == true && static_cast<bool>(ch_language_type & LanguageType::kUpperAlphabet))
        {
            return static_cast<index_type>(_ch + (U'a' - U'A'));
        }
        else
        {
            return static_cast<index_type>(_ch);
        }
    }
    else if (static_cast<bool>(ch_language_type & LanguageType::kHangul)) // Hangul assigned 0x0100 to 0x01FF (jamo), 0x0200 to 0x02DA3 (syllable)
    {
        if (static_cast<bool>(ch_language_type & LanguageType::kHangulJamo))
            return static_cast<index_type>((_ch - 0x1100) + 0x0100);
        if (static_cast<bool>(ch_language_type & LanguageType::kHangulSyllable))
            return static_cast<index_type>((_ch - 0xAC00) + 0x0200);
    }

    return static_cast<index_type>(U'\0');
}

bool DoubleArrayTrie::is_korean_char(char32_t _ch) const
{
    return (_ch >= 0x1100 && _ch <=0x11FF) || (_ch >= 0xAC00 && _ch <= 0xD7A3);
}

bool DoubleArrayTrie::is_english_char(char32_t _ch) const
{
    return (_ch >= 0x1100 && _ch <=0x11FF) || (_ch >= 0xAC00 && _ch <= 0xD7A3);
}

LanguageType DoubleArrayTrie::get_char_language_type(char32_t _ch) const
{
    if (_ch < 128)
    {
        if (_ch >= U'A' && _ch <= U'Z')
			return LanguageType::kUpperAlphabet;
		else if (_ch >= U'a' && _ch <= U'z')
			return LanguageType::kLowerAlphabet;
		else
			return LanguageType::KAsciiWithoutAlphabet;
	}
	else if ((_ch >= 0x1100 && _ch <=0x11FF))
	{
		return LanguageType::kHangulJamo;
	}
	else if ((_ch >= 0xAC00 && _ch <= 0xD7A3))
	{
		return LanguageType::kHangulSyllable;
	}
	else
	{
		return LanguageType::kUndefined;
	}
}

typename DoubleArrayTrie::s_index_type DoubleArrayTrie::get_proper_base(const std::vector<index_type>& char_code_vec)
{
    while (true)
    {
        index_type min_char_code = *std::min_element(char_code_vec.begin(), char_code_vec.end());
        const size_t chain_que_size = chain_queue_.size();

        if (char_code_vec.size() == 1)
        {
            while (!dense_chain_queue_.empty())
            {
                uint32_t empty_node_index = dense_chain_queue_.front();
                dense_chain_queue_.pop();
    
                if (empty_node(empty_node_index))
                {
                    return empty_node_index - min_char_code;
                }
            }
        }
    
        for (size_t i = 0; i < chain_que_size; i++)
        {
            uint32_t empty_node_index = chain_queue_.front();
            chain_queue_.pop();

            if (!empty_node(empty_node_index)) continue;
            else if (sparse_block(empty_node_index))
            {
                int32_t base_candidate = empty_node_index - min_char_code;
                bool find_flag = true;

                for (index_type char_code : char_code_vec)
                {
                    if (!empty_node(base_candidate + char_code))
                    {
                        find_flag = false;
                        break;
                    }
                }

                if (find_flag)
                {
                    reset_block_fail_count(empty_node_index);
                    return base_candidate;
                }
            
                increase_block_fail_count(empty_node_index);
                chain_queue_.push(empty_node_index);
            }
            else
            {
                dense_chain_queue_.push(empty_node_index);
            }
        }
    
        reserve_trie();
    }
}

void DoubleArrayTrie::erase_all()
{
    std::fill(base_vec_.begin(), base_vec_.end(), k_unused_sentinal_val_);
    std::fill(check_vec_.begin(), check_vec_.end(), k_unused_sentinal_val_);
    std::fill(fail_vec_.begin(), fail_vec_.end(), k_unused_sentinal_val_);
    std::fill(output_size_vec_.begin(), output_size_vec_.end(), k_unused_sentinal_val_);
    std::fill(block_used_size_vec_.begin(), block_used_size_vec_.end(), k_unused_sentinal_val_);
    std::fill(block_fail_count_vec_.begin(), block_fail_count_vec_.end(), k_unused_sentinal_val_);

    for (size_t chain_i = 0; chain_i <= max_chain_node_index; chain_i++)
    {
        chain_queue_.push(chain_i);
    }

    chain_queue_.pop();
}

void DoubleArrayTrie::reserve_trie()
{
    size_type old_max_chain_node_index = max_chain_node_index;

    do
    {
        map_size_ += k_map_increase_size_;
        element_capacity_ += k_map_increase_size_ * k_block_size_;
        max_chain_node_index = (element_capacity_ > k_max_val_of_char_code_) ? element_capacity_ - k_max_val_of_char_code_ : 0;
    } while (max_chain_node_index == 0);

    base_vec_.resize(element_capacity_, k_unused_sentinal_val_);
    check_vec_.resize(element_capacity_, k_unused_sentinal_val_);
    fail_vec_.resize(element_capacity_, k_unused_sentinal_val_);
    output_size_vec_.resize(element_capacity_, 0);
    block_used_size_vec_.resize(map_size_, 0);
    block_fail_count_vec_.resize(map_size_, 0);

    for (size_t chain_i = old_max_chain_node_index; chain_i <= max_chain_node_index; chain_i++)
    {
        chain_queue_.push(chain_i);
    }

    for (auto& block_fail_count_i : block_fail_count_vec_)
    {
        block_fail_count_i /= 2;
    }
}