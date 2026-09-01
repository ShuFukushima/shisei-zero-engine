# 調査レポート: IBL(Diffuse Irradiance / Specular IBL + BRDF LUT)実装

## §1 教材マップ(順番つき)

| # | タイトル | URL | 前提章 | GLM依存度 |
|---|---|---|---|---|
| 0 | (既習前提) Framebuffers | https://learnopengl.com/Advanced-OpenGL/Framebuffers | — | 低。シャドウマッピングで既にFBOは使用済みのはずなので実質クリア |
| 0 | (既習前提) Cubemaps | https://learnopengl.com/Advanced-OpenGL/Cubemaps | Framebuffers | 低 |
| 1 | PBR/Theory | https://learnopengl.com/PBR/Theory | Basic-Lighting系一式 | 中。GGX法線分布・Fresnel-Schlickなどの数式はスカラー/vec3演算のみで、GLM固有機能への依存はほぼない |
| 2 | PBR/Lighting | https://learnopengl.com/PBR/Lighting | PBR/Theory | 中。同上 |
| 3 | PBR/IBL(概要) | https://learnopengl.com/PBR/IBL | PBR/Lighting, Cubemaps, Framebuffers | 低(概要のみでコードなし) |
| 4 | PBR/IBL/Diffuse-irradiance | https://learnopengl.com/PBR/IBL/Diffuse-irradiance | 上記すべて | 中〜高 |
| 5 | PBR/IBL/Specular-IBL | https://learnopengl.com/PBR/IBL/Specular-IBL | Diffuse-irradiance | 中〜高 |

GLM依存の具体的な中身(GitHub上のソース `2.1.1.ibl_irradiance_conversion.cpp` / `2.2.1.ibl_specular.cpp` を直接確認):
- equirectangular→cubemapのキャプチャ用: `glm::perspective(90°, 1.0, 0.1, 10.0)` を1つと、原点から6方向を向く `glm::lookAt()` を6個(配列)。
- prefilter時も同じ6方向の `captureViews` を使い回す。
- 通常のシーン描画側は `glm::translate`, `glm::scale`, 法線行列の `glm::transpose(glm::inverse())` など、これまでのMVP実装と同種の処理。

つまり **GLM依存箇所は「Perspective行列生成」「LookAt行列生成」「6方向分の配列化」に限定**されており、自作Mat4に `Perspective()` / `LookAt()` 関数が既にあれば(第1フェーズのFPSカメラで実装済みのはず)そのまま書き換え可能。GLM特有の機能(quaternionやスプライン等)は登場しない。(根拠: GitHub該当ソースのWebFetch要約、確信度: 検証済み)

## §2 初学者の典型的なつまづき

**① equirectangular HDR→キューブマップ変換**

- 症状: キューブマップ6面が全て真っ黒になる。
  原因: FBOにカラーアタッチメントを付けず深度アタッチメントのみで描画していた。
  対処: `glFramebufferTexture2D` で各面をカラーアタッチメントとして明示的に設定する。
  出典: https://community.khronos.org/t/render-equirectangular-environment-map-to-cubemap/110444 (2024年1月, 中級者の投稿)

- 症状: 1面目だけ正しく描画され、残り5面が黒いまま。
  原因: パノラマテクスチャのバインドをループの外(1回だけ)で行っており、各面描画時に有効になっていなかった。
  対処: テクスチャバインドを6面ループの内側に移動する。
  出典: 同上スレッド

- 症状: 通常の `stbi_load()` でHDRファイルを読むと壊れる/範囲がおかしい。
  原因: HDRは浮動小数点データなので `stbi_loadf()` を使う必要がある(8bit整数用の関数とは別)。
  対処: `stbi_loadf()` に切り替え、`GL_RGB16F` 等の浮動小数点内部フォーマットでテクスチャ作成。
  出典: https://www.turais.de/how-to-load-hdri-as-a-cubemap-in-opengl/ (2021年5月)

**② Diffuse irradiance畳み込み後の見た目異常**

- 症状: envCubemapをprefilterする際に黒い点(ドット)状のノイズが出る。
  原因: equirectangular→cubemap変換直後にミップマップを生成していない状態でサンプリングしている。
  対処: 変換後すぐに `glGenerateMipmap(GL_TEXTURE_CUBE_MAP)` をenvCubemapに対して呼ぶ。
  出典: LearnOpenGL公式ソース `ibl_specular_textured.cpp`(コード内コメントで明記、検索経由で確認)

