# Godot Engine グラフィックス能力・ソースコード可読性 調査報告

## 結論(サマリ)

| 項目 | 結論 |
|---|---|
| グラフィックス実装 | Forward+(Vulkan/D3D12/Metal, クラスタード)、SDFGI、VoxelGI、ボリューメトリックフォグ、TAA、FSR2は**すべて公式ドキュメント・公式devblogで実装が確認済み**(検証済み) |
| 「グラフィックに強い」の妥当性 | 中〜高スペック帯までは強いが、Nanite/Lumen/World Partition相当の技術は**存在しない**ことを開発元自身が公式ブログで認めている(検証済み) |
| ソース構造 | レンダラー本体は `servers/rendering/`(RenderingServer層)と `servers/rendering/renderer_rd/`(Vulkan/D3D12/Metal実装, Forward+含む)、GPUドライバ抽象化は `drivers/vulkan/` `drivers/d3d12/` `drivers/metal/`(検証済み・GitHub API実測) |
| コード規模 | 出典により差が大きく一元的な答えはない(200万行〜1300万行、後述)。C++が全言語中バイト数で約87%を占める(検証済み・GitHub API実測) |
| ソースを読むガイド | 公式に "Engine development" セクション(contributing.godotengine.org)と「Internal rendering architecture」ページが存在するが**概要止まり**。日本語では個人技術記事(Qiita)がメインループ解説を提供 |
| ライセンス | MIT(Expatライセンス)。公式に明記(検証済み) |

---

## (1) Godot 4系レンダラーの技術的中身 ― グラフィックに強い証拠

### レンダラー種別と機能マトリクス

