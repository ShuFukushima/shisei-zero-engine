#include <iostream>
#include <GLFW/glfw3.h>

using namespace std;

// テスト
int main()
{
	// GLFWの初期化処理。戻り値はint。
	int init = glfwInit();
	// 初期化が失敗したら停止する
	if (init == GLFW_FALSE)
	{
		cout << "GLFW initialize false..." << endl;
		return 1;
	}

	// 初期化が成功したことを知らせる
	cout << "GLFW initialize done!" << endl;

	// ウィンドウを作成する。glfwCreateWindowはウィンドウがどのメモリにあるかを指すポインタが返り値。
	GLFWwindow* window = glfwCreateWindow(800, 600, "shisei-zero-engine", nullptr, nullptr);
	if (window == nullptr)
	{
		cout << "Window generate false..." << endl;
		// GLFWの終了処理
		glfwTerminate();
		return 1;
	}

	// ウィンドウ生成に成功したと表示
	cout << "Window generate done!" << endl;

	// ウィンドウの終了処理
	glfwDestroyWindow(window);
	cout << "Window format." << endl;

	// GLFWの終了処理。（GLFWは準備でOSから様々なものを借りるため、それを返す）
	glfwTerminate();
	cout << "GLFW format." << endl;

	return 0;
}