#include "redb_analyzer.h"

namespace redb {

// ===== RedbAnalyzer Implementation =====
RedbAnalyzer::RedbAnalyzer() 
    : current_view_(ViewMode::Overview)
    , selected_page_(-1)
    , selected_tree_depth_(0)
    , show_page_details_(false) {
}

RedbAnalyzer::~RedbAnalyzer() {
    close_file();
}

bool RedbAnalyzer::open_file(const std::string& filepath) {
    parser_ = std::make_unique<RedbParser>();
    
    if (!parser_->open(filepath)) {
        return false;
    }
    
    // Cache key-value pairs
    cached_key_values_ = parser_->export_all_key_values();
    
    // Parse all pages
    parser_->parse_all_pages();
    
    return true;
}

void RedbAnalyzer::close_file() {
    if (parser_) {
        parser_->close();
        parser_.reset();
    }
    cached_key_values_.clear();
    selected_page_ = -1;
}

void RedbAnalyzer::render() {
    if (!has_file()) {
        render_empty_state();
        return;
    }
    
    // Render menu bar
    render_menu_bar();
    
    // Render main content area
    ImGui::Separator();
    
    // Render different content based on view mode
    switch (current_view_) {
        case ViewMode::Overview:
            render_overview();
            break;
        case ViewMode::TreeView:
            render_tree_view();
            break;
        case ViewMode::PageList:
            render_page_list();
            break;
        case ViewMode::KeyValueList:
            render_key_value_list();
            break;
        case ViewMode::RawData:
            render_raw_data();
            break;
    }
}

void RedbAnalyzer::render_menu_bar() {
    // View selection buttons
    if (ImGui::RadioButton("Overview", current_view_ == ViewMode::Overview)) {
        current_view_ = ViewMode::Overview;
    }
    ImGui::SameLine();
    
    if (ImGui::RadioButton("Tree View", current_view_ == ViewMode::TreeView)) {
        current_view_ = ViewMode::TreeView;
    }
    ImGui::SameLine();
    
    if (ImGui::RadioButton("Page List", current_view_ == ViewMode::PageList)) {
        current_view_ = ViewMode::PageList;
    }
    ImGui::SameLine();
    
    if (ImGui::RadioButton("Key-Value List", current_view_ == ViewMode::KeyValueList)) {
        current_view_ = ViewMode::KeyValueList;
    }
    ImGui::SameLine();
    
    if (ImGui::RadioButton("Raw Data", current_view_ == ViewMode::RawData)) {
        current_view_ = ViewMode::RawData;
    }
}

void RedbAnalyzer::render_overview() {
    if (ImGui::BeginChild("OverviewPanel", ImVec2(0, 0), true)) {
        // File info
        if (ImGui::CollapsingHeader("File Info", ImGuiTreeNodeFlags_DefaultOpen)) {
            const auto& stats = parser_->get_stats();
            
            ImGui::Text("File Path: %s", parser_->get_filepath().c_str());
            ImGui::Text("Format Version: v%d", static_cast<int>(stats.version));
            ImGui::Text("Page Size: %u bytes (2^%d)", 
                       stats.page_size, 
                       parser_->get_super_header().page_size_exponent);
            ImGui::Text("Total Pages: %u", stats.total_pages);
            ImGui::Text("Complete Regions: %u", stats.complete_regions);
            ImGui::Text("Tail Region Pages: %u", stats.tail_region_pages);
        }
        
        // Transaction info
        if (ImGui::CollapsingHeader("Transaction Info", ImGuiTreeNodeFlags_DefaultOpen)) {
            const auto& stats = parser_->get_stats();
            
            ImGui::Text("Transaction ID: %llu", (unsigned long long)stats.transaction_id);
            ImGui::Text("B-Tree Element Count: %llu", (unsigned long long)stats.tree_length);
            ImGui::Text("Root Page: %u", stats.root_page);
        }
        
        // Super header info
        if (ImGui::CollapsingHeader("Super Header")) {
            const auto& sh = parser_->get_super_header();
            
            ImGui::Text("Magic: 0x%08X (%s)", 
                       sh.magic, 
                       sh.is_valid() ? "Valid" : "Invalid");
            ImGui::Text("God Byte: 0x%02X", sh.god_byte);
            ImGui::Text("Region Header Pages: %u", sh.region_header_pages);
            ImGui::Text("Max Data Pages Per Region: %u", sh.max_data_pages_per_region);
        }
        
        // Statistics summary
        if (ImGui::CollapsingHeader("Statistics Summary")) {
            const auto& pages = parser_->get_pages();
            
            int leaf_count = 0;
            int branch_count = 0;
            int total_entries = 0;
            
            for (const auto& page : pages) {
                if (page.header.type == PageType::Leaf) {
                    leaf_count++;
                    total_entries += page.leaf_entries.size();
                } else if (page.header.type == PageType::Branch) {
                    branch_count++;
                }
            }
            
            ImGui::Text("Leaf Page Count: %d", leaf_count);
            ImGui::Text("Branch Page Count: %d", branch_count);
            ImGui::Text("Total Key-Value Pairs: %d", total_entries);
            ImGui::Text("Cached Key-Value Pairs: %zu", cached_key_values_.size());
        }
    }
    ImGui::EndChild();
}

void RedbAnalyzer::render_tree_view() {
    // Left panel: tree structure
    if (ImGui::BeginChild("TreePanel", ImVec2(300, 0), true)) {
        ImGui::Text("B-Tree Structure");
        ImGui::Separator();
        
        const auto& stats = parser_->get_stats();
        if (stats.root_page > 0) {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
            if (ImGui::TreeNode("Root Page")) {
                ImGui::Text("Page #: %u", stats.root_page);
                render_tree_node(stats.root_page, 1);
                ImGui::TreePop();
            }
            ImGui::PopStyleColor();
        } else {
            ImGui::Text("No root page info");
        }
    }
    ImGui::EndChild();
    
    ImGui::SameLine();
    
    // Right panel: selected page details
    if (ImGui::BeginChild("PageDetailPanel", ImVec2(0, 0), true)) {
        if (selected_page_ >= 0) {
            auto& pages = parser_->get_pages();
            if (selected_page_ < static_cast<int>(pages.size())) {
                render_page_info(pages[selected_page_]);
            }
        } else {
            ImGui::Text("Select a page to view details");
        }
    }
    ImGui::EndChild();
}

void RedbAnalyzer::render_tree_node(uint32_t page_num, int depth) {
    if (depth > 10) {  // Prevent infinite recursion
        ImGui::Text("...");
        return;
    }
    
    Page page;
    if (!parser_->read_page(page_num, page)) {
        ImGui::Text("[Error] Failed to read page %u", page_num);
        return;
    }
    
    ImGui::Indent();
    
    // Display current page info
    ImVec4 color = get_page_type_color(page.header.type);
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(color.x * 255, color.y * 255, color.z * 255, 255));
    
