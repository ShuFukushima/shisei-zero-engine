# 調査レポート: Dear ImGui導入 / Blinn-Phong / ガンマ補正 / スカイボックス / ノーマルマッピング

## §1 教材マップ(順番つき)

前提: フェーズ1完了時点(Phong照明、自作Vec3/Mat4、自作OBJパーサー、シャドウマッピング着手/未完)。

| # | 章 | URL | 前提章 | GLM/他ライブラリ依存度 |
|---|---|---|---|---|
| 1 | Cubemaps(スカイボックス) | https://learnopengl.com/Advanced-OpenGL/Cubemaps | Advanced-OpenGL/Depth-testing、基本のカメラ/ビュー行列 | 中。`glm::mat4(glm::mat3(view))`でビュー行列から平行移動成分を除去する処理を自作Mat4で書き直す必要(4列目のtranslation要素をゼロにする自前関数を1個足せば済む規模)。キューブマップ読込ループの`GL_TEXTURE_CUBE_MAP_POSITIVE_X + i`はGLM無関係の素のOpenGL API |
| 2 | Advanced Lighting(Blinn-Phong) | https://learnopengl.com/Advanced-Lighting/Advanced-Lighting | Lighting/Basic-Lighting、Getting-started/Shaders | 高いが軽量。`normalize()`, `dot()`, `pow()`, `reflect()`のみで、Phong実装で既に自作済みの内積・正規化の延長線上。halfwayベクトル(`normalize(lightDir+viewDir)`)を追加するだけなので実装コストは小さい |
| 3 | Gamma Correction | https://learnopengl.com/Advanced-Lighting/Gamma-Correction | Getting-started/Textures(テクスチャ読込) | ほぼゼロ。本文にGLMへの言及なし。シェーダー内`pow(color, vec3(1.0/2.2))`とC++側`glEnable(GL_FRAMEBUFFER_SRGB)`/`glTexImage2D(..., GL_SRGB, ...)`が中心で、自作数学ライブラリへの影響はほぼない |
| 4 | Normal Mapping | https://learnopengl.com/Advanced-Lighting/Normal-Mapping | Model-Loading系(自作OBJパーサー)、Advanced Lighting(Blinn-Phong)、Getting-started/Coordinate-Systems | 最重量級。`normalize()`, `cross()`, `transpose()`, `mat3`コンストラクタに依存。TBN行列構築`mat3 TBN = mat3(T,B,N)`とGram-Schmidt再直交化`T = normalize(T - dot(T,N)*N)`は自作Vec3/Mat3への全面書き直しが必須 |
| (独立) | Dear ImGui導入 | https://github.com/ocornut/imgui (`docs/BACKENDS.md`, `examples/example_glfw_opengl3/`) | 特になし(GLFW+OpenGLの基本セットアップのみ) | なし。ImGuiは独自の`ImVec2`/`ImVec4`を使い、GLMともユーザー自作数学とも無関係 |

Cubemapsのみ「Advanced OpenGL」セクション、残り3章は「Advanced Lighting」セクション。LearnOpenGL公式の章立てでもCubemapsはAdvanced Lightingセクションより手前にあるため、この順番は公式の並びとも一致する(検証済み、各ページを直接WebFetchで確認)。

## §2 初学者の典型的なつまづき

**1. 共有頂点でタンジェントのハンドネスが食い違い、法線マッピングが特定の三角形だけ崩れる**
症状: スペキュラハイライトが一部の面だけ明らかにおかしい。原因: 「異なるタンジェント空間ハンドネスを持つ三角形は頂点を共有すべきではない」にもかかわらず共有してしまい、`tan1[i1] += sdir`のような累積で「反対のワインディングからsdir/tdirが加算される」ため平均が壊れる。対処: ハンドネスビットを持たせて異なるハンドネスの三角形では頂点を複製する、またはタンジェントかビタンジェントの片方を反転させるだけでも解決する場合がある(kRogue氏の指摘)。
出典: https://community.khronos.org/t/confused-about-handedness-winding-in-tangent-space/61705

**2. UVがミラーリングされたモデルで法線マッピングが反転する**
症状: 左右対称モデル(顔の片側をミラーリングしてUV節約する等)で、片側だけ陰影が反転する。原因: UVを水平/垂直反転すると接空間の片軸が反転し、行列式が負になる(ハンドネス反転)。対処: タンジェントのw成分に±1のハンドネス符号を持たせ、`bitangent = cross(normal, tangent.xyz) * tangent.w`で復元する(MikkTSpaceなど業界標準の方式と同じ)。
出典: https://www.gamedev.net/forums/topic/347799-mirrored-uvs-and-tangent-space-solved/ / https://docs.marmoset.co/docs/tangent-handedness/

