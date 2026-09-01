# 調査レポート: オフスクリーンレンダリング(FBO)/HDR/トーンマッピング/Bloom/描画順ソート/トゥーンシェーディング+輪郭線

## §1 教材マップ(順番つき)

推奨学習順に並べています。LearnOpenGL公式のセクション内順序(Advanced OpenGL: Depth→Stencil→**Blending**→**Face culling**→**Framebuffers**→Cubemaps…、Advanced Lighting: …→Normal Mapping→Parallax Mapping→**HDR**→**Bloom**→…)は本サイト自体で確認済み([学習LearnOpenGLサイドバー](https://learnopengl.com/)を直接WebFetchして確認)。

| # | タイトル / URL | 前提章 | GLM依存度 |
|---|---|---|---|
| 1 | [Blending](https://learnopengl.com/Advanced-OpenGL/Blending)(透明・描画順ソート) | Depth Testing | 低い。距離ソートに`glm::vec3`同士の距離計算を使うが、自作Vec3の距離関数に単純に置き換え可能(確認済み: `std::map`で距離をキーに逆順イテレートする実装) |
| 2 | [Face culling](https://learnopengl.com/Advanced-OpenGL/Face-culling) | 特になし | 低いと推測(状態設定APIが中心でベクトル計算が少ない章のため。**本文は未フェッチのため推測**)。背面法アウトラインの前提概念として重要 |
| 3 | [Framebuffers](https://learnopengl.com/Advanced-OpenGL/Framebuffers) | Depth Testing章のシーンを流用(公式に明記) | 低い(確認済み。コード例にGLM言及なし、NDC座標を直書き) |
| 4 | [HDR](https://learnopengl.com/Advanced-Lighting/HDR) | Framebuffers、基本的な照明計算 | あり(確認済み。`glm::vec3`使用箇所があるが、自作Vec3で問題なく置換可能な範囲) |
| 5 | [Bloom](https://learnopengl.com/Advanced-Lighting/Bloom) | **HDR必須**(浮動小数点フレームバッファ前提。公式に明記) | 低い(確認済み。ベクトル演算はGLSL標準の`vec2/vec3/vec4`のみ) |
| 6(発展) | [Phys. Based Bloom](https://learnopengl.com/Guest-Articles/2022/Phys.-Based-Bloom)(ゲスト記事、Karis average・ダウン/アップサンプリング方式) | Bloom(基本版)を完成させた後 | シェーダー中心の記事のため低いと推測(未確認) |
| 7 | [床井研究室 - トゥーンシェーディング](https://marina.sys.wakayama-u.ac.jp/~tokoi/?date=20080218)(理論解説、GLSL不使用) | Face culling、基本ライティング | GLMなし(理論記事、コードなし) |
| 8 | [wgld.org - トゥーンレンダリング(w048)](https://wgld.org/d/webgl/w048.html)(背面法アウトライン、WebGL/GLSL実装) | Face culling(cullFaceの概念) | GLM相当の依存なし。ただしWebGL(GLSL ES)の文法で書かれており、デスクトップOpenGL 3.3 CoreのGLSL 330への読み替えが必要(単純写経不可) |
| 9(代替手法) | [wgld.org - ステンシルバッファでアウトライン(w039)](https://wgld.org/d/webgl/w039.html) | ステンシルテストの基礎 | 同上。3パス構成でやや複雑、背面法より実装コストが高い |

補足: LearnOpenGLには「トゥーンシェーディング」「輪郭線」の章が存在しない(公式サイト内検索で該当なし)。この2テーマは日本語圏の個人サイト(床井研究室・wgld.org)が主教材になる。

## §2 初学者の典型的なつまづき

### ① incomplete framebuffer 系エラー

- **症状**: `glCheckFramebufferStatus()`が`GL_FRAMEBUFFER_COMPLETE`以外を返し、画面が真っ黒/真っ白になる。
- **原因(頻度が高い順)**:
  1. アタッチメント(色/深度/ステンシル)を1つも付けていない、または必須のものが欠けている → `GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT`(出典: [Khronos OpenGL Wiki - Framebuffer](https://wikis.khronos.org/opengl/Framebuffer)、[Khronosコミュニティフォーラム](https://community.khronos.org/t/incomplete-framebuffer-error/110615))
  2. 複数アタッチメントの**幅・高さが不一致** → `GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS`。実例として、経験者ですら異なるサイズのテクスチャを同じFBOに付けて「小さい方のテクスチャに引きずられてバッファ全体が1/4に縮小される」というバグにハマり、`assertSameSize()`という自作マクロで検証する羽目になった体験談がある(出典: [Swiftlessチュートリアルのコメント欄、ユーザーSeppo](https://www.swiftless.com/tutorials/opengl/framebuffer.html))。同様の報告が[NVIDIA開発者フォーラム](https://forums.developer.nvidia.com/t/framebuffer-incomplete-when-attaching-color-buffers-of-different-sizes-with-dsa/211550)にもある。
  3. キューブマップ(レイヤード画像)と通常2Dテクスチャを混在してアタッチ → `GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS`。実際にKhronosフォーラムで初学者が質問し、キューブマップは`glFramebufferTextureLayer()`で面ごとにバインドし直す必要があると判明した実例がある(出典: [community.khronos.org](https://community.khronos.org/t/incomplete-framebuffer-error/110615))。
  4. フォーマットの組み合わせが実装非対応 → `GL_FRAMEBUFFER_UNSUPPORTED`(出典: [Khronos Wiki - Framebuffer](https://wikis.khronos.org/opengl/Framebuffer))。
- **対処**: FBO構築直後に必ず`glCheckFramebufferStatus()`をチェックし、返り値をログ出力する習慣をつける。アタッチメントの幅・高さ・フォーマットを毎回書き出して見比べる。

### ② Bloomのfireflies(ちらつき)と閾値問題

- **症状**: カメラを動かすと、明るい1ピクセル程度の点が明滅する。
- **原因**: 「明るいサブピクセルがピクセルサイズ以下の大きさになると、カメラが少し動いただけでラスタライズされる/されないが切り替わり、輝度抽出パスに入ったり入らなかったりする」ため(出典: [LearnOpenGL - Phys. Based Bloom](https://learnopengl.com/Guest-Articles/2022/Phys.-Based-Bloom)、[gamedev.netフォーラム](https://www.gamedev.net/forums/topic/673136-bloom-flickering/))。単純な閾値抽出方式はこの問題に弱い。
- **対処法(フォーラムでの実際のアドバイス)**:
  - 閾値判定を`if (brightness > threshold)`のような硬い境界にせず、`smoothstep`で「立ち上がりが緩やかなトゥ(toe)カーブ」を作る(出典: gamedev.netフォーラム)。
  - より根本的な対処として、単純閾値ではなく**Karis average**(ダウンサンプリング時に`1/(1+luma)`の重みで4ピクセルを平均する)を使い、閾値そのものを撤廃してシーン全体に均一にBloomをかける方式がある(出典: LearnOpenGL Phys. Based Bloom)。
  - 閾値の目安: HDRで適正露出のシーンなら「閾値≈1.0付近」から始める、という記述がある(出典: [Unity Manual Bloom](https://docs.unity3d.com/2018.2/Documentation/Manual/PostProcessing-Bloom.html)。※Unity固有の実装だが閾値の考え方自体は流用可能)。
  - **限界**: 「初学者が具体的にどの数値から閾値を決めていくか」という手順化された説明は今回見つけられなかった(§6参照)。

### ③ FBO+半透明ブレンディングで「窓の外の背景が透けて見える」

- **症状**: 不透明背景色でクリアしたFBOに半透明オブジェクトを描き、それをそのまま画面に貼ると、FBO全体がうっすら透けて見える。
- **原因**: 不透明ピクセル(α=1)に半透明ピクセル(0<α<1)を通常のアルファブレンドすると、出力側のα値も1未満になってしまうため(出典: [Qiita - 半透明の要素を描いたFBOを描画する際のハマりポイント](https://qiita.com/nariakiiwatani/items/df3eb6a49be0fa431387))。
- **対処**: `glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE)`のようにアルファチャンネルのブレンド式だけ変える、またはFBO描画時にブレンディングを切る。

### ④ 半透明の描画順ソートは「近似」に過ぎない

- **症状**: 距離順にソートして描いても、複雑な形状や交差するポリゴンでは破綻する。
- **原因**: LearnOpenGL自身が「この単純な距離ソートは回転・スケール・その他の変形を考慮しておらず、複雑な形状には不十分」と明記している(出典: [LearnOpenGL - Blending](https://learnopengl.com/Advanced-OpenGL/Blending))。床井研究室でも「ポリゴン同士が交差する『三すくみ』状態には対応できない」と同種の限界が指摘されている(出典: [床井研究室 - 半透明処理](https://marina.sys.wakayama-u.ac.jp/~tokoi/?date=20081122))。
- **対処**: 完全解はOrder Independent Transparency等の高度な技法だが、初学者教材の範囲では「オブジェクト単位の距離ソートで妥協する」が標準的な落としどころ。

### ⑤ トゥーン輪郭線(背面法)が太くなりすぎる/破綻する

- **症状**: 輪郭線が異様に太い、モデルの継ぎ目で穴が開く。
- **原因**: 背面法は頂点を法線方向に押し出す量(膨張量)を大きくしすぎると不自然になる。wgld.orgの実装では「ほんの少しだけ膨らませる」ことで違和感なく複数モデルの重なりにも対応させている(出典: [wgld.org - トゥーンレンダリング](https://wgld.org/d/webgl/w048.html))。また床井研究室では、視線と法線の内積で輪郭を判定する別方式について「面と面のなす角度によって線の太さが変わってしまう」限界を指摘している(出典: [床井研究室 - トゥーンシェーディング](https://marina.sys.wakayama-u.ac.jp/~tokoi/?date=20080218))。
- **対処**: 押し出し量は小さい値から試す。cullFaceの表裏(`GL_BACK`/`GL_FRONT`)の意味を正確に理解してから2パス目を実装する。

## §3 所要時間の実例

正直に申し上げると、**「FBO/HDR/Bloom/トゥーン+輪郭線の実装に具体的に何日/何週間かかった」という定量的な体験談は、今回の調査範囲(WebSearch/WebFetch)では見つけられませんでした。** Reddit(r/opengl, r/GraphicsProgramming等)への複数回の検索を試みましたが、投稿自体がヒットしないか、検索エンジンの索引が薄いためと考えられます(Redditに実在しないと断定はできません。未検証)。

見つかったのは以下のような**間接的な苦労の証拠**のみです:

- LearnOpenGL公式自身が「Bloomは複雑な技術ではないが、正確に仕上げるのは難しい。見た目の質はほぼブラーフィルタの質と種類で決まる」と述べている(出典: [LearnOpenGL - Bloom](https://learnopengl.com/Advanced-Lighting/Bloom)。経験者=著者の見解、定量的な時間情報なし)。
- Qiita edo_m18氏のBloom実装記事では「ぼかし処理が間違っていました。直したバージョンに差し替えています」という修正履歴が明記されており、一度で正しく実装できず試行錯誤したことが読み取れる(出典: [Qiita - [WebGL] Bloom表現を実装してみる](https://qiita.com/edo_m18/items/c43177c0a18a2ea210b6)。中〜経験者、定量情報なし)。
- FBOのサイズ不一致バグは経験者(複雑なDeferred Rendererを開発中のユーザー)でも自作の検証マクロが必要になるほど原因特定に手間取っている(出典: [Swiftlessコメント欄、Seppo](https://www.swiftless.com/tutorials/opengl/framebuffer.html)。定量情報なし)。
- Khronosフォーラムのincomplete framebufferの質問は、原因究明までに複数回のやり取り(コード提示→回答→再質問)を要している(出典: [community.khronos.org](https://community.khronos.org/t/incomplete-framebuffer-error/110615)。時間の記載なし)。

**結論**: 「数時間で終わる軽い章」と楽観視すべきではなく、特にFBO関連のデバッグとBloomのブラー品質調整は経験者でも複数回の試行錯誤が常態化している、というのが今回集められた証拠から言える範囲です。定量的な目安(何時間/何日)は提示できません。

## §4 日本語の補助資料

| サイト/記事 | テーマ | 評価コメント |
|---|---|---|
| [床井研究室](https://marina.sys.wakayama-u.ac.jp/~tokoi/oglarticles.html)(和歌山大学・床井浩平氏の個人サイト、OpenGL関係記事一覧) | 全般 | 老舗の個人サイトで理論的な説明が丁寧。[トゥーンシェーディング記事](https://marina.sys.wakayama-u.ac.jp/~tokoi/?date=20080218)はGLSLコード自体は載っていない(理論のみ)ため、実装は自分でGLSL化する必要あり。[半透明処理記事](https://marina.sys.wakayama-u.ac.jp/~tokoi/?date=20081122)はやや専門用語が多く初学者にはやや難、複数手法の比較に価値がある |
| [wgld.org](https://wgld.org/) - [トゥーンレンダリング(w048)](https://wgld.org/d/webgl/w048.html) / [ステンシルアウトライン(w039)](https://wgld.org/d/webgl/w039.html) | トゥーン+輪郭線 | WebGL入門の定番個人サイト。**GLSLコード例あり**、cullFaceの使い方が図解的に説明されており初学者向けに良質。ただしWebGL(GLSL ES)の文法であり、デスクトップOpenGL 3.3 Core(GLSL 330)への読み替えは必要(単純コピペ不可) |
| [Qiita - 半透明の要素を描いたFBOを描画する際のハマりポイント](https://qiita.com/nariakiiwatani/items/df3eb6a49be0fa431387) | FBO+透明 | 実践的な落とし穴を数式付きで明確に説明。初学者のデバッグに直結する良記事 |
| [Qiita - [OpenGL] FrameBufferとRenderBufferについてメモ](https://qiita.com/edo_m18/items/95483cabf50494f53bb5) | FBO | 概念説明(「FBOはRenderBufferを統合するマネージャー」等の比喩)は分かりやすいが、コード例に誤字や省略があり、そのままでは動かない点に注意 |
| [Qiita - [WebGL] Bloom表現を実装してみる](https://qiita.com/edo_m18/items/c43177c0a18a2ea210b6) | Bloom | 輝度抽出→ガウスブラー(縦横2パス)→合成の流れが図解付きで丁寧。コードも充実しており実装の参考度は高い。ただしWebGL/JavaScript |
| [Qiita - Real-Time Glow & Bloom](https://qiita.com/UWATechnology/items/6ea9c25da8da151e2ca8)、[グラフィックス勉強のノート 4.1 Bloomアルゴリズム](https://qiita.com/wyt5818956/items/315b079400d47c2d7bed)、[openFrameworksでBloomエフェクト](https://qiita.com/sketchbooks99/items/09fca26b3e910aff049b)、[DirectX12でミクさんを躍らせてみよう17](https://qiita.com/dpals39/items/8954c311149e9380c624) | Bloom(概念) | いずれも未精読・概要のみ確認。APIは異なる(DirectX12/openFrameworks等)が「輝度抽出→ブラー→合成」という概念自体は共通なので補助的に参考可 |
| [Zenn - 【Unity】Bloomの基本処理を紐解いてみる](https://zenn.dev/zero_0r0/articles/b8460bed827b60) | Bloom(概念) | Unity実装だが概念の噛み砕きは分かりやすい。Unity固有APIなので実装のコピペには使えない |
| [はてなブログ - トーンマップいろいろ](https://hikita12312.hatenablog.com/entry/2017/08/27/002859)、[tech-tldr.com - HDRとトーンマッピングとは](https://www.tech-tldr.com/graphics/hdr-tonemapping-exposure/) | トーンマッピング | Reinhard式(`x/(1+x)`)を含む複数手法の数式解説。LearnOpenGLの記述と整合していることを確認済み |
| [Qiita - LearnOpenGLをモダンにしてみた話](https://qiita.com/kkkjp/items/cd79704b479d4fdb3e7e) | 参考 | シャドウ・法線マッピング・HDR・Bloom・Deferred Shading・SSAOを1シーンに統合する取り組みの紹介記事。所要期間等の体験談は書かれていなかったが、実装の全体像を掴む参考になる |

なお、**LearnOpenGLの日本語全訳サイトは見つからなかった**(中国語訳の[LearnOpenGL-CN](https://learnopengl-cn.github.io/)は存在するが日本語ではない)。[opengl-tutorial.org 日本語版](http://www.opengl-tutorial.org/jp/)は別系統の入門チュートリアルで、LearnOpenGLの直接の代替ではない。

## §5 学習順序のアドバイス

1. **Blending → Face culling → Framebuffers → HDR → Bloom** の順が最も手戻りが少ない。特にHDR→Bloomは直列依存(BloomはHDRの浮動小数点フレームバッファが前提)なので、この2つの順序は絶対に入れ替えないこと(出典: LearnOpenGL Bloom本文で明記)。
2. **Face cullingを先に済ませておく**と、後のトゥーン輪郭線(背面法)で`cullFace(GL_FRONT)`の意味に迷わずに済む。wgld.orgの実装もFace cullingの表裏反転そのものが輪郭線のトリックの核心なので、ここが曖昧なまま輪郭線に進むと詰まりやすい。
3. **描画順ソート(透明)はFBO/HDR/Bloomと技術的に独立**しているため、どのタイミングで挟んでも実装上は問題ない。ただし後で「半透明オブジェクト+Bloomの輝度抽出」を組み合わせる際に相互作用を考える必要が出てくるため、Bloomに着手する前までに一度片付けておくと余計な手戻りが減る。
4. **トゥーン+輪郭線は他の4テーマと技術的な依存関係がほぼゼロ**なので、モチベーション維持のために先にやっても、他を終えてから最後にやってもどちらでも構わない。ただし②で述べた通りFace cullingの理解だけは前提として必要。
5. **抜けが起きやすい箇所への先回り対策**:
   - FBO作成コードを書いたら、動作確認の前に必ず`glCheckFramebufferStatus()`のログ出力を仕込む習慣を最初から持つ(§2①)。これを後回しにすると、incomplete framebufferの原因究明に不釣り合いに時間を取られる。
   - HDR導入時、既存のテクスチャ/カラーアタッチメントのフォーマットを`GL_RGB`(8bit)から`GL_RGBA16F`(浮動小数点)に変える箇所を明示的にチェックリスト化しておく。ここを見落とすと「Bloomの閾値を上げても何も光らない」という分かりにくい不具合になる。
   - Ping-pong FBOは「2つのFBOを交互に切り替えながら同じブラー処理をN回繰り返す」という構造がコードだけでは把握しにくいため、図(2つの箱を交互に指すポインタの絵)を自分で描いてから実装に入ることを推奨する。

## §6 確信度と限界

- **検証済み(高確信度)**: LearnOpenGL公式ページ(Framebuffers/HDR/Bloom/Blending)の技術内容・前提章・GLM依存度は、該当ページを直接WebFetchして本文を確認した。incomplete framebufferの原因分類(Khronos Wiki)、Bloomのfireflies対処法(Karis average、LearnOpenGL Guest Article)も直接引用元を確認済み。
- **状況証拠(中確信度)**:
  - Face cullingの章のGLM依存度は本文を直接フェッチしておらず、他章の傾向からの推測。
  - トゥーン+輪郭線の日本語資料(床井研究室・wgld.org)は内容を要約確認したが、実際にコードを動かして検証したわけではない(調査モードのため実行不可)。
  - gamedev.netのbloom flickeringスレッドは、直接WebFetchが403で拒否されたため、WebSearchのスニペット経由での間接的な内容確認にとどまる。
- **限界・見つからなかったこと**:
  - **§3の所要時間の定量データは実質的に見つからなかった**。Reddit等の一次体験談への複数回の検索を試みたが有効な結果を得られず、これは「存在しない」ことの証明にはならない(検索ツールの索引の限界の可能性がある)。
  - Bloomの閾値を「具体的にどんな数値・手順で決めるか」という初学者向けのステップバイステップな説明は見つからなかった。見つかったのは断片的な指針(HDRなら閾値≈1、smoothstepでtoeカーブを作る)のみ。
  - [www.arakin.dyndns.org](http://www.arakin.dyndns.org/glsl_cartoon.php)(GLSLカートゥーンレンダリング解説として検索に出てきたサイト)はDNS解決に失敗しており、**サイト自体が消失している可能性が高い**。参照先候補から除外した。
  - LearnOpenGLの日本語全訳サイトの不在は、複数の検索クエリで確認したが、非公開のサイトが存在する可能性までは排除できない。