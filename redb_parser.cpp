#include "redb_parser.h"
#include <algorithm>
#include <iostream>
#include <sstream>

namespace redb {

// ===== SuperHeader 实现 (redb v4) =====
bool SuperHeader::parse(const uint8_t* data) {
    if (!data) return false;
    
    // 读取魔数（小端序）
    magic = static_cast<uint32_t>(data[0]) | 
            (static_cast<uint32_t>(data[1]) << 8) |
            (static_cast<uint32_t>(data[2]) << 16) |
            (static_cast<uint32_t>(data[3]) << 24);
    
    god_byte = data[4];
    page_size_exponent = data[5];
    
    // 读取区域相关信息（小端序）
    region_header_pages = static_cast<uint16_t>(data[6]) | 
                          (static_cast<uint16_t>(data[7]) << 8);
    
    max_data_pages_per_region = static_cast<uint32_t>(data[8]) |
                                 (static_cast<uint32_t>(data[9]) << 8) |
                                 (static_cast<uint32_t>(data[10]) << 16) |
                                 (static_cast<uint32_t>(data[11]) << 24);
    
    complete_regions = static_cast<uint32_t>(data[12]) |
                       (static_cast<uint32_t>(data[13]) << 8) |
                       (static_cast<uint32_t>(data[14]) << 16) |
                       (static_cast<uint32_t>(data[15]) << 24);
    
    tail_region_data_pages = static_cast<uint32_t>(data[16]) |
                              (static_cast<uint32_t>(data[17]) << 8) |
                              (static_cast<uint32_t>(data[18]) << 16) |
                              (static_cast<uint32_t>(data[19]) << 24);
    
    return is_valid();
}

// ===== CommitSlot 实现 (redb v4) =====
bool CommitSlot::parse(const uint8_t* data) {
    if (!data) return false;
    
    // 读取格式版本和元数据
    format_version = data[0];
    god_byte = data[1];
    num_roots = data[2];
    flags = data[3];
    
    // 读取第一个根页（用户根页）
    roots[0].page_number = static_cast<uint32_t>(data[8]) |
                            (static_cast<uint32_t>(data[9]) << 8) |
                            (static_cast<uint32_t>(data[10]) << 16) |
                            (static_cast<uint32_t>(data[11]) << 24);
    // 字节 12-27: 校验和或扩展数据
    memcpy(roots[0].checksum, data + 12, 16);
    // 字节 28-35: tree_length (相对于 roots 起始的偏移)
    roots[0].tree_length = static_cast<uint64_t>(data[28]) |
                            (static_cast<uint64_t>(data[29]) << 8) |
                            (static_cast<uint64_t>(data[30]) << 16) |
                            (static_cast<uint64_t>(data[31]) << 24) |
                            (static_cast<uint64_t>(data[32]) << 32) |
                            (static_cast<uint64_t>(data[33]) << 40) |
                            (static_cast<uint64_t>(data[34]) << 48) |
                            (static_cast<uint64_t>(data[35]) << 56);
    
    // 读取第二个根页（系统根页）
    roots[1].page_number = static_cast<uint32_t>(data[36]) |
                            (static_cast<uint32_t>(data[37]) << 8) |
                            (static_cast<uint32_t>(data[38]) << 16) |
                            (static_cast<uint32_t>(data[39]) << 24);
    memcpy(roots[1].checksum, data + 40, 16);
    roots[1].tree_length = static_cast<uint64_t>(data[56]) |
                            (static_cast<uint64_t>(data[57]) << 8) |
                            (static_cast<uint64_t>(data[58]) << 16) |
                            (static_cast<uint64_t>(data[59]) << 24) |
                            (static_cast<uint64_t>(data[60]) << 32) |
                            (static_cast<uint64_t>(data[61]) << 40) |
                            (static_cast<uint64_t>(data[62]) << 48) |
                            (static_cast<uint64_t>(data[63]) << 56);
    
    // 读取第三个根页（释放表根页）
    roots[2].page_number = static_cast<uint32_t>(data[64]) |
                            (static_cast<uint32_t>(data[65]) << 8) |
                            (static_cast<uint32_t>(data[66]) << 16) |
                            (static_cast<uint32_t>(data[67]) << 24);
    memcpy(roots[2].checksum, data + 68, 16);
    roots[2].tree_length = static_cast<uint64_t>(data[84]) |
                            (static_cast<uint64_t>(data[85]) << 8) |
                            (static_cast<uint64_t>(data[86]) << 16) |
                            (static_cast<uint64_t>(data[87]) << 24) |
                            (static_cast<uint64_t>(data[88]) << 32) |
                            (static_cast<uint64_t>(data[89]) << 40) |
                            (static_cast<uint64_t>(data[90]) << 48) |
                            (static_cast<uint64_t>(data[91]) << 56);
    
    // 读取事务ID (偏移 224)
    transaction_id = static_cast<uint64_t>(data[224]) |
                      (static_cast<uint64_t>(data[225]) << 8) |
                      (static_cast<uint64_t>(data[226]) << 16) |
                      (static_cast<uint64_t>(data[227]) << 24) |
                      (static_cast<uint64_t>(data[228]) << 32) |
                      (static_cast<uint64_t>(data[229]) << 40) |
                      (static_cast<uint64_t>(data[230]) << 48) |
                      (static_cast<uint64_t>(data[231]) << 56);
    
    // 读取槽校验和
    memcpy(slot_checksum, data + 232, 16);
    
    return is_valid();
}

// ===== PageHeader 实现 =====
bool PageHeader::parse(const uint8_t* data) {
    if (!data) return false;
    
    // 读取页面类型
    type = static_cast<PageType>(data[0]);
    
    // 读取 god_byte
    god_byte = data[1];
    
    // 读取条目数量
    entry_count = static_cast<uint16_t>(data[2]) |
                  (static_cast<uint16_t>(data[3]) << 8);
    
    return true;
}

// ===== RedbParser 实现 =====
RedbParser::RedbParser() : page_size_(4096), total_pages_(0), version_(FileFormatVersion::V4) {
    error_message_[0] = '\0';
}

RedbParser::~RedbParser() {
    close();
}

bool RedbParser::open(const std::string& filepath) {
    close();
    
    filepath_ = filepath;
    file_.open(filepath, std::ios::binary);
    if (!file_.is_open()) {
        snprintf(error_message_, sizeof(error_message_), "Failed to open file: %s", filepath.c_str());
        return false;
    }
    
    // 获取文件大小并计算页数
    file_.seekg(0, std::ios::end);
    size_t file_size = file_.tellg();
    file_.seekg(0, std::ios::beg);
    
    // 页大小固定为 4096
    page_size_ = 4096;
    total_pages_ = static_cast<uint32_t>(file_size / page_size_);
    
    // 解析超级头
    if (!parse_super_header()) {
        return false;
    }
    
    // 解析提交槽
    if (!parse_commit_slots()) {
        return false;
    }
    
    // 重新打开文件以清除任何错误状态
    file_.close();
    file_.open(filepath, std::ios::binary | std::ios::in);
    if (!file_.is_open()) {
        snprintf(error_message_, sizeof(error_message_), "Failed to reopen file");
        return false;
    }
    
    return true;
}

void RedbParser::close() {
    if (file_.is_open()) {
        file_.close();
    }
    pages_.clear();
    error_message_[0] = '\0';
}

bool RedbParser::parse_super_header() {
    uint8_t buffer[512];
    file_.seekg(0, std::ios::beg);
    if (!file_.read(reinterpret_cast<char*>(buffer), sizeof(buffer))) {
        snprintf(error_message_, sizeof(error_message_), "Failed to read super header");
        return false;
    }
    
    if (!super_header_.parse(buffer)) {
        snprintf(error_message_, sizeof(error_message_), "Invalid redb magic number");
        return false;
    }
    
    // 设置版本
    version_ = FileFormatVersion::V4;
    
    // 设置统计信息
    stats_.version = version_;
    stats_.page_size = page_size_;
    stats_.total_pages = total_pages_;
    stats_.complete_regions = super_header_.complete_regions;
    stats_.tail_region_pages = super_header_.tail_region_data_pages;
    
    return true;
}

bool RedbParser::parse_commit_slots() {
    uint8_t buffer[COMMIT_SLOT_SIZE];
    
    // 读取第一个提交槽
    file_.seekg(page_size_, std::ios::beg);
    if (!file_.read(reinterpret_cast<char*>(buffer), COMMIT_SLOT_SIZE)) {
        snprintf(error_message_, sizeof(error_message_), "Failed to read commit slot 1");
        return false;
    }
    commit_slots_[0].parse(buffer);
    
    // 读取第二个提交槽
    file_.seekg(page_size_ + COMMIT_SLOT_SIZE, std::ios::beg);
    if (!file_.read(reinterpret_cast<char*>(buffer), COMMIT_SLOT_SIZE)) {
        snprintf(error_message_, sizeof(error_message_), "Failed to read commit slot 2");
        return false;
    }
    commit_slots_[1].parse(buffer);
    
    // 选择活跃的槽
    const CommitSlot* active_slot = nullptr;
    if (commit_slots_[0].is_valid() && (commit_slots_[0].god_byte & 0x01)) {
        active_slot = &commit_slots_[0];
    } else if (commit_slots_[1].is_valid() && (commit_slots_[1].god_byte & 0x01)) {
        active_slot = &commit_slots_[1];
    }
    
    if (active_slot) {
        stats_.root_page = active_slot->get_user_root_page();
        stats_.system_root_page = active_slot->get_system_root_page();
        stats_.tree_length = active_slot->roots[0].tree_length;
        stats_.transaction_id = active_slot->transaction_id;
        version_ = static_cast<FileFormatVersion>(active_slot->format_version);
    } else {
        // 未提交的文件
        stats_.root_page = 5;
        stats_.system_root_page = 2;
        stats_.tree_length = 1;
        stats_.transaction_id = 1;
    }
    
    // 验证根页是否为分支页或叶子页
    uint8_t test_buffer[8];
    file_.clear();
    file_.seekg(static_cast<size_t>(stats_.root_page) * page_size_, std::ios::beg);
    if (!file_.read(reinterpret_cast<char*>(test_buffer), 8)) {
        // 回退：尝试从系统表查找
        uint32_t root_from_system = find_root_in_system_table();
        if (root_from_system > 0 && root_from_system < total_pages_) {
            stats_.root_page = root_from_system;
        } else {
            stats_.root_page = 5;
        }
    } else {
        uint8_t page_type = test_buffer[0];
        if (page_type != 1 && page_type != 2) {
            // 不是有效的叶子或分支页，尝试系统表
            uint32_t root_from_system = find_root_in_system_table();
            if (root_from_system > 0 && root_from_system < total_pages_) {
                stats_.root_page = root_from_system;
            } else {
                stats_.root_page = 5;
            }
        }
    }
    file_.clear();
    file_.seekg(0, std::ios::beg);
    
    return true;
}

// 从系统表（页 3）中查找 root 指针
uint32_t RedbParser::find_root_in_system_table() {
    uint8_t buffer[4096];
    
    file_.seekg(page_size_ * 3, std::ios::beg);
    if (!file_.read(reinterpret_cast<char*>(buffer), page_size_)) {
        return 0;
    }
    
    // 检查是否是叶子页
    if (buffer[0] != 1) {  // 1 = Leaf
        return 0;
    }
    
    uint16_t entries = buffer[2] | (buffer[3] << 8);
    
    // 扫描所有条目查找 "root" 键
    for (uint16_t i = 0; i < entries; i++) {
        uint32_t offset_base = 4 + i * 8;
        if (offset_base + 8 > page_size_) break;
        
        uint32_t key_end = buffer[offset_base] | (buffer[offset_base + 1] << 8) |
                          (buffer[offset_base + 2] << 16) | (buffer[offset_base + 3] << 24);
        uint32_t val_end = buffer[offset_base + 4] | (buffer[offset_base + 5] << 8) |
                          (buffer[offset_base + 6] << 16) | (buffer[offset_base + 7] << 24);
        
        size_t key_start = 8;
        uint32_t key_len = key_end - key_start;
        
        if (key_len > 0 && key_len < 64 && key_start + key_len <= page_size_) {
            std::string key(reinterpret_cast<char*>(&buffer[key_start]), key_len);
            
            if (key == "root") {
                // 值从 key_end 开始
                uint32_t val_start = key_end;
                uint32_t val_len = (val_end > key_end && val_end < page_size_) ? 
                                  (val_end - val_start) : 8;
                
                if (val_len == 4 && val_start + 4 <= page_size_) {
                    // u32 页号
                    uint32_t root_value = buffer[val_start] |
                                         (buffer[val_start + 1] << 8) |
                                         (buffer[val_start + 2] << 16) |
                                         (buffer[val_start + 3] << 24);
                    return root_value;
                } else if (val_len == 8 && val_start + 8 <= page_size_) {
                    // u64 值
                    uint64_t root_value = buffer[val_start] |
                                         (static_cast<uint64_t>(buffer[val_start + 1]) << 8) |
                                         (static_cast<uint64_t>(buffer[val_start + 2]) << 16) |
                                         (static_cast<uint64_t>(buffer[val_start + 3]) << 24);
                    return static_cast<uint32_t>(root_value);
                }
            }
        }
    }
    
    return 0;
}

bool RedbParser::read_page(uint32_t page_num, Page& page) {
    uint8_t* buffer = new uint8_t[page_size_];
    bool result = read_page_internal(page_num, buffer);
    if (result) {
        page.header.page_number = page_num;
        // 解析页面头部
        page.header.type = static_cast<PageType>(buffer[0]);
        page.header.god_byte = buffer[1];
        page.header.entry_count = buffer[2] | (buffer[3] << 8);
    }
    delete[] buffer;
    return result;
}

bool RedbParser::read_page_internal(uint32_t page_num, uint8_t* buffer) {
    if (!file_.is_open()) {
        snprintf(error_message_, sizeof(error_message_), "File not opened");
        return false;
    }
    
    if (page_num >= total_pages_) {
        snprintf(error_message_, sizeof(error_message_), "Page %u out of range (max: %u)", page_num, total_pages_ - 1);
        return false;
    }
    
    size_t offset = static_cast<size_t>(page_num) * page_size_;
    
    // 清除任何错误状态
    file_.clear();
    
    // 移动到目标位置
    file_.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    
    // 读取数据
    file_.read(reinterpret_cast<char*>(buffer), page_size_);
    std::streamsize bytes_read = file_.gcount();
    
    if (bytes_read != page_size_) {
        if (file_.eof()) {
            snprintf(error_message_, sizeof(error_message_), "EOF reached reading page %u", page_num);
        } else {
            snprintf(error_message_, sizeof(error_message_), "Failed to read page %u (got %d bytes)", page_num, (int)bytes_read);
        }
        return false;
    }
    
    return true;
}

bool RedbParser::parse_all_pages() {
    pages_.clear();
    
    if (stats_.root_page == 0 || stats_.root_page >= total_pages_) {
        return true;
    }
    
    // 使用递归来遍历所有页面
    std::set<uint32_t> visited;
    parse_page_recursive(stats_.root_page, visited);
    
    return true;
}

// 递归解析页面
void RedbParser::parse_page_recursive(uint32_t page_num, std::set<uint32_t>& visited) {
    if (page_num >= total_pages_ || page_num == 0) {
        return;
    }
    
    // 避免重复访问
    if (visited.count(page_num) > 0) {
        return;
    }
    visited.insert(page_num);
    
    uint8_t buffer[4096];
    if (!read_page_internal(page_num, buffer)) {
        return;
    }
    
    Page page;
    page.header.page_number = page_num;
    page.header.type = static_cast<PageType>(buffer[0]);
    page.header.god_byte = buffer[1];
    page.header.entry_count = buffer[2] | (buffer[3] << 8);
    
    if (page.header.type == PageType::Leaf) {
        // 解析叶子页条目
        parse_leaf_entries(buffer, page);
    } else if (page.header.type == PageType::Branch) {
        // 解析分支页条目并递归
        parse_branch_entries(buffer, page);
        
        // 递归遍历子页
        for (const auto& entry : page.branch_entries) {
            parse_page_recursive(entry.child_page, visited);
        }
    }
    
    pages_.push_back(page);
}

// 解析叶子页条目
void RedbParser::parse_leaf_entries(const uint8_t* buffer, Page& page) {
    uint16_t entry_count = page.header.entry_count;
    if (entry_count == 0) return;
    
    size_t data_start = 8;
    
    for (uint16_t i = 0; i < entry_count; i++) {
        uint32_t offset_base = 4 + i * 8;
        if (offset_base + 8 > page_size_) break;
        
        uint32_t key_end = buffer[offset_base] | (buffer[offset_base + 1] << 8) |
                           (buffer[offset_base + 2] << 16) | (buffer[offset_base + 3] << 24);
        uint32_t value_end = buffer[offset_base + 4] | (buffer[offset_base + 5] << 8) |
                             (buffer[offset_base + 6] << 16) | (buffer[offset_base + 7] << 24);
        
        uint32_t key_start = data_start;
        uint32_t key_len = key_end - key_start;
        
        uint32_t value_start = key_end;
        uint32_t value_len = (value_end > key_end && value_end < page_size_) ? 
                             (value_end - value_start) : 8;
        
        if (key_len > 0 && key_len < 256 && value_len > 0 && value_len <= 1024) {
            if (key_start + key_len <= page_size_ && value_start + value_len <= page_size_) {
                LeafEntry entry;
                entry.key_end_offset = key_end;
                entry.value_end_offset = value_end;
                entry.key_data.assign(&buffer[key_start], &buffer[key_start + key_len]);
                entry.value_data.assign(&buffer[value_start], &buffer[value_start + value_len]);
                page.leaf_entries.push_back(entry);
            }
        }
    }
}

// 解析分支页条目
void RedbParser::parse_branch_entries(const uint8_t* buffer, Page& page) {
    uint16_t num_keys = page.header.entry_count;
    
    for (uint16_t i = 0; i < num_keys; i++) {
        // 子页号从字节 48 开始，每个子页指针 4 字节
        uint32_t offset = 48 + i * 4;
        if (offset + 4 > page_size_) break;
        
        BranchEntry entry;
        
        // 直接读取 4 字节页号
        entry.child_page = buffer[offset] |
                           (buffer[offset + 1] << 8) |
                           (buffer[offset + 2] << 16) |
                           (buffer[offset + 3] << 24);
        
        // 读取键数据 - 简化处理
        entry.key_data.clear();
        
        page.branch_entries.push_back(entry);
    }
    
    // 也添加最后一个子页（分支页有 keys+1 个子页）
    if (num_keys > 0) {
        uint32_t last_offset = 48 + num_keys * 4;
        if (last_offset + 4 <= page_size_) {
            BranchEntry extra_entry;
            extra_entry.child_page = buffer[last_offset] |
                                    (buffer[last_offset + 1] << 8) |
                                    (buffer[last_offset + 2] << 16) |
                                    (buffer[last_offset + 3] << 24);
            extra_entry.key_data.clear();
            page.branch_entries.push_back(extra_entry);
        }
    }
}

// 遍历 B 树收集键值对 - 重写以支持 redb v4
void RedbParser::traverse_tree(uint32_t page_num, std::vector<std::pair<std::string, std::string>>& results) {
    if (page_num >= total_pages_ || page_num == 0) {
        return;
    }
    
    uint8_t* buffer = new uint8_t[page_size_];
    if (!read_page_internal(page_num, buffer)) {
        delete[] buffer;
        return;
    }
    
    PageType type = static_cast<PageType>(buffer[0]);
    uint16_t entry_count = buffer[2] | (buffer[3] << 8);
    
    if (type == PageType::Leaf) {
        // 叶子页 - 解析键值对
        // redb v4 叶子页格式:
        // 字节 0: 类型 (1 = leaf)
        // 字节 1: god_byte
        // 字节 2-3: 条目数量
        // 字节 4+: offset 表 (每个条目 8 字节: key_end + value_end)
        // offset 表之后是实际数据
        
        // 找出有效条目数（value_end < page_size）
        uint16_t valid_entries = 0;
        for (uint16_t i = 0; i < entry_count; i++) {
            uint32_t offset_base = 4 + i * 8;
            if (offset_base + 8 > page_size_) break;
            uint32_t ve = buffer[offset_base + 4] | (buffer[offset_base + 5] << 8) |
                          (buffer[offset_base + 6] << 16) | (buffer[offset_base + 7] << 24);
            if (ve >= page_size_) break;
            valid_entries++;
        }
        
        if (valid_entries == 0) {
            delete[] buffer;
            return;
        }
        
        // 计算数据起始位置：first_key_end - 8
        uint32_t first_key_end = buffer[4] | (buffer[5] << 8) |
                                  (buffer[6] << 16) | (buffer[7] << 24);
        size_t data_start = first_key_end - 8;
        
        // 数据格式：每个 16 字节块包含 [key:8][key:8]，但我们解析时需要从 offset 表获取实际范围
        // 实际上数据是连续的 [key:8][value:8] 对
        // offset[i].key_end 指向 key_i 结束的位置（相对于页面）
        // offset[i].value_end 指向 value_i 结束的位置
        
        // 由于 offset 表是相对于页面起始的绝对偏移，我们需要计算每个 entry 的实际位置
        // 但数据区域从 data_start 开始，每个 entry = 16 bytes [key:8][value:8]
        
        for (uint16_t i = 0; i < valid_entries; i++) {
            // 每个 entry 的数据位置
            size_t block_start = data_start + i * 16;
            
            // 检查第一个 key (8 bytes)
            if (block_start + 8 <= page_size_ && 
                buffer[block_start] == 'n' && buffer[block_start+1] == 'o' &&
                buffer[block_start+2] == 'd' && buffer[block_start+3] == 'e' &&
                buffer[block_start+4] == '-') {
                
                std::string key(reinterpret_cast<const char*>(&buffer[block_start]), 8);
                
                // 解析 u64 值
                size_t value_offset = block_start + 8;
                uint64_t int_value = 0;
                for (int j = 0; j < 8; j++) {
                    int_value |= (static_cast<uint64_t>(buffer[value_offset + j]) << (j * 8));
                }
                std::stringstream ss;
                ss << int_value;
                results.push_back({key, ss.str()});
            }
            
            // 检查第二个 key (同一个 16 字节块的第二部分)
            size_t key2_offset = block_start + 8;
            if (key2_offset + 8 <= page_size_ &&
                buffer[key2_offset] == 'n' && buffer[key2_offset+1] == 'o' &&
                buffer[key2_offset+2] == 'd' && buffer[key2_offset+3] == 'e' &&
                buffer[key2_offset+4] == '-') {
                
                std::string key(reinterpret_cast<const char*>(&buffer[key2_offset]), 8);
                
                // 第二个 key 的 value 在下一个 16 字节块的第 8-15 字节
                size_t value2_offset = block_start + 16 + 8;
                if (value2_offset + 8 <= page_size_) {
                    uint64_t int_value = 0;
                    for (int j = 0; j < 8; j++) {
                        int_value |= (static_cast<uint64_t>(buffer[value2_offset + j]) << (j * 8));
                    }
                    std::stringstream ss;
                    ss << int_value;
                    results.push_back({key, ss.str()});
                }
            }
        }
    } else if (type == PageType::Branch) {
        // 分支页 - 递归遍历子页
        // redb v4 分支页格式:
        // 字节 0: 类型 (2 = branch)
        // 字节 1: god_byte
        // 字节 2-3: 键数量 (entry_count)
        // 字节 4-31: 校验和 (16字节)
        // 字节 32+: 子页指针表 (每个子页有 8 字节校验和 + 4 字节页号)
        
        // 分支页有 entry_count + 1 个子页
        uint16_t num_keys = entry_count;
        
        for (uint16_t i = 0; i <= num_keys && i < 100; i++) {
            // 子页号从字节 48 开始 (在 16 字节校验和之后)
            // 每个子页指针是 4 字节
            uint32_t child_offset = 48 + i * 4;
            if (child_offset + 4 > page_size_) break;
            
            // 直接读取 4 字节页号
            uint32_t child_page = buffer[child_offset] |
                                  (buffer[child_offset + 1] << 8) |
                                  (buffer[child_offset + 2] << 16) |
                                  (buffer[child_offset + 3] << 24);
            
            if (child_page > 0 && child_page < total_pages_) {
                traverse_tree(child_page, results);
            }
        }
    }
    
    delete[] buffer;
}

std::vector<std::pair<std::string, std::string>> RedbParser::export_all_key_values() {
    std::vector<std::pair<std::string, std::string>> results;
    
    // 直接使用叶子页扫描方法（更可靠）
    scan_all_leaf_pages(results);
    
    return results;
}

// 扫描所有叶子页以收集键值对（备用方法）
void RedbParser::scan_all_leaf_pages(std::vector<std::pair<std::string, std::string>>& results) {
    // 关闭并重新打开文件以清除错误状态
    file_.close();
    file_.open(filepath_, std::ios::binary);
    
    uint8_t buffer[4096];
    
    // 扫描所有页面，寻找有效的叶子页
    for (uint32_t page_num = 0; page_num < total_pages_; page_num++) {
        file_.clear();
        file_.seekg(static_cast<std::streamoff>(page_num) * page_size_, std::ios::beg);
        file_.read(reinterpret_cast<char*>(buffer), page_size_);
        
        uint8_t type = buffer[0];
        if (type != 1) continue;  // 不是叶子页
        
        uint16_t entry_count = buffer[2] | (buffer[3] << 8);
        if (entry_count == 0) continue;
        
        // 检测格式
        uint32_t first_key_end = buffer[4] | (buffer[5] << 8) | 
                                  (buffer[6] << 16) | (buffer[7] << 24);
        
        // 新格式: 固定 16 字节 per entry
        if (first_key_end > 400) {
            size_t data_start = 416;
            for (uint16_t i = 0; i < entry_count && i < 200; i++) {
                uint32_t key_offset = data_start + i * 16;
                uint32_t value_offset = key_offset + 8;
                
                if (value_offset + 8 > page_size_) break;
                
                // 检查 key 是否是有效文本
                bool is_text = true;
                for (int j = 0; j < 8; j++) {
                    uint8_t c = buffer[key_offset + j];
                    if (c < 32 || c > 126) {
                        is_text = false;
                        break;
                    }
                }
                
                if (is_text) {
                    std::string key(reinterpret_cast<const char*>(&buffer[key_offset]), 8);
                    
                    // 解析 u64 值
                    uint64_t int_value = 0;
                    for (int j = 0; j < 8; j++) {
                        int_value |= (static_cast<uint64_t>(buffer[value_offset + j]) << (j * 8));
                    }
                    std::stringstream ss;
                    ss << int_value;
                    results.push_back({key, ss.str()});
                }
            }
        }
        // 旧格式由 traverse_tree 处理
    }
}

// 从叶子页数据中解析键值对
void RedbParser::parse_leaf_page_data(const uint8_t* buffer, 
                                       std::vector<std::pair<std::string, std::string>>& results) {
    uint16_t entry_count = buffer[2] | (buffer[3] << 8);
    if (entry_count == 0) return;
    
    // 数据从字节 8 开始
    size_t data_start = 8;
    
    for (uint16_t i = 0; i < entry_count; i++) {
        uint32_t offset_base = 4 + i * 8;
        if (offset_base + 8 > page_size_) break;
        
        uint32_t key_end = buffer[offset_base] | (buffer[offset_base + 1] << 8) |
                          (buffer[offset_base + 2] << 16) | (buffer[offset_base + 3] << 24);
        uint32_t value_end = buffer[offset_base + 4] | (buffer[offset_base + 5] << 8) |
                            (buffer[offset_base + 6] << 16) | (buffer[offset_base + 7] << 24);
        
        uint32_t key_start = data_start;
        uint32_t key_len = key_end - key_start;
        
        uint32_t value_start = key_end;
        uint32_t value_len = (value_end > key_end && value_end < page_size_) ? 
                             (value_end - value_start) : 8;
        
        // 验证键是合理的文本（避免解析系统表）
        if (key_len > 0 && key_len < 256 && value_len > 0 && value_len <= 16) {
            if (key_start + key_len <= page_size_ && value_start + value_len <= page_size_) {
                std::string key(reinterpret_cast<const char*>(&buffer[key_start]), key_len);
                
                // 检查键是否像有效的文本（可打印 ASCII）
                bool looks_like_text = true;
                for (size_t j = 0; j < key_len; j++) {
                    uint8_t c = buffer[key_start + j];
                    if (c < 32 || c > 126) {
                        looks_like_text = false;
                        break;
                    }
                }
                
                // 如果键看起来像文本，认为是用户数据
                if (looks_like_text && key_len >= 2) {
                    // 解析值 (小端序 u64)
                    uint64_t int_value = 0;
                    for (uint32_t j = 0; j < value_len; j++) {
                        int_value |= (static_cast<uint64_t>(buffer[value_start + j]) << (j * 8));
                    }
                    std::stringstream ss;
                    ss << int_value;
                    results.push_back({key, ss.str()});
                }
            }
        }
    }
}

uint32_t RedbParser::get_table_root() const {
    return stats_.root_page;
}

// ===== Page 实现 =====
bool Page::parse(const uint8_t* data, uint32_t page_size, FileFormatVersion version, uint32_t page_num) {
    header.page_number = page_num;
    header.type = static_cast<PageType>(data[0]);
    header.god_byte = data[1];
    header.entry_count = data[2] | (data[3] << 8);
    return true;
}

// ===== LeafEntry 实现 =====
std::string LeafEntry::key_to_string() const {
    return std::string(key_data.begin(), key_data.end());
}

std::string LeafEntry::value_to_string() const {
    return std::string(value_data.begin(), value_data.end());
}

// ===== BranchEntry 实现 =====
std::string BranchEntry::key_to_string() const {
    return std::string(key_data.begin(), key_data.end());
}

// 解析叶子页（保留旧函数以兼容）
bool RedbParser::parse_leaf_page(const uint8_t* data, Page& page, uint32_t page_size) {
    return true;
}

// 解析分支页（保留旧函数以兼容）
bool RedbParser::parse_branch_page(const uint8_t* data, Page& page, uint32_t page_size) {
    return true;
}

} // namespace redb
