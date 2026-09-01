# 読解はしご(Reading Ladder)設計のための調査レポート

調査日: 2026-08-20(GitHub統計はすべてこの日時点の値)

---

## 結論(サマリ)

C++初学者(2026年9月学習開始、OpenGL 3.3自作レンダラー1本経験)向けに、以下の**4段階はしご**を推奨する。

| 段階 | 対象 | 主眼 | 目安規模 |
|---|---|---|---|
| ① 極小・逐語読み | tinyrenderer, Ray Tracing in One Weekend | アルゴリズムそのもの(ラスタライズ、バリセントリック座標、Z-buffer、レイ-球交差 等)を1行ずつ追う | 数百〜1,000行 |
| ② 用例集めぐり | LearnOpenGL公式サンプル、raylib examples | 「1機能=1サンプル」のパターン集を横断的に眺め、APIの使い方の型を掴む | 章/例ごとに数百行 |
| ③ 中規模構造物 | raylib本体、sokol、Hazel Engine | モジュール分割・ヘッダ/実装分離・命名規約など「設計」を見る | 数千〜数万行 |
| ④ 大規模(将来) | Godot等 | 公式の設計解説文書と併読しながらサブシステム境界を追う。単体で全読はしない | 数十万行〜 |

**確信度が高い**のはリポジトリごとの規模・活動状況(GitHub APIで直接検証済み)。**確信度が中程度**なのは各段階の「読み方」の一般論(グラフィックス特化ではなく一般的なOSS読解論からの類推)。**確信度が低い**のは「中規模を挟むべき」という主張そのものを名指しで論じた一次資料、および初学者の挫折体験談(ゲームエンジン領域に特化した一次資料は乏しく、検索で十分な数を拾えなかった)。詳細は末尾「確信度と限界」を参照。

---

## 1. 超小規模で読み切れる教材コード

| 名前 | 実装規模 | Stars / Forks | 最終更新 | 特徴 |
|---|---|---|---|---|
| tinyrenderer (ssloy) | コア8ファイル合計約700行(main.cpp 80, geometry.h 187, our_gl.* 70, model.* 109, tgaimage.* 255) | 24,112 / 2,294 | 2026-07-29(活発) | 「500行で書くOpenGLクローン講座」。TGA出力のみ、GUIなし。著者自身のコードは提供されるが「写経せず自分で書け」と明記 |
| Ray Tracing in One Weekend (RayTracing/raytracing.github.io) | Book1(In One Weekend)のsrcは11ファイル、各1〜6KB(合計でも数百〜千行規模) | 10,592 / 1,005 | 2026-08-10(活発) | 3部作構成(In One Weekend → The Next Week → The Rest of Your Life)で**難度が段階的に上がる**。シリーズ内部自体がミニ「読解はしご」になっている |
| LearnOpenGL公式サンプル (JoeyDeVries/LearnOpenGL) | 章ごとに1ファイル、数百行単位 | 12,596 / 2,957 | **2024-08-05で停止(約2年停滞)** | 章立てが学習トピックと1対1対応。ただし現在ビルド関連のissueが多数未解決(Mac/Windows/Linuxそれぞれで報告あり) |
| raylib examples (raysan5/raylib同梱) | 140以上の独立サンプル、1例=数十〜200行程度 | (本体と同じ)34,398 / 3,245 | 2026-08-19(非常に活発) | 公式が「伝統的なAPIドキュメントではなくexampleを主参照にする設計」と明言。cheatsheetは補助のみ |

出典:
- https://github.com/ssloy/tinyrenderer
- https://github.com/ssloy/tinyrenderer/wiki/Lesson-0:-getting-started
- https://raytracing.github.io/
- https://github.com/RayTracing/raytracing.github.io
- https://github.com/JoeyDeVries/LearnOpenGL
- https://learnopengl.com/Code-repository
- https://github.com/raysan5/raylib/tree/master/examples
- https://www.raylib.com/examples.html

---

## 2. 中規模で構造が学べるもの