**③ Specular prefilterのミップ/シーム問題**

- 症状: 低いミップレベル(粗さが高い=ぼやけた反射)で立方体の面と面の境目に黒い継ぎ目が見える。
  原因: OpenGLはデフォルトでキューブマップの面をまたいだ線形補間をしない。ミップが低解像度・広いサンプルローブになるほど目立つ。
  対処: `glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS)` を有効化する。
  出典: https://www.khronos.org/opengl/wiki/Seamless_Cubemap 、LearnOpenGL公式ソースコード内コメント

- 症状(初心者ツール利用例だが構造は同じ): 特定のパノラマ画像でroughness>0のときにキューブマップのミップにアーティファクトが出る。
  出典: https://github.com/yasuhirohoshino/ofxPBR/issues/4 (issue本文のみ確認、詳細な解決過程は未確認)

**④ BRDF LUT生成時のNaN**

- 症状: BRDF LUTテクスチャの縁が異常な値になる。
  原因: 計算過程でNaNが混入している(視線ベクトルが接線方向に近くn·vが0付近になるゼロ除算)。
  対処: dot積を小さい正の値でクランプする。
  出典: https://www.technicalife.net/ibl-rendering-wip/ (2024年12月、日本語ブログ・実装経験者)。※この1件のみで裏付けは薄い(状況証拠)。

## §3 所要時間の実例

定量的な(「◯時間/◯日かかった」という数値付きの)初学者体験談は、英語圏のReddit/フォーラムを含めて**検索では発見できなかった**(確信度: 低い/情報が薄い、後述§6参照)。見つかった中で最も近いのは以下:

- **TechnicaLife(日本語ブログ、経験者: DirectX12/Vulkan経験のある中〜上級者)**: 2024年12月1日の記事でDiffuse/Specular IBLを一通り実装したが、Diffuse側の見た目の不具合(タンジェント/バイノーマルの反転ノイズ)とBRDF LUTのNaN問題に直面し、力尽きて未完のまま終了。その後半年以上を経た2025年6月22日の記事で再挑戦を開始し、2025年11月29日更新版で完結させた。前回挑戦では最後まで到達できなかった旨を著者自身が振り返っている。
  出典: https://www.technicalife.net/ibl-rendering-wip/ 、https://www.technicalife.net/pbr-ibl-rendering-2025/
  → **経験豊富なエンジニアでも複数ヶ月・複数回の挑戦を要した実例**として参考になる。ただし「初学者が何時間で終わるか」への直接の答えではない。

- Technik90のブログ(2025年12月29日)はIBLの実装時間ではなく**実行時パフォーマンス最適化**(RTX 5080で9ms→0.62ms等)が主題であり、学習・実装にかかった時間の参考にはならない。

## §4 日本語の補助資料

1. **TechnicaLife「IBLによる間接光を考慮したレンダリングの流れ(未完)」**(2024/12/1) https://www.technicalife.net/ibl-rendering-wip/
   評価: 挫折の実例として非常に価値が高い。NaN、タンジェント反転ノイズなど具体的な失敗が赤裸々に記録されている。ただし著者は中〜上級者で前提知識は高め。

2. **TechnicaLife「PBRとIBLによる間接光を考慮したレンダリング」**(初版2025/6/22、更新2025/11/29) https://www.technicalife.net/pbr-ibl-rendering-2025/
   評価: 上記の完結編。Diffuse/Specular双方を最後まで実装。理論と実装の両方をカバーしていると要約からは読めるが、コードの正確性そのものは今回精読していない。

3. **matcha-choco010.net「OpenGLでDeferredシェーディングを実装する(Specular IBL)」**(2020/4/18) https://matcha-choco010.net/2020/04/18/opengl-deferred-specular-ibl/
   評価: Specular IBLに特化。roughness 0/0.25/0.5/0.75/1.0でミップに格納しその間をブレンドする実装方針、Hammersley/ImportanceSampleGGXのGLSLコード例あり。中程度の難度で実装可能な具体性。