    std::string label = "Page #" + std::to_string(page_num) + " (" + get_page_type_string(page.header.type) + ")";
    
    if (page.header.type == PageType::Leaf) {
        if (ImGui::TreeNode(label.c_str())) {
            ImGui::Text("Entry Count: %u", page.header.entry_count);
            
            // Show first few keys
            for (size_t i = 0; i < page.leaf_entries.size() && i < 5; i++) {
                std::string key = page.leaf_entries[i].key_to_string();
                if (key.length() > 20) key = key.substr(0, 20) + "...";
                ImGui::BulletText("Key: %s", key.c_str());
            }
            
            if (page.leaf_entries.size() > 5) {
                ImGui::Text("... Total %zu entries", page.leaf_entries.size());
            }
            
            ImGui::TreePop();
        }
    } else if (page.header.type == PageType::Branch) {
        if (ImGui::TreeNode(label.c_str())) {
            ImGui::Text("Child Page Count: %zu", page.branch_entries.size() + 1);
            
            // Show first child page
            if (!page.branch_entries.empty()) {
                ImGui::Text("First Child Page: %u", page.branch_entries[0].child_page);
            }
            
            // Recursively display child pages
            for (size_t i = 0; i < page.branch_entries.size(); i++) {
                std::string child_label = "Child Page " + std::to_string(page.branch_entries[i].child_page);
                std::string key = page.branch_entries[i].key_to_string();
                if (key.length() > 15) key = key.substr(0, 15) + "...";
                child_label += " (key: " + key + ")";
                
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 200, 0, 255));
                if (ImGui::TreeNode(child_label.c_str())) {
                    render_tree_node(page.branch_entries[i].child_page, depth + 1);
                    ImGui::TreePop();
                }
                ImGui::PopStyleColor();
            }
            
            ImGui::TreePop();
        }
    }
    
    ImGui::PopStyleColor();
    ImGui::Unindent();
}

