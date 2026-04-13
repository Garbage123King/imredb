cmake_minimum_required(VERSION 3.10)
project(ImGuiExample)

# 1. 编码补丁（放在最前面，确保所有文件都按 UTF-8 处理）
if(MSVC)
    add_compile_options(/utf-8)
endif()

# 2. 寻找 GLFW 库 (vcpkg 建议加上 CONFIG)
find_package(glfw3 CONFIG REQUIRED)

# 3. 设置 ImGui 源码路径
set(IMGUI_DIR "imgui-1.92.5")

# redb 解析器源文件
set(REDB_PARSER_SOURCES
    redb_parser.cpp
    redb_analyzer.cpp
)

# 使用 list(APPEND) 比 GLOB 更可控，但 GLOB 在简单项目中也可以
file(GLOB IMGUI_SOURCES 
    "${IMGUI_DIR}/*.cpp" 
    "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp" 
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
)

# 4. 创建可执行文件 (目标名为 imgui_demo)
add_executable(imgui_demo main.cpp ${IMGUI_SOURCES} ${REDB_PARSER_SOURCES})

# 5. 包含头文件路径
# 必须包含 backends 目录，否则编译 imgui_impl_xxxx.cpp 时找不到头文件
target_include_directories(imgui_demo PRIVATE 
    ${IMGUI_DIR} 
    ${IMGUI_DIR}/backends
)

# 6. 链接库 (注意：这里的目标名必须和 add_executable 保持一致)
# vcpkg 安装的 glfw3 导出的 target 名字通常就是 glfw
target_link_libraries(imgui_demo PRIVATE glfw opengl32)

# 7. 创建命令行工具 redb_cli (无 GUI)
add_executable(redb_cli redb_cli.cpp redb_parser.cpp)
target_include_directories(redb_cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})