4. **Qiita「物理ベースレンダリングを柔らかく説明してみる(6)」**(emadurandal氏、初版2022/7/10・更新2025/5/10) https://qiita.com/emadurandal/items/b2ae09c5cc1b3da821c8
   評価: IBL全体(拡散・鏡面畳み込み、Split Sum近似、BRDF LUT)を理論から解説し、`PrefilterEnvMap`・`ImportanceSampleGGX`・`IntegrateBRDF`等のコード例が充実。ただしGGXやImportance Samplingの前提知識を要求する中〜やや高度な内容(著者自身も途中で疑問が生じる読者を想定した記述をしている)。

5. **Qiita「three.js + キューブマップでお手軽IBL」**(kaneta1992氏) https://qiita.com/kaneta1992/items/df1ae53e352f6813e0cd
   評価: three.js(WebGL)向けの簡易IBL。自作C++エンジンにはコードはそのまま使えないが、概念理解の補助として言及のみ(内容は精読していない)。

## §5 学習順序のアドバイス

- **FBO/キューブマップの理解がボトルネックになりやすい。** equirect→cubemap変換の「6面ループ描画」でつまづく例が多いが(§2①)、ユーザーは既にシャドウマッピングでFBOを扱っているはずなので、そこは強み。ただし「なぜキャプチャ用projectionはFOV 90度固定なのか」「なぜ毎回viewportをキャプチャ解像度に設定し直す必要があるのか」を先に自分の言葉で説明できるようにしておくと、黒面・部分描画系のバグを未然に減らせる。
- **`glGenerateMipmap(GL_TEXTURE_CUBE_MAP)` を「おまじない」で済ませない。** これを忘れると後工程(prefilter)で原因不明のドット状ノイズが出て、デバッグ箇所を誤認しやすい(§2②)。
- **`GL_TEXTURE_CUBE_MAP_SEAMLESS` は実装の最初期段階で1行有効化しておく。** 後からシームの原因を探す時間を丸ごと節約できる(§2③)。
- **Diffuse irradianceは「精度より先に動くもの」を優先してよい。** LearnOpenGL公式実装自体が32×32という低解像度・`sampleDelta = 0.025` という粗いサンプリングで運用しており、高周波成分が少ない用途では破綻しない(§1確認済み)。
- **BRDF LUTの縁の異常値(NaN)は、n·vのゼロ除算を疑う。** 実装時にdot積へのクランプを入れる箇所として意識しておくと、TechnicaLifeが経験した詰まりを事前に回避できる可能性がある。

## §6 確信度と限界

- equirect→cubemap変換のつまづき(黒面・バインド漏れ・stbi_loadf)、`glGenerateMipmap` の黒点対策、`GL_TEXTURE_CUBE_MAP_SEAMLESS` のシーム対策、GLM依存箇所の特定: **検証済み**(公式コード・Khronosフォーラム・Khronos wikiなど一次情報で裏付けあり)。
- BRDF LUTのNaN問題: **状況証拠**(TechnicaLifeブログ1件のみ。他ソースでの裏付けは取れていない)。
- **所要時間の定量データはほぼ見つからなかった。** 英語圏Reddit/フォーラムでの直接的な体験談を複数パターンの検索クエリで試したが該当なし。TechnicaLifeの「複数ヶ月・複数回挑戦」という1事例のみで、統計的傾向は言えない。
- **「Diffuseだけで止める」縮小がどの程度一般的かは、直接的な証拠が見つからなかった。** これは今回の調査で最も手薄な箇所。推測(未検証)として、完走してブログ/GitHubに公開する人には「最後まで実装できた人」へのバイアスがかかりやすく、diffuseだけで止めた人の記録はWeb上に残りにくい構造的可能性がある、とは言えるが実証はしていない。
- LearnOpenGL公式の `PBR/IBL` 概要ページ本文は、WebFetchツールが「開発中」という古いキャッシュスナップショットしか返さず、**本文全体を直接確認できていない**(検索結果のスニペットからの部分情報のみで補完)。Diffuse-irradiance/Specular-IBLの両詳細ページは正常に取得・確認できているため、実務上の支障は小さいと考えられるが、概要ページの精読は未達成。
- 日本語資料は該当記事の存在と要旨は確認したが、掲載コードの正確性そのもの(バグの有無等)は検証していない。