| 名前 | 実装規模 | Stars / Forks | 最終更新 | 特徴 |
|---|---|---|---|---|
| Hazel Engine (TheCherno/Hazel) | C++本体約29万バイト(概算1万数千〜2万行規模)。src/Hazel配下がCore/Events/ImGui/Math/Physics/Project/Renderer/Scene/Scripting/UI/Utilsの**11サブシステムに分割** | 13,081 / 1,565 | **2024-04-20で停止(約2年停滞)** | The CherноのYouTube「Game Engine series」(400本超)と1対1対応する教育用エンジン。ただし本人による自己コードレビュー動画で「static/global状態の乱用」「ライフタイム管理の甘さ」「Rendererの抽象化が細かすぎる」等の弱点を自己指摘しており、**本番品質ではなく教材品質**と明記すべき |
| raylib本体 (raysan5/raylib) | C 274万バイト。主要ファイルはrcore.c 4,683行、rmodels.c 7,359行、rshapes.c/rtextures.c等、機能別に.cファイル分割 | 34,398 / 3,245 | 2026-08-19(非常に活発) | CONTRIBUTING.mdで「学生/ユーザーが読み書き理解しやすいようフォーマットを決めている」と明記。Pascal/camelCase命名でC言語にしては読みやすい設計哲学 |
| sokol (floooh) | 公開ヘッダ自体は大きい: sokol_gfx.h **28,090行**、sokol_app.h **15,030行**(GL/D3D11/Metal/WebGPU/WebGL2の全バックエンドを1ファイルに実装しているため)。ただし公開API(呼び出し側が触る関数)は非常に少ない「ミニマル」設計 | 10,219 / 661(本体)。姉妹リポジトリsokol-samplesは803 stars | 2026-08-19(本体)/2026-08-17(samples)、両方とも非常に活発 | 「ヘッダオンリーで最小限」という評判はAPI設計についてのものであり、**実装行数そのものは中〜大規模**という点は要注意。実際にはsokol-samplesの個々のサンプル(用例)から入り、必要なバックエンドのセクションだけを読むのが現実的 |

出典:
- https://github.com/TheCherno/Hazel
- https://hazelengine.com/about/
- https://www.classcentral.com/course/youtube-let-s-make-something-in-hazel-game-engine-series-228901
- https://www.youtube.com/watch?v=1W1FtRaY69Y (Hazel - My Game Engine // Code Review)
- https://github.com/raysan5/raylib
- https://github.com/raysan5/raylib/blob/master/CONTRIBUTING.md
- https://github.com/floooh/sokol
- https://github.com/floooh/sokol-samples
- https://notes.billmill.org/programming/C/Sokol.html

---

## 3. 「大規模の前に中規模を挟むべき」という議論

ゲームグラフィックス領域に特化して「小→中→大の段階を踏め」とピンポイントで主張する記事は見つからなかった(検索で確認済み、**確信度: 低〜中の状況証拠による類推**)。ただし以下の間接証拠が、大規模コードを単体でいきなり読むことの困難さを裏付けている。

1. **Google Filamentの評**: 「サンプルは非常に理解しづらく、APIドキュメントはゼロに等しい」との評あり。出典: https://www.ravbug.com/blog/ravengine/rendering-megapost/ (元評は https://www.bytesbeneath.com/p/choosing-the-right-graphics-api を引用)
2. **Godot公式が「コードだけでは把握困難」と自認**: Godot 3のレンダラー設計を解説した公式ブログ記事の中で、著者自身が「このドキュメントは、レンダリングコードを書きたい開発者を見つけるために書いた」「コードだけでは内部構造に入り込むのが相当難しい」と明言している。**公式が別途、設計解説の一次文書を用意しなければならないほど大規模コードは単体で読みにくい**、という直接的な一次証拠。出典: https://godotengine.org/article/godot-3-renderer-design-explained/
3. **bgfxの評**: ドキュメントは「相対的に貧弱」としつつ「他と比べれば酷くはない」との評。同上ravbug.com記事より。
4. **一般的なOSS読解論**(グラフィックス特化ではない): Linus Torvaldsの「小さい取るに足らないプロジェクトから始めよ、最初から大規模になると期待するな」という言葉、およびHacker News議論・pncnmnp氏のブログが共通して「いきなり全体を掴もうとせず、フォルダ階層を絞り込み、段階的に理解を広げよ」と述べている。これはグラフィックス領域限定の主張ではないが、規模のはしごを段階的に登るべきという一般則としては裏付けになる。出典: https://news.ycombinator.com/item?id=30754269 、https://pncnmnp.github.io/blogs/oss-guide.html

---

## 4. 各段階で「何を読み取るべきか」を推奨する記事

「小規模=アルゴリズム、中規模=クラス設計、大規模=サブシステム分割」という三段階の枠組みをそのまま提示する記事はピンポイントでは見つからなかった(**確信度: 推測**、複数の一般記事からの合成)。ただし近い枠組みを述べる資料は複数ある。

- **AlgoCademyブログ / pncnmnp氏ブログ**: 「まずプロジェクトを実際に使う」「テストコードを読む(開発者の意図が明確に表れる)」「初期コミットを確認してプロジェクトの根本目標を掴む」を推奨。出典: https://algocademy.com/blog/strategies-for-learning-from-codebase-of-open-source-projects/ 、https://pncnmnp.github.io/blogs/oss-guide.html
- **deeprepo.dev / dev.to記事群**: トップダウン方式(まずアーキテクチャ図→サブシステム→個別実装)を推奨。「巨大システムでもハブとなるコンポーネントは少数なので、それさえ押さえれば全体を恐れる必要はない」という指摘。出典: https://deeprepo.dev/blog/understand-new-codebase 、https://dev.to/itric/struggling-with-large-codebases-heres-the-fast-track-to-master-them-58kn
- **Hacker News議論**: 「エントリーポイントから段階的に深掘り」「設計パターンや層構造を認識する」「ログ出力で動作を追跡する」といった具体策。出典: https://news.ycombinator.com/item?id=30754269

