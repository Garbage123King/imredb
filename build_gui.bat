@echo off
cd /d d:\git\imredb

REM Clean old files
del /q *.o 2>nul
del /q main.exe 2>nul
del /q imgui*.o 2>nul

set IMGUI_DIR=C:\git\imredb\imgui-1.92.5

REM Compile imgui core
g++ -std=c++17 -O2 -I. -I%IMGUI_DIR% -c %IMGUI_DIR%\imgui.cpp -o imgui.o
if errorlevel 1 goto error
g++ -std=c++17 -O2 -I. -I%IMGUI_DIR% -c %IMGUI_DIR%\imgui_demo.cpp -o imgui_demo.o
if errorlevel 1 goto error
g++ -std=c++17 -O2 -I. -I%IMGUI_DIR% -c %IMGUI_DIR%\imgui_draw.cpp -o imgui_draw.o
if errorlevel 1 goto error
g++ -std=c++17 -O2 -I. -I%IMGUI_DIR% -c %IMGUI_DIR%\imgui_tables.cpp -o imgui_tables.o
if errorlevel 1 goto error
g++ -std=c++17 -O2 -I. -I%IMGUI_DIR% -c %IMGUI_DIR%\imgui_widgets.cpp -o imgui_widgets.o
if errorlevel 1 goto error

REM Compile imgui backends
g++ -std=c++17 -O2 -I. -I%IMGUI_DIR% -I%IMGUI_DIR%\backends -c %IMGUI_DIR%\backends\imgui_impl_glfw.cpp -o imgui_impl_glfw.o
if errorlevel 1 goto error
g++ -std=c++17 -O2 -I. -I%IMGUI_DIR% -I%IMGUI_DIR%\backends -c %IMGUI_DIR%\backends\imgui_impl_opengl3.cpp -o imgui_impl_opengl3.o
if errorlevel 1 goto error

REM Compile redb_parser
g++ -std=c++17 -O2 -I. -I%IMGUI_DIR% -c redb_parser.cpp -o redb_parser.o
if errorlevel 1 goto error

REM Compile redb_analyzer
g++ -std=c++17 -O2 -I. -I%IMGUI_DIR% -c redb_analyzer.cpp -o redb_analyzer.o
if errorlevel 1 goto error

REM Compile main
g++ -std=c++17 -O2 -I. -I%IMGUI_DIR% -c main.cpp -o main.o
if errorlevel 1 goto error

REM Link everything
g++ main.o redb_parser.o redb_analyzer.o imgui.o imgui_demo.o imgui_draw.o imgui_tables.o imgui_widgets.o imgui_impl_glfw.o imgui_impl_opengl3.o -lglfw3 -lgdi32 -lopengl32 -lcomdlg32 -lole32 -lcomctl32 -luuid -o main.exe

if errorlevel 1 goto error

echo Build successful!
goto end

:error
echo Build failed!
echo.
pause

:end