void RedbAnalyzer::render_page_list() {
    // Top toolbar
    if (ImGui::BeginChild("Toolbar", ImVec2(0, 30))) {
        ImGui::Text("Page Count: %zu", parser_->get_pages().size());
    }
    ImGui::EndChild();
    
    // Page list
    if (ImGui::BeginTable("PageTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Sortable)) {
        ImGui::TableSetupColumn("Page #", ImGuiTableColumnFlags_DefaultSort);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_DefaultSort);
        ImGui::TableSetupColumn("Entries", ImGuiTableColumnFlags_DefaultSort);
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_None);
        ImGui::TableHeadersRow();
        
        for (const auto& page : parser_->get_pages()) {
            ImGui::TableNextRow();
            
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%u", page.header.page_number);
            
            ImGui::TableSetColumnIndex(1);
            ImGui::PushStyleColor(ImGuiCol_Text, 
                IM_COL32(get_page_type_color(page.header.type).x * 255,
                         get_page_type_color(page.header.type).y * 255,
                         get_page_type_color(page.header.type).z * 255, 255));
            ImGui::Text("%s", get_page_type_string(page.header.type).c_str());
            ImGui::PopStyleColor();
            
            ImGui::TableSetColumnIndex(2);
            if (page.header.type == PageType::Leaf) {
                ImGui::Text("%zu", page.leaf_entries.size());
            } else {
                ImGui::Text("%zu", page.branch_entries.size());
            }
            
            ImGui::TableSetColumnIndex(3);
            if (ImGui::SmallButton(("View##" + std::to_string(page.header.page_number)).c_str())) {
                selected_page_ = page.header.page_number;
                show_page_details_ = true;
            }
        }
        
        ImGui::EndTable();
    }
    
    // Page details popup
    if (show_page_details_ && selected_page_ >= 0) {
        ImGui::SetNextWindowSize(ImVec2(500, 400));
        if (ImGui::Begin("Page Details", &show_page_details_)) {
            auto& pages = parser_->get_pages();
            if (selected_page_ < static_cast<int>(pages.size())) {
                render_page_info(pages[selected_page_]);
            }
        }
        ImGui::End();
    }
}

void RedbAnalyzer::render_key_value_list() {
    // Search bar
    static char search_buffer[256] = "";
    ImGui::InputText("Search", search_buffer, sizeof(search_buffer));
    ImGui::Separator();
    
    // Key-value list
    if (ImGui::BeginTable("KVTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Sortable)) {
        ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_DefaultSort);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_DefaultSort);
        ImGui::TableHeadersRow();
        
        for (const auto& kv : cached_key_values_) {
            // Filter by search
            if (strlen(search_buffer) > 0) {
                if (kv.first.find(search_buffer) == std::string::npos &&
                    kv.second.find(search_buffer) == std::string::npos) {
                    continue;
                }
            }
            
            ImGui::TableNextRow();
            
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", kv.first.c_str());
            
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", kv.second.c_str());
        }
        
        ImGui::EndTable();
    }
    
    ImGui::Text("Total Key-Value Pairs: %zu", cached_key_values_.size());
}

void RedbAnalyzer::render_raw_data() {
    const auto& stats = parser_->get_stats();
    static int selected_page_for_raw = 0;
    
    // Left panel: page selector with preview
    if (ImGui::BeginChild("PageSelector", ImVec2(320, 0), true)) {
        ImGui::Text("Select Page:");
        ImGui::Separator();
        
        // Create a scrollable list of pages with preview
        for (uint32_t page_num = 0; page_num < stats.total_pages; page_num++) {
            // Read first 32 bytes for preview
            uint8_t preview_buf[32];
            std::vector<uint8_t> buffer(4096);
            
            if (!parser_->read_page_internal(page_num, buffer.data())) {
                continue;
            }
            memcpy(preview_buf, buffer.data(), 32);
            
            // Build preview string
            char preview[64];
            uint8_t page_type = preview_buf[0];
            
            if (page_type == 1) {
                // Leaf page
                uint16_t entries = preview_buf[2] | (preview_buf[3] << 8);
                snprintf(preview, sizeof(preview), "[Leaf] entries=%u", entries);
            } else if (page_type == 2) {
                // Branch page
                uint16_t keys = preview_buf[2] | (preview_buf[3] << 8);
                uint32_t child = preview_buf[48] | (preview_buf[49] << 8) | 
                                (preview_buf[50] << 16) | (preview_buf[51] << 24);
                snprintf(preview, sizeof(preview), "[Branch] keys=%u, child0=%u", keys, child);
            } else if (page_type == 0) {
                // Empty page
                snprintf(preview, sizeof(preview), "[Empty]");
            } else {
                // Unknown type
                snprintf(preview, sizeof(preview), "[Type=%d]", page_type);
            }
            
            // Add hex preview of first bytes
            char hex_preview[32] = "";
            if (preview_buf[0] != 0) {
                snprintf(hex_preview, sizeof(hex_preview), " %02X %02X %02X %02X...", 
                        preview_buf[0], preview_buf[1], preview_buf[2], preview_buf[3]);
            }
            
            std::string label = std::string("Page ") + std::to_string(page_num) + 
                               ": " + preview + hex_preview;
            
            bool is_selected = (selected_page_for_raw == static_cast<int>(page_num));
            if (is_selected) {
                ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);
            }
            
            if (ImGui::Selectable(label.c_str(), is_selected)) {
                selected_page_for_raw = page_num;
            }
            
            if (is_selected) {
                ImGui::PopStyleColor();
            }
        }
    }
    ImGui::EndChild();
    
    ImGui::SameLine();
    
    // Right panel: raw data display
    if (ImGui::BeginChild("HexView", ImVec2(0, 0), true)) {
        ImGui::Text("Page #%d - Raw Data", selected_page_for_raw);
        ImGui::Separator();
        
        // Read raw page data
        std::vector<uint8_t> buffer(4096);
        if (parser_->read_page_internal(selected_page_for_raw, buffer.data())) {
            const int bytes_per_row = 16;
            int rows = (4096 + bytes_per_row - 1) / bytes_per_row;
            
            // Display hex with ASCII
            // Column headers
            ImGui::Text("Offset    00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F  | ASCII");
            ImGui::Separator();
            
            char line[256];
            for (int r = 0; r < rows; r++) {
                // Offset
                snprintf(line, sizeof(line), "%04X     ", r * bytes_per_row);
                std::string hex_part = line;
                std::string ascii_part;
                
                // Hex bytes
                for (int c = 0; c < bytes_per_row; c++) {
                    int idx = r * bytes_per_row + c;
                    if (idx < 4096) {
                        snprintf(line, sizeof(line), "%02X ", buffer[idx]);
                        hex_part += line;
                    } else {
                        hex_part += "   ";
                    }
                }
                
                hex_part += "| ";
                
                // ASCII characters
                for (int c = 0; c < bytes_per_row; c++) {
                    int idx = r * bytes_per_row + c;
                    if (idx < 4096) {
                        char ch = buffer[idx];
                        if (ch >= 32 && ch < 127) {
                            ascii_part += ch;
                        } else {
                            ascii_part += '.';
                        }
                    }
                }
                
                ImGui::Text("%s%s", hex_part.c_str(), ascii_part.c_str());
            }
        } else {
            ImGui::Text("Failed to read page %d", selected_page_for_raw);
        }
    }
    ImGui::EndChild();
}

