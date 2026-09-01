# OSSレンダラー/グラフィックスライブラリ調査レポート

## 結論

**OpenGL 3.3自作レンダラー学習者が「次に読む」対象としての適性順位(総合)**

| 順位 | ライブラリ | 一言理由 |
|---|---|---|
| 1位 | **raylib (rlgl)** | 同じOpenGL 3.3をベースにした薄いラッパーで地続き。読みやすさ最優先の設計方針が公式に明言されており、コードを読む練習として最短距離 |
| 2位 | **Google Filament** | コード本体は難度が高いが、付属理論文書 `Filament.html` はPBR理論を数式・GLSL実装付きで解説する「教科書」級の資料。理論学習の主教材として最適 |
| 3位 | **bgfx** | コアは低レベルAPI抽象化だが、`examples/` 以下に deferred・RSM(反射シャドウマップ)などPBR/GI関連技術が単体サンプルとして分離されており、技術単位でのつまみ食い学習に向く |
| 4位 | **Diligent Engine** | チュートリアルが「三角形→立方体→テクスチャ→PBR/glTF→レイトレーシング」と体系立っているが、対象がD3D12/Vulkan/Metalの抽象化でOpenGL 3.3からの飛躍が大きい。次の学習ステップ(モダンAPI移行)向け |
| 5位 | **The Forge** | 最もAAA/コンソール実績が濃い(可視性バッファ、レイトレ、VRS等)が、Cスタイルの低レベルコードでドキュメントが薄く、初中級者がいきなり読むには最難関。就活での「引用元」としては強いが「読解対象」としては最後 |

以下、各ライブラリの詳細根拠。

---

## 各ライブラリの詳細

### 1. Google Filament