出典: [Overview of renderers — 公式ドキュメント](https://docs.godotengine.org/en/stable/tutorials/rendering/renderers.html)

| 項目 | Forward+ | Mobile | Compatibility |
|---|---|---|---|
| グラフィックスAPI | Vulkan / Direct3D 12 / Metal | Vulkan / Direct3D 12 / Metal | OpenGL |
| 対象 | デスクトップ専用・最新ハード向け | モバイル+デスクトップ | モバイル・低性能デスクトップ・Web(WebGL2) |
| ライティング方式 | クラスタードフォワード(1クラスタあたりOmni/SpotLight最大512) | フォワード単一パス(メッシュあたり最大8灯) | フォワード単一パス |
| VoxelGI / SDFGI | ○ | × | × |
| ボリューメトリックフォグ | ○ | × | × |
| TAA | ○ | × | × |
| FSR2 | ○ | × | × |
| Sub-surface Scattering | ○ | × | × |

### 個別技術の一次資料

- **RenderingDevice抽象化(Vulkan)**: 「RenderingDevice is a rendering backend, an abstraction layer between the renderer and the rendering driver … gives you direct access to GPU buffers, pipelines, and barriers」— [Internal rendering architecture(公式)](https://docs.godotengine.org/en/latest/engine_details/architecture/internal_rendering_architecture.html)
- **開発の歴史**: Vulkanレンダラーへの全面書き換えはJuan Linietskyが2019年に開始し、Godot 4として2023年3月にリリース。第一報 — [Vulkan progress report #1(godotengine.org)](https://godotengine.org/article/vulkan-progress-report-1/)
- **SDFGI**: 「This technique makes heavy use of Signed Distance Fields」、レイトレーシング不要でGTX1060クラスでも60FPS安定動作を確認、と2020年6月の公式devblogで発表 — [Godot 4.0 gets SDF based real-time global illumination(godotengine.org, 2020-06-28)](https://godotengine.org/article/godot-40-gets-sdf-based-real-time-global-illumination/)
- **VoxelGI**: 公式クラスリファレンス — [VoxelGI — 公式ドキュメント](https://docs.godotengine.org/en/stable/classes/class_voxelgi.html)。SDFGIより高速だが要事前ベイク、という使い分けが公式に明記。
- **ボリューメトリックフォグ**: 「Volumetric Fog uses a 3-dimensional buffer to calculate and store fog density values allowing fog to interact with light and shadows」— [Volumetric fog and fog volumes(公式)](https://docs.godotengine.org/en/stable/tutorials/3d/volumetric_fog.html)、導入記事 [Fog Volumes arrive in Godot 4.0(godotengine.org)](https://godotengine.org/article/fog-volumes-arrive-in-godot-4/)
- **FSR2**: Godot 4.2で追加。「FidelityFX Super Resolution 2.2 (FSR 2.2), which is the slowest option but provides even higher quality」— 実装PR [Add FidelityFX Super Resolution 2.2 support #81197(GitHub)](https://github.com/godotengine/godot/pull/81197)、解説 [Resolution scaling(公式ドキュメント)](https://github.com/godotengine/godot-docs/blob/master/tutorials/3d/resolution_scaling.rst)
- **開発チーム自身による技術講演**: Godotのレンダリングチームリード Clay John が GodotCon で毎年レンダリング技術アップデートを発表している(SIGGRAPH/GDC/CEDECではなく自主カンファレンス GodotCon での発表が該当)。
  - [The Future of Rendering in Godot — GodotCon 2023(media.ccc.de)](https://media.ccc.de/v/godotcon2023-57791-the-future-of-rendering-in-godot)
  - [Godot Rendering Update — GodotCon 2024(YouTube)](https://www.youtube.com/watch?v=6ak1pmQXJbg)
  - [Integrating Vulkan Ray Tracing in Godot — GodotCon 2026(YouTube)](https://www.youtube.com/watch?v=RM4xglHHYtY)(2026年にレイトレーシング統合の議論が始まっている段階、という位置づけの参考情報)

**注**: GDC/SIGGRAPH/CEDECでの外部発表は今回の調査では確認できなかった。Godotの技術発表の主戦場は自主カンファレンス「GodotCon」と公式devblog。

---

## (2) 「Godotのグラフィックスは弱い」という批判(公平な収集)

Godot開発元自身が2023年1月に公開した記事が最も信頼できる一次資料。

**公式記事**: [Godot for AA/AAA game development - What's missing?(godotengine.org, 2023-01, Juan Linietsky著)](https://godotengine.org/article/whats-missing-in-godot-for-aaa/)

- 実装済みとして: Vulkan/D3D12による最新レンダリング、ライトマッピング、VoxelGI/SDFGI(オープンワールド対応)、被写界深度、ボリューメトリックフォグ、AMD FSR、GPUコンピュート(完全サポート済みと明記)
- **不足として明記**: メッシュ**ストリーミング技術**全般。記事内で「Naniteのような技術」に相当するものが無いと名指しで言及。低レベルレンダリングへのカスタムコード挿入(GDExtension経由)の必要性も指摘。

**開発チームの立ち位置**(Clay John, Godotレンダリングチームリードのインタビュー): 「Godotは意図的に "曲線より後ろ" にいる戦略を採用。業界がコンセンサスを得た技術を、シンプルで使いやすい形で取り入れることに注力しており、最先端ではなく安定性と実用性を追求」「ハードウェアレイトレーシングはまだ先」— [Designing GPUs for Developers: A Conversation with Godot(Imagination Technologies Blog)](https://blog.imaginationtech.com/designing-gpus-for-developers-a-conversation-with-godot)(元記事はsemiengineering.comだが403で直接取得不可、ミラー先で確認)

**第三者比較記事**(参考程度、業者ブログのため中立性は限定的):
- 「Nanite, Lumen, and World Partition are tools that simply do not exist in Godot」— [Godot vs Unreal Engine Comparison(kevurugames.com)](https://kevurugames.com/blog/godot-vs-unreal-engine-which-is-better-for-game-development/)
- 「大規模環境での詳細な幾何学処理、フォトリアリスティック環境、密集した植生のスケール処理、高度なサブサーフェススキャッタリングが弱い。実現できる品質は "PS4時代相当"」— [Godot vs Unreal Engine in 2026(StraySpark Studio)](https://www.strayspark.studio/blog/godot-vs-unreal-engine-2026-comparison)

**実装未成熟の具体例**(GitHub issue、技術的欠点の実例として):
- VoxelGI/SDFGIがTAA有効時にちらつく既知の不具合。"confirmed"ラベル付与済み — [Vulkan: VoxelGI and SDFGI flicker when TAA is enabled #62080](https://github.com/godotengine/godot/issues/62080)

---

## (3) ソースコードの構造

### レンダラーのディレクトリ位置(GitHub API実測・検証済み)

```
servers/rendering/                    ← RenderingServer層(API窓口・全レンダラー共通)
├── rendering_server.cpp/.h           ← RenderingServer本体
├── rendering_server_default.cpp      ← デフォルト実装
├── rendering_device.cpp/.h           ← RenderingDevice抽象化層(Vulkan/D3D12/Metal共通API)
├── shader_language.cpp / shader_compiler.cpp ← 独自シェーダー言語処理系
├── renderer_scene_render.cpp/.h      ← シーン描画の抽象インターフェース
├── renderer_rd/                      ← ★Forward+/Mobile本体の実装(RD=RenderingDevice)
│   ├── forward_clustered/            ← Forward+(クラスタード)本体
│   ├── forward_mobile/               ← Mobileレンダラー本体
│   ├── environment/                  ← SDFGI, VoxelGI, フォグ等の環境エフェクト
│   ├── effects/                      ← ポストプロセス(TAA, DOF, ブルーム等)
│   └── shaders/                      ← GLSLシェーダー本体
└── storage/                          ← GPUリソース管理

drivers/
├── vulkan/                           ← Vulkan APIバインディング
├── d3d12/                            ← Direct3D 12バインディング
├── metal/                            ← Metalバインディング(Apple)
└── gles3/                            ← OpenGL ES 3(Compatibilityレンダラー)
```
出典: [servers/rendering(GitHub)](https://github.com/godotengine/godot/tree/master/servers/rendering)、GitHub API実測(`api.github.com/repos/godotengine/godot/contents/...`)

### コード規模

GitHub API実測(2026-08-20時点、検証済み):
- リポジトリ全体サイズ: 約1,879,274 KB(≒1.8GB、git履歴込み)
- スター 115,881 / フォーク 26,401
- 言語バイト数(上位): **C++ 62,578,614バイト(全言語中約87%)**、C# 2,279,445、Java 1,653,068、C 1,460,832、GLSL 1,309,915、Objective-C++ 836,463、Python 704,237、Kotlin 447,111、GDScript 324,958

行数については**出典間で乖離が大きく一元的な答えはない**(状況証拠レベル):
- Godot公式proposals issueでのコメント(clocツール使用、時期不明・おそらく2022年頃): 「about 2 million lines of pure code (one million without the code in thirdparty/)」— [Rust提案 issue #1330(GitHub)](https://github.com/godotengine/godot-proposals/issues/1330)
- 個人ブログ(2025年7月、`find`+`wc -l`使用、コメント・空行・翻訳ファイル込みの粗い集計): 総計13.13M行、うちC/C++ 8.6M行(65%)、翻訳ファイル3.3M行(25%) — [Analyzing the Godot Engine Codebase(russell.ballestrini.net)](https://russell.ballestrini.net/analyzing-godot-engine-codebase-millions-lines-of-code/)

計測ツール・thirdparty包含有無・計測時期がすべて異なるため単純比較不可。**「概ね数百万行規模、大部分がC++」という粒度でしか確信を持って言えない。**

### C++規格・独自規約

出典: [C++ rules and guidelines(公式contributing docs)](https://contributing.godotengine.org/en/latest/engine/guidelines/cpp_usage_guidelines.html)

- Godot 4.0以降、**C++17のサブセット**を採用
- **STL禁止**(`std::string`/`std::vector`等は不可、Godot独自データ型を使用)
- `auto`は原則禁止(型推論よりも明示記述を優先。コードレビューツールの型検査制約が理由)
- ラムダ式は避けられない場合を除き非推奨
- **例外処理(try-catch)禁止**、マクロベースのエラー処理を使用
- 命名規則: 型・名前空間は`PascalCase`、マクロ/定数は`UPPER_SNAKE_CASE`、それ以外は`snake_case`
- インデントはタブ

初学者への含意: STL不使用・例外禁止という点はC++入門書の標準的な書き方とかなり異なるため、**「読める」ことと「素直に真似できる」ことは別**という点に留意が必要。

---

## (4) ソースコードを読むためのガイド

| ガイド | 種別 | 内容 | URL |
|---|---|---|---|
| Introduction to engine contributions | 公式 | 貢献の入口。「最初からレンダラーを書き直そうとするな、good first issueから」という初心者向け助言あり | https://contributing.godotengine.org/en/latest/engine/introduction.html |
| Internal rendering architecture | 公式 | レンダラー種別(Forward+/Mobile/Compatibility)とAPI層の**概要のみ**。層構造・スレッディングモデルの詳細説明はなく「high-level overview」と明言、深掘りには結局ソース直読が必要 | https://docs.godotengine.org/en/latest/engine_details/architecture/internal_rendering_architecture.html |
| Best practices for engine contributors | 公式 | 「コアAPIの可読性がまず学習の出発点」という方針 | https://contributing.godotengine.org/en/latest/engine/guidelines/best_practices.html |
| Godotのソースコードを理解せよ!メインループ編 | 日本語(個人・Qiita) | `Main::iteration()`の解説からスタートするシリーズ。「Godotは実は読みやすく改造しやすい」という著者評価あり | https://qiita.com/chocola-mint/items/1e1dd12f58cbefd5efaf |
| Godot4メモ | 日本語(個人ブログ) | 4系の変更点を断片的にメモ。レンダリング構造そのものの解説ではない | https://tech.framesynthesis.co.jp/godot/ |

**評価**: 公式ガイドは「貢献プロセス」と「概要」を示すのみで、レンダリング内部構造を体系的に解説する一次ドキュメントは薄い。実質的には**ソースコード自体(コメント込み)とGodotCon講演動画が最も詳しい一次資料**になっている。日本語での体系的なレンダリング内部解説記事は今回の調査では発見できなかった(Qiitaのシリーズはメインループ止まりで、まだレンダラー本体には到達していない可能性がある)。

---

## (5) ライセンス

**MIT(Expat)ライセンスで確定(検証済み)**。

> "Godot is completely free and open source under the very permissive MIT license."

出典: [License – Godot Engine(公式)](https://godotengine.org/license/)、[GitHub リポジトリのライセンス表記](https://github.com/godotengine/godot/blob/master/LICENSE.txt)

商用利用・改変・再配布いずれも自由(著作権表示の保持のみ条件)。Godotで作ったゲーム自体のライセンスはGodotのライセンスと無関係で、開発者が自由に選べる。ソースコードを参考・引用する上での法的障壁は事実上ない。

---

## 調査過程

1. Godot公式レンダラードキュメント(renderers.html)で機能マトリクスを確認 → Forward+専用機能を特定
2. SDFGI/VoxelGI/FSR2について公式devblog・PR・クラスリファレンスを個別に裏取り
3. 批判については、まず第三者比較記事で仮説を作り、次に**Godot公式自身の「What's missing」記事**という一次資料に到達できたため、そちらを主軸に据え直した(第三者記事は補強材料に格下げ)
4. ソース構造はGitHub Web検索だけでは不十分だったため、**GitHub APIを直接叩いて実データ(ディレクトリ一覧・言語バイト数・repoメタデータ)を取得**し、Web上の要約記事の数字(古い/不正確な可能性がある36%/54.9%等)より実測値を優先した
5. コード行数は情報源によって数字が大きく割れたため、両方を出典付きで併記し「単一の正解はない」と明記する方針に転換
6. ソースを読むガイドは公式ページの内容が薄いことが判明したため、正直にその限界を報告に含めた
7. 探したが見つからなかったもの: GDC/SIGGRAPH/CEDECでの外部発表(Godotの発表の場は主にGodotCon)、日本語での体系的なレンダリング内部構造解説記事、行数の権威的な単一の数字

---

## 確信度と限界

| 結論 | 確信度 | 覆り得る条件 |
|---|---|---|
| Forward+/SDFGI/VoxelGI/ボリューメトリックフォグ/TAA/FSR2の実装 | **検証済み** | 公式ドキュメント・公式PRベース。将来のバージョンで仕様変更の可能性はあるが現時点の記述として確度は高い |
| レンダラーのディレクトリ位置 | **検証済み** | GitHub API実測。ただしmasterブランチの現時点のスナップショットであり、将来のリファクタで変わりうる |
| MITライセンス | **検証済み** | 公式ライセンスページ・LICENSE.txt直接確認 |
| C++が全体の約87%(バイト数ベース) | **検証済み**(ただし指標はバイト数であり行数ではない点に注意) | GitHub API実測時点(2026-08-20)の値。thirdpartyライブラリのコードも合算されている可能性が高く、「Godot独自コード」の比率とは異なる |
| コード行数(200万〜1300万行) | **状況証拠**(出典間で大きく乖離) | 計測ツール(cloc vs wc -l)、計測時期、thirdparty包含有無で数倍変動。専門学校生向けの参考情報としては「大規模・数百万行規模のC++コードベース」という粒度に留めるのが安全 |
| 「Naniteのような大規模ストリーミング技術が無い」という弱点 | **検証済み**(開発元自身の公式記事) | 2023年1月時点の記述。以降のバージョンで部分的に改善されている可能性があり、最新のGodot 5系ロードマップは今回未調査 |
| GDC/SIGGRAPH/CEDEC発表の有無 | **推測に近い(未発見=不在の証明にはならない)** | 検索で見つからなかっただけであり、Godot Foundation関係者が外部カンファレンスで発表した実績が存在しない、と断定はできない。追加調査の余地あり |
| 日本語のレンダリング内部構造解説記事が薄い | **状況証拠** | 検索言語・キーワードの限界による可能性があり、専門コミュニティ(Discord、個人ブログの奥深く)には未発見の資料が存在しうる |

**総括**: 「Godot 4はグラフィックに強い」という主張の裏付けとしては、Forward+/SDFGI/VoxelGI/FSR2等の実装は公式一次資料で強く裏付けられる。一方で「AAA/フォトリアル領域では明確に非対応」という限界も開発元自身が認めており、この両面を併記すれば就活作品での説明として誠実かつ根拠のある主張になる。ソースコードとしての可読性は、公式ガイドが薄い分、実質的にはコード本体とGodotCon講演動画を直接読む/観る必要がある点は留意事項として伝えるべき。