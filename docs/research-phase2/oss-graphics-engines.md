# OSSゲームエンジン グラフィックス力 調査レポート(Godot以外)

## 結論(格付けサマリー)

グラフィックス力の証拠の厚さ・ライセンスの健全性・C++初学者への現実性を総合すると、優先順位は次の通り。

| 順位 | エンジン | ライセンス | 言語 | グラフィックス力の証拠 | C++学習教材としての現実性 |
|---|---|---|---|---|---|
| 1 | **Wicked Engine** | MIT(真のOSS) | C++17 | 検証済み: SurfelGI/DDGI/VXGI/RTDiffuseなど複数GI手法、HW RT、開発者の技術ブログが濃い | ◎ 個人開発の一貫したC++コードベース、外部依存最小、最も現実的 |
| 2 | **O3DE** | Apache2.0 or MIT(真のOSS) | C++/Python/Lua | 検証済み: Atomレンダラーの実時間RTGI(Diffuse Probe Grid)、GDC 2022デモ | △ AAA級の巨大コードベースで初学者には非常に重い |
| 3 | **Google Filament(追加候補)** | Apache2.0(真のOSS) | C++ | 検証済み: PBR実装解説ドキュメントが業界標準級に高評価 | ○ フルゲームエンジンではないがPhong→PBRの次段階の副教材として有力 |
| 4 | **Bevy** | MIT/Apache-2.0デュアル(真のOSS) | **Rust**(C++ではない) | 検証済み: 2025年にReSTIR方式の実時間レイトレGI「Solari」を実装、技術ブログが極めて詳細 | △ 技術解説は一級品だが言語がRustなのでコードは直接参考にしにくい |
| 5 | **Ogre-Next(追加候補)** | MIT(真のOSS) | C++ | 検証済み: PBR/PBS、HDR実世界単位対応 | ○ 古典的エンジン構造の学習に向く可能性(状況証拠) |
| 6 | **Stride** | MIT(真のOSS) | **C#**(C++ではない) | 検証済み: PBRマテリアル、GDC 2015登壇歴(旧Xenko時代) | △ 言語ギャップあり、GDC情報がやや古い |
| 7 | **Fyrox(追加候補)** | MIT(真のOSS) | **Rust** | 検証済み: PBR、deferred、volumetric lighting | △ 言語ギャップあり |
| 8 | **Flax Engine** | **独自EULA(非OSS/ソース公開型)** | C++/C# | 検証済み: DDGI、SSR、透明面の深度バッファRTなど実装は強い | ○ コードは読めるが「OSSではない」点に要注意 |

---

## 1. Wicked Engine(turanszkij氏)

- **リポジトリ**: https://github.com/turanszkij/WickedEngine (約7.2k stars)
- **ライセンス**: MIT — 検証済み(GitHubリポジトリのライセンス表記より)
- **言語/規模**: C++17、外部依存最小。DirectX12(Windows既定)/Vulkan(Linux)/Metal対応

**(1) 実装済みレンダリング技術**
PBR(複数BRDF)、IBL、環境プローブ、そして複数のGI手法 — **SurfelGI、DDGI(Dynamic Diffuse GI)、VXGI(Voxel GI)、RTDiffuse(レイトレース拡散GI)**、ハードウェアレイトレースによるライトマップベイク(1フレーム1サンプルで蓄積)、SSGIも追加実装済み。
- 根拠: https://80.lv/articles/the-code-oriented-and-lightweight-wicked-engine-now-supports-ssgi
- 公式ドキュメント: https://github.com/turanszkij/WickedEngine/blob/master/Content/Documentation/WickedEngine-Documentation.md

**(2) ライセンス**: MIT。学習・改変・商用利用に法的制約なし。

**(3) 言語とコード規模・読みやすさ**: C++17単一開発者(turanszkij氏)中心のプロジェクトで、外部依存がほぼなくスタイルが一貫している(状況証拠: ドキュメント記載)。「Wicked Engineのソースコードを読む」というYouTube動画も存在し、コード読解対象として一定の知名度がある。
- 根拠: 検索結果内 "Let's read the Wicked Engine source code - YouTube"

