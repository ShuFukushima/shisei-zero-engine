# レイトレーシング導入 選択肢比較調査レポート

## 結論(推奨)

初学者(C++歴半年・週5〜10h)には **(1) CPUパストレーサー(Ray Tracing in One Weekend 第1巻、余力があれば第2巻まで)を第一候補として推奨**する。理由は以下の3点に集約される。

1. 前提知識がベクトル代数と基礎C++のみで、フェーズ1で構築済みの自作Vec3/Mat4がそのまま土台になる。GPU要件がゼロなので、ユーザーの実機がRTX搭載か未確認という不確実性リスクを回避できる。
2. 現実的な所要時間が週5〜10hペースで2週間〜2ヶ月程度(下記の体験記調査による)に収まり、フェーズ1.5の規模に合う。DXR/Vulkan RTは前提API(D3D12/Vulkan本体)の習得だけで数ヶ月級となり、フェーズ2を丸ごと飲み込むリスクがある。
3. 「GLM/assimp不使用、数学とパーサーを自作して理解して語る」という既存プロジェクトの差別化戦略とそのまま一直線に繋がる。

次点として、時間が余れば **(2) コンピュートシェーダーによるGPUレイトレ** をフェーズ2で追加することを提案する。既存のOpenGL/GLFW環境をコンテキストバージョン変更だけで拡張でき、ハードウェア要件も緩い。RTIOWで学んだアルゴリズムをGPU並列化する自然な発展課題になる。

DXR・Vulkan RTは「本物の業界標準」という見栄えの魅力は大きいが、**現時点では非推奨**(理由は各論参照)。ただし2025年10月に体系的な日本語新刊(『Vulkan実践入門』)が出たため、フェーズ2で半年以上の余裕が確保できるなら挑戦する価値はある。

---

## 比較表

| | (1) CPUパストレーサー | (2) コンピュートシェーダーGPU RT | (3) DXR | (4) Vulkan RT |
|---|---|---|---|---|
| 前提知識 | ベクトル代数+基礎C++のみ | OpenGL基礎+GLSL+並列処理の考え方 | **D3D12の実務経験が事実上必須** | **Vulkanの実務経験が事実上必須** |
| GPU要件 | 不要(CPUのみ) | OpenGL 4.3対応GPU(ほぼ全機種) | GTX10番台以降(Tier1.0=エミュレーション)、RTX20番台以降でハード加速 | Pascal世代以降で対応例あるが、ハード加速はRTX20番台等同世代 |
| 初学者の現実的所要時間 | 第1巻:2週間〜2ヶ月(週5-10h) | 第1巻習得後+数週間〜1ヶ月 | 前提のD3D12習得だけで数ヶ月、DXR本体はさらに追加 | 前提のVulkan習得だけで数ヶ月〜半年、RT拡張はさらに追加 |
| 就活での見栄え | 数学・アルゴリズムを自分で理解して語れる(差別化路線と一致) | CPU/GPU実装の違いを語れる、実装は独自色 | 業界標準API、深く語れれば強いが未完走リスク大 | 業界標準+クロスプラットフォーム、最も強いが最難 |
| 日本語資源 | mebiusbox『レイトレーシング入門』(無料) | 体系的な日本語教材は乏しい(未検証) | 断片的なQiita記事、専門書は前提知識要求 | 2025年新刊あり(体系的) |
| 挫折リスク | 「完走はするが理解が浅くなる」リスク(証言複数) | RTIOW未経験だと二重に難しい | D3D12自体で挫折しやすい構造(状況証拠) | 4選択肢中で最も学習曲線が急という一般認識(個別未検証) |

---

## (1) CPUパストレーサー: Ray Tracing in One Weekend

### サイト構成(実際にraytracing.github.ioを開いて確認)

