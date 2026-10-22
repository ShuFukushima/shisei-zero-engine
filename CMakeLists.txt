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

# GLAD（OpenGLの関数ポインタを聞いて回る）を使用できるようにする。
# 自分のコードではなく外部の部品のため、exeのもとになるファイルに混ぜず、別のライブラリにする
add_library(glad external/glad/src/gl.c)
# GLFWと違い、pythonがPCにないと構成の際に取ってこれないため、ファイルをexternal/gladに置く方式
target_include_directories(glad PUBLIC external/glad/include)

# opengl32 は Windows 標準の OpenGL ライブラリ(gl* 関数のリンクに必要)
target_link_libraries(shisei-zero PRIVATE glad glfw opengl32)

if (MSVC)
    # ソースファイルはUTF-8で書く。/utf-8 を指定してコードページ依存の文字化けを防ぐ
    target_compile_options(shisei-zero PRIVATE /utf-8)
endif()

# stb_image.h を探す場所
target_include_directories(shisei-zero PRIVATE external/stb)

# assets フォルダを、exe のあるフォルダの隣へ複製する(ビルドのたびに行う)
add_custom_target(copy-assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:shisei-zero>/assets
)
add_dependencies(shisei-zero copy-assets)