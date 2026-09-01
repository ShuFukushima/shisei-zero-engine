# ミニエンジン化アーキテクチャ調査レポート

## 結論(サマリー)

| 論点 | 結論 | 確信度 |
|---|---|---|
| (1) Mesh/Material/Shader/Texture抽象化 | 必須。ポートフォリオ規模なら**素朴なクラス設計で十分**(状態駆動なので巨大なクラス階層は不要という意見あり) | 状況証拠 |
| (1) シーングラフ | 階層変換(親子関係)を見せたいなら軽量な実装で十分。フルスケールは不要 | 状況証拠 |
| (1) ECS | **ポートフォリオ規模ではやり過ぎ、というのが業界の標準的見解**。「クラスが手に負えなくなってから導入する」のが定石であり、シングルシーンのレンダラーには過剰投資になりやすい | 検証済み(複数の一次情報が一致) |
| (2) Dear ImGui | 導入コストは低く(1〜2日)、就活での見栄えは明確にプラス。業界標準ツールであり求人でも言及される | 検証済み |
| (3) レンダーグラフ/フレームグラフ | **新卒ポートフォリオには不要**。大規模・多機能エンジン(Frostbite等)向けの最適化技術であり、単機能の学生エンジンでは複雑さに見合うリターンがない | 検証済み(複数の一次情報が一致) |
| (4) glTF自作 vs ライブラリ | 自作は「相当な労力」と評される難易度。**tinygltf/cgltfのようなヘッダオンリーライブラリの利用は「妥当な判断」として広く許容**されている。OBJ自作パーサーで差別化ポイントは既に立証済みなので、フェーズ2でのライブラリ解禁は正当化しやすい | 検証済み+状況証拠 |
| (5) レンダラー止まり vs ゲーム1本乗るエンジン | 明確な二択ではなく**「完成度の高い技術デモで十分」という見解が優勢**。実在の現役グラフィックスプログラマーのポートフォリオ(後述)もECS/ImGui/シーングラフを前面に出さず、技術の深さで勝負している | 状況証拠 |

以下、根拠とともに詳述する。

---

## (1) クラス抽象化・シーングラフ・ECSの要否

### Mesh/Material/Shader/Texture抽象化
LearnOpenGLの定番チュートリアルでは、Mesh構造体(頂点・法線・UV・インデックス・マテリアル参照)とModelクラス(複数Meshの集合)という2階層構成が標準として教えられている。
- https://learnopengl.com/Model-Loading/Mesh
- https://learnopengl.com/Model-Loading/Model

一方で、OpenGLは本質的にステート駆動のAPIであるため、過度なクラス設計は不要という実務的な意見もある。個人開発者向け記事では「レンダリングシステムに専用クラスは通常不要。OpenGL自体がステートマシンだから」と述べられている(IndieGameDev, 2020)。
- https://indiegamedev.net/2020/01/13/game-engine-development-for-the-hobby-developer-part-1-rendering/

**解釈**: フェーズ2の規模(ミニエンジン)であれば、Mesh/Material/Shader/Textureは素直な構造体+薄いラッパークラスで十分。過度な抽象化(インターフェース分離、ファクトリパターン等)は投資対効果が低い。

### シーングラフ
ゲームエンジン一般では、シーングラフは親子変換の伝播を扱う標準的な仕組みとされる。ECSベースのエンジン(Unity DOTS、Unreal、Bevy、Godot)でも、シーングラフはECS内の「階層インデックス」として併用されるハイブリッド構成が一般的、との整理がある。
- ezEngine公式ドキュメント(World/Scenegraph): https://ezengine.net/pages/docs/runtime/world/world-overview.html
- Harold Serrano「How to Implement a Scene Graph in ECS」: https://www.haroldserrano.com/blog/how-to-implement-a-scene-graph-in-ecs-a-simple-level-based-approach

### ECSは過剰か
最も明確な根拠は『Game Programming Patterns』のComponentパターン章(Bob Nystrom)。

> "For a large codebase, this complexity may be worth it for the decoupling and code reuse it enables, but take care to ensure you aren't over-engineering a 'solution' to a non-existent problem before applying this pattern."

同書は「クラスが手に負えないほど巨大になった」「複数ドメインにまたがるコードが増えた」といった**具体的な痛みが出てから導入する**ことを推奨しており、事前の予防的導入には否定的。
- https://gameprogrammingpatterns.com/component.html

