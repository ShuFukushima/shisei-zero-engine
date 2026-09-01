# OSSエンジン読解ガイド — 何を読むか・どう読むか

作成: 2026-08-20(Web調査6本の統合。原本は docs/research-phase2/oss-*.md の6ファイル。各原本の末尾に「確信度と限界」があるので、鵜呑みにする前にそこを読むこと)
**実読の記録**: 本ガイドの方針に基づき、Godot / Wicked Engine / raylib / Filament の実コードをAIが読解した記録10本が **docs/oss-code-notes/**(索引README付き)にある。PLAN2 v0.2 の根拠資料。各マイルストーンで該当分だけ読めばよい。

対象時期: **第1.5フェーズ以降**(フェーズ1の間はこのガイドを開かなくてよい。自分のレンダラーを書くのが先)。
規律との関係: OSSのコードを読むこと自体は自由(AGENTS.mdの参照禁止は検証フォルダのu*コードの話)。ただし精神は同じ — **読む→閉じる→自分の言葉で書く**。コピペで自分のsrc/に貼らない(MITでも、就活作品の「自分で書いた」という価値が消える)。

---

## §1 結論: 読解はしご(この順で登る)

いきなり大物を読むと確実に挫折する(「全部読もうとして詰む」のが最頻の失敗パターン — 調査で複数確認)。小→中→大の順で、各段階で読み取るものを変える。

| 段階 | 対象 | 何を読み取るか | いつ |
|---|---|---|---|
| ① 極小(数百行) | **tinyrenderer**(ssloy氏、~700行のソフトウェアラスタライザ講座)/ RTiOWのコード | アルゴリズムそのもの(ラスタライズ、Z-buffer、レイと球の交差) | M10前後 |
| ② 用例集 | **raylib の examples**(140本、1本=数十〜200行) | 「1機能=1サンプル」でAPIの使い方の型 | 随時 |
| ③ 中規模(数千〜数万行) | **raylib本体(rlgl)** — 自分と同じOpenGL 3.3の薄いラッパー。読みやすさを公式が設計方針として明言 | モジュール分割・命名・ヘッダ/実装の分離=「設計」 | 第1.5後半 |
| ④ 理論文書 | **Filament.html**(Googleの公式PBR理論文書) | PBRの数式とGLSL実装の対応。**M8〜M9の副教材に最適** | M8〜M9 |
| ⑤ 大規模・グラフィック特化 | **Wicked Engine**(MIT、C++17、単独開発者でスタイル一貫、devblogが濃い) | 「複数のGI手法が1つのエンジンにどう共存するか」 | 第2フェーズ以降 |
| ⑥ 大規模・シェア1位 | **Godot**(§3参照。全読は絶対にしない — servers/rendering/ に絞る) | 商用級エンジンのサブシステム分割・抽象化の層 | 第2フェーズ以降 |

## §2 「グラフィックに強い」OSSエンジン格付け(証拠つき)

| エンジン | ライセンス | 言語 | グラフィックに強い証拠(URL) | 読解対象としての評価 |
|---|---|---|---|---|
| **Godot**(シェア1位) | MIT | C++17サブセット | SDFGI(独自のリアルタイムGI): https://godotengine.org/article/godot-40-gets-sdf-based-real-time-global-illumination/ / Forward+・VoxelGI・ボリューメトリックフォグ・TAA・FSR2: https://docs.godotengine.org/en/stable/tutorials/rendering/renderers.html | 数百万行規模。§3の絞り込みが必須 |
| **Wicked Engine** | MIT | C++17 | SurfelGI・DDGI・VXGI・RTDiffuseと**GI手法を4種以上実装**+開発者本人の技術ブログ: https://wickedengine.net/category/devblog/ | ◎ グラフィック特化の本命。依存最小・単独開発者でコードが一貫 |
| **O3DE**(旧Lumberyard) | Apache2.0/MIT | C++ | Atomレンダラーのリアルタイムレイトレース GI(Diffuse Probe Grid): https://docs.o3de.org/docs/atom-guide/features/ | △ AAA由来の巨大さ(推定786人年)。部分参照のみ |
| **Bevy** | MIT/Apache2.0 | **Rust** | 2025年にReSTIRベースのレイトレGI「Solari」を実装。開発者の実装解説ブログが極めて詳細: https://jms55.github.io/posts/2025-09-20-solari-bevy-0-17/ | コードはRustなので読まない。**解説記事だけ読む**価値が高い |
| **Stride**(旧Xenko) | MIT | **C#** | PBRマテリアル・GDC2015登壇: https://www.stride3d.net/blog/gdc-2015-scene-editor-new-rendering/ | △ 言語ギャップ |
| **Flax Engine** | **独自EULA(OSSではない)** | C++/C# | DDGI等の実装は強い: https://docs.flaxengine.com/manual/graphics/lighting/gi/realtime.html | 閲覧は可能だが「OSSを参考にした」とは言えない。注意 |

レンダラー/ライブラリ系(エンジンではないがグラフィック学習に強い):

| 名前 | ライセンス | 証拠 | 使い方 |
|---|---|---|---|
| **Google Filament** | Apache2.0 | 業界で参照されるPBR理論文書: https://google.github.io/filament/main/filament.html (D/F/G項を数式+GLSL付きで解説) | 文書を教科書として読む。コード本体は難物なので通読しない |
| **bgfx** | BSD-2 | deferred/RSM等が単体サンプルに分離: https://bkaradzic.github.io/bgfx/examples.html | 技術単位のつまみ食い |
| **The Forge** | Apache2.0(PC) | Visibility Buffer等AAA実績: https://github.com/ConfettiFX/The-Forge | 上級向け。ドキュメント薄く最後 |
| **raylib** | zlib | 読みやすさを公式方針として明言: https://github.com/raysan5/raylib/blob/master/CONTRIBUTING.md | 読解練習の教材(高度技術の引用元ではない) |

## §3 Godot の読み方(必須調査対象なので詳しく)

**強い証拠**: Forward+(クラスタード、1クラスタ最大512灯)/ SDFGI / VoxelGI / ボリューメトリックフォグ / TAA / FSR2 — すべて公式ドキュメント・公式devblogで実装確認済み(§2のURL)。SDFGIは「レイトレ不要でGTX1060でも60FPS」という独自GIで、面接ネタとして特に良い。

**公平な弱み**(これも語れると強い): 開発元自身が「AAA向けに何が足りないか」を公表しており、Nanite級のメッシュストリーミングは存在しない — https://godotengine.org/article/whats-missing-in-godot-for-aaa/ 。レンダリングリードは「業界のコンセンサスが取れた技術を採り入れる後追い戦略」と明言。

**どこを読むか**(全読は不可能・不要。数百万行ある):

```
servers/rendering/                  ← 入口。RenderingServer(API窓口)
├── rendering_device.cpp/.h         ← Vulkan/D3D12/Metal共通の抽象化層
├── renderer_rd/forward_clustered/  ← Forward+本体(★最初に読むならここ)
├── renderer_rd/environment/        ← SDFGI・VoxelGI・フォグ
├── renderer_rd/effects/            ← ポストプロセス(TAA・DOF・ブルーム)
└── renderer_rd/shaders/            ← GLSLシェーダー本体
```

**罠**: GodotはSTL禁止・例外禁止・auto原則禁止という独自C++規約(https://contributing.godotengine.org/en/latest/engine/guidelines/cpp_usage_guidelines.html)。**「読める」が「入門書の流儀と違うので真似しない」**こと。

**補助ツール**: DeepWiki(AIがリポジトリを解析してWiki化したもの)にGodotのレンダリング構造解説が生成済み — https://deepwiki.com/godotengine/godot/7.1-renderingserver 。公式の内部解説(https://docs.godotengine.org/en/latest/engine_details/architecture/internal_rendering_architecture.html)は概要止まりなので、DeepWiki+GodotCon講演動画(「The Future of Rendering in Godot」等)を併用する。

## §4 OSSを読み解くコツ(調査で英日両圏の記事が一致した王道)

**5段階の型**(これだけ覚えればよい):

1. **まずビルドして動かす** — 読む前に触る。動かない物のコードを読んでも定着しない
2. **デバッガで1フレームを追う** — ブレークポイントを張り、コールスタックを「生きたガイドツアー」として読む(Visual Studioの Call Hierarchy / Find All References が主武器)
3. **機能名で逆引き** — 「影はどこ?」→ "shadow" でgrep→縦に1本、入口から出口まで寄り道せず追う。**全部読もうとしない**(最頻の挫折パターン)
4. **git log / blame で「なぜ」を読む** — コードに書かれていない設計判断はコミット履歴にある
5. **アーキテクチャ文書を先に読む** — 「コードより先に地図」。Godotなら公式内部解説+DeepWiki、DOOMならFabien Sanglard氏のレビュー

**グラフィックス特有の武器 — RenderDoc逆引き**: F12でフレームをキャプチャ→Event Browserでドローコール一覧→「この影のパスはどのシェーダー?」をPipeline Stateで確認→コールスタック(要 Tools→Resolve Symbols)でC++の発行元へ逆引き。**M3でRenderDocを習うのは、この読解術の下準備でもある**。

**読解を学習に変える**(写経の代わりに):
- 読んだパスを**フロー図に描き起こす**(描けなければ理解していない)
- **1機能だけ自分のレンダラーに移植する**(Learn by Porting — 例: raylibのカメラ実装を読んで自分のCameraクラスを改善)
- 各モジュールについて「**数段落の説明文を書ける**」を到達目標にする(interview-qa.md がそのままこの器になる)

**お手本**: Fabien Sanglard氏の DOOM/Quake コードレビュー群(https://fabiensanglard.net/doom3/index.php)と『Game Engine Black Book』。①clocで規模を測ってから読む ②全体図を先に描く ③機能ごとに深掘り、という手順そのものが教材。

**地図としての書籍**: 『ゲームエンジンアーキテクチャ 第3版』(Jason Gregory、ボーンデジタル)は「エンジンソースを読む前の概念地図」として実際のUE4読解記事でも名指し推奨。ただし1100頁・中上級向けなので、買うなら第2フェーズ以降に辞書として。

## §5 就活での使い方

- 面接で「Godotを参考にした」と言うなら、**具体的なファイル/技術名+一次資料**まで言えると強い(例: 「GodotのSDFGIのdevblogを読み、自分はもっと単純なIBLを選んだ。理由は〜」)。§2の証拠URLはそのための弾
- 「参考にした」と「写した」の区別を自分から説明できるようにする: 読解ノート(フロー図)とdisclosure-logが証拠になる
- Flax Engineは読んでもよいが「OSSを参考にした」という文脈では名前を出さない(独自EULAでOSSではないため)

## §6 この調査の限界

- 各エンジンの「読みやすさ」評は情報源が薄い(個人ブログ・状況証拠が中心)。**実際に30分読んでみて合わなければ、はしごの1段下に戻ってよい**
- Godotのコード行数は出典間で200万〜1300万行と乖離(計測方法の差)。「数百万行・大部分C++」という粒度で理解しておく
- Reddit系コミュニティの生の声は調査ツールの制約で取得できていない
- star数・機能は2026-08-20時点のスナップショット。原本6ファイル(docs/research-phase2/oss-*.md)の「確信度と限界」を参照
