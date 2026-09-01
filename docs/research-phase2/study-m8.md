# 調査レポート: PBR(物理ベースレンダリング)理論の咀嚼方法とCook-Torrance直接光実装

## §1 教材マップ(順番つき)

### 1. 電通総研テックブログ「PBR(物理ベースレンダリング)マテリアル 入門編」
- URL: https://tech.dentsusoken.com/entry/pbr-material-1
- 前提章: なし(3DCGの基礎知識があれば読める)
- GLM/他ライブラリ依存度: なし。数式もほぼ登場しない
- 位置づけ: エネルギー保存則・metallic/roughness・フレネル効果・マイクロファセット理論を「概念」だけ、図解中心で説明。Cook-Torranceには踏み込まない。**数学が苦手な人の最初の一歩に最適**

### 2. LearnOpenGL - PBR/Theory
- URL: https://learnopengl.com/PBR/Theory
- 前提章: Advanced Lighting、Normal Mapping(公式に推奨。フレームバッファ・キューブマップ・ガンマ補正・HDRも既知が望ましい)
- GLM依存度: 低い(概念説明中心、GLSL疑似コードは3〜4個のみ)
- 内容: マイクロファセットモデル→エネルギー保存→反射率方程式→Cook-Torrance BRDF(D/F/G)→PBRマテリアルのテクスチャ仕様、の5段階構成
- 数式量・難易度: 中〜やや高(約15式。放射測定学の用語・積分表記が出る)

### 3. LearnOpenGL - PBR/Lighting
- URL: https://learnopengl.com/PBR/Lighting
- 前提章: PBR/Theory
- GLM依存度: 中。C++側のサンプルコード自体はglm前提で書かれているため、カメラ・行列部分は自作Vec3/Mat4への書き換えが必要。フラグメントシェーダのCook-Torrance本体はGLSLの素の数式なので書き換え不要
- 内容: Fresnel-Schlick、NDF(GGX/Trowbridge-Reitz)、Geometry(Schlick-GGX)、Cook-Torrance統合、線形色空間の重要性、Reinhardトーンマッピング

