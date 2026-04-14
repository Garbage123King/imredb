#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <windows.h>
#include <commdlg.h>
#include <vector>
#include <string>
#include <fstream>
#include <iomanip>

const size_t PAGE_SIZE = 4096;

struct ViewerState {
    std::vector<uint8_t> fileData;
    int selectedPage = -1;
    std::string currentFileName = "None";
};

// --- 系统对话框与辅助函数 ---
std::string OpenFileDialog() {
    OPENFILENAMEA ofn;
    char szFile[260] = { 0 };
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Redb Files\0*.bin;*.db\0All Files\0*.*\0";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn) == TRUE) return std::string(ofn.lpstrFile);
    return "";
}

void Property(const char* label, const char* fmt, ...) {
    ImGui::TextDisabled("%s:", label);
    ImGui::SameLine(140);
    va_list args; va_start(args, fmt);
    ImGui::TextV(fmt, args);
    va_end(args);
}

// --- 渲染中间栏：元数据 ---
void RenderMetadata(uint8_t* pagePtr, int pageIdx) {
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "PAGE %d DETAILS", pageIdx);
    ImGui::Separator();
    ImGui::Spacing();

    if (pageIdx == 0) {
        Property("Magic", "%.9s", (char*)pagePtr);
        Property("God Byte", "%02X", pagePtr[9]);
        Property("Page Size", "%u", *(uint32_t*)(pagePtr + 12));
        Property("Region Header Pgs", "%u", *(uint32_t*)(pagePtr + 16));
        Property("Max Data Pages", "%u", *(uint32_t*)(pagePtr + 20));
        Property("Full Regions", "%u", *(uint32_t*)(pagePtr + 24));
    } else {
        uint8_t type = pagePtr[0];
        Property("Type", "%d", (int)type);
        uint16_t count = *(uint16_t*)(pagePtr + 2);

        if (type == 2) {
            Property("Num Keys", "%d", count);
            ImGui::Separator();
            uint64_t* pnums = (uint64_t*)(pagePtr + 8 + (count + 1) * 16);
            for(int i=0; i <= count; ++i) ImGui::Text("[%d] Child: %llu", i, (unsigned long long)pnums[i]);
        } 
        else if (type == 1) {
            Property("Num Entries", "%d", count);
            ImGui::Separator();
            uint32_t* k_ends = (uint32_t*)(pagePtr + 4);
            uint32_t* v_ends = (uint32_t*)(pagePtr + 4 + count * 4);
            if (ImGui::BeginTable("Offsets", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Idx"); ImGui::TableSetupColumn("K-End"); ImGui::TableSetupColumn("V-End");
                ImGui::TableHeadersRow();
                for (int i = 0; i < count; i++) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Text("%d", i);
                    ImGui::TableSetColumnIndex(1); ImGui::Text("%u", k_ends[i]);
                    ImGui::TableSetColumnIndex(2); ImGui::Text("%u", v_ends[i]);
                }
                ImGui::EndTable();
            }
        }
    }
}

// --- 渲染右栏：十六进制 ---
void RenderHexView(const uint8_t* data, size_t size) {
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "RAW DATA VIEW");
    ImGui::Separator();
    ImGui::BeginChild("hex_scroll");
    ImGuiListClipper clipper;
    clipper.Begin((int)(size / 16)); 
    while (clipper.Step()) {
        for (int line = clipper.DisplayStart; line < clipper.DisplayEnd; line++) {
            size_t offset = line * 16;
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%04X ", (unsigned int)offset);
            ImGui::SameLine();
            char hex_buf[64]; char* p = hex_buf;
            for (int i = 0; i < 16; i++) {
                uint8_t val = data[offset + i];
                *p++ = "0123456789ABCDEF"[val >> 4];
                *p++ = "0123456789ABCDEF"[val & 0xF];
                *p++ = ' ';
            }
            *p = '\0';
            ImGui::TextUnformatted(hex_buf); ImGui::SameLine();
            char ascii_buf[18];
            for (int i = 0; i < 16; i++) {
                uint8_t c = data[offset + i];
                ascii_buf[i] = (c >= 32 && c <= 126) ? (char)c : '.';
            }
            ascii_buf[16] = '\0';
            ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "| %s", ascii_buf);
        }
    }
    ImGui::EndChild();
}

int main() {
    if (!glfwInit()) return 1;
    GLFWwindow* window = glfwCreateWindow(1400, 800, "Redb Explorer", NULL, NULL);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    ViewerState state;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("Layout", nullptr, ImGuiWindowFlags_NoDecoration);

        if (ImGui::Button("Open File...")) {
            std::string path = OpenFileDialog();
            if (!path.empty()) {
                std::ifstream file(path, std::ios::binary | std::ios::ate);
                if (file.is_open()) {
                    std::streamsize size = file.tellg();
                    file.seekg(0, std::ios::beg);
                    state.fileData.assign(size, 0);
                    file.read((char*)state.fileData.data(), size);
                    state.currentFileName = path;
                    state.selectedPage = -1;
                }
            }
        }
        ImGui::SameLine(); ImGui::Text("File: %s", state.currentFileName.c_str());
        ImGui::Separator();

        ImGui::Columns(3, "three_panes", true);
        static bool set_width = false;
        if (!set_width) {
            ImGui::SetColumnWidth(0, 240.0f); // 稍微加宽以显示 E: 或 K:
            ImGui::SetColumnWidth(1, 350.0f);
            set_width = true;
        }

        // 1. 左栏：增强版列表
        ImGui::BeginChild("list_pane");
        int totalPages = (int)(state.fileData.size() / PAGE_SIZE);
        for (int i = 0; i < totalPages; i++) {
            uint8_t* p = &state.fileData[i * PAGE_SIZE];
            uint8_t type = p[0];
            char label[128];

            if (i == 0) {
                sprintf(label, "P0 (Header)");
            } else {
                uint16_t count = *(uint16_t*)(p + 2);
                if (type == 1) sprintf(label, "P%d (T:1, E:%u)", i, count);
                else if (type == 2) sprintf(label, "P%d (T:2, K:%u)", i, count);
                else sprintf(label, "P%d (T:%u)", i, (int)type);
            }

            if (ImGui::Selectable(label, state.selectedPage == i)) state.selectedPage = i;
        }
        ImGui::EndChild();
        ImGui::NextColumn();

        // 2. 中栏
        ImGui::BeginChild("meta_pane");
        if (state.selectedPage >= 0) RenderMetadata(&state.fileData[state.selectedPage * PAGE_SIZE], state.selectedPage);
        ImGui::EndChild();
        ImGui::NextColumn();

        // 3. 右栏
        ImGui::BeginChild("hex_pane");
        if (state.selectedPage >= 0) RenderHexView(&state.fileData[state.selectedPage * PAGE_SIZE], PAGE_SIZE);
        ImGui::EndChild();

        ImGui::End();
        ImGui::Render();
        int dw, dh;
        glfwGetFramebufferSize(window, &dw, &dh);
        glViewport(0, 0, dw, dh);
        glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }
    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext(); glfwTerminate();
    return 0;
}