実例として、現役のグラフィックスプログラマー2名(いずれもポートフォリオサイトを公開)の作品を直接確認したところ、**どちらもポートフォリオ上でECS・シーングラフ・ImGuiを前面に出していない**:
- Niels Brunekreef(Lumion / Act-3D勤務)のOxC3フレームワーク: レイトレーシング・GPU最適化・カスタムアロケータ等の低レベル技術が中心。ECS/ImGui/シーングラフへの言及なし。 https://nielsbrunekreef.com/
- Volkan Ilbeyli(UE4グラフィックス)の"Unlit"プロジェクト: アニメーション・物理・シェーダー最適化が中心。ECSへの言及なし。 https://vilbeyli.github.io/games/

一方で、学習用エンジンとして著名なHazel Engine(The Cherno制作、後述)は**EnTTベースのECSを採用**しており、「ECS自体が悪ではなく、エディタ的なシーン編集(オブジェクトの動的追加・削除・コンポーネント付与)を見せたい場合には有効」という文脈依存の判断であることが分かる。
- https://github.com/skypjack/entt/wiki/EnTT-in-Action

**総合判断(状況証拠)**: フェーズ1のOBJパーサー+Phongライティング程度の単一シーンなら、ECSは「使ってみたい」という学習目的以外では過剰投資。ただし第2フェーズで「複数オブジェクトを動的に配置・操作するエディタ的UI」を作る計画があるなら、軽量ECS(EnTT等のライブラリ利用)導入は正当化される。自作ECSは工数対効果が低い。

---

## (2) Dear ImGuiによるデバッグUI/エディタ風UI

### 導入コスト
Dear ImGui公式は「ゲームエンジン(特にツール用途)、リアルタイム3Dアプリ、コンソールプラットフォームでOS標準UIが使えない環境」に適していると明言しており、実装の主目的は**開発者/デバッグ用ツールであってエンドユーザー向けUIではない**。
- https://github.com/ocornut/imgui
- 公式記事(プロフェッショナルなデバッグツール構築): https://imgui.org/building-professional-debugging-tools-and-in-engine-editors-with-dear-imgui/

Hacker Newsでの議論でも同様の位置づけが確認できる:「Dear ImGuiは開発/デバッグツール用であり、エンドユーザー向けUIではない」
- https://news.ycombinator.com/item?id=40839769

GLFW+OpenGLへの統合は公式バックエンド(imgui_impl_glfw.cpp / imgui_impl_opengl3.cpp)をそのまま使えるため、**数時間〜1日程度**で最初のデバッグウィンドウ(FPS表示、ライトパラメータのスライダー等)が動く規模の作業と見積もれる(公式サンプル構成から判断。所要時間そのものを明言する一次情報は見つからず、ここは推測)。

### 就活での見栄え
- ImGui公式リポジトリには「Dear ImGui Job Board」というIssueが存在し、スタジオ側がImGui関連スキルを持つ人材を募集する専用スレッドとして運用されている。これはImGuiスキルが業界で明示的に求人要件になり得ることの直接証拠。
  https://github.com/ocornut/imgui/issues/5031
- Hazel Engine(著名な学習用エンジン)は「Hazelnut」というImGuiベースのエディタをコア機能として持ち、シーン編集・保存/読込・ゲーム内プレイテストを行える設計になっている。ImGui採用は学習用エンジンにおいて一般的なパターンと言える。
  https://github.com/TheCherno/Hazel

**解釈**: 導入コストは低く、リターン(「デバッグ用ツールを自分で組み込める」という実務スキルの証明)は大きい。フェーズ2で真っ先に着手する価値のある機能。

---

## (3) レンダーグラフ/フレームグラフは新卒作品に必要か

結論から言うと**不要**、という見解が複数の一次情報で一致している。

- Riccardo Logginiの技術記事: 「レンダーグラフは大規模で機能過多なエンジンで最大の効果を発揮する。ホビー/インディープロジェクトではその潜在能力を十分に活かせない可能性が高い」
  https://logins.github.io/graphics/2021/05/31/RenderGraphs.html
- 「レンダーグラフへの反論」("A rebuttal of render graphs", Jotun Studios): Frostbiteのフレームグラフは、Battlefield 4が54種のレンダリング機能・数百のレンダーパスを持つという特殊な複雑さに対応するために生まれたものであり、**「多くのホビー開発者と同様に、特定の1本のゲームのためにエンジンを作るなら」その複雑さは必要ない**、という趣旨の主張。
  https://blog.jotunstudios.com/a-rebuttal-of-render-graphs/ (WebFetch不可のためWeb検索スニペット経由での確認。原文の直接引用はできていない点に注意)
- Liam Tylerの記事は、レンダーグラフの簡易版(タスクグラフ)についてすら「最初のタスクグラフシステムを書くなら、絶対に高度な機能から始めるべきではない」と述べている。
  https://liamtyler.github.io/posts/task_graph/