| 項目 | 内容 | 出典 |
|---|---|---|
| 目的 | Android/iOS/Windows/Linux/macOS/WebGL2向けのリアルタイム物理ベースレンダリング(PBR)エンジン | https://github.com/google/filament |
| レンダリング技術 | Clustered forward renderer、Cook-Torranceベースの鏡面/拡散BRDF、IBL(イメージベースドライティング)、複数シャドウ技術(EVSM, PCSS等)、SSAO/SSR/屈折、HDRブルーム、被写界深度、複数トーンマッパー(ACES, AgX, GT7等) | https://github.com/google/filament |
| ライセンス | Apache-2.0 | https://github.com/google/filament |
| 規模/評判 | GitHub 20.4k star(2026-08時点スナップショット、変動あり)。C++。実プロダクト(Android標準3Dエンジン)採用の大規模コードベース | https://github.com/google/filament |
| 付属ドキュメント | **`Filament.html`(https://google.github.io/filament/main/filament.html)** は Notation → Material system(鏡面BRDFのD/G/F項を個別に数式+GLSL実装付きで解説、拡散BRDF、クリアコート、異方性、サブサーフェス等) → Lighting → Volumetric → AA → Imaging pipeline → Bibliography という構成。各選択の設計理由も明記されており、「実装重視のPBRリファレンス」として高く評価できる内容 | https://google.github.io/filament/main/filament.html |

**弱点(実際の学習者の声)**: ある開発者のレンダラー選定記事では、Filamentは「オブジェクト構築順序に制約がある」「ECSがスレッドセーフでない」「APIドキュメント(コード本体のリファレンス)が乏しい」「ビルドに16GB超のRAMを要する」として不採用にしている。理論文書は優れているがエンジン本体のコードリーディングは難度が高いことの傍証。
出典: https://www.ravbug.com/blog/ravengine/rendering-megapost/ (個人ブログ、一次情報ではなく状況証拠)

**推奨する使い方**: エンジン本体を通読するのではなく、`Filament.html` でPBR理論(BRDF・フレネル・エネルギー保存)を学び、必要な箇所だけ実装(GLSLシェーダ)を参照する、という使い方が最も費用対効果が高い。

---

### 2. bgfx

| 項目 | 内容 | 出典 |
|---|---|---|
| 目的 | "Bring Your Own Engine/Framework"方式のクロスプラットフォーム・グラフィックスAPI非依存レンダリングライブラリ(エンジンではなく描画バックエンド) | https://github.com/bkaradzic/bgfx |
| レンダリング技術 | D3D11/12、Metal、OpenGL 4.3+/ES3.0+、Vulkan、WebGL2、WebGPU、GNM(PS4)に対応。`examples/21-deferred`(MRTによるdeferred shading)、`examples/31-rsm`(Reflective Shadow Map=間接光GI手法)などPBR/GI関連の単体サンプルを収録 | https://github.com/bkaradzic/bgfx/blob/master/examples/21-deferred/deferred.cpp, https://github.com/bkaradzic/bgfx/blob/master/examples/31-rsm/reflectiveshadowmap.cpp, https://bkaradzic.github.io/bgfx/examples.html |
| ライセンス | BSD 2-Clause | https://github.com/bkaradzic/bgfx |
| 規模/評判 | GitHub 17.4k star。9,400+コミット。70以上の言語バインディングあり、コミュニティ実装も多数(独自にClustered Shading+PBR実装を追加している例もある) | https://github.com/bkaradzic/bgfx |
| 付属ドキュメント | 公式サイトにサンプル集(https://bkaradzic.github.io/bgfx/examples.html)、各サンプルが技術単位で完結。理論文書としての厚みはFilamentほどではない | https://bkaradzic.github.io/bgfx/examples.html |

**学習者の実体験(参考)**: 個人開発者のブログでは、bgfx採用時に最初は「グラフィックス知識不足で数ヶ月苦戦」したが理解後は「オブジェクト構築順序の制約がない」「奇妙なスレッド処理ルールがない」として最終的に他ライブラリより高評価で採用したと述べている。学習コストは初期にあるが、コード自体の設計は素直という評価。
出典: https://www.ravbug.com/blog/ravengine/rendering-megapost/ (個人ブログ、状況証拠)

---

### 3. The Forge

| 項目 | 内容 | 出典 |
|---|---|---|
| 目的 | Confetti社(ConfettiFX)開発のクロスプラットフォーム・レンダリングフレームワーク。独自ゲームエンジンの描画層として使うことを想定した、AAA/コンソール実績のある低レベルフレームワーク | https://github.com/ConfettiFX/The-Forge |
| レンダリング技術 | Triangle Visibility Buffer 2.0(GPUバンド幅を抑えるG-buffer代替手法)、ハードウェアレイトレーシング/レイクエリ、Screen-Space Reflections、Variable Rate Shading、Global Illumination、TressFX(毛髪レンダリング)等 | https://github.com/ConfettiFX/The-Forge, https://github.com/ConfettiFX/The-Forge/wiki/Triangle-Visibility-Buffer-Pre-Skinned-Animations |
| グラフィックに強い証拠 | Visibility Buffer手法はGDCE 2016・XFest 2018等の業界カンファレンスで発表された技術の実装(検索結果からの要約、一次スライド未確認のため状況証拠) | https://github.com/ConfettiFX/The-Forge/wiki/Triangle-Visibility-Buffer-Pre-Skinned-Animations |
| ライセンス | PC/macOS/iOS/Android/SteamDeck/Quest向けはApache License 2.0。コンソール(PS/Xbox/Switch)向けは別途商用ライセンスが必要 | https://github.com/ConfettiFX/The-Forge |
| 規模/評判 | GitHub 5.6k star、534+コミット、`Common_3`/`Examples_3`/`Tools`の大規模構成。Issue #99での開発者自身のコメントでは「レンダラーはほぼCスタイルの関数とポインタで構成」「ビルドシステムは粗いがシンプル」と説明されている(=低レベルでラップが薄い=読解難度は高いが実装は素直との評価とも取れる) | https://github.com/ConfettiFX/The-Forge/issues/99 |
| 付属ドキュメント | READMEとGitHub Wiki(FSL Programming Guide等)が中心。FilamentやDiligent Engineのような体系立ったチュートリアル/理論文書は確認できず | https://github.com/ConfettiFX/The-Forge/issues/99 |

---

### 4. Diligent Engine

| 項目 | 内容 | 出典 |
|---|---|---|
| 目的 | D3D11/12・Vulkan・Metal・OpenGL/GLES・WebGPUを単一APIで抽象化するモダンなクロスプラットフォーム低レベルグラフィックスライブラリ | https://github.com/DiligentGraphics/DiligentEngine |
| レンダリング技術 | Screen-Space Reflections、SSAO、被写界深度、Bloom、TAA、ハードウェアレイトレーシング(D3D12/Vulkan)、Mesh Shaders、glTF 2.0 PBRレンダラー(IBL対応) | https://github.com/DiligentGraphics/DiligentEngine, https://github.com/DiligentGraphics/DiligentSamples/tree/master/Samples/GLTFViewer |
| グラフィックに強い証拠 | Vulkanised 2023カンファレンスでの技術発表資料(一次スライドPDF)が公開されている | https://vulkan.org/user/pages/09.events/vulkanised-2023/vulkanised_2023_diligent_engine_building_a_modern_graphics_abstraction_layer.pdf |
| ライセンス | Apache 2.0 | https://github.com/DiligentGraphics/DiligentEngine |
| 規模/評判 | GitHub 4.4k star、2,174+コミット。Core/Tools/FX/Samplesの複数サブモジュール構成。ある開発者のレビューでは「ドキュメントとサンプルは優秀、CMake統合も簡単」だが「ライブラリが巨大でダウンロード・コンパイルに時間がかかる」と評されている | https://www.ravbug.com/blog/ravengine/rendering-megapost/ (状況証拠) |
| 付属ドキュメント | チュートリアルが「三角形描画→立方体(頂点/インデックス/UniformBuffer)→テクスチャ→…→glTF PBR→レイトレーシング」と初級から上級へ段階的に構成されている。教材としての完成度は高い | https://diligentgraphics.com/diligent-engine/samples/ |

---

### 5. raylib(教育向け)

| 項目 | 内容 | 出典 |
|---|---|---|
| 目的 | 「ビデオゲームプログラミングを楽しく学ぶ」ことを掲げるシンプルなCライブラリ。OpenGLを`rlgl`という薄い抽象化層で包む | https://www.raylib.com/, https://github.com/raysan5/raylib |
| レンダリング技術 | `rlgl`はOpenGL 1.1/2.1/3.3 Core/ES2/ES3を切り替え可能な抽象化層で、**デスクトップ既定はOpenGL 3.3 Core**(ユーザーの自作レンダラーと同じバージョン)。ただし高度なPBR/GI等の先進レンダリング機能は標準では持たず、あくまで「シンプルな3D描画+シェーダ差し替え」が主眼 | https://github.com/raysan5/raylib/blob/master/src/rlgl.h, https://github.com/raysan5/raylib/wiki/raylib-platforms-and-graphics |
| ライセンス | zlib/libpngライセンス(改変なし)。OSI認定の非常に緩いライセンスでクローズドソースへの静的リンクも可能 | https://github.com/raysan5/raylib |
| 規模/評判 | GitHub 34.4k star(調査対象中最多)、10,000+コミット、140以上のサンプル、70以上の言語バインディング。**「読みやすさ」は公式が明言する設計方針**: 「コードの読み書き・理解のしやすさを最優先してフォーマットを決定している」「学生/初心者向けに徹底的にコメントされている」 | https://github.com/raysan5/raylib/blob/master/CONTRIBUTING.md, https://gamefromscratch.com/raylib-a-c-game-library-perfect-for-beginners/ |
| 付属ドキュメント | 巨大なチュートリアル文書は無いが、1枚の「チートシート」+140超のサンプルで学習が完結する設計。GameFromScratchのレビューでも初心者向けとして高評価 | https://gamefromscratch.com/raylib-a-c-game-library-perfect-for-beginners/ |

**重要な留意点**: raylibは「読みやすさ」と「教育向け設計」の証拠は強いが、就活で「グラフィックに強い」ことを主張する根拠としては弱い(先進レンダリング技術のショーケースではないため)。**読解練習の教材としては最適だが、「参考にした高度な技術」の引用元としてはFilament/The Forge/Diligent Engineの方が適切**、という使い分けを推奨する。

---

## 調査過程

1. WebSearchで5対象それぞれの公式GitHubリポジトリ・公式サイトを特定
2. WebFetchで各GitHubリポジトリのREADME(ライセンス、star数、主要言語、対応API、README記載のレンダリング機能)を直接取得
3. Filamentについては `Filament.html`(理論文書)を直接フェッチし、目次構成とPBR理論の解説の厳密さを確認(初回は `google.github.io/filament/Filament.html` がリダイレクトのみ返したため、`main/filament.html` に切り替えて再取得し成功)
4. The Forgeについては公式Wiki(Visibility Buffer解説)とGitHub Issue #99(開発者自身によるアーキテクチャ説明)を確認
5. Diligent Engineについては公式サンプルページとVulkanised 2023カンファレンス資料(一次スライド)を確認
6. 各ライブラリの「実際の学習者/採用検討者の評判」を補強するため、個人ブログ(ravbug.com のレンダリングライブラリ比較記事)を確認。これは一次情報ではなく個人の経験談である点に留意
7. 見つからなかったもの: 各ライブラリの正確なコード行数(cloc等の定量データ)。GitHub上のコミット数・star数・ディレクトリ構成から規模を推測するに留めた。bgfx・Diligent Engine・The Forgeについて日本語または英語での「初心者向け読解難度」を直接論じたRedditスレッドは発見できず、個人ブログ1件の状況証拠に依拠している

---

## 確信度と限界

| 項目 | 確信度 | 備考 |
|---|---|---|
| 各ライブラリの目的・対応API・README記載機能 | **検証済み** | 公式GitHub README/公式サイトを直接取得して確認 |
| ライセンス(Apache-2.0 / BSD-2-Clause / zlib) | **検証済み** | 各リポジトリのLICENSEファイル記載内容を直接確認 |
| Filament.htmlの章構成・理論の厳密さ | **検証済み**(構成)+**状況証拠**(「教科書として使える」という評価自体は複数の検索結果・GameDev.net等の言及に基づく間接評価) | 一次文書を直接読解して章構成は確認したが、「教科書級」という評価語は調査エージェントによる要約であり第三者の明示的な星評価等ではない点に注意 |
| star数・コミット数などの規模指標 | **検証済み(取得時点)** | 2026年8月20日時点のスナップショット。GitHub star数は日々変動するため、今後参照時は再確認を推奨 |
| コード規模(行数)そのもの | **未検証/推測** | 正確なcloc等の定量データは取得できず、ディレクトリ構成・コミット数から規模感を推測したのみ |
| 「読みやすさ」「学習コスト」の評判 | **状況証拠** | raylibの読みやすさ方針は公式CONTRIBUTING.mdに明記された一次情報だが、bgfx/Filament/Diligent Engineの学習コストに関する評価は個人ブログ1件(ravbug.com)に依拠しており、サンプル数が少ない。The Forgeについては読みやすさを直接論じた情報源が見つからず、開発者自身のIssue発言から間接的に推測した |
| The Forgeの「GDC/XFest発表」の一次資料 | **状況証拠** | Wiki記載内容の要約であり、実際のカンファレンス資料(スライド/動画)は未確認。就活で引用する場合は一次スライドの追加確認を推奨 |
| 総合ランキング(次に読む適性順位) | **調査エージェントの判断** | 事実(規模・ドキュメント・技術内容)を根拠にした推論であり、唯一の正解ではない。特に「Filamentを2位に置く」判断は、コード本体ではなく理論文書の価値を重視した結果である点は明記しておく |

**覆り得る条件**: (a) star数・コミット数は流動的であり数ヶ月後には数値が変わる、(b) 個人ブログの評価は執筆者固有の経験に基づくため、別の開発者は逆の評価をする可能性がある、(c) The Forgeの読みやすさ評判は情報源が薄く、実際にコードを数十分読んでみないと最終判断はできない。