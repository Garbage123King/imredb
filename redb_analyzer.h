#ifndef REDB_ANALYZER_H
#define REDB_ANALYZER_H

#include "redb_parser.h"
#include "imgui.h"
#include <string>
#include <vector>
#include <memory>

namespace redb {

// ===== 视图模式 =====
enum class ViewMode {
    Overview,      // 总览视图
    TreeView,      // 树视图
    PageList,      // 页面列表
    KeyValueList,  // 键值列表
    RawData        // 原始数据
};

// ===== redb 分析器 UI 类 =====
class RedbAnalyzer {
public:
    RedbAnalyzer();
    ~RedbAnalyzer();
    
    // 渲染 UI
    void render();
    
    // 打开文件
    bool open_file(const std::string& filepath);
    
    // 关闭文件
    void close_file();
    
    // 是否已加载文件
    bool has_file() const { return parser_ != nullptr && parser_->is_open(); }
    
    // 获取解析器
    RedbParser* get_parser() { return parser_.get(); }
    
    // 获取文件路径
    std::string get_filepath() const { return parser_ ? parser_->get_filepath() : ""; }
    
    // 获取当前视图模式
    ViewMode get_current_view() const { return current_view_; }
    
    // 设置视图模式
    void set_view(ViewMode view) { current_view_ = view; }
    
private:
    std::unique_ptr<RedbParser> parser_;
    ViewMode current_view_;
    
    // UI 组件
    int selected_page_;
    int selected_tree_depth_;
    bool show_page_details_;
    
    // 缓存的键值对列表
    std::vector<std::pair<std::string, std::string>> cached_key_values_;
    
    // 渲染各个视图
    void render_overview();
    void render_tree_view();
    void render_page_list();
    void render_key_value_list();
    void render_raw_data();
    void render_empty_state();
    void render_menu_bar();
    
    // 渲染工具函数
    void render_file_header_info();
    void render_transaction_info();
    void render_page_info(const Page& page);
    void render_tree_node(uint32_t page_num, int depth);
    
    // 辅助函数
    std::string get_page_type_string(PageType type) const;
    ImVec4 get_page_type_color(PageType type) const;
};

} // namespace redb

#endif // REDB_ANALYZER_H
