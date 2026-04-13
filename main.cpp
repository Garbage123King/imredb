// main.cpp - redb Database Analyzer (ImGui UI)
// Displays redb file content and structure

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include "redb_analyzer.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>

// Window settings
const char* glsl_version = "#version 130";
const int WINDOW_WIDTH = 1280;
const int WINDOW_HEIGHT = 720;

// Global analyzer
redb::RedbAnalyzer g_analyzer;

// File browser functions
static void render_file_browser();
static void render_toolbar();
static void render_help();

// File browser state
static bool show_file_browser = true;
static char db_filepath[512] = "";
static std::vector<std::string> recent_files;
static const int MAX_RECENT_FILES = 10;

int main(int argc, char** argv) {
    // Initialize GLFW
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }
    
    // Configure OpenGL
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
    
    // Create window
    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, 
                                         "redb Database Analyzer", NULL, NULL);
    if (!window) {
        fprintf(stderr, "Failed to create window\n");
        glfwTerminate();
        return 1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    
    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    // Set ImGui style
    ImGui::StyleColorsDark();
    
    // Initialize platform/renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    
    // Set ImGui font
    io.Fonts->AddFontDefault();
    
    // Handle command line arguments
    if (argc > 1) {
        strncpy(db_filepath, argv[1], sizeof(db_filepath) - 1);
        db_filepath[sizeof(db_filepath) - 1] = '\0';
        show_file_browser = false;
        
        // Try to open file
        if (!g_analyzer.open_file(db_filepath)) {
            printf("Failed to open file: %s\n", db_filepath);
            show_file_browser = true;
        }
    }
    
    // Main loop
    while (!glfwWindowShouldClose(window)) {
        // Event handling
        glfwPollEvents();
        
        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Set ImGui window
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(WINDOW_WIDTH, WINDOW_HEIGHT));
        
        // Render main UI
        {
            ImGui::Begin("redb Analyzer", NULL, 
                        ImGuiWindowFlags_NoResize | 
                        ImGuiWindowFlags_NoMove |
                        ImGuiWindowFlags_NoTitleBar |
                        ImGuiWindowFlags_MenuBar);
            
            // Menu bar
            if (ImGui::BeginMenuBar()) {
                if (ImGui::BeginMenu("File")) {
                    if (ImGui::MenuItem("Open...", "Ctrl+O")) {
                        show_file_browser = true;
                    }
                    if (ImGui::MenuItem("Close", "Ctrl+W")) {
                        g_analyzer.close_file();
                        db_filepath[0] = '\0';
                        show_file_browser = true;
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Exit", "Alt+F4")) {
                        glfwSetWindowShouldClose(window, GLFW_TRUE);
                    }
                    ImGui::EndMenu();
                }
                
                if (ImGui::BeginMenu("View")) {
                    ImGui::MenuItem("Overview", NULL, 
                                   g_analyzer.get_current_view() == redb::ViewMode::Overview);
                    ImGui::MenuItem("Tree View", NULL,
                                   g_analyzer.get_current_view() == redb::ViewMode::TreeView);
                    ImGui::MenuItem("Page List", NULL,
                                   g_analyzer.get_current_view() == redb::ViewMode::PageList);
                    ImGui::MenuItem("Key-Value List", NULL,
                                   g_analyzer.get_current_view() == redb::ViewMode::KeyValueList);
                    ImGui::MenuItem("Raw Data", NULL,
                                   g_analyzer.get_current_view() == redb::ViewMode::RawData);
                    ImGui::EndMenu();
                }
                
                if (ImGui::BeginMenu("Help")) {
                    if (ImGui::MenuItem("Usage Guide")) {
                        render_help();
                    }
                    ImGui::EndMenu();
                }
                
                ImGui::EndMenuBar();
            }
            
            // Toolbar
            render_toolbar();
            
            ImGui::Separator();
            
            // Main content area
            if (show_file_browser || !g_analyzer.has_file()) {
                render_file_browser();
            } else {
                g_analyzer.render();
            }
            
            ImGui::End();
        }
        
        // Render
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glfwMakeContextCurrent(window);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        glfwSwapBuffers(window);
    }
    
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}