これらを実際のグラフィックスはしごに当てはめると、収集した実データ(tinyrendererの700行、Hazelの11サブシステム分割、Godotの4層レンダラー抽象化)と整合的に以下のように整理できる(**この対応付け自体は本調査による合成であり、既存記事の直接引用ではない**):

- 小規模(①): アルゴリズムを1行ずつ追う(tinyrendererのラスタライズ・バリセントリック座標・Z-buffer、RTIOWのレイ-球交差・拡散反射)
- 中規模(③): ヘッダ/実装の分離、モジュール境界、命名規約といった「設計」を見る(raylibのrcore.c/rmodels.c分割、Hazelのsrc/Core, Events, Renderer, Scene…という11分割)
- 大規模(④): VisualServer→Rasterizer→シェーダー抽象化、のような「層(レイヤー)とサブシステム境界」を、コードだけでなく公式解説文書(Godotの例のように)と併読しながら追う

---

## 5. 初学者の挫折パターンの証言

ゲームエンジンOSS特化の一次資料(Reddit等)は、WebSearchツールの`site:reddit.com`指定が機能せず(itch.ioの投稿ばかりヒットし、目的の文脈に合致するものは発見できなかった)、**十分な数を集められなかった**。この項目の確信度は他の項目より明確に低い。

見つかった近い証言:
- GameDev.netフォーラムの投稿で「巨大エンジンのソースが公開されているのは良いことだが、初心者が全部理解するには複雑すぎる。Doomのソースでさえ今日読むのは難しく、それよりずっと規模が小さいのに」という趣旨の発言。出典: https://gamedev.net/forums/topic/710335-how-to-practically-learn-to-make-a-game-engine/
- 一般的なOSS学習論(グラフィックス特化ではない)で繰り返し現れる「全部を理解しようとするのは不可能」「スコープを絞れ」「"分析の麻痺"を避けよ」というアドバイス(pncnmnp氏ブログ、Quora等)は、裏を返せば**「全部読もうとして挫折する」という失敗パターンが実際に頻発している**ことを示唆する間接証拠と解釈できる。出典: https://pncnmnp.github.io/blogs/oss-guide.html 、https://www.quora.com/How-does-one-begin-to-tackle-understanding-a-large-open-source-code-base

---

## 確信度と限界

| 項目 | 確信度 | 根拠・限界 |
|---|---|---|
| 各リポジトリのstar数・フォーク数・最終更新日・行数/ファイルサイズ | **検証済み** | GitHub API (`api.github.com/repos/...`)から2026-08-20時点で直接取得。生の数値であり誤読の余地は小さい |
| Filament/Godotの「読みにくさ」評、raylib/sokolの設計哲学記述 | **状況証拠(中〜高)** | 該当ブログ記事・公式ドキュメントからの直接引用。ただしFilamentの評はravbug.com経由の孫引きで、一次発言者(bytesbeneath.com)を直接確認できていない箇所がある |
| 「小=アルゴリズム/中=クラス設計/大=サブシステム分割」という三段階整理 | **推測** | この枠組みをそのまま述べた一次資料は発見できず、複数の一般的アドバイスと実データから本調査が合成した整理。妥当性は高いと考えるが、既存文献の直接引用ではない点に注意 |
| 「大規模の前に中規模を挟むべき」という主張そのもの | **状況証拠(低〜中)** | グラフィックス領域でピンポイントに論じた記事は未発見。Godot公式が解説文書を別途用意している事実、Filamentの評、一般的なOSS読解論から間接的に裏付けられるのみ |
| 初学者の挫折体験談(ゲームエンジンOSS特化) | **低い** | Reddit検索がツールの制約で機能せず、十分な一次資料を集められなかった。GameDev.netの投稿1件のみが直接該当。**この項目のみ、時間があれば別ツール(Reddit公式検索等)での追加調査を推奨** |
| Hazel Engine・LearnOpenGLコードリポジトリの現在の実用性 | **検証済み(懸念点として)** | 両方とも約2年間更新が止まっており(Hazel: 2024-04-20、LearnOpenGL: 2024-08-05)、LearnOpenGLはビルド失敗issueが複数未解決。**「読んで学ぶ」用途には支障ないが、「手元で動かして学ぶ」用途では2026年現在の環境でのビルド躓きに要注意** |

覆り得る条件: 各リポジトリの数値は今後の更新で変動する(特にstar数は増加傾向が続く可能性が高い)。Hazel/LearnOpenGLが将来的に開発再開されれば「停滞」の評価は変わる。挫折体験談の項目は追加調査(Reddit直接検索等)により証拠が補強される可能性がある。