**(4) 開発者の発信**: 開発者turanszkij氏自身が技術ブログ(devblog)でSSGIやVXGIなど個々の技術を詳細に解説しており、「コードと一緒に読める解説」が豊富。
- https://wickedengine.net/category/devblog/(2024年に旧wordpressブログから移行)
- itch.io: https://turanszkij.itch.io/wicked-engine
- 日本語カバレッジ(サンプルプロジェクト公開のニュース、2026年6月): https://gamemakers.jp/article/2026_06_22_139986/
- SIGGRAPH/GDC/CEDEC単独登壇は検索では確認できず(**未検出**。個人開発者エンジンのため公式カンファレンス発表がない可能性が高い)

**(5) C++初学者としての現実性**: 対象5エンジンの中で最も現実的。Visual StudioでF5ビルド、Lua scriptingという低摩擦な入口もある。ただしDX12/Vulkanレベルの低レベルAPIが前提であり、GI手法自体は上級内容。

---

## 2. O3DE(旧Amazon Lumberyard、Atomレンダラー)

- **リポジトリ**: https://github.com/o3de/o3de (約9.6k stars, 2.5k forks)
- **ライセンス**: Apache 2.0 または MIT の選択制 — 検証済み(https://github.com/o3de/o3de/blob/development/LICENSE_APACHE2.TXT)

**(1) 実装済みレンダリング技術**: Atomレンダラーは物理ベースレンダリング、Vulkan/DirectX12(Metalは開発中)でのハードウェアレイトレーシング対応。目玉機能は**Diffuse Probe Grid**による実時間レイトレースGI(材質・光源が動的に変化してもリアルタイムで間接照明が再計算される)。
- https://www.docs.o3de.org/docs/atom-guide/features/
- https://docs.o3de.org/docs/user-guide/components/reference/atom/diffuse-probe-grid/
- GDC 2022でImagination Technologies社が制作した実時間RTGIデモ「BuonGIorno」で実証(ただし制作主体はO3DE公式チームではなくパートナー企業): https://docs.o3de.org/blog/posts/imagination-o3de-ray-tracing/ / https://blog.imaginationtech.com/the-making-of-the-imagination-o3de-ray-tracing-gdc-2022-demo

**(2) ライセンス**: Apache 2.0/MIT選択制で真のOSS。手数料・商業義務なし。

**(3) 言語とコード規模**: C++主体(+Python、Lua)。OpenHubのCOCOMOモデル推定で**786人年相当の開発工数**(2021年3月の最初のコミット以降)— AWS Lumberyard由来の商用AAAエンジンがベースであり、非常に大規模。
- https://openhub.net/p/open_3d_engine
- ビルドにはVisual Studio 2019/2022、CMake 3.24+、Git LFS、Wwise Audio SDKなど重量級ツールチェーンが必要(状況証拠として初学者には高いハードル)。GitHub上での明示的な「読みやすさの評判」コメントは検索で発見できず(**未検出**)。

**(4) 開発者の発信**: O3DE公式ブログ(docs.o3de.org/blog)、2021年7月22日GDCパネル(エンジン発表イベント寄り)。2025年振り返り記事で四半期あたり活動的コントリビューター59名、継続率58%と活動は健全。
- https://o3de.org/a-look-back-at-2025-and-what-to-look-forward-to-in-2026/

**(5) C++初学者としての現実性**: △。技術力は本物だが、AAAスタジオ由来の巨大コードベース・重いビルド環境のため、初学者が「読んで学ぶ」対象としては非常に重い。上級編で部分的にAtomレンダラーのGI実装だけを参照する、といった使い方が現実的(推測)。

---

## 3. Stride(旧Xenko)

- **リポジトリ**: https://github.com/stride3d/stride (約7.3〜7.8k stars、検索結果内で表記に若干のブレあり)
- **ライセンス**: MIT — 検証済み

**(1) 実装済みレンダリング技術**: PBRレイヤードマテリアルエディタ、フォトリアリスティックなポストエフェクト(Depth of Field, Bloom, Lens Flare, Tone Mapping等)、柔軟なレンダリングコンポジター、Direct3D/Vulkanバックエンド。
- https://www.stride3d.net/blog/gdc-2015-scene-editor-new-rendering/

**(2) ライセンス**: MIT、.NET Foundation支援下の真のOSS。

**(3) 言語とコード規模**: **C#**(C++ではない)。モジュール性が高いとされる。

**(4) 開発者の発信**: GDC 2015で旧名Xenko時代に「An overview of Xenko」「Advanced Rendering」「Beyond Uber Shaders(Xenko Shading Language)」の3セッションで登壇 — 検証済みだが2015年時点の情報でやや古い。現在はDiscord/YouTubeでのコミュニティ活動が中心(状況証拠)。

**(5) C++初学者としての現実性**: △。レンダリングパイプラインの設計思想は参考になり得るが、言語がC#であるためユーザーの現在の学習対象(C++)への直接転用は難しい。

---

## 4. Bevy(Rust)

- **リポジトリ**: https://github.com/bevyengine/bevy (約47.7k stars, 4.8k forks — 今回調査した中で圧倒的に最大)
- **ライセンス**: MIT または Apache-2.0 のデュアルライセンス(2021年に再ライセンス化) — 検証済み: https://github.com/bevyengine/bevy/pull/2509

**(1) 実装済みレンダリング技術**: wgpu上に構築されたECSベースの2D/3Dレンダラー。特筆すべきは2025年9月リリースのBevy 0.17で導入された**bevy_solari** — ReSTIR DI/GIとワールド空間イラディアンスキャッシュを組み合わせた、静的ベイク不要の完全動的ハードウェアレイトレースGI/直接照明システム。NVIDIA DLSS Ray Reconstructionにも対応。0.18でも継続開発中(2025年12月時点)。
- https://bevy.org/news/bevy-0-17/
- https://jms55.github.io/posts/2025-09-20-solari-bevy-0-17/
- https://jms55.github.io/posts/2025-12-27-solari-bevy-0-18/

**(2) ライセンス**: MIT/Apache-2.0デュアルで真のOSS(Rustエコシステムの標準形式)。

**(3) 言語とコード規模**: **Rust**(C++ではない)。開発初期段階で破壊的API変更が約3ヶ月ごとに発生する旨がREADMEに明記。

**(4) 開発者の発信**: コントリビューターjms55氏の個人技術ブログがSolariの実装(ReSTIR、ワールドキャッシュ、デノイズ)を非常に詳細に解説 — 「コードと一緒に読める解説」としては今回調査した中で最も充実。日本語メディアでも各バージョンのレンダリング機能追加が継続的に報じられている。
- https://gamemakers.jp/article/2025_04_30_101293/(v0.16)
- https://gamemakers.jp/article/2026_01_23_129074/(v0.18)

**(5) C++初学者としての現実性**: 技術解説の質は最高水準だが、コード自体はRustでありC++との言語ギャップがある。「実装を読んで真似る」には不向きだが「最新GI手法の解説記事として読む」用途では非常に価値が高い(判断: 用途を分けて使うのが妥当)。

---

## 5. Flax Engine ※ライセンス要注意

- **リポジトリ**: https://github.com/FlaxEngine/FlaxEngine (約7k stars)
- **ライセンス**: **Flax Engine End User License Agreement(独自EULA)** — 検証済み。MIT/Apacheのような標準OSSライセンスではない。
  - 根拠: https://github.com/FlaxEngine/FlaxEngine/blob/master/LICENSE.md、詳細条項 https://flaxengine.com/licensing
  - 具体条件: ソースコードはGitHubで公開されており**学習目的の閲覧は可能**。ただし商用リリース時、四半期あたり25万ドルを超える収益に対し**4%のロイヤリティ**が発生する契約。再配布・改変配布の制限も一般的なOSSより強い可能性が高い(EULAの再配布条項までは今回未確認)。
  - **結論: 「ソースが読めるだけの独自ライセンス(source-available)」であり、真のOSSではない。** 就活作品の参考としてコードを「読む」分には問題ないが、コードの流用・再配布には注意が必要。

**(1) 実装済みレンダリング技術**: DDGI(独自ソフトウェアレイトレースベースの動的GI)、Hi-Zバッファベースの高速化されたSSR、Box Projection対応の環境プローブ、透明面向けの深度バッファピクセル単位レイトレース反射(水たまり等のリアリズム向上)。
- https://docs.flaxengine.com/manual/graphics/lighting/gi/realtime.html
- https://flaxengine.com/features/visuals/

**(3) 言語とコード規模**: C++(コア) + C#(エディタ/スクリプティングAPI)。

**(4) 開発者の発信**: 公式ブログ「Flax Facts」を月次更新(2026年7月時点でも継続 — https://flaxengine.com/blog/flax-facts-35/ )。SIGGRAPH/GDC/CEDEC等の公式カンファレンス発表は検索で確認できず(**未検出**)。

**(5) C++初学者としての現実性**: コードとドキュメントは充実しているが、ライセンスがOSSでない点が今回の目的(「証拠を示しながらOSSを参考にした」と就活で説明する場面)においてはマイナス材料になり得る(判断)。

---

## 追加調査したエンジン

### Ogre-Next(OGREの後継)
- **リポジトリ**: https://github.com/OGRECave/ogre-next — MIT — 検証済み
- PBR/PBS、HDR(ルーメン・ルクス・EV値など実世界単位対応、DCCツールとのワークフロー整合を重視) — 根拠: https://ogrecave.github.io/ogre-next/api/3.0/_p_b_s_changes_in30.html
- C++。2001年発のOGRE系譜で、モダンなGIやレイトレースGIの証拠は今回の検索では確認できず(Wicked EngineやO3DEほど最先端GI技術の証拠は強くない)。2026年1月時点で最新安定版14.5.2が出ており開発は継続中。

### Fyrox(旧rg3d、Rust)
- **リポジトリ**: https://github.com/FyroxEngine/Fyrox — MIT — 検証済み(https://github.com/FyroxEngine/Fyrox/blob/master/LICENSE.md)
- Deferred shading、PBR、ライトマッピング、ボリューメトリックライティング、ソフトシャドウ
- **Rust**製のためC++初学者には言語ギャップあり。「Rustで唯一実用的なビジュアルエディタを持つ3Dエンジン」との評価あり(状況証拠、2026年時点のMedium記事)。

### Google Filament(フルゲームエンジンではなくレンダリングライブラリ)
- **リポジトリ**: https://github.com/google/filament — Apache 2.0(真のOSS) — 検証済み
- C++製。PBR実装の解説ドキュメント"Physically Based Rendering in Filament"( https://google.github.io/filament/Filament.md.html )は業界でしばしば参照される高品質な技術文書として知られる。
- **注意**: ゲームエンジンそのものではなくレンダリングエンジン/ライブラリ(シーングラフ、物理、オーディオ等は含まない)。ユーザーの現在のPhong実装の「次のステップ(PBR)」を学ぶ副教材としては非常に有力だが、今回の依頼スコープ(フル機能OSSゲームエンジン)には厳密には該当しない点を明記しておく。

### (参考・スコープ外)TheCherno氏のHazel Engine
検索中に発見。C++でのゲームエンジン自作をYouTube動画とコードが1対1対応する形で解説する教育目的の個人プロジェクト(https://github.com/TheCherno/Hazel )。「評判の高いグラフィックスOSSエンジン」ではなく教材寄りのプロジェクトのためランキング外の参考情報として記載。ユーザーの現在の立場(C++初学者、自作レンダラー制作中)に近い視点のコードとして親和性は高いかもしれない(推測)。

---

## CEDEC等、日本語圏カンファレンスでの言及について

検索した範囲では、CEDECで単独セッションとして扱われていたのはGodot(CEDEC2025「Godotからの教訓」、CEDEC2026でのGodot Foundation共同登壇)のみであり、今回調査した5エンジン+追加候補については**CEDECでの単独セッション登壇は確認できなかった**(未検出)。ただし日本のゲーム開発者向けメディア「ゲームメーカーズ」がWicked EngineとBevyのアップデートをニュースとして継続的に報じている点は確認できた。
- https://gamemakers.jp/article/2026_06_22_139986/(Wicked Engine)
- https://gamemakers.jp/article/2025_04_30_101293/ 、 https://gamemakers.jp/article/2026_01_23_129074/(Bevy)

---

## 確信度と限界

| 項目 | 確信度 | 備考 |
|---|---|---|
| 各エンジンのライセンス種別(MIT/Apache/独自EULA) | **検証済み** | 公式リポジトリのLICENSEファイル・公式ライセンスページを直接確認 |
| Wicked EngineのGI手法一覧(SurfelGI/DDGI/VXGI/RTDiffuse) | **検証済み** | 80.lv記事、公式ドキュメントで確認。ただし一次情報の公式Feature一覧ページは今回未直接閲覧のため、記事経由の情報である点に留意 |
| O3DEのDiffuse Probe Grid(実時間RTGI) | **検証済み** | 公式docs.o3deで確認 |
| BevyのSolari(2025年実装) | **検証済み** | 公式ニュース記事+開発者本人のブログで確認。かなり新しい機能(2025年9月〜)のため今後も仕様変更の可能性あり |
| 各エンジンのGitHubスター数 | **検証済みだが変動する** | 調査時点(2026年8月)のスナップショット。日々変動する |
| O3DEの「786人年」規模推定 | **状況証拠** | OpenHubのCOCOMOモデルによる自動推定であり、実際の開発工数と一致するとは限らない。ただし「大規模である」という定性的結論の裏付けとしては妥当 |
| 各エンジンの「コードの読みやすさ」の評判 | **状況証拠〜推測が混在** | Wicked Engineは「読解動画が存在する」という直接証拠があるが、O3DE/Stride/Ogre-Nextについては明示的な「読みやすさ評」を示すRedditやHacker Newsの投稿を検索で発見できなかった(未検出)。ビルド環境の重さから「初学者に難しい」と推論した箇所は推測であることを明記する |
| SIGGRAPH/GDC/CEDEC発表の網羅性 | **限界あり** | 検索エンジン経由の調査であり、各カンファレンスの公式アーカイブを個別に精査したわけではない。「見つからなかった」ことは「存在しない」ことの証明にはならない点に注意(特にGDC Vaultなど有料/会員限定アーカイブは未確認) |
| Flax EngineのEULA再配布条項の詳細 | **限界あり** | ロイヤリティ条件(四半期25万ドル超で4%)は確認できたが、「ソースコードの改変版を再配布してよいか」等の詳細条項までは今回のWebFetchでは取得できなかった。厳密な法的判断が必要な場合は https://flaxengine.com/licensing の全文および実際のEULA文書を直読することを推奨 |
| Filamentのフル機能ゲームエンジンとしての位置づけ | **検証済み(注記あり)** | Filamentはレンダリングエンジン/ライブラリであり、シーン管理・物理・オーディオ等を含むフルゲームエンジンではない点は公式説明から明確。依頼スコープ外である可能性を明記した上で参考掲載した |

**総括**: 「グラフィックスの技術的証拠の厚さ」で最も強いのはWicked Engine(個人開発ながら複数のGI手法を実装し、開発者本人が技術解説を書いている)とO3DE(AAA企業由来のAtomレンダラー)。ただしC++初学者が実際に読んで学ぶ現実性まで含めると、**Wicked Engineが最もバランスが良い**(判断)。BevyはRust製だが技術解説の質は随一で、「実装は読まないが解説記事だけ読む」使い方であれば非常に有力。O3DEは技術力は本物だがコードベースの規模とビルド環境の重さから、参照は上級フェーズ以降に限定するのが妥当と考える(推測)。