// ===== Toolbar =====
static void render_toolbar() {
    if (ImGui::BeginChild("Toolbar", ImVec2(0, 40), false)) {
        // File info
        if (g_analyzer.has_file()) {
            ImGui::Text("Open: %s", g_analyzer.get_filepath().c_str());
            ImGui::SameLine(300);
            
            auto stats = g_analyzer.get_parser()->get_stats();
            ImGui::Text("Version: v%d | Page Size: %u | Total Pages: %u", 
                       static_cast<int>(stats.version),
                       stats.page_size,
                       stats.total_pages);
            
            ImGui::SameLine();
            if (ImGui::SmallButton(" Close ")) {
                g_analyzer.close_file();
                show_file_browser = true;
            }
        } else {
            ImGui::Text("redb Database Analyzer - No file open");
        }
    }
    ImGui::EndChild();
}

// ===== File Browser =====
static void render_file_browser() {
    ImGui::SetNextWindowPos(ImVec2(WINDOW_WIDTH * 0.5f, WINDOW_HEIGHT * 0.5f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    
    ImGui::BeginChild("FileBrowser", ImVec2(500, 400), true);
    
    ImGui::Text("Open redb Database File");
    ImGui::Separator();
    
    // File path input
    ImGui::InputText("File Path", db_filepath, sizeof(db_filepath));
    
    // Quick path
    ImGui::Text("Quick Path:");
    ImGui::SameLine();
    if (ImGui::SmallButton(" my_db.redb ")) {
        strcpy(db_filepath, "my_redb_demo/my_db.redb");
    }
    
    ImGui::Separator();
    
    // Recent files
    if (!recent_files.empty()) {
        if (ImGui::CollapsingHeader("Recent Files")) {
            for (const auto& file : recent_files) {
                if (ImGui::Selectable(file.c_str())) {
                    strcpy(db_filepath, file.c_str());
                }
            }
        }
    }
    
    ImGui::Separator();
    
    // Open button
    if (ImGui::Button(" Open File ", ImVec2(120, 30))) {
        if (strlen(db_filepath) > 0) {
            if (g_analyzer.open_file(db_filepath)) {
                show_file_browser = false;
                
                // Add to recent files
                bool found = false;
                for (const auto& f : recent_files) {
                    if (f == db_filepath) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    recent_files.insert(recent_files.begin(), db_filepath);
                    if (recent_files.size() > MAX_RECENT_FILES) {
                        recent_files.pop_back();
                    }
                }
            } else {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error: %s", 
                                  g_analyzer.get_parser()->get_error());
            }
        }
    }
    
    ImGui::SameLine();
    
    if (ImGui::Button(" Cancel ", ImVec2(80, 30))) {
        show_file_browser = false;
    }
    
    ImGui::EndChild();
}

// ===== Help =====
static void render_help() {
    ImGui::Begin("Usage Guide", NULL, ImGuiWindowFlags_AlwaysAutoResize);
    
    ImGui::Text("redb Database Analyzer");
    ImGui::Separator();
    
    ImGui::Text("Features:");
    ImGui::BulletText("Overview: Display database file info and statistics");
    ImGui::BulletText("Tree View: Show B-tree structure");
    ImGui::BulletText("Page List: List all pages with their types");
    ImGui::BulletText("Key-Value List: Show all key-value pairs with search");
    ImGui::BulletText("Raw Data: View page content in hex format");
    
    ImGui::Separator();
    
    ImGui::Text("Shortcuts:");
    ImGui::BulletText("Ctrl+O: Open file");
    ImGui::BulletText("Ctrl+W: Close file");
    ImGui::BulletText("ESC: Exit");
    
    ImGui::Separator();
    
    ImGui::Text("redb File Format:");
    ImGui::BulletText("Page size: 4KB (configurable via super header)");
    ImGui::BulletText("Page types: Leaf (key-value data), Branch (index)");
    ImGui::BulletText("Uses Copy-on-Write (COW) B-tree storage");
    
    if (ImGui::Button("Close")) {}
    ImGui::End();
}
