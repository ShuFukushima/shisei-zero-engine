cmake_minimum_required(VERSION 3.20)
# C も有効化しておく(M1で導入する GLAD の gl.c はC言語ソースのため、CXXだけだとビルドが通らない)
project(shisei-zero C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# GLFW をバージョン固定で取得する(vcpkgや手動インストールは使わない)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glfw)

add_executable(shisei-zero src/main.cpp)
# opengl32 は Windows 標準の OpenGL ライブラリ(gl* 関数のリンクに必要)
target_link_libraries(shisei-zero PRIVATE glfw opengl32)

if (MSVC)
    # ソースファイルはUTF-8で書く。/utf-8 を指定してコードページ依存の文字化けを防ぐ
    target_compile_options(shisei-zero PRIVATE /utf-8)
endif()
