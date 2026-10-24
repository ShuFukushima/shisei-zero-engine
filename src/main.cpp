#include <iostream>
#include <filesystem>
#include <cmath>
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION	// この行がないとstb_image内のヘッダーだけ読まれる
#include "stb_image.h"

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

// OpenGLが呼ぶエラー時のコールバック関数
// APIENTRY：Windowsで「関数の呼び方の取り決め」を、呼ぶ側・呼ばれる側で合わせるための印
void APIENTRY onGlDebugMessage(GLenum source, GLenum type, GLuint id, GLenum severity,
	GLsizei length, const GLchar* message, const void* userParam)
{
	cerr << "OpenGL error : " << "source : " << source << " type : " << type << " id : " << id << " severity : " << severity << endl;
	cerr << "message : " << message << endl;
}

// エラーのコールバックの定義
void callError(int errorNum, const char* expSentence)
{
	cout << "GLFW error number : " << errorNum << " " << expSentence << endl;
}

// ウィンドウの大きさ変更時に呼ばれるコールバック関数
void changeWindowSize(GLFWwindow* window, int width, int height)
{
	// 受け取った幅と高さを出力
	cout << "window size  width : " << width << " height : " << height << endl;
	// ビューポートを設定し、描画する範囲を設定し直す
	glViewport(0, 0, width, height);
}

int main()
{
	// -----ここから始まり-----
	// 作業ディレクトリを出力する
	cout << "cwd : " << filesystem::current_path() << endl;

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

	// ウィンドウの初期状態の大きさをチェック
	int windowWidth = 0;
	int windowHeight = 0;
	glfwGetFramebufferSize(window, &windowWidth, &windowHeight);
	cout << "window size  width : " << windowWidth << " height : " << windowHeight << endl;

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
		// 報告を受け取るコールバックを登録する関数
		glDebugMessageCallback(onGlDebugMessage, nullptr);
	}

	// コールバック関数にキー入力時の処理を記述した関数を登録
	glfwSetKeyCallback(window, callKey);
	// コールバック関数にウィンドウサイズが変更されたときの処理を記述した関数を登録
	glfwSetFramebufferSizeCallback(window, changeWindowSize);

	// 頂点配列を格納する（位置が3つ、テクスチャ座標2つ）
	float vertexArray[] = {
		-0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
		0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f, 1.0f,
		-0.5f, 0.5f, 0.0f, 0.0f, 1.0f
	};
	
	unsigned int triangleVer[6] = { 0, 1, 2, 0, 2, 3 };

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
layout (location = 1) in vec2 texturePos;

out vec2 texturePosOut;

