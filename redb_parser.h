#ifndef REDB_PARSER_H
#define REDB_PARSER_H

#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <fstream>
#include <optional>
#include <array>
#include <set>

namespace redb {

// ===== 常量定义 =====
// 超级头大小 (redb v4 uses 4096 bytes page)
constexpr size_t SUPER_HEADER_SIZE = 4096;

// 事务槽大小
constexpr size_t COMMIT_SLOT_SIZE = 256;

// 魔数 (小端序)
constexpr uint32_t REDB_MAGIC = 0x62646572;  // 'redb' in little-endian

// 版本号
enum class FileFormatVersion : uint32_t {
    V1 = 1,
    V2 = 2,
    V3 = 3,
    V4 = 4
};

// ===== 页面类型 =====
enum class PageType : uint8_t {
    Leaf = 1,    // 叶子页 - 存储键值对数据
    Branch = 2   // 分支页 - 存储子页指针和键
};

// ===== 数据类型长度 =====
enum class DataType : uint8_t {
    Fixed = 0,   // 定长数据
    Variable = 1 // 变长数据
};

// ===== 超级头结构 (redb v4) =====
struct SuperHeader {
    uint32_t magic;                     // 魔数：'redb' (0x72656462)
    uint8_t god_byte;                   // 上帝字节
    uint8_t page_size_exponent;          // 页大小指数 (2^exponent = page_size)
    uint16_t region_header_pages;        // 区域头页数
    uint32_t max_data_pages_per_region; // 每个区域最大数据页数
    uint32_t complete_regions;          // 已完全使用的区域数
    uint32_t tail_region_data_pages;    // 尾部区域数据页数
    
    // 解析超级头
    bool parse(const uint8_t* data);
    
    // 获取页大小
    uint32_t get_page_size() const { return 4096; }  // v4 固定使用 4096
    
    // 检查魔数
    bool is_valid() const { return magic == REDB_MAGIC; }
};

// ===== 事务槽结构 (redb v4) =====
struct CommitSlot {
    uint8_t format_version;             // 文件格式版本
    uint8_t god_byte;                   // 上帝字节
    uint8_t num_roots;                  // 根页数量
    uint8_t flags;                       // 标志位
    
    // 根页信息 (最多3个)
    struct RootInfo {
        uint32_t page_number;            // 页号
        uint8_t checksum[16];            // XXH3-128 校验和
        uint64_t tree_length;            // 树长度
    };
    
    RootInfo roots[3];
    uint64_t transaction_id;             // 事务ID
    uint8_t slot_checksum[16];           // 槽校验和
    
    // 解析事务槽
    bool parse(const uint8_t* data);
    
    // 检查槽是否有效
    bool is_valid() const { return format_version >= 1 && format_version <= 4; }
    
    // 获取用户根页号
    uint32_t get_user_root_page() const { return roots[0].page_number; }
    
    // 获取系统根页号
    uint32_t get_system_root_page() const { return roots[1].page_number; }
};

// ===== 页面头部结构 =====
struct PageHeader {
    PageType type;                       // 页面类型
    uint8_t god_byte;                    // 上帝字节
    uint16_t entry_count;                // 条目数量
    uint32_t page_number;                // 页号
    
    // 解析页面头部
    bool parse(const uint8_t* data);
};

// ===== 叶子页条目 =====
struct LeafEntry {
    uint32_t key_end_offset;             // 键结束偏移
    uint32_t value_end_offset;           // 值结束偏移
    std::vector<uint8_t> key_data;        // 键数据
    std::vector<uint8_t> value_data;      // 值数据
    
    // 获取键的字符串表示
    std::string key_to_string() const;
    
    // 获取值的字符串表示
    std::string value_to_string() const;
};

// ===== 分支页条目 =====
struct BranchEntry {
    uint32_t key_end_offset;             // 键结束偏移
    uint64_t child_checksum;             // 子页校验和
    uint32_t child_page;                 // 子页页号
    std::vector<uint8_t> key_data;       // 键数据
    