**解釈**: フェーズ1〜2で想定される機能数(Phong+シャドウマッピング程度)では、レンダーパスの依存関係は数個〜十数個程度に収まり、手動でのリソース管理・パス順序制御で十分。レンダーグラフの実装コストは、その他の学習項目(ImGui、glTF対応)に比べて投資対効果が低い。

---

## (4) アセット読み込み: glTF対応の是非

### 自作パーサーの難易度
複数の情報源が、glTFパーサーの自作は「相当な労力」を要すると評価している:
- .gltf/.glbの両フォーマット対応、JSON解析、バイナリバッファ管理、base64エンコードデータの処理など、OBJよりも構造的に複雑
  (Vulkan公式チュートリアルの記述を基にした検索エンジン要約。原文: https://docs.vulkan.org/tutorial/latest/Building_a_Simple_Engine/Loading_Models/04_loading_gltf.html )
- fastgltf公式ドキュメントは、cgltf/tinygltfを「最も広く使われている2大glTFライブラリ」と位置づけている
  https://fastgltf.readthedocs.io/latest/overview.html

### ライブラリ選定(tinygltf vs cgltf)
- **tinygltf**: C++11、ヘッダオンリー、json.hpp(nlohmann)+stb_imageに依存。統合が容易でドキュメント・実績が豊富。初学者にはこちらが推奨されやすい。
  https://github.com/syoyo/tinygltf
- **cgltf**: 単一ヘッダのC99ライブラリ。高速だが拡張機能(extensions)対応がtinygltfほど堅牢ではないとの指摘あり(CesiumGSのIssueでの議論)。
  https://github.com/CesiumGS/cesium-native/issues/31

### 自作 or ライブラリ利用の判断材料
フェーズ1で自作OBJパーサーという差別化ポイントを既に確立していることを踏まえると、「パーサーが書けることは証明済み」という前提のもとで、フェーズ2ではglTFのような複雑フォーマットにライブラリ(tinygltf推奨)を使うのは実務的にも妥当という判断がしやすい。3Dモデル読み込みライブラリの活用は業界チュートリアル(LearnOpenGL含む)でも標準的なパターンとして扱われている。
- https://learnopengl.com/Model-Loading/Assimp

**assimpについて**: assimp解禁の是非は明確な一次情報(「新卒ポートフォリオでassimp使用は許容されるか」を直接論じた記事)は発見できなかった(Reddit検索が本ツールでは機能せず、直接的なコミュニティ意見の収集は不成功)。ただし、assimpは多数のプロ向けチュートリアル(LearnOpenGL, OGLdev等)で標準的に採用されており、「モデル読み込みの実装力」自体はOBJ自作で既に示せているため、**フェーズ2でのassimp利用そのものは技術的な減点要素にはなりにくい**と考えられる(状況証拠)。ただしフェーズ1のウリが「GLM・assimp不使用」である以上、assimp解禁は差別化ストーリーの一貫性を壊すリスクがある点は注意(これは調査結果というより設計上の論点として付記)。

---

## (5) 「ゲームが1本乗るエンジン」まで行くべきか、「高機能レンダラー」で止めるべきか

明確な業界コンセンサスの一次情報は少なく、判断の材料となる複数の傍証を提示する。

### 「技術デモで十分」派の根拠
- Game Industry Career Guide: グラフィックスプログラマーのポートフォリオ例として、Shadertoy/glslsandboxでの**単体シェーダーデモ3本**(完成品ゲームではない)を紹介し、「画面写真とリンクだけで採用担当者は創造性とGPUシェーダー能力を素早く判断できる」としている。
  https://www.gameindustrycareerguide.com/video-game-graphics-programmer-portfolio/
- 同ガイドの別記事: 「量より質。機能を絞り込んで、選んだものを本当に使い倒す方がよい」
  https://www.gameindustrycareerguide.com/what-should-i-put-into-my-video-game-programming-portfolio/
- 実在の現役グラフィックスプログラマー2名(前掲Niels Brunekreef、Volkan Ilbeyli)の主要ポートフォリオ作品は、完全なゲームというより**技術的に深いレンダリング/最適化デモ**が中心。

### 「ゲームが動くところまで」派の根拠
- Hazel Engineは「教育ツールであると同時に本物のゲームエンジン」を目指しており、README上の直近目標も「エディタ内でゲームを作成できる完全な2Dワークフロー」と明記されている。つまり著名な学習用エンジンの到達点は**単なるレンダラーではなくゲームが動く水準**。
  https://github.com/TheCherno/Hazel
- 日本のゲーム業界向け情報(Geekly記事)では、グラフィックスプログラマーの職務として「自社専用エンジンの設計・開発」「アーティストの表現を支える開発パイプラインの構築」が挙げられており、パイプライン全体(=ある程度エンジンとして機能する状態)を意識した経験が評価軸になり得る。
  https://www.geekly.co.jp/column/cat-position/game-graphics-engineer/

**総合判断(状況証拠、確信度は中程度)**: 「ゲーム1本」を完成させることそのものが必須条件だという直接的な一次情報は見当たらなかった。実務では**技術の深さ・完成度**が評価軸であり、「小さくても実際に動くデモシーン(簡単な操作・簡単な目的のある1シーン)」があれば、フルゲームでなくとも十分に説得力を持たせられると考えられる。一方で、ImGuiエディタでオブジェクトを配置してプレイテストできる、といった「エンジンらしい体験」を見せられると、Hazelのような著名エンジンとの比較で見劣りしにくくなる。この論点は事実調査だけで白黒つく話ではなく、最終的にはメインエージェントでの計画判断(deep-plan)に委ねるべき領域と考える。

---

## 著名な学習用エンジンの参照: Hazel Engine

The Cherno(Yan Chernikov)が制作する教育目的の3D/2Dエンジン。特徴:
- **ECS**: EnTTライブラリベース。物理演算コンポーネント追加時にon_construct()シグナルで物理ボディを自動生成するなど、実践的なECS活用例。
- **エディタ**: "Hazelnut"というImGuiベースの独立エディタアプリでシーン編集・保存/読込・プレイテストが可能。
- **レンダラー**: Vulkan SDKベース(元々OpenGLからスタートし後にVulkanへ移行)。PBR(Cook-Torrance BRDF)、IBL、マテリアルのリアルタイム編集に対応。
- **位置づけ**: 教育コンテンツ(YouTube連動)であると同時に、実際にゲームを作れることを目標とした本格エンジン。

参照URL:
- https://github.com/TheCherno/Hazel
- https://docs.hazelengine.com/HazelReleaseNotes/Hazel-2024.1
- https://github.com/skypjack/entt/wiki/EnTT-in-Action

派生プロジェクト(Hazelベースの学生製作エンジン)も存在し、ECS(EnTT)+マルチスレッドアセットシステム+SPIR-Vシェーダーコンパイル+PBRという構成が「Hazel系アーキテクチャ」の典型例として観察できる:
- https://github.com/drsnuggles8/OloEngineBase

---

## 調査過程

1. まずECS/シーングラフ/クラス抽象化について、Hazel Engineの実装と、Game Programming Patternsの理論的根拠を当たった。
2. ImGuiについて公式サイト・GitHub・HN議論を確認し、就活文脈での言及(Job Board Issue)を発見。
3. レンダーグラフについて、専門ブログ記事を複数横断し、「大規模エンジン向け・ホビー規模では過剰」という一致した見解を確認。
4. glTF対応について、tinygltf/cgltf/fastgltfの技術文書を比較し、自作の難易度感を確認。
5. 「レンダラー止まり vs ゲームが乗るエンジン」について、実在の現役グラフィックスプログラマー2名の公開ポートフォリオを直接確認し、実例からの傍証を得た。
6. 日本語ソース(学生が自作エンジンで就活した体験記等)も探索したが、該当記事(nekocha.hatenablog.com、gothlab.hatenablog.com)は404/ログイン要求により本文を取得できず、不採用とした。
7. Reddit(r/GraphicsProgramming, r/gamedev)への直接アクセス・site:検索はいずれも失敗(WebFetchでアクセス拒否、WebSearchのsite:検索は的確な結果を返さなかった)。コミュニティの生の議論は本調査ではカバーできていない欠落点。

## 確信度と限界

- **検証済み**として扱った項目は、一次ソース(公式リポジトリ、公式ドキュメント、技術者個人のブログ本文)を直接WebFetchで確認できたもの。
- **状況証拠**とした項目は、WebSearchのAI要約のみに基づくもの、または複数の間接的傍証から合理的に推論したもの。WebSearchツールの要約は元記事のスニペットに基づくAI生成文であり、細部が不正確な可能性がある(特にJotun Studiosの記事はWebFetch失敗のためWebSearch要約のみに依拠)。
- Reddit上のコミュニティ意見(r/GraphicsProgramming, r/gamedev)は本調査ツールでは取得できず、**この調査の最大の欠落点**。もし現場エンジニアの生の温度感(特に「ECSはやり過ぎ」「assimp解禁はアリ」といった賛否)をさらに補強したい場合、ユーザー自身がRedditで直接検索するか、別セッションでブラウザツールを使った調査が必要。
- 日本国内特有の新卒採用における評価基準(特にコンシューマー機大手 vs 中小スタジオでの温度差)についての一次情報は乏しく、今回引用した日本語記事は一般的な「未経験転職向けポートフォリオ」記事が中心で、グラフィックスプログラマー職・新卒ゲーム専門学校生に特化した一次情報は少ない。この点は本調査の弱点として明記する。