// redb_cli.cpp - redb Database Analyzer Command Line Tool
// Analyze redb files without GUI

#include "redb_parser.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <getopt.h>

namespace redb {

// CLI options
struct CLIOptions {
    std::string input_file;
    bool show_help = false;
    bool show_overview = false;
    bool show_pages = false;
    bool show_key_values = false;
    bool show_raw_page = false;
    int raw_page_num = 0;
    std::string search_key;
};

// Print help message
void print_help(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS] <input_file>\n"
              << "\nOptions:\n"
              << "  -h, --help           Show this help message\n"
              << "  -o, --overview       Show file overview\n"
              << "  -p, --pages          Show page list\n"
              << "  -k, --key-values     Show all key-value pairs\n"
              << "  -r, --raw <num>      Show raw data of specified page\n"
              << "  -s, --search <key>  Search for records containing key\n"
              << "\nExamples:\n"
              << "  " << program_name << " my_db.redb\n"
              << "  " << program_name << " -o my_db.redb\n"
              << "  " << program_name << " -k my_db.redb\n"
              << "  " << program_name << " -r 0 my_db.redb\n"
              << "  " << program_name << " -s \"my_key\" my_db.redb\n";
}

// Show file overview
void show_overview(RedbParser& parser) {
    const auto& stats = parser.get_stats();
    const auto& sh = parser.get_super_header();
    
    std::cout << "\n========== redb File Overview ==========\n\n";
    
    std::cout << "[File Info]\n";
    std::cout << "  File Path: " << parser.get_filepath() << "\n";
    std::cout << "  Format Version: v" << static_cast<int>(stats.version) << "\n";
    std::cout << "  Page Size: " << stats.page_size << " bytes (2^" 
              << static_cast<int>(sh.page_size_exponent) << ")\n";
    std::cout << "  Total Pages: " << stats.total_pages << "\n";
    std::cout << "  Complete Regions: " << stats.complete_regions << "\n";
    std::cout << "  Tail Region Pages: " << stats.tail_region_pages << "\n";
    
    std::cout << "\n[Transaction Info]\n";
    std::cout << "  Transaction ID: " << stats.transaction_id << "\n";
    std::cout << "  B-Tree Element Count: " << stats.tree_length << "\n";
    std::cout << "  Root Page: " << stats.root_page << "\n";
    
    std::cout << "\n[Super Header Info]\n";
    std::cout << "  Magic: 0x" << std::hex << sh.magic << std::dec;
    if (sh.is_valid()) {
        std::cout << " (Valid)";
    } else {
        std::cout << " (Invalid!)";
    }
    std::cout << "\n";
    std::cout << "  God Byte: 0x" << std::hex << (int)sh.god_byte << std::dec << "\n";
    std::cout << "  Region Header Pages: " << sh.region_header_pages << "\n";
    std::cout << "  Max Data Pages Per Region: " << sh.max_data_pages_per_region << "\n";
    
    std::cout << "\n[Page Statistics]\n";
    const auto& pages = parser.get_pages();
    int leaf_count = 0, branch_count = 0;
    for (const auto& page : pages) {
        if (page.header.type == PageType::Leaf) leaf_count++;
        else if (page.header.type == PageType::Branch) branch_count++;
    }
    std::cout << "  Leaf Page Count: " << leaf_count << "\n";
    std::cout << "  Branch Page Count: " << branch_count << "\n";
    
    std::cout << "\n===================================\n";
}

// Show page list
void show_pages(RedbParser& parser) {
    const auto& pages = parser.get_pages();
    
    std::cout << "\n========== Page List ==========\n\n";
    std::cout << std::setw(8) << "Page #" 
              << std::setw(12) << "Type" 
              << std::setw(12) << "Entries" 
              << "\n";
    std::cout << std::string(32, '-') << "\n";
    
    for (const auto& page : pages) {
        std::cout << std::setw(8) << page.header.page_number;
        
        if (page.header.type == PageType::Leaf) {
            std::cout << std::setw(12) << "Leaf";
            std::cout << std::setw(12) << page.leaf_entries.size();
        } else if (page.header.type == PageType::Branch) {
            std::cout << std::setw(12) << "Branch";
            std::cout << std::setw(12) << page.branch_entries.size();
        } else {
            std::cout << std::setw(12) << "Unknown";
            std::cout << std::setw(12) << page.header.entry_count;
        }
        std::cout << "\n";
    }
    
    std::cout << "\n================================\n";
}

// Show key-value pairs
void show_key_values(RedbParser& parser, const std::string& search = "") {
    auto key_values = parser.export_all_key_values();
    
    std::cout << "\n========== Key-Value List ==========\n\n";
    
    int count = 0;
    for (const auto& kv : key_values) {
        // Filter by search keyword
        if (!search.empty()) {
            if (kv.first.find(search) == std::string::npos) {
                continue;
            }
        }
        
        std::cout << "[" << count << "] "
                  << "Key: " << kv.first << "\n"
                  << "    Value: " << kv.second << "\n"
                  << "\n";
        count++;
    }
    
    std::cout << "Found " << count << " records\n";
    std::cout << "\n===================================\n";
}

