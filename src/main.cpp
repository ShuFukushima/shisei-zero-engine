#include <iostream>
#include <cmath>
#include <glad/gl.h>
#include <GLFW/glfw3.h>

using namespace std;

// キー入力のコールバックの定義
void callKey(GLFWwindow* window, int firstKeyNum, int num, int pressState, int secondKeyNum)
{
	cout << "key number : " << firstKeyNum << " press state : " << pressState << endl;

	// キーがEsc、かつ、押された場合
	if (firstKeyNum == GLFW_KEY_ESCAPE && pressState == GLFW_PRESS)
	{
		// ウィンドウを閉じる印を立てる
		glfwSetWindowShouldClose(window, GLFW_TRUE);
	}
}

// エラーのコールバックの定義
void callError(int errorNum, const char* expSentence)
{
	cout << "GLFW error number : " << errorNum << " " << expSentence << endl;
}

int main()
{
	// GLFWのエラーコールバック関数に呼び出す関数を登録
	glfwSetErrorCallback(callError);

	// GLFWの初期化処理。戻り値はint。
	int init = glfwInit();
	// 初期化が失敗したら停止する
	if (init == GLFW_FALSE)
	{
		cout << "GLFW initialize false..." << endl;
		return 1;
	}
	else
	{
		// 初期化が成功したことを知らせる
		cout << "GLFW initialize done!" << endl;
	}

	// 作成するウィンドウの細かい指示（OpenGLのバージョンや機能）
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// ウィンドウを作成する。glfwCreateWindowはウィンドウがどのメモリにあるかを指すポインタが返り値。
	GLFWwindow* window = glfwCreateWindow(800, 600, "shisei-zero-engine", nullptr, nullptr);
	if (window == nullptr)
	{
		cout << "Window generate false..." << endl;
		// GLFWの終了処理
		glfwTerminate();
		return 1;
	}
	else
	{
		// ウィンドウ生成に成功したと表示
		cout << "Window generate done!" << endl;
	}

	// OpenGLがどのウィンドウに対してグラフィックス描画をするかを指定する（今回なら上で作成したウィンドウ）
	glfwMakeContextCurrent(window);

	// GLADにOpenGLの関数の場所をいれる（失敗したら戻り値0）
	int openglVersion = gladLoadGL(glfwGetProcAddress);
	if (openglVersion == 0)
	{
		cout << "glad not get address..." << endl;
		glfwDestroyWindow(window);
		glfwTerminate();

		return 1;
	}
	else
	{
		cout << "glad get address! opengl version : " << GLAD_VERSION_MAJOR(openglVersion) << "." << GLAD_VERSION_MINOR(openglVersion) << endl;
	}

	// コールバック関数にキー入力時の処理を記述した関数を登録
	glfwSetKeyCallback(window, callKey);

	// 三角形の頂点を登録する
	float vertexArray[] = {
		0.0f, 0.5f, 0.0f,
		-0.5f, -0.5f, 0.0f,
		0.5f, -0.5f, 0.0f
	};

	// 配列全体のバイト数を出力
	cout << "vertex array bytes : " << sizeof(vertexArray) << endl;

	// オブジェクト（データの置き場所）のIDを確保する
	GLuint vboID = 0;
	cout << "vboID befor : " << vboID << endl;
	// バッファオブジェクト用の使用されていないIDをもらう（個数、IDのいれる場所）
	glGenBuffers(1, &vboID);
	cout << "vboID after : " << vboID << endl;

	// GLFW初期化からどれぐらい経ったかの秒数を出す
	cout << "init time : " << glfwGetTime() << endl;

	// メインループ（glfwWindowShouldClose()が0ならループ）
	int frameCount = 0;	// ループ回数を数える変数
	float redValue = 0.0f;	// 赤色の強さ（0～1）
	while (!glfwWindowShouldClose(window))
	{
		// たまっているイベントをOS側から取得する関数（マウス操作など）
		glfwPollEvents();

		// update
		// ループ回数を増やす
		frameCount++;

		// 時間に応じて赤の強さを変える
		double loopTime = glfwGetTime();
		double sinValue = (sin(loopTime) * 0.5) + 0.5;
		redValue = (float)sinValue;

		// 塗りつぶす色を設定する
		glClearColor(redValue, 0.5f, 0.5f, 1.0f);

		// ウィンドウを塗りつぶす
		glClear(GL_COLOR_BUFFER_BIT);

		// 描く絵を切り替える（ダブルバッファ。OpenGLはバックバッファに描く）
		glfwSwapBuffers(window);
	}

	// ループした回数を出力
	cout << "Loop Count : " << frameCount << endl;

	// ループ終了時点で何秒か？
	double loopEndTime = glfwGetTime();
	cout << "Loop End Time : " << loopEndTime << endl;
	cout << "1 sec loop : " << frameCount / loopEndTime << endl;

	// ウィンドウの終了処理
	glfwDestroyWindow(window);
	cout << "Window format." << endl;

	// GLFWの終了処理。（GLFWは準備でOSから様々なものを借りるため、それを返す）
	glfwTerminate();
	cout << "GLFW format." << endl;

	return 0;
}