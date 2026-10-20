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
	glfwWindowHint(GLFW_CONTEXT_DEBUG, GLFW_TRUE);	// 間違いの報告を行う特別な机を使用する

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

	// デバッグコンテキストがオンになっているかチェック
	GLint flags = 0;
	glGetIntegerv(GL_CONTEXT_FLAGS, &flags);	// このコンテキストにおけるデバッグコンテキストがオンになってるかを出力引数に渡す
	bool debugCtx = (flags & GL_CONTEXT_FLAG_DEBUG_BIT) != 0;	// フラグ情報から、デバッグビットのビットだけを取り出し、1か0かを判断する
	std::cout << "debug context: " << (debugCtx ? "yes" : "no") << "\n";
	cout << "flags : " << flags << endl;
	// デバッグコンテキストである　かつ　GL_KHR_debugが使用できるなら
	if (debugCtx && GLAD_GL_KHR_debug) {
		// OpenGLの機能を1ついれる関数
		// 報告機能そのもの
		glEnable(GL_DEBUG_OUTPUT);
		// 報告を、原因となった命令のその場で行わせる機能
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
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

	// 操作するバッファを示す（頂点バッファ用メモリ、メモリのID）
	glBindBuffer(GL_ARRAY_BUFFER, vboID);
	// 現在バインドされているバッファにデータを送る
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertexArray), vertexArray, GL_STATIC_DRAW);
	// 今バインドされているバッファは何バイトかを出力
	GLint gpuBufferSize = 0;
	glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &gpuBufferSize);
	cout << "now bind buffer size : " << gpuBufferSize << endl;

	// 頂点シェーダーを記述（GLSL この文字列をGPUのドライバがGLSLとして解釈する）
	const char* vertexShaderCode = R"(#version 330 core
layout (location = 0) in vec3 vertexInput;

void main()
{
	gl_Position = vec4(vertexInput, 1.0);
}
)";
	// 記述できたか確認
	cout << "----------" << endl;
	cout << vertexShaderCode;
	cout << "----------" << endl;

	// 頂点シェーダー用
	// シェーダー1本分の入れ物を作成
	GLuint vertexShaderID = glCreateShader(GL_VERTEX_SHADER);
	cout << "vertex shader ID : " << vertexShaderID << endl;

	// シェーダーオブジェクトにGLSLのコードを渡す
	glShaderSource(vertexShaderID, 1, &vertexShaderCode, nullptr);

	// シェーダーをコンパイルする
	glCompileShader(vertexShaderID);

	// 正常にコンパイルできたかを尋ねる
	GLint successVertexShaderCompile = 0;
	glGetShaderiv(vertexShaderID, GL_COMPILE_STATUS, &successVertexShaderCompile);
	cout << "vertex shader compile success? : " << successVertexShaderCompile << endl;

	// 失敗した場合にログを受け取る
	if (successVertexShaderCompile == 0)
	{
		// ログを受け取るための配列を作成
		char logArray[512];
		glGetShaderInfoLog(vertexShaderID, 512, nullptr, logArray);
		cout << "vertex shader compile false..." << endl;
		cout << logArray << endl;
	}
	else
	{
		cout << "vertex shader compile success!" << endl;
	}

	// フラグメントシェーダーを記述
	const char* fragmentShaderCode = R"(#version 330 core
out vec4 fragmentColor;

void main()
{
	fragmentColor = vec4(0.0, 1.0, 1.0, 1.0);
}
)";

	// フラグメントシェーダー用
	// シェーダー1本分の入れ物を作成
	GLuint fragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);
	cout << "fragment shader ID : " << fragmentShaderID << endl;

	// シェーダーオブジェクトにGLSLのコードを渡す
	glShaderSource(fragmentShaderID, 1, &fragmentShaderCode, nullptr);

	// シェーダーをコンパイルする
	glCompileShader(fragmentShaderID);

	// 正常にコンパイルできたかを尋ねる
	GLint successFragmentShaderCompile = 0;
	glGetShaderiv(fragmentShaderID, GL_COMPILE_STATUS, &successFragmentShaderCompile);
	cout << "fragment shader compile success? : " << successFragmentShaderCompile << endl;

	// 失敗した場合にログを受け取る
	if (successFragmentShaderCompile == 0)
	{
		// ログを受け取るための配列を作成
		char logArray[512];
		glGetShaderInfoLog(fragmentShaderID, 512, nullptr, logArray);
		cout << "fragment shader compile false..." << endl;
		cout << logArray << endl;
	}
	else
	{
		cout << "fragment shader compile success!" << endl;
	}

	// シェーダープログラム（頂点シェーダーとフラグメントシェーダーをリンクしたもの）を作成する
	// シェーダープログラムのIDを作成
	GLuint shaderProgramID = glCreateProgram();
	cout << "shader program ID : " << shaderProgramID << endl;
	// シェーダープログラムに取り付ける頂点シェーダーとフラグメントシェーダーを指定
	glAttachShader(shaderProgramID, vertexShaderID);
	glAttachShader(shaderProgramID, fragmentShaderID);

	// リンクするシェーダープログラムを指定
	glLinkProgram(shaderProgramID);

	// 正常にリンクできたかを尋ねる
	GLint successShaderProgramLink = 0;
	glGetProgramiv(shaderProgramID, GL_LINK_STATUS, &successShaderProgramLink);
	cout << "shader program link success? : " << successShaderProgramLink << endl;

	// 失敗した場合にログを受け取る
	if (successShaderProgramLink == 0)
	{
		// ログを受け取るための配列を作成
		char logArray[512];
		glGetProgramInfoLog(shaderProgramID, 512, nullptr, logArray);
		cout << "shader program link false..." << endl;
		cout << logArray << endl;
	}
	else
	{
		cout << "shader program link success!" << endl;
	}

	// リンクが終了し不要になったシェーダーオブジェクトを破棄する
	glDeleteShader(vertexShaderID);
	glDeleteShader(fragmentShaderID);

	// VAO（VBOをどう読めばいいか）を作成し、バインドする
	GLuint vaoID = 0;
	glGenVertexArrays(1, &vaoID);
	cout << "vao ID : " << vaoID << endl;
	// VAOをバインドする
	glBindVertexArray(vaoID);
	// 読み方を教える
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);
	// 0番の入力を有効化する
	glEnableVertexAttribArray(0);

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

		// 使用するシェーダープログラムを指定する
		glUseProgram(shaderProgramID);

		// 三角形を描画する
		glDrawArrays(GL_TRIANGLES, 0, 3);

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