void RedbAnalyzer::render_page_info(const Page& page) {
    ImGui::Text("Page #: %u", page.header.page_number);
    ImGui::Text("Type: %s", get_page_type_string(page.header.type).c_str());
    ImGui::Text("Entry Count: %u", page.header.entry_count);
    ImGui::Separator();
    
    if (page.header.type == PageType::Leaf) {
        ImGui::Text("Leaf Page Content:");
        ImGui::Separator();
        
        for (size_t i = 0; i < page.leaf_entries.size(); i++) {
            const auto& entry = page.leaf_entries[i];
            std::string key = entry.key_to_string();
            std::string value = entry.value_to_string();
            
            ImGui::Text("[%zu] Key Offset: %u, Value Offset: %u", i, entry.key_end_offset, entry.value_end_offset);
            
            ImGui::BulletText("Key: %s", key.c_str());
            ImGui::BulletText("Value: %s", value.c_str());
            ImGui::Spacing();
        }
    } else if (page.header.type == PageType::Branch) {
        ImGui::Text("Branch Page Content:");
        ImGui::Separator();
        
        for (size_t i = 0; i < page.branch_entries.size(); i++) {
            const auto& entry = page.branch_entries[i];
            std::string key = entry.key_to_string();
            
            ImGui::Text("[%zu] Key Offset: %u", i, entry.key_end_offset);
            ImGui::BulletText("Child Page: %u", entry.child_page);
            ImGui::BulletText("Key: %s", key.c_str());
            ImGui::Spacing();
        }
    }
}

void RedbAnalyzer::render_empty_state() {
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, 
                                   ImGui::GetIO().DisplaySize.y * 0.5f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    
    ImGui::BeginChild("EmptyState", ImVec2(400, 200), true, 
                      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
    
    ImGui::Text("redb Database Analyzer");
    ImGui::Separator();
    ImGui::Text("Please open a .redb file to analyze");
    
    static char filepath[512] = "";
    ImGui::InputText("File Path", filepath, sizeof(filepath));
    
    if (ImGui::Button("Open File")) {
        if (strlen(filepath) > 0) {
            if (!open_file(filepath)) {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error: %s", parser_->get_error());
            }
        }
    }
    
    ImGui::EndChild();
}

std::string RedbAnalyzer::get_page_type_string(PageType type) const {
    switch (type) {
        case PageType::Leaf: return "Leaf";
        case PageType::Branch: return "Branch";
        default: return "Unknown";
    }
}

ImVec4 RedbAnalyzer::get_page_type_color(PageType type) const {
    switch (type) {
        case PageType::Leaf: return ImVec4(0.2f, 0.8f, 0.2f, 1.0f);   // Green
        case PageType::Branch: return ImVec4(0.2f, 0.6f, 1.0f, 1.0f); // Blue
        default: return ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    }
}

} // namespace redb