// Show raw page data
void show_raw_page(RedbParser& parser, int page_num) {
    auto stats = parser.get_stats();
    
    if (page_num < 0 || page_num >= static_cast<int>(stats.total_pages)) {
        std::cerr << "Error: Page #" << page_num << " out of range (0-" 
                  << (stats.total_pages - 1) << ")\n";
        return;
    }
    
    std::vector<uint8_t> buffer(stats.page_size);
    if (!parser.read_page_internal(page_num, buffer.data())) {
        std::cerr << "Error: Cannot read page " << page_num << "\n";
        return;
    }
    
    std::cout << "\n========== Raw Page " << page_num << " Data ==========\n\n";
    
    // Page header info
    PageType type = static_cast<PageType>(buffer[0]);
    uint16_t entry_count = buffer[2] | (buffer[3] << 8);
    
    std::cout << "Page #: " << page_num << "\n";
    std::cout << "Type: " << (type == PageType::Leaf ? "Leaf" : 
                             type == PageType::Branch ? "Branch" : "Unknown") << "\n";
    std::cout << "Entry Count: " << entry_count << "\n";
    std::cout << "Page Size: " << stats.page_size << " bytes\n\n";
    
    // Hex display (first 256 bytes)
    const int bytes_per_line = 16;
    int display_bytes = std::min(256, static_cast<int>(buffer.size()));
    int lines = (display_bytes + bytes_per_line - 1) / bytes_per_line;
    
    std::cout << "Offset    Hex (first " << display_bytes << " bytes)\n";
    std::cout << std::string(60, '-') << "\n";
    
    for (int i = 0; i < lines; i++) {
        // Offset
        std::cout << std::setfill('0') << std::setw(4) << std::hex 
                  << (i * bytes_per_line) << "   ";
        
        // Hex data
        for (int j = 0; j < bytes_per_line; j++) {
            int idx = i * bytes_per_line + j;
            if (idx < display_bytes) {
                std::cout << std::setfill('0') << std::setw(2) << std::hex 
                          << (int)buffer[idx] << " ";
            } else {
                std::cout << "   ";
            }
            if (j == 7) std::cout << " ";
        }
        
        std::cout << " ";
        
        // ASCII characters
        for (int j = 0; j < bytes_per_line; j++) {
            int idx = i * bytes_per_line + j;
            if (idx < display_bytes) {
                char ch = buffer[idx];
                if (ch >= 32 && ch < 127) {
                    std::cout << ch;
                } else {
                    std::cout << '.';
                }
            }
        }
        
        std::cout << "\n" << std::dec;
    }
    
    std::cout << "\n======================================\n";
}

// Parse command line options
CLIOptions parse_arguments(int argc, char* argv[]) {
    CLIOptions options;
    
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"overview", no_argument, 0, 'o'},
        {"pages", no_argument, 0, 'p'},
        {"key-values", no_argument, 0, 'k'},
        {"raw", required_argument, 0, 'r'},
        {"search", required_argument, 0, 's'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "hopkr:s:", 
                             long_options, &option_index)) != -1) {
        switch (opt) {
            case 'h':
                options.show_help = true;
                break;
            case 'o':
                options.show_overview = true;
                break;
            case 'p':
                options.show_pages = true;
                break;
            case 'k':
                options.show_key_values = true;
                break;
            case 'r':
                options.show_raw_page = true;
                options.raw_page_num = std::atoi(optarg);
                break;
            case 's':
                options.search_key = optarg;
                break;
            default:
                break;
        }
    }
    
    // Remaining arguments are input file
    if (optind < argc) {
        options.input_file = argv[optind];
    }
    
    // If no option specified, show overview by default
    if (!options.show_overview && !options.show_pages && 
        !options.show_key_values && !options.show_raw_page && 
        options.search_key.empty()) {
        options.show_overview = true;
    }
    
    return options;
}

} // namespace redb

int main(int argc, char* argv[]) {
    using namespace redb;
    
    // Parse command line options
    CLIOptions options = parse_arguments(argc, argv);
    
    if (options.show_help) {
        print_help(argv[0]);
        return 0;
    }
    
    if (options.input_file.empty()) {
        std::cerr << "Error: Please specify input file\n";
        std::cerr << "Use --help for usage\n";
        return 1;
    }
    
    // Open and parse file
    RedbParser parser;
    if (!parser.open(options.input_file)) {
        std::cerr << "Error: Cannot open file - " << parser.get_error() << "\n";
        return 1;
    }
    
    std::cout << "Successfully opened: " << options.input_file << "\n";
    
    // Execute based on options
    if (options.show_overview) {
        parser.parse_all_pages();
        show_overview(parser);
    }
    
    if (options.show_pages) {
        parser.parse_all_pages();
        show_pages(parser);
    }
    
    if (options.show_key_values) {
        show_key_values(parser, options.search_key);
    }
    
    if (options.show_raw_page) {
        show_raw_page(parser, options.raw_page_num);
    }
    
    // If only search keyword specified, show matching key-values
    if (!options.search_key.empty() && !options.show_key_values) {
        show_key_values(parser, options.search_key);
    }
    
    return 0;
}