公式サイト(https://raytracing.github.io )は**3巻シリーズ**。

| 巻 | タイトル | 内容 |
|---|---|---|
| 第1巻 | Ray Tracing in One Weekend | シンプルな総当たり方式のパストレーサー。球体・法線・アンチエイリアス・拡散マテリアルなど |
| 第2巻 | Ray Tracing: The Next Week | テクスチャ、ボリューム(霧)、矩形、インスタンス、ライト、BVH |
| 第3巻 | Ray Tracing: The Rest Of Your Life | モンテカルロ積分・確率・重点サンプリングなど本格的な数学 |

第1巻の本文(https://raytracing.github.io/books/RayTracingInOneWeekend.html )を実読した結果、序文には以下の記述がある(検証済み・原文引用):

- 前提知識: 「ベクトル(ドット積とベクトル加算など)の基本的な理解」
- 言語: C++だが「必須ではない」。「最新の機能のほとんどを回避するが、継承と演算子オーバーロードは有用」
- 所要時間: "You should be able to do this in a weekend."(週末で完成できるはず、と明記)
- GPU: 言及なし=CPU実装が前提

章立ては13〜14章程度(Defocus Blur→Where Next?で終わる構成。版によって章番号が前後しており正確な最終章数は未確認)。

### 完走した初学者の体験記(日英で調査・所要時間の実証言)

公式の「週末で完成」という謳い文句に反し、**実際は超過するのが大半**という証言が一致している。

| 出典 | 証言内容 |
|---|---|
| kugi-masa氏(はてなブログ, 2019) https://kugi-masa.hatenablog.com/entry/2019/08/29/215941 | 「週末のつもりが実際はゆっくり進めて約2ヶ月かかった」。「コードを追っただけ」と正直に告白しつつ「基本が大事」「わかりやすい」と高評価 |
| kjumanenobikto氏(Zenn, Rust実装) https://zenn.dev/kjumanenobikto/articles/e01b520d57ba51 | 「数学の理解度が足りないので、基本的にはコードを翻訳していくだけ」と告白。「正直自分では何もわかっていない」という箇所もあり、Python出身者がRust特有の所有権/トレイトで苦戦し、ChatGPT等の助けを借りた |
| draftcode氏(2021) https://draftcode.osak.jp/blog/2021/02/07/ray-tracing-in-one-weekend%E3%82%84%E3%81%A3%E3%81%9F/ | 基本実装は半日程度だが、画像生成の高速化(並列化)にC++/Go/Rustで追加の時間を投じた |
| Unreal Engine公式ブログ(日本語) https://www.unrealengine.com/ja/blog/ue4-ray-tracing-night-week | 集中すれば折り返し地点まで2〜3時間だが、記事執筆等と並行すると伸びる |
| GitHub実装の一つ(英語圏) | "Spoiler: Takes more than 'a' weekend!"(週末以上かかる、と明言) |
| 別の英語圏の体験者 | Swiftへの移植込みで約10時間。別の一人は新ドメイン+新言語同時学習のため「1週間強」 |

**総合すると**、"自分の頭で理解しながら"進める真の初学者(プログラミング経験浅め)の現実的な所要時間は、**週5〜10hペースで第1巻のみなら2週間〜2ヶ月程度**が妥当な見積もりである(状況証拠に基づく推定であり、統計的な定量データではない)。

第3巻については本文中に「本気でレイトレーシングを職業にする人向け」「専門家の世界に入るための用語と数学」と明記されており(https://raytracing.github.io/books/RayTracingTheRestOfYourLife.html )、モンテカルロ積分・確率・重点サンプリングという本格数学が要求される。**第1・2巻とは難度が一段跳ね上がる**ため、第3巻はストレッチゴールとして切り離すのが妥当。

### 就活での見栄え

自作Vec3/Mat4・自作OBJパーサーという既存の「ライブラリに頼らず理解して書く」路線とテーマが完全に一致する。「モンテカルロ積分・重点サンプリングを理解して自作した」まで語れれば、CPU/GPUを問わない物理ベースレンダリングの普遍的基礎として評価されやすい(このパストレーシングのアルゴリズム自体は、後にDXR/Vulkan RTに進む際にも再利用できる)。

### 日本語学習資源

- **mebiusbox『レイトレーシング入門』**(Zenn, 無料) https://zenn.dev/mebiusbox/books/8d9c42883df9f6 — C++によるモンテカルロレイトレーシングまでを扱う日本語の入門書(全3章、約11万字、2021年公開、実際にページを開いて確認済み)。RTIOWとほぼ同じ範囲を日本語でカバーしており、英語原典の補完・理解の裏取りに最適。
- 体験記ブログ多数(教材ではないが実感の参考になる): kugi-masa氏、48's diary(https://bleu48.hatenablog.com/entry/2021/10/25/142523 )、draftcode氏、すらりん日記(https://blog.techlab-xe.net/try-raytracing-one-weekend-part1/ )

---

## (2) コンピュートシェーダーによるGPUレイトレ

- **前提知識**: 既存のOpenGL基礎(FPSカメラ・Phongライティングまで実装済みなので土台あり)+GLSL+並列処理の基本概念(ワークグループ・invocation)。RTIOWで学んだアルゴリズムをGLSLに移植する形が最も効率的な学習経路。
- **ハードウェア要件**: OpenGL 4.3以降が必要(ARB_compute_shaderがコアに昇格したのは4.3)。現行プロジェクトはOpenGL 3.3 coreなので**コンテキストバージョンの引き上げが必要**(GLFW/GLADの設定変更のみで対応可、大改修ではない)。専用RTコアは不要で、2012年以降のほぼ全GPU(Intel内蔵含む)で動作する点が最大の利点。
- **学習資源(英語)**: LWJGL Wikiの"Ray tracing with OpenGL Compute Shaders"シリーズ https://github.com/LWJGL/lwjgl3-wiki/wiki/2.6.1.-Ray-tracing-with-OpenGL-Compute-Shaders-(Part-I) 、Intel公式のPath-Tracing Workshop(GLSL/Shadertoy形式) https://www.intel.com/content/www/us/en/developer/videos/path-tracing-workshop-part-1.html
- **日本語資源**: 体系的な専門教材は見当たらなかった(調査した範囲では未検出、網羅的ではない)。doxas氏のQiita記事群(WebGL/GLSLレイトレーシング・レイマーチング入門) https://qiita.com/doxas/items/477fda867da467116f8d や、sketchbooks99氏のQiita記事(NVIDIA OptiXでのRTIOW実装、OptiX自体はcompute shaderとは別APIだが概念は近い) https://qiita.com/sketchbooks99/items/0631247e48bc08eae82e が参考になる程度。
- **就活での見栄え**: 「CPU実装とGPU実装の両方を経験し、メモリレイアウトやwarp divergence対策などの違いを語れる」という技術ストーリーになる。ただし業界標準のハードウェアBVHアクセラレーション(DXR/Vulkan RT)とは別物である点は面接で誤解のないよう説明が必要。

---

## (3) DirectX Raytracing (DXR)

- **前提知識**: Direct3D 12の実務経験が事実上の前提。2018年のブログ記事でも「D3D12 API知識が不可欠」と明言されている(http://masafumi.cocolog-nifty.com/masafumis_diary/2018/04/directx-raytrac.html )。D3D12自体がディスクリプタヒープ・コマンドリスト・パイプラインステートオブジェクト等を扱う低レベルAPIとして難度が高く、DXR固有にはさらに加速構造(BLAS/TLAS)・シェーダーテーブルの概念が加わる。
- **ハードウェア要件**: Windows 10 version 1809(build 17763)以降。GPUはNVIDIA GTX 1000番台(Pascal)以降でTier 1.0対応(ハードウェアRTコアなし、ドライバのcompute経由でエミュレート=低速)、RTX 20番台以降でTier 1.1(専用RTコアでハード加速)。AMD RX 6000番台以降・Intel Arc A番台以降がTier 1.1相当。非対応環境向けにMicrosoft公式Fallback Layerもあるが、開発の複雑さはむしろ増す。**ユーザーの実機がRTX搭載か未確認**である点はリスク。
- **就活での見栄え**: Windows/Xboxスタジオの実務に直結するAPIであり、"本物の業界標準リアルタイムRT"を語れる価値は非常に高い。ただしD3D12習熟だけで数ヶ月級の投資が必要。
- **日本語資源**:
  - 『DirectX12+DXRによるリアルタイムレイトレーシング』鎌田茂雄著、BAREPIXEL、2022年6月、8,000円+税 https://barepixel.co.jp/books-2 — ただし紹介文に「DX12の経験が必要」と明記されており、初学者がいきなり読む本ではない。
  - trap.jp(東京科学大学デジタル創作同好会)の実践記事「DirectX Raytracingに入門してみる」 https://trap.jp/post/2696/
  - Qiita: katsusanw氏「DirectX Raytracing(DXR)について調べてみた」https://qiita.com/katsusanw/items/d8e5f79bcfc20ce87baf 、taqu氏「DXRで遊ぶ」https://qiita.com/taqu/items/d49fad210354cc77e811 など断片的な実装メモが複数。
- **挫折率**: 定量データは見つからず(未検証)。ただし複数の記事がD3D12既習を前提として書かれていることから、D3D12自体が挫折ポイントになりやすい構造的リスクがあると推測される。

---

## (4) Vulkan Ray Tracing

- **前提知識**: Vulkan自体への習熟が事実上の前提。QiitaのAqoole氏の記事群でも「Vulkanへの一定の慣れが前提」と明記されている(https://qiita.com/Aqoole/items/5121cd64dc88ee4d9daf )。VK_KHR_ray_tracing_pipelineはVulkan 1.1以上・SPIR-V 1.4以上が必要。
- **ハードウェア要件**: DXRとほぼ並行。NVIDIA Pascal世代でも対応事例はあるが、ハード加速はRTX 20番台以降・AMD RX6000番台以降・Intel Arc A番台以降。Khronos公式ドキュメントも「実際の対応はドライババージョンごとに要確認」と注意書きしている。
- **就活での見栄え**: クロスプラットフォーム性(Windows/Linux/Android)があり、Vulkanを使いこなせること自体が強い技術シグナルになる。4選択肢の中で最も習得コストが高い分、完走できれば見栄えも最大級。
- **日本語資源**:
  - **『Vulkan実践入門 グラフィックスの基礎からレイトレーシング、メッシュシェーダーまで』山田英伸著、技術評論社、2025年10月27日発売、512ページ、4,500円+税** https://gihyo.jp/book/2025/978-4-297-15257-4 — Vulkanの基礎からレイトレーシング拡張までを一冊で体系的にカバーする、現時点で最も充実した日本語書籍(著者はセガ所属エンジニア、出版から日が浅く情報も新しい)。4選択肢の中で唯一「体系的な日本語専門書」が存在する点は特筆に値する。
  - Qiita: Aqoole氏「[Vulkan ray tracing] サンプルコードからの次の一歩」シリーズ(複数object・影・透過・raygenシェーダーを実践解説、Khronos公式サンプル前提) https://qiita.com/Aqoole/items/1331fa69ab94ea2c538b 等
  - hatoo氏のZenn本「Rustで始めるVulkan Raytracing」(Rust言語だが概念は共通) https://zenn.dev/hatoo/books/52bcb9e9f7c87d
- **挫折率**: 定量データなし(未検証)。「Vulkanは4API中で最も学習曲線が急」というのは業界で広く言われる一般認識だが、本調査で個別の統計的裏取りはしていない。

---

## 確信度と限界

- **検証済み**: raytracing.github.ioの構成・序文記述、各書籍/資源の出版情報・URL・価格、DXR/Vulkan RTのハードウェア世代要件(公式ドキュメント・技術記事に基づく)。
- **状況証拠**: 初学者の現実的所要時間(複数の個人体験記から推定した幅であり、統計的サンプルではない)。D3D12/Vulkanが挫折ポイントになりやすいという推測(直接的な挫折率データはなし)。
- **推測**: 「未完走のDXR/Vulkan RTよりCPUパストレーサーを完全に理解して語れる方が就活で評価されやすい」という判断は、調査した一般的な採用基準(自走力・技術的深さを重視する記述)からの筆者の推論であり、レイトレーシング機能単体に対する採用側の一次評価データではない。
- **未確認事項**: ユーザーが現在使用している実機のGPU(RTX搭載か否か)。これはDXR/Vulkan RTの現実性を左右する重要な変数であり、本人への確認を推奨する。