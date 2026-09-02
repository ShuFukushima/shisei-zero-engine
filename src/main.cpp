#include <GLFW/glfw3.h>

#include <cstdio>

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)scancode;
    (void)mods;
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

int main() {
    if (!glfwInit()) {
        std::fprintf(stderr, "GLFWの初期化に失敗しました\n");
        return 1;
    }

    // OpenGL 3.3 コアプロファイルのコンテキストを明示的に要求する
    // (指定しないとドライバ任せの互換プロファイルになり、環境によって挙動が変わる)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "試製零号発動機", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "ウィンドウの作成に失敗しました\n");
        glfwTerminate();
        return 1;
    }

    glfwSetKeyCallback(window, key_callback);
    glfwMakeContextCurrent(window);

    while (!glfwWindowShouldClose(window)) {
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
