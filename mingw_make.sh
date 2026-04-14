g++ main.cpp \
    ./imgui-1.92.5/imgui.cpp \
    ./imgui-1.92.5/imgui_draw.cpp \
    ./imgui-1.92.5/imgui_tables.cpp \
    ./imgui-1.92.5/imgui_widgets.cpp \
    ./imgui-1.92.5/backends/imgui_impl_glfw.cpp \
    ./imgui-1.92.5/backends/imgui_impl_opengl3.cpp \
    -I./imgui-1.92.5 \
    -I./imgui-1.92.5/backends \
    -lglfw3 -lopengl32 -lgdi32 -luser32 -lshell32 -lcomdlg32 \
    -o page_viewer.exe