    // 获取键的字符串表示
    std::string key_to_string() const;
};

// ===== 页面结构 =====
struct Page {
    PageHeader header;                   // 页面头部
    std::vector<LeafEntry> leaf_entries; // 叶子页条目（如果是叶子页）
    std::vector<BranchEntry> branch_entries; // 分支页条目（如果是分支页）
    
    // 解析页面
    bool parse(const uint8_t* data, uint32_t page_size, FileFormatVersion version, uint32_t page_num);
};

// ===== 数据库统计信息 =====
struct DatabaseStats {
    FileFormatVersion version;            // 文件格式版本
    uint32_t page_size;                  // 页大小
    uint32_t total_pages;                // 总页数
    uint32_t complete_regions;           // 完整区域数
    uint32_t tail_region_pages;          // 尾部区域页数
    uint64_t transaction_id;              // 最新事务ID
    uint64_t tree_length;                // B树元素数量
    uint32_t root_page;                  // 根页号
    uint32_t system_root_page;           // 系统根页号
};

// ===== redb 解析器类 =====
class RedbParser {
public:
    RedbParser();
    ~RedbParser();
    
    // 打开数据库文件
    bool open(const std::string& filepath);
    
    // 关闭数据库文件
    void close();
    
    // 检查是否打开
    bool is_open() const { return file_.is_open(); }
    
    // 获取最后错误信息
    const char* get_error() const { return error_message_; }
    
    // 获取超级头
    const SuperHeader& get_super_header() const { return super_header_; }
    
    // 获取数据库统计
    const DatabaseStats& get_stats() const { return stats_; }
    
    // 获取所有页面信息
    const std::vector<Page>& get_pages() const { return pages_; }
    
    // 解析所有页面
    bool parse_all_pages();
    
    // 读取指定页面
    bool read_page(uint32_t page_num, Page& page);
    
    // 获取文件路径
    const std::string& get_filepath() const { return filepath_; }
    
    // 导出所有键值对
    std::vector<std::pair<std::string, std::string>> export_all_key_values();
    
    // 获取表的根页（简化版本，假设所有数据在一个表）
    uint32_t get_table_root() const;
    
    // 读取原始页面数据 (用于 CLI 工具)
    bool read_page_internal(uint32_t page_num, uint8_t* buffer);
    
private:
    std::string filepath_;
    std::ifstream file_;
    char error_message_[256];
    
    SuperHeader super_header_;
    CommitSlot commit_slots_[2];  // 两个事务槽
    DatabaseStats stats_;
    std::vector<Page> pages_;
    
    FileFormatVersion version_;
    uint32_t page_size_;
    uint32_t total_pages_;
    
    // 内部解析函数
    bool parse_super_header();
    bool parse_commit_slots();
    
    // 从系统表（页 3）中查找 root 指针
    uint32_t find_root_in_system_table();
    
    // 递归解析页面
    void parse_page_recursive(uint32_t page_num, std::set<uint32_t>& visited);
    
    // 解析叶子页条目
    void parse_leaf_entries(const uint8_t* buffer, Page& page);
    
    // 解析分支页条目
    void parse_branch_entries(const uint8_t* buffer, Page& page);
    
    // 遍历 B 树收集键值对
    void traverse_tree(uint32_t page_num, std::vector<std::pair<std::string, std::string>>& results);
    
    // 扫描所有叶子页以收集键值对（备用方法）
    void scan_all_leaf_pages(std::vector<std::pair<std::string, std::string>>& results);
    
    // 从叶子页数据中解析键值对
    void parse_leaf_page_data(const uint8_t* buffer, 
                              std::vector<std::pair<std::string, std::string>>& results);
    
    // 解析叶子页
    bool parse_leaf_page(const uint8_t* data, Page& page, uint32_t page_size);
    
    // 解析分支页
    bool parse_branch_page(const uint8_t* data, Page& page, uint32_t page_size);
};

} // namespace redb

#endif // REDB_PARSER_H
