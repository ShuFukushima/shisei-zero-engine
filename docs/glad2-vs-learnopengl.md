# LearnOpenGL(GLAD1) ↔ GLAD2 対応表(M1の写経の読み替え)

主教材LearnOpenGLのGetting started章(**Creating a window / Hello Window / Hello Triangle**)は、**GLAD1**(glad.dav1d.de で生成する旧版)を使うよう明示的に指示している。一方このプロジェクトは、先行検証の結果に基づき **GLAD2**(gen.glad.sh で生成する新版。GL_KHR_debug込みの事前選択URLをPLAN.md §3に固定済み)を使う。**両者はヘッダ名・ローダー関数名・ソースファイル名が食い違う**ため、教材どおりに写経すると最初の三角形の前でコンパイルエラーになる。GLAD周りの数行だけは写経せず、この表で読み替えること(規則の正本: PLAN.md §5)。

**タイミング注意**: 教材のGLAD生成手順(glad.dav1d.de)は第1週に読む「Creating a window」章に出てくるが、**そこでは生成しない**。GLADの生成は第2週の冒頭に、PLAN.md §3の事前選択URL(gen.glad.sh)で行う(docs/week-plan.md 第1〜2週)。

## 読み替え表(この3つだけ)

| LearnOpenGL(GLAD1側の写経コード) | このプロジェクト(GLAD2) | どの章に出るか・補足 |
|---|---|---|
| `#include <glad/glad.h>` | `#include <glad/gl.h>` | Creating a window / Hello Window。GLADヘッダをGLFWのincludeより先に書くのは両者同じ |
| `if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))` | `if (!gladLoadGL(glfwGetProcAddress))` | **Hello Window**(コンテキスト作成直後)。GLAD2の戻り値は「ロードできたGLバージョン」で、0なら失敗。※GLAD1にも**引数なし**の同名`gladLoadGL()`があるため、関数名だけでは版を判定できない |
| `glad.c` をプロジェクトに追加 | `gl.c`(external/glad 配下)を add_library | Creating a window。**このCMake追記はまだ本体に入っていない**(現状のCMakeLists.txtはC言語の有効化だけ済ませてある)。GLAD導入時に、決定#13の作業台規定どおりAIに出させ、自分の設計意図ブロックを書いたうえで `add_library`+`target_include_directories`+`target_link_libraries` を足す |

## この症状が出たらこの表に戻る

| エラーの症状 | 原因と対処 |
|---|---|
| `'glad/glad.h': No such file or directory` | 教材のinclude行をそのまま写経している → 1行目の読み替え |
| `'glad/gl.h': No such file or directory`(**gl.hの方**) | **読み替えは正しい**。CMakeに external/glad のinclude指定/add_libraryがまだ無いのが原因 → 3行目。慌てて`glad/glad.h`へ戻さないこと |
| `'gladLoadGLLoader': 識別子が見つかりません`(または`GLADloadproc`が未定義) | ヘッダだけ直してローダー呼び出しが教材のまま → 2行目の読み替え(**関数名も引数の形も違う**ので、名前の置換だけでは直らない) |
| `gladLoadGL`: 関数に渡す引数が多すぎます(C2660) | 手元の**生成物**がGLAD1になっている(GLAD1の`gladLoadGL`は引数なし)。コードではなく生成物の版を疑い、PLAN §3のURLでGLAD2を生成し直す |
| `gladLoadGL` 等が未解決の外部シンボル(リンクエラー) | `gl.c`がビルド対象に入っていない → 3行目のadd_libraryを確認 |

## してはいけないこと

- **教材に合わせて glad.dav1d.de(GLAD1)で再生成しない**。PLAN.md §3の事前選択URLはGL_KHR_debug込みのGLAD2構成で固定してある。GLAD1へ入れ替えるとヘッダ名・ソース名が本計画の前提とずれてビルドが即座に落ち、教材どおり拡張なしで生成し直すと`glDebugMessageCallback`自体が未宣言になってデバッグ配管ごと使えなくなる。エラー自体は派手に出るが、**原因が「生成物の版」なので教材を読み返しても答えが載っていない**(コンパイルは通るのに静かに壊れるGLMの罠〈docs/glm-vs-hikari.md〉とは別種)
- **先行検証コード(CODE-MAP経由の最終手段)もGLAD1**(`glad/glad.h`+`gladLoadGLLoader`)で書かれている。あれを見てもGLAD1へ戻さない — **この計画で参照するもの(LearnOpenGL教材と先行検証コード)はどちらもGLAD1**なので、外から持ってきたGLAD関連の行は原則読み替えが要ると思ってよい(Web上には当然GLAD2の記事もある — GLAD2の情報は公式〈gen.glad.sh とGLADのGitHub〉だけを見る。本計画がGLAD2なのは、導入方式を実機検証で選び直した結果 — 経緯はPLAN.md §3)
- この読み替えは「教材が間違っている」のではない(教材はGLAD1前提として正しい)。教材と手元の**生成物の版が違う**だけ。どちらが正しいか迷ったら、手元(GLAD2)に教材を合わせて読み替える側が常に正解