**このページの「球グリッドデモ」の定番構成(公式ソース直接確認、URL: https://github.com/JoeyDeVries/LearnOpenGL/blob/master/src/6.pbr/1.1.lighting/lighting.cpp)**
- `nrRows = 7, nrColumns = 7` の7×7グリッド
- metallicは行方向に `(float)row / nrRows` で0.0→1.0
- roughnessは列方向に `glm::clamp((float)col / nrColumns, 0.05f, 1.0f)` で **0.05→1.0**(roughness=0を意図的に避けている点に注意、理由は§2参照)
- `spacing = 2.5`
- 点光源4個、位置は`(±10, ±10, 10)`、色はいずれも `(300, 300, 300)` の強い白色点光源(IBLなし)
- 1つの球メッシュに毎回uniformでmetallic/roughnessを注入するだけの単純な二重ループで、多様な質感を一望できる構成

### 4. Google Filament - Physically Based Rendering in Filament
- URL: https://google.github.io/filament/Filament.md.html
- 前提章: ベクトル計算・ドット積、BRDF/BSDFの基本概念(LearnOpenGL Theory相当の知識があると読みやすい)
- 依存度: 特定ライブラリへの依存なし。GLSL実装スニペット併記
- 内容: マイクロファセットBRDF基礎→誘電体/導体の区別→エネルギー保存→Specular BRDF(D/G/F、fp16数値安定化のためroughnessを0.089にクランプする理由まで解説)→Diffuse BRDF→Clear Coat/Anisotropic/Clothモデル(発展)
- LearnOpenGLとの違い: より実装寄り・近似の採用理由を厚く説明する「リファレンス」的性格。数式の教科書的な導出はLearnOpenGLの方が丁寧

### 5. Cygames Engineers' Blog「物理ベースレンダリング -基礎編-」
- URL: https://tech.cygames.co.jp/archives/2129/
- 中級者向け。Cook-Torranceの概念紹介まで進むが、D/F/Gの完全な数式展開は「今後の連載」に持ち越されており、この1本では実装まで到達しない

### 6. Qiita(emadurandal)「物理ベースレンダリングを柔らかく説明してみる」シリーズ 第4回
- URL: https://qiita.com/emadurandal/items/76348ad118c36317ec5c(第3回・第5回へのリンクあり)
- 「手っ取り早くわかった気になりたい人向け」と著者自身が明言。正確性より直感を優先した説明。Torrance-Sparrowモデル、D/G/F、GGX分布に言及

### 7. Qiita(kyasbal_1994)「PBR勉強がてら実装したのでまとめる」
- URL: https://qiita.com/kyasbal_1994/items/c81bceb7819f956f15a4
- 中〜上級者向け。著者自身「ここから難しいかもしれない」「マクロが多くて読みにくい」と明言。GLSLでD/F/Gを個別関数として実装したコードが載っており、コード参照用には有用

### 8. LIGHT11(はてなブログ)「物理ベースレンダリング入門 その③」
- URL: https://light11.hatenadiary.com/entry/2020/03/05/220957
- 3部構成の最終回。Unity/HLSLでGGX+Height-Correlated Smith+Schlickフレネルを実装。数式とコードを並記しており、日本語資料の中では実装解説の完成度が高い部類

### 9. Kinda Technical - Lesson 19: Cook-Torrance Model(英語)
- URL: https://kindatechnical.com/computer-graphics/lesson-19-cook-torrance-model.html
- 初中級向け。導出過程は省略し、実装重視。LearnOpenGLよりアクセシブルな入口になり得る。GLSLコード例あり

推奨順序: **1(電通総研)→2(LearnOpenGL Theory)、必要なら6・9を並走→3(LearnOpenGL Lighting)、8(LIGHT11)でコード対応を補強→4(Filament)を辞書的に参照**。5・7は難度が高いのでコード参照用に留める。

## §2 初学者の典型的なつまづき

### (1) 画面の一部が突然「黒い染み」になる(NaN伝播)
- 症状→原因→対処: フレネル項`fresnelSchlickRoughness`で使う`cosTheta`が浮動小数点演算の誤差で`1.00001`のようにわずかに1を超えることがあり、`pow(1.0 - cosTheta, 5.0)`が`pow(負の数, 5.0)`となってNaNを生成、それが計算全体に伝播して黒い円/染みとして現れる。対処は`max(1.0 - cosTheta, 0.0)`でクランプすること。
- 出典: GitHub Issue「PBR black circle」#217(2020-12-13、報告者samrrr)、リポジトリオーナーJoeyDeVriesが同日「Good point, I've updated all code samples on the website and updated the shaders as well」と返信し公式コードを修正済み。 https://github.com/JoeyDeVries/LearnOpenGL/issues/217

### (2) roughnessが0付近でゼロ割り・NaNが起きる
- 症状→原因→対処: GGX等のNDFはroughness(またはalpha=roughness²)を分母に含むため、roughness=0だと`1/0`や`1/roughness^4`でInf/NaNになる。
  - LearnOpenGLの球グリッドデモが roughness の下限を **0.05** にクランプしているのはこの回避策そのもの(§1参照)。
  - Filament公式ドキュメントは、fp16(半精度浮動小数点)実装での非正規化数を避けるため roughness を **0.089** にクランプし、`1/roughness^4`が`6.1×10⁻⁵`を下回らないようにしていると明記。副次効果としてスペキュラエイリアシングの抑制にもなる。
  - LearnOpenGL標準コードでは分母全体に`+ 0.0001`のepsilonを加算している(`4.0 * max(dot(N,V),0.0) * max(dot(N,L),0.0) + 0.0001`)。
- 出典: Filament公式ドキュメント(https://google.github.io/filament/Filament.md.html)、LearnOpenGLシェーダーソース(https://github.com/JoeyDeVries/LearnOpenGL/blob/master/src/6.pbr/1.1.lighting/1.1.pbr.fs)

### (3) フレネル項の引数を取り違える
- 症状→原因→対処: Fresnel-Schlickは本来「視線方向Vとハーフベクトル(またはNとVのなす角)のcos」を渡す必要があるが、内積の向きを逆にしたり別のベクトルを渡すと「本来暗くなるべき正面が明るくなる/中心が光ってしまう」といった見た目の破綻が起きる。対処は`1.0 - dot(...)`のように向きを反転させて確認すること。上記(1)のIssue #217も、まさにフレネル項の引数(cosTheta)の扱いに起因するバグである。
- 出典: CoreDumping「Fresnel shader in Unity(With source)」(https://blog.coredumping.com/fresnel-shader-in-unitywith-source/、内積を反転させないと中心が光ってしまう具体例を掲載)、GitHub Issue #217(同上)

### (4) 金属(metallic=1)が真っ黒に見える ― 2種類の異なる原因
- **バグの場合**: `F0`をalbedoにmixし忘れる(`F0 = mix(vec3(0.04), albedo, metallic)`を書き忘れる)、または`kD *= 1.0 - metallic`を忘れてdiffuse項が金属にも乗ってしまう、といった実装漏れが典型。LearnOpenGL標準コードのこの2行は必須。
- **バグではなく仕様の場合**: 直接光(点光源)のみでIBL(環境マップ)を実装していない構成では、金属はほぼdiffuse反射を持たないため、ハイライトの外側は物理的に正しく真っ暗になる。これを「実装ミスでは」と誤解しがち。Unityフォーラムの実例(投稿者PlazmaInteractive、回答者bgolus 2017-04-10)で、まさに「roughness=1・metallic=1で真っ黒になる」問題の原因が「金属はほぼdiffuseがなく、環境光からの鏡面反射が実装されていないため」と指摘されている。**ユーザーが今回作るのは直接光のみのCook-Torranceなので、粗い金属が暗く沈むこと自体は想定内**という点を先に知っておくと安心。
- 出典: LearnOpenGLシェーダーソース(同上)、Unity Discussions(https://discussions.unity.com/threads/my-pbr-shader-goes-completely-black-when-it-shouldnt-be-at-roughness-value-of-1.465258/)

### (5) 線形色空間・ガンマ補正の抜け/二重掛け
- 症状→原因→対処: PBRはリニア色空間での計算が前提だが、テクスチャのsRGB→リニア変換を忘れると色がおかしくなり、逆に最終出力のガンマ補正を二重にかけると画面全体が「washed out(白っぽく眠い)」になる。LearnOpenGL PBR/Lightingがこの点を明示的に警告している。
- 出典: LearnOpenGL - PBR/Lighting(https://learnopengl.com/PBR/Lighting)、GameDev.netフォーラム「Gamma correction confusion」(https://www.gamedev.net/forums/topic/697254-gamma-correction-confusion/)

## §3 所要時間の実例

正直に言うと、**「PBR理論の理解に何週間/何ヶ月かかった」と具体的な期間を明言した体験談は、英語・日本語とも検索で見つけられなかった**。見つかった近い情報は以下の断片のみ:

- Mike Turitzin氏のブログ(経験者。グラフィックス分野全般について書いたもので、PBRに限定した記述ではない): 「グラフィックス分野全体を一通りマッピングできるようになるまで数年間の専念的な学習を要した」と述べ、「絶対初心者ならまず入門書・チュートリアルを1〜2つ終えてから読むべき」「同じトピックを複数の著者の説明で繰り返し読む必要がある」とアドバイス。出典: https://miketuritzin.com/post/how-to-learn-computer-graphics-techniques-and-programming/
- Warwick New氏のブログ(大学でOpenGL/SDL既習という準経験者): LearnOpenGLの記事に沿ってPBRを実装したと書かれているが、所要時間の記載はなし。出典: https://warwicknew.co.uk/blog/graphics-nothing-to-pbr/
- 別の検索で見つかったグラフィックスプログラマーの言及として、PBRは「unknown unknownsのウサギの穴(rabbit hole)」で見た目以上に深いと表現されているものがあったが、これも定量的な期間の記述ではない。

→ この項目は情報が薄い。定量的な体験談が欲しい場合は、日本語であればQiita/Zenn個人アカウントの「PBR実装記」のようなタイトルを直接探す、英語であればr/GraphicsProgramming等のRedditを検索エンジン経由でなく直接ブラウジングする、といった追加調査が必要(検索エンジン経由ではRedditのインデックスが薄く、有効な結果を得られなかった)。

## §4 日本語の補助資料

| 資料 | URL | 評価 |
|---|---|---|
| 電通総研「PBRマテリアル入門編」 | https://tech.dentsusoken.com/entry/pbr-material-1 | ◎ 数式ほぼなし、図解豊富。**最初の一歩に最適**。ただしCook-Torranceには触れないので、これだけでは実装に進めない |
| Cygames「物理ベースレンダリング-基礎編-」 | https://tech.cygames.co.jp/archives/2129/ | ○ 中級。段階的で親切な語り口だが、Cook-Torranceの完全展開は次回送りで未完結 |
| Qiita(emadurandal)第4回 | https://qiita.com/emadurandal/items/76348ad118c36317ec5c | ○ 「わかった気になりたい人向け」と明言。直感重視で読みやすいが厳密性は犠牲にしている |
| Qiita(kyasbal_1994) | https://qiita.com/kyasbal_1994/items/c81bceb7819f956f15a4 | △ 中〜上級。著者自身「難しい」「読みにくい」と明言。コード参照用 |
| LIGHT11「PBR入門その③」 | https://light11.hatenadiary.com/entry/2020/03/05/220957 | ◎ 数式とコード(Unity/HLSL)が並記され、日本語実装解説では完成度が高い部類。GLSLへの置き換えは自分で行う必要あり |
| tech-branch.9999ch.com のPBR記事 | https://tech-branch.9999ch.com/archives/597 | △ 内容はLearnOpenGL/PBR Theoryとほぼ同一構成・同一の論旨。翻訳/要約なのか独自記述なのか出典表記を直接確認できておらず、一次資料として引用するのは避けた方がよい。英語が読めない場合の「先読み」用途には使えるかもしれない |

## §5 学習順序のアドバイス

1. **数式ゼロの電通総研記事→LearnOpenGL Theory**の順で、まず「metallic/roughness/F0/エネルギー保存」という語彙と概念を数式抜きで掴んでから、数式入りの説明に進むと摩擦が少ない。LIGHT11やKinda Technicalのような「噛み砕き系」をLearnOpenGL Theoryの前後に並走させ、同じ概念を複数の説明で読む(Turitzin氏が推奨する学習法)のも有効。
2. **抜けやすい前提**: LearnOpenGL PBR/Lightingは公式にフレームバッファ・キューブマップ・ガンマ補正・HDR・ノーマルマッピングの既習を前提としている。ユーザーの第1フェーズ完了時点の想定レベル(Phongライティング・OBJパーサーまで)では、ガンマ補正とHDRが未修得の可能性が高い。ここを飛ばして進むと「なぜリニア空間で計算するのか」「なぜReinhardトーンマッピングをかけるのか」でつまづく(§2-5)。先にLearnOpenGLのGamma Correction章・HDR章(Advanced Lighting配下)を潰しておくと楽になる。
3. 今回のゴールは**直接光のみ**のCook-Torranceなので、IBL章(PBR/IBL/*)は後回しでよい。ただしその代わりに「粗い金属+単一点光源では暗く沈むのが仕様」(§2-4)を事前に知っておかないと、実装後に「バグかどうか」の切り分けに無駄な時間を使う。
4. §1・§3で詳述した**7×7球グリッドデモ**は、実装の検証手段として先に用意しておくと効率が良い。行=metallic、列=roughnessなので、特定の行/列だけ異常が出れば原因を絞り込みやすい(例: roughness列の左端だけ壊れる→NDF/Gのゼロ割り、metallic行の上端だけ異常→F0/kDのmix忘れ、等)。

## §6 確信度と限界

- **検証済み**: LearnOpenGL Theory/Lightingの章構成と前提章、球グリッドデモの正確なパラメータ(公式GitHubソースを直接WebFetchで確認)、GitHub Issue #217の症状・原因・修正・メンテナーの対応(Issueページとcomments APIを直接確認)、Filamentのroughness 0.089クランプの理由(公式ドキュメントの記述として複数箇所で一貫)、LearnOpenGLシェーダーの`+0.0001`epsilonと`F0 = mix(...)`/`kD *= 1.0 - metallic`のコード(ソース直接確認)。
- **状況証拠**: 各記事の「初心者にとっての読みやすさ」評価は、WebFetch経由で得た要約(記事全文を私が精読したわけではなく、要約用の小型モデルの評価を経由)に基づく。大筋は複数記事で一貫していたが、細部の難易度感覚にはズレがあり得る。
- **推測**: §5の学習順序の推奨は、収集した各資料の難易度評価から論理的に組み立てたものであり、「実際にその順で学んで成功した」という体験談による裏付けではない(該当する体験談は見つからなかった)。
- **見つからなかったもの/薄い箇所**:
  - §3(所要時間の実例)は、PBR理解に特化した定量的な期間を述べる一次体験談を英語・日本語とも発見できなかった。Reddit上の投稿は検索エンジン経由ではほぼヒットせず(Redditのインデックスの薄さによる限界の可能性が高い)、この点は追加調査(Reddit直接検索など)の余地がある。
  - 「フレネル引数を取り違えた」ことを一人称で語る実装者の失敗談は、GitHub Issue #217以外に強い実例を見つけられなかった。一般論としての注意喚起(内積の向きに注意)は複数あるが、「私はこう間違えた」という体験談形式のものは少ない。
  - tech-branch.9999ch.comの記事がLearnOpenGL Theoryの翻訳なのか独立した記述なのかは未確認。