**3. TBN行列が非直交になり、法線マッピングにノイズが乗る**
症状: 大きいメッシュで頂点ごとにタンジェントを平均すると、TBNの3ベクトルが互いに垂直でなくなり微妙な陰影の歪みが出る。対処: 頂点シェーダーでGram-Schmidt法により`T = normalize(T - dot(T,N) * N)`として再直交化し、`B = cross(N,T)`で求める。
出典: https://learnopengl.com/Advanced-Lighting/Normal-Mapping(本文で直接言及)

**4. ガンマの二重補正で画面が白飛びする**
症状: 「the image ends up way too bright」。原因: 既にsRGB(モニター編集済み)のテクスチャを`GL_RGB`で読み込んだままシェーダーでさらに`pow(color,1/2.2)`をかけると二重に明るくなる。対処: ディフューズ系テクスチャは読込時に`GL_SRGB`/`GL_SRGB_ALPHA`を指定して自動リニア化させ、法線マップ/スペキュラマップなど元々リニアなテクスチャには指定しない。
出典: https://learnopengl.com/Advanced-Lighting/Gamma-Correction(本文引用)、補強としてhttps://www.gamedev.net/forums/topic/697254-gamma-correction-confusion/ (このスレッドはWebFetchが403で拒否されたため、検索エンジンのスニペット要約に依拠。一次情報を直接確認できていない点に留意)

**5. `glEnable(GL_FRAMEBUFFER_SRGB)`がデフォルトフレームバッファに効かない(プラットフォーム依存)**
症状: `glEnable(GL_FRAMEBUFFER_SRGB)`を呼んでも色が変わらない。原因: GLFWでウィンドウ作成時に`glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE)`を設定していないとデフォルトフレームバッファがsRGB対応にならず、`glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING)`が`GL_LINEAR`のままになることがある。さらにmacOSなど一部環境ではこの組み合わせでも不具合報告あり(Windows+MSVCでの挙動は本調査では未検証)。対処: `GLFW_SRGB_CAPABLE`を明示的に立てるか、確実性を優先してシェーダー内で手動`pow()`する方式にする。
出典: https://github.com/glfw/glfw/issues/978 、検索エンジン要約 https://gamedev.net/forums/topic/704616-gl_framebuffer_srgb-and-framebuffer/ (こちらも直接WebFetchはしていない)

**6. ガンマ補正(sRGBフレームバッファ)を有効にするとDear ImGuiのUIが不自然に明るくなる**
症状: 「everything looks wrong」、UIのボーダーやボタンが過度に明るい。原因: ImGuiの頂点カラーはsRGB値で渡されるが、`GL_FRAMEBUFFER_SRGB`有効時は出力段で自動的にsRGB圧縮がかかるため二重補正になる。対処: ImGui用シェーダーの頂点カラーを`Frag_Color = vec4(pow(Color.rgb, vec3(2.2)), Color.a)`のように先にリニア化しておく。著者は「This simple solution works for Vulkan as well」と述べている。
出典: https://tuket.github.io/posts/2022-11-24-imgui-gamma/ (2022-11-24付。ImGuiのバージョンにより挙動が変わっている可能性は未検証)

**7. スカイボックスが他オブジェクトより手前に描画されてしまう**
症状: スカイボックスが1×1×1の立方体のため、深度テストの初期状態(`GL_LESS`)だと他の全オブジェクトを覆い隠す。対処: スカイボックス描画時のみ深度関数を`GL_LEQUAL`に変更し、頂点シェーダーで`gl_Position = pos.xyww`として深度値を常に1.0にするテクニックを使う。
出典: https://learnopengl.com/Advanced-OpenGL/Cubemaps(検索エンジン要約による、本文の該当箇所はページ全体を直接読めていないため状況証拠)

**8. Dear ImGuiのOpenGLローダーがプロジェクト側のGLADと衝突してビルドエラーになる**
症状: `imgui_impl_opengl3.cpp`が独自のGLローダー(`imgui_impl_opengl3_loader.h`)を内蔵しており、GLAD等と関数シンボルが重複してリンクエラーになることがある。対処: `imconfig.h`またはコンパイラのコマンドラインで`IMGUI_IMPL_OPENGL_LOADER_CUSTOM`を定義し、自前のGLADローダーを使わせる。ただし新しめのバージョンではImGui内蔵ローダーが「どのGLローダーを使っていても共存できる」設計に変わっているため、バージョンによって挙動が異なる。
出典: https://github.com/ocornut/imgui/issues/4810 、https://github.com/ocornut/imgui/issues/2178 、https://github.com/ocornut/imgui/issues/4445