void main()
{
	gl_Position = vec4(vertexInput, 1.0);

	texturePosOut = texturePos;
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
in vec2 texturePosOut;
uniform sampler2D useTex;
out vec4 fragmentColor;

void main()
{
	fragmentColor = texture(useTex, texturePosOut);
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
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (void*)0);					// 頂点の位置
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (void*)(sizeof(float) * 3));	// 頂点の色
	// 0番と1番の入力を有効化する
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	// EBO(Element Buffer Object 番号をGPUに送る)
	// IDをもらう
	GLuint eboID = 0;
	glGenBuffers(1, &eboID);
	// EBOをGL_ELEMENT_ARRAY_BUFFERにバインド
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eboID);
	// バインドした場所に番号の配列を送る
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(triangleVer), triangleVer, GL_STATIC_DRAW);
	cout << "eboID : " << eboID << endl;


	// シェーダープログラムのuniformに値を入れる
	//GLint fragmentVarNum = glGetUniformLocation(shaderProgramID, "uChangeColor");	// GLSLにおける変数の番号を受け取るこの時、フラグメントシェーダーではなくシェーダープログラムのIDをいれる点に注意
	//cout << "uChangeColor is " << fragmentVarNum << endl;

	// 画像データを読み込む
	int imageWidth = 0;
	int imageHeight = 0;
	int imageChannel = 0;
	// 配列の読む順番を反対にする
	stbi_set_flip_vertically_on_load(1);
	// 画像データを配列化する
	unsigned char* imageArray = stbi_load("assets/imageTest.png", &imageWidth, &imageHeight, &imageChannel, 4);
	if (imageArray == nullptr)
	{
		cout << "image load failed..." << endl;
		cout << "fail reason : " << stbi_failure_reason() << endl;
		glfwTerminate();
		return 1;
	}
	else
	{
		cout << "image  width : " << imageWidth << " height : " << imageHeight << " channel : " << imageChannel << endl;
	}

	for (int i = 0; i < 3; i++)
	{
		cout << "imageArray[" << i << "] : " << (int)imageArray[i] << endl;
	}

	// 画像の配列情報をGPU側に送る
	GLuint imageTexID = 0;
	glGenTextures(1, &imageTexID);
	glBindTexture(GL_TEXTURE_2D, imageTexID);
	cout << "image texture ID : " << imageTexID << endl;
	// テクスチャの使われ方を設定する
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	// 色の並びをGPUに送る
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, imageWidth, imageHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, imageArray);
	// 縮小版の絵（ミップマップ）を作らせる
	glGenerateMipmap(GL_TEXTURE_2D);
	// stbi_loadが返したポインタを受け取り、指す色の並びを開放する
	stbi_image_free(imageArray);

	// GLFW初期化からどれぐらい経ったかの秒数を出す
	cout << "init time : " << glfwGetTime() << endl;

	// メインループ（glfwWindowShouldClose()が0ならループ）
	int frameCount = 0;	// ループ回数を数える変数
	float redValue = 0.0f;	// 赤色の強さ（0～1）

	// -----ここから毎フレーム-----

	while (!glfwWindowShouldClose(window))
	{
		// ----- poll input -----
		// たまっているイベントをOS側から取得する関数（マウス操作など）
		glfwPollEvents();

		// ----- update -----
		// ループ回数を増やす
		frameCount++;

		// 時間に応じて赤の強さを変える
		double loopTime = glfwGetTime();
		double sinValue = (sin(loopTime) * 0.5) + 0.5;
		redValue = (float)sinValue;
		// 時間に応じて行き来するfloat
		float blueValue = (float)sinValue;

		// ----- render -----
		// 塗りつぶす色を設定する
		glClearColor(redValue, 0.5f, 0.5f, 1.0f);

		// ウィンドウを塗りつぶす
		glClear(GL_COLOR_BUFFER_BIT);

		// 念のためもう一度使用するテクスチャをバインドする
		glBindTexture(GL_TEXTURE_2D, imageTexID);

		// 使用するシェーダープログラムを指定する
		glUseProgram(shaderProgramID);

		// シェーダープログラムの色を実際にC++から変更する
		//glUniform4f(fragmentVarNum, 0.0, 0.5f, blueValue, 1.0);

		// 三角形を描画する
		// glDrawArrays(GL_TRIANGLES, 0, 3);

		// 矩形を描画する
		// EBOに入っている番号を前から読み、その順で頂点を使って三角形を出す
		glDrawElements(GL_TRIANGLES, sizeof(triangleVer) / sizeof(int), GL_UNSIGNED_INT, (void*)0);

		// ----- present -----
		// 描く絵を切り替える（ダブルバッファ。OpenGLはバックバッファに描く）
		glfwSwapBuffers(window);
	}

	// ------ここから終わり------

	// ループした回数を出力
	cout << "Loop Count : " << frameCount << endl;

	// ループ終了時点で何秒か？
	double loopEndTime = glfwGetTime();
	cout << "Loop End Time : " << loopEndTime << endl;
	cout << "1 sec loop : " << frameCount / loopEndTime << endl;

	// テクスチャの終了処理
	glDeleteTextures(1, &imageTexID);
	cout << "texture deleted!" << endl;

	// EBOの終了処理
	glDeleteBuffers(1, &eboID);
	cout << "EBO deleted!" << endl;

	// VAOの終了処理
	glDeleteVertexArrays(1, &vaoID);
	cout << "VAO deleted!" << endl;

	// シェーダープログラムの終了処理
	glDeleteProgram(shaderProgramID);
	cout << "shader program deleted!" << endl;

	// VBOの終了処理
	glDeleteBuffers(1, &vboID);
	cout << "VBO deleted!" << endl;

	// ウィンドウの終了処理
	glfwDestroyWindow(window);
	cout << "Window format." << endl;

	// GLFWの終了処理。（GLFWは準備でOSから様々なものを借りるため、それを返す）
	glfwTerminate();
	cout << "GLFW format." << endl;

	return 0;
}