## §3 所要時間の実例

正直に書くと、**この調査では「初学者/経験者がこの5テーマにどれくらいの時間を要したか」という具体的な体験談をほぼ発見できませんでした。**

見つかった唯一近いものは、ストックホルム大学のゲーム開発学生William de Try氏によるdevlogシリーズ(https://dev.to/noticeablesmeh/opengl-catching-up-week-1-1k8o、初学者)で、「週次で進めている」ことは分かりますが、記事はGetting Startedレベル(ウィンドウ作成・シェーダー・テクスチャ基礎)までしか書かれておらず、Blinn-Phong/ガンマ補正/ノーマルマッピング/スカイボックス/ImGuiへの具体的な所要時間の記述はありませんでした。

Reddit(r/opengl, r/GraphicsProgramming等)を複数のクエリで検索しましたが、該当する体験談スレッドを検索エンジン経由では発見できませんでした(Redditの検索インデックスがWeb検索に十分反映されていない可能性があります)。憶測で時間数を書くことはしません。

## §4 日本語の補助資料

| 資料 | 評価 |
|---|---|
| [LearnOpenglをモダンにしてみた話](https://qiita.com/kkkjp/items/cd79704b479d4fdb3e7e) (Qiita) | シャドウ・ノーマルマッピング・HDR・Bloom・Deferred Shading・SSAOを1シーンに統合実装した記録。**上級者向け**で「LearnOpenGLを読めば分かる」前提の記述が多く、GLM使用が前提(自作数学ライブラリへの言及なし)。初学者の入門にはならないが、「複数の技法を組み合わせた時だけ出るバグ」への言及は将来役立つ可能性 |
| [imguiのサンプルを動かす](https://qiita.com/sukakako/items/6ba6631e0c58754500cb) (Qiita, 2016年公開) | GLFW+ImGui(opengl2_example)をWindows/CMakeで動かす手順。**8年以上前の情報で古い**。ImGuiは頻繁に破壊的変更が入るため現行バージョンにそのまま使えない可能性が高い。リンカエラー対応など断片的なデバッグ知識のみ参考になる |
| [3DCGにおいて表面のディテールを表現する技術](https://qiita.com/sakana_hug/items/c1c24b6ac5d584048296) (Qiita, 2024-06-09) | バンプ/ノーマル/ノーマルブレンドを比較する**概念解説記事**。初学者に非常にわかりやすい構成だが、タンジェント空間への言及・コード例は一切なし。「なぜ必要か」の導入に向くが実装の助けにはならない |
| [ガンマ補正のうんちく](https://qiita.com/yoya/items/122b93970c190068c752) / [PNG画像のガンマ補正](https://qiita.com/yoya/items/ce8dffc8a8a19746d87c) (Qiita) | sRGBカーブの数学的定義(低輝度でlinear、高輝度でgamma2.4)など背景理解に有用。OpenGL固有の`GL_FRAMEBUFFER_SRGB`等の実装手順への言及は確認できていない(推測: 汎用的なガンマ補正解説であり、OpenGL API寄りではない) |
| [ガンマ色空間、リニア色空間とsRGB](https://qiita.com/sshuv/items/87ea929e1a62c2f47503) (Qiita) | Unity前提の解説。概念(ガンマ/リニアの切替設定)は流用できるがAPIはOpenGLと異なる |
| [そろそろShaderをやるパート74 Phong鏡面反射](https://zenn.dev/kento_o/articles/fc4c8773a0b752) (Zenn) | Phong鏡面反射の解説記事。Blinn-Phongとの差分(halfwayベクトル)への直接言及は本調査では確認できず |
| [WebGLでキューブマッピング](https://qiita.com/aa_debdeb/items/57b805691efa27ff905e) (Qiita) | WebGL(JavaScript)向けだが、6面キューブマップの読込構造はOpenGLと共通の考え方で参考になる |

**発見できなかったもの(正直な報告)**: 自作OBJパーサーへのタンジェント計算追加(共有頂点・ハンドネス)を扱う日本語記事、Dear ImGuiのCMakeへの手動組み込み(vcpkgなし・ソース直置き)を扱う2020年代の新しい日本語記事は、複数のクエリで検索しましたが発見できませんでした。この2点(重点調査①②)は英語一次資料(§1・§2の出典)に頼らざるを得ません。

## §5 学習順序のアドバイス

- **Cubemaps(スカイボックス)を先に片付ける**。自作Mat4に「平行移動成分を除去する」関数を1つ足すだけで済み、後続の重い章(ノーマルマッピング)の前に軽い成功体験を積める。LearnOpenGL公式の章立て(Advanced OpenGL→Advanced Lighting)とも合致する。
- **Blinn-PhongはPhongの延長**なので、既存のPhong実装のspecular項をhalfwayベクトル版に差し替えるだけで完了する軽量な章。ここで詰まる場合は元のPhong実装自体を疑うべき。
- **Gamma CorrectionはGLM依存がほぼゼロ**で数学的には最も軽いが、テクスチャ読込関数の設計判断が後々効いてくる。全テクスチャを一括で`GL_SRGB`化すると法線マップ・スペキュラマップまでリニア色空間が壊れる典型ミス(§2-4)につながるため、**着手前に「このテクスチャはsRGBか否か」を引数やenumで明示する設計にしておく**ことを推奨します(これは一般的なベストプラクティスからの提案であり、特定の一次情報に基づくものではありません)。またGLFW+`glEnable(GL_FRAMEBUFFER_SRGB)`の組み合わせはプラットフォーム依存の不具合報告があるため(§2-5)、確実性を優先するなら最初はシェーダー内`pow()`の手動方式から始め、動作確認後に余裕があれば`glEnable`方式へ移行するほうが安全です(提案)。
- **Normal Mappingは今回の中で最重量級**であり、重点調査①への回答として: 着手前に「タンジェントはどの単位(面ごとか、共有頂点で平均するか)で計算し、ハンドネスの食い違う頂点をどう扱うか」を先に設計判断として決めておくべきです。自作OBJパーサーが既に法線を共有頂点で平均化する仕組みを持っているなら、タンジェントも同じ仕組みに乗せられますが、UVがミラーリングされたモデルを扱う予定があるならタンジェントのw成分にハンドネス符号を持たせる設計(§2-2)を最初から入れておかないと後から総取り替えになります。
- **Dear ImGuiの導入タイミングはLearnOpenGL本編と独立**しているため、いつ入れても学習上の支障はありません。ただし、Gamma CorrectionやNormal Mappingのようにパラメータ(ガンマ値、法線強度など)を数値で調整しながら見た目を確認したい章に入る前に導入しておくと、スライダーでリアルタイム調整しながらデバッグできて効率が良くなります(これは筆者の提案であり、出典のある一次情報ではありません)。

## §6 確信度と限界

- **検証済み(高確信度)**: LearnOpenGL各章のURL・内容・GLM依存箇所(§1)は公式サイトを直接WebFetchして確認。タンジェント計算のハンドネス/共有頂点問題(§2-1,2)はKhronosフォーラム・Lengyelの原典・GameDev.netで複数一致。ガンマの二重補正とImGuiとの相互作用(§2-4,6)は一次ソース本文から直接引用できている。
- **状況証拠(中確信度)**: GameDev.netのガンマ補正スレッド(§2-4)とスカイボックスのGL_LEQUALスレッド(§2-7)、GLFW_SRGB_CAPABLEの不具合報告(§2-5)は、WebFetchが403で拒否されたか、検索エンジンのスニペット要約経由でしか確認できていません。一次ページを直接読めていない点に注意してください。
- **弱い/未検証**: Dear ImGui公式リポジトリに実際にCMakeLists.txtが同梱されているかは確認できておらず(ディレクトリ一覧のみ取得できファイル内容は読めなかった)、複数のブログ記事(decovar.dev等)の再構成に基づく情報です。公式は基本的にCMakeを提供せず、ユーザーが自前で書く前提である可能性が高いと考えられます(推測)。
- **明確な限界**: §3(所要時間の実例)はほぼ空振りに終わりました。Reddit・個人ブログでの体験談を複数クエリで探しましたが、この5テーマに特化した時間の言及を発見できませんでした。§4の日本語資料も、重点調査①(OBJパーサー+タンジェント)②(ImGui CMake手動組み込み)に直接一致する日本語記事は発見できず、英語一次資料への依存度が高い結果になっています。
- **Windows+MSVC環境固有の検証はしていません**。GLFW_SRGB_CAPABLEの不具合報告はmacOS中心であり、この学生の実環境(Windows)での挙動は未確認です。