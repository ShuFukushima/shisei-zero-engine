# 大規模C++コードベース(ゲームエンジン)読解方法論 調査レポート

## 結論

英語圏・日本語圏を問わず収束している「王道の流れ」は以下の5段階である。

1. **まずビルドして実際に動かす**(読む前に触る)
2. **デバッガでブレークポイントを張り、1つの操作/1フレームを実行順に追う**(エントリポイントまたは「触った機能」から開始)
3. **機能名・UI文言・API名でgrep検索し、逆引きで実装箇所を特定する**(「影はどこで描いてる?」型の逆引き)
4. **git log/git blameで「なぜこう書かれているか」という歴史的経緯を読む**
5. **README/ARCHITECTURE.md/ディレクトリ構成など設計文書があれば、コードより先に読む**

グラフィックス(レンダリング)部分に特有の技術として、**RenderDoc等のGPUフレームキャプチャツールで1フレームの描画イベント列を可視化し、コールスタックのシンボル解決でC++ソースの呼び出し箇所に逆引きする**手法が業界標準として確立している。

「写経ではなく読解」に昇華する方法として、英語圏では**Learn by Porting(他言語/自分のエンジンへ機能を移植しながら読む)**、日本語圏では**「読んだ処理をフロー図・依存関係図に描き起こす」**という手法が独立に何度も推奨されており、一致度が高い。

ゲームエンジンのソースコード読解の"お手本"として最も繰り返し言及されるのは**Fabien Sanglard氏**(DOOM/Quake/Wolfenstein 3Dのコードレビュー記事群、Game Engine Black Bookシリーズ)である。

---

## 1. 定番の手法(build→run→debug→逆引き→git史→設計文書)

| 手法 | 出典が推奨する具体的内容 | 出典 |
|---|---|---|
| まず動かしてから読む | 「Start with an existing feature」実際に動く機能をUIから触り、そこを起点にする | Nicolas Carlo, *Dive into an unfamiliar codebase from its edges*, https://understandlegacycode.com/blog/dive-into-an-unfamiliar-codebase-from-its-edges/ |
| デバッガ+ブレークポイントで1操作を追う | 「follow the code execution to discover more code」ブレークポイントを置き、データの変化を見ながら段階的に進む | 同上 |
| デバッガでコールスタックを読む(UE4例) | 「ブレークポイント設定後、コールスタックを読み各フレームで周囲の関数を理解する」実際のエンジンが書いたコードを"生きたガイドツアー"として読む | Tristan Soliven, *Large Unreal Engine Codebase: A Practical Guide*(2026-05-11), https://www.wholetomato.com/blog/finding-your-way-large-unreal-engine-codebase/ |
| エントリポイントから読む | 「I start from invocation and go deeper. This could be main, or perhaps specific API endpoints.」 | Hacker News, *Ask HN: How do you learn to read large code bases?*, https://news.ycombinator.com/item?id=30754269 |
| 機能名から逆引き(「縦に1本通す」) | Step1外形把握(README/設定/CI)→Step2エントリポイントgrep→**Step3「中心的なユースケースを入口から出口まで寄り道せず1本たどる」**(例: 検索機能ならControllerから追う)→Step4変更対象周辺を深掘り | Qiita @kotaro_ai_lab, *初めて入る現場の巨大なコードベースの読み方*(2026-07-30), https://qiita.com/kotaro_ai_lab/items/d3528b88f30512e67731 |
| 「なぜ今これを読むか」目的を先に決める | 目的を明確化→UI要素/文言でgrep→入力欄→関数呼び出し→データ更新→画面表示を矢印でつなぎフロー図化 | Qiita やむぅ。, *8年目エンジニアが初見のコードを読むとき頭の中でやっていること*(2026-08-11), https://qiita.com/yamu_official/items/ce22b41b7fbf52239f85 |
| 動的→静的の順で読む | 「動的にコードを追う(デバッガ・ログ確認)→静的に読む」の順が理想。静的読解では変数宣言・例外処理は飛ばし関数名から処理を推測 | Qiita masakun1150, *コードリーディング-膨大なソースを解析するためのTips*(2020-05-23), https://qiita.com/masakun1150/items/b82b5da8142270268b59 |
| git log/blameで歴史を読む | 「gitの履歴(git log、git blame)でコード内に記録されていない『なぜ』を把握」/ git blameでPRの意図・コミット背景を確認 | Qiita @kotaro_ai_lab(同上); Zenn okmttdhr, *大規模なソースコードを理解する*(2021-04-30), https://zenn.dev/okmttdhr/articles/78af1e87cbd9d0 |
| もっとも変更頻度の高いファイルを先に把握 | 「Use version control to identify the most commonly edited files ... these are usually the files doing all the work」 | Hacker News各スレ(要約), https://news.ycombinator.com/item?id=30754269 ほか |
| アーキテクチャ文書・ディレクトリ構成を先に読む | 「ARCHITECTURE.md、wiki、ドキュメントを確認」→全体構造把握→依存関係分析(インターフェイスと責務に注目)の順 | Zenn okmttdhr(同上) |
| 「コードより先に地図を持て」 | アーキテクチャ図がある場合は最優先で読む。ディレクトリ構成だけでも設計思想が読み取れる | Zenn naokky, *フクロウと学ぶアーキテクチャ*, https://zenn.dev/naokky/articles/202511-architecture-journey-intro (検索結果からの要約、未フェッチ=状況証拠) |
| テストコードを読む | 「By reading the tests, you can understand the expected behaviour」 | Sam Jarman, *Navigating Large, Unfamiliar Codebases*(2017-11-19), https://www.samjarman.co.nz/blog/codebases |
| バグ修正/機能追加という具体目標を持つ | 「The only way I've ever been able to learn a code base is by fixing bugs or implementing features.」 | Hacker News(同上) |

---

## 2. ツール

### 2-1. IDEのコードジャンプ

| ツール/機能 | 内容 | 出典 |
|---|---|---|
| Go to Definition / Find All References / Call Hierarchy | VS/VSCode標準機能。「Find All References」は"最も過小活用される機能"と強調される。呼び出し階層(Call Hierarchy)で「誰がこの関数を呼ぶか、その呼び出し元は誰か」を追える | Microsoft Learn, https://learn.microsoft.com/en-us/visualstudio/ide/call-hierarchy?view=visualstudio ; VS Code Docs, https://code.visualstudio.com/docs/cpp/cpp-ide |
| Visual Assist | UEの`UCLASS()`/`UFUNCTION()`マクロを標準IntelliSenseより正確に解析する専用パーサを搭載。UE系の大規模コードベースで推奨 | Whole Tomato(前掲, 2026-05-11) |
| c_cpp_properties.json | VSCodeで大規模/カスタムビルド系プロジェクトの「Go to Definition」が効かない場合、ヘッダ探索パス・コンパイラ設定を明示する必要がある | codegenes.net, https://www.codegenes.net/blog/vscode-go-to-definition-not-working/ |

### 2-2. RenderDoc(GPUキャプチャ→ソースコード対応)

RenderDocはVulkan/D3D11/D3D12/OpenGL/OpenGL ES対応のオープンソースGPUフレームキャプチャデバッガで、業界で広く使われている(https://renderdoc.org/ , https://github.com/baldurk/renderdoc)。

**基本ワークフロー**(公式Quick Start, https://renderdoc.org/docs/getting_started/quick_start.html および Qiita @tsukino_ 2025-12-02入門記事 https://qiita.com/tsukino_/items/ec33fbf7c5b6e96736f7 より):

1. File→Launch Applicationでアプリを起動し、F12(既定のCapture Key)で次フレームをキャプチャ
2. **Event Browser**でフレーム内の描画イベント(ドローコール)を時系列一覧表示
3. **コールスタック→ソース対応**: 対象イベントを選択し、下部の折りたたみパネルでコールスタックを表示。ただし表示には**事前にTools→Resolve Symbolsでシンボル解決が必要**
4. **Pipeline State**でパイプライン各ステージ(頂点シェーダ→ラスタライザ→ピクセルシェーダ等)のフローチャートと状態を確認
5. **Mesh Viewer**でパイプラインを通過する頂点データを生データ/3D表示で検証
6. **Texture Viewer**でピクセルを選択し、シェーダーデバッグ情報が埋め込まれていればステップ実行と変数確認が可能(DirectXでは`#pragma enable_d3d11_debug_symbols`等の指定が必要な場合あり)

「影はどこで描いてる?」を実際に確認する具体手順としては、**該当ドローコールをEvent Browserで探す→Pipeline Stateでどのシェーダが使われているか確認→コールスタックからC++側の発行元コードへ逆引き**、という流れになる。

参考: Zenn/Qiita/noteの日本語RenderDoc記事群(https://qiita.com/emadurandal/items/abe3488300ec9dccf9fe 、https://note.com/rull_arc/n/n6b495b0a6cdf 、https://www.technicalife.net/shader-debugging-with-renderdoc-vulkan/ )も同様の手順を解説している(個別フェッチはしていないため状況証拠扱い)。

### 2-3. Sourcetrail的な可視化ツールの現状

**検証済み**: Sourcetrailは**2021年12月に開発終了**しており、GitHubにアーカイブされ、コミュニティフォークはあるが活発にメンテナンスされていない(Slashdot, https://slashdot.org/software/p/Sourcetrail/alternatives)。

現行の代替候補:

| ツール | 特徴 | URL | 確信度 |
|---|---|---|---|
| Woboq Code Browser | libclangでセマンティック解析し静的HTMLを生成するC/C++専用のWebコードブラウザ。セマンティックハイライト・仮想関数へのジャンプ対応。KDABが`codebrowser.dev`として無償公開、多数のOSS(Qt等)が登録済み | https://codebrowser.dev/ , https://woboq.com/codebrowser.html | 検証済み(直接フェッチ) |
| Understand(SciTools) | 静的解析・依存関係グラフ・コールツリー・UMLクラス図可視化・Hyper-Xref相互参照・AI機能を備えた商用ツール。NASA等が利用 | https://scitools.com/ | 検証済み(直接フェッチ) |
| Source Insight | C/C++/C#等向けの高速コードブラウザ・エディタ。ライブ解析で大規模プロジェクトの理解を支援 | (検索結果からの要約) | 状況証拠 |
| CodeLayers / CodeViz | Sourcetrailの直接的な後継ではないが2026年時点でよく挙がる新興ツール。3D/多言語対応・AI統合(CodeLayers)、VSCode向け可視化マップ(CodeViz) | https://codelayers.ai/blog/complete-guide-code-visualization-2026 | 状況証拠(未フェッチ) |

**結論として**: Sourcetrailに完全に代わる「C++特化・無料・高精度」ツールは2026年時点で存在せず、用途により**Woboq Code Browser(read-only閲覧に強い)**と**IDE標準のコードジャンプ+Understand(有償・高機能)**を組み合わせるのが現実的、という状況。

---

## 3. 著名な実例: Fabien Sanglard氏によるゲームエンジンコードリーディング

**確信度: 検証済み**(本人サイトを直接フェッチ)。Fabien Sanglard氏(https://fabiensanglard.net/)は、id Softwareのゲームエンジン群を対象にした一連のコードレビュー記事・書籍で知られる、ゲームエンジンソース読解の代表的な「お手本」。

| 作品 | 内容 | URL |
|---|---|---|
| DOOM3 Code Review(2012-08-06) | 601,047行のコード規模をclocで定量化し、「テキストより図解で説明する方向へシフトしている」と明言。レンダラーをfrontend/backendの2部構成として図解(`renderer_big_picture.png`)し、専用ページで詳解 | https://fabiensanglard.net/doom3/index.php |
| Doom3 BFG Code Review(2013-05-23) / BFGドキュメント補遺(2013-08-31) | id Tech 4改良版の解説 | https://fabiensanglard.net/doom3_bfg/index.php |
| Quake Code Review(2009-03-09)/Quake2(2011-09-16)/Quake3(2012-06-30) | 各エンジン世代のソースレビュー | https://fabiensanglard.net/quakeSource/ 、/quake2/ 、/quake3/ |
| Wolfenstein 3D for iPhone code review(2009-04-20) | 移植版コードのレビュー | https://fabiensanglard.net/wolf3d/index.php |
| Game Engine Black Book: Wolfenstein 3D 第2版(2018-12-06)/ DOOM(2018-12-10) | 427ページフルカラー。当時のハードウェア制約(80486, VESA Local BUS, DOS Extender, NeXTStation)から説き起こし、John Carmack本人の序文付き | https://fabiensanglard.net/gebbwolf3d/ 、https://fabiensanglard.net/gebbdoom/ |
| シャドウ・ライティング等の個別技術解説 | Shadow Mapping(2008-02-14)、Soft Shadows PCF/VSM(2009-02-14)、Light Scattering(2008-11-25)など、レンダリング技術単体の深掘り記事も多数 | https://fabiensanglard.net/shadowmapping/ ほか |

この一連の記事の**方法論的特徴**は、①コード規模をツール(cloc)で定量化してから読む、②アーキテクチャ全体図を先に描いて掲載する、③機能(レンダラーのfrontend/backend分割など)ごとに専用ページを割いて深掘りする、という構成であり、本調査の(1)(4)で挙げた「アーキテクチャ文書を先に」「図解して理解する」を、著者自身が読者への解説形式として体現している点が示唆的である。

**関連する教育目的の実例**(補助的、確信度は状況証拠〜検証済み混在):
- **TheCherno(YouTube)のHazelエンジン**: 「教育目的」を明言した3Dゲームエンジンで、開発過程が全て動画化・GitHub公開されている。読解学習用のコードレビュー動画もある(検証済み: https://github.com/TheCherno/Hazel 、https://www.youtube.com/watch?v=aWGVjFTbXc8)
- **tinyrenderer(ssloy)**: 「read it and implement your own renderer, only by suffering through all the tiny details will you learn」という明言があり、写経ではなく自作を強制する教育設計(検証済み: https://github.com/ssloy/tinyrenderer/wiki)

---

## 4. 「写経ではなく読解」を学習法に変える方法論

| 手法 | 内容 | 出典 |
|---|---|---|
| **Learn by Porting**(移植による学習) | 「converting code between programming languages is a great way to learn new languages」。他言語/他フレームワークへ実際に機能を移植する過程で、微妙な差異(`!=`と`!==`等)への注意力や双方向の学習効果が得られる。著者は複数のゲーム・オーディオアプリを実際に移植した実例を提示 | Mark Heath, *Learn by Porting*(2022-05-13), https://markheath.net/post/learn-by-porting |
| 読んだ処理をフロー図に描き起こす | 「入力欄→ボタン→関数呼び出し→データ更新→画面表示」を矢印でつなぎフロー図化。「入力・処理・出力」を分解して段階整理 | Qiita やむぅ。(前掲, 2026-08-11) |
| 関連関数・クラスの関係図を作成 | draw.io、Graphvizなどで静的読解の成果を図として残す | Qiita masakun1150(前掲) |
| コードベースの"地図"を自動生成して読む順序を決める | ディレクトリ構造走査→レイヤリング(関連ディレクトリを責務ごとに層として再構成)→依存集中点(ホットスポット)抽出→優先読順リスト作成、という3段階パイプラインをAIエージェント(Claude Custom Skill)で自動化した事例 | RAKSUL Techblog 新開, *コードリーディング革命: コードベースの"地図"を作れ*(2025-12-11), https://techblog.raksul.com/entry/2025/12/11/204458 |
| 各モジュールについて「数段落の説明を書ける」レベルを目標にする | アウトプット(説明文)を書けることを理解度の到達目標とする | Hacker News, *Ask HN: How do you learn to read large code bases?*, https://news.ycombinator.com/item?id=30754269 |
| テストを自分で書いてみる | 「writing [tests] is a great way to get into understanding specific functionality」— 読むだけでなくテストという形でアウトプットする | 同上 |
| 用語辞典を自作する | プロジェクト固有の用語・略語をまとめた辞書を作りながら読む | Qiita masakun1150(前掲) |

これらは表現は違うが、**「読んだ内容を何らかの形(図・移植コード・テスト・要約文)としてアウトプットすることで理解が定着する」**という一点で英語圏・日本語圏の記事が独立に一致している。

---

## 5. 日本語圏(Qiita/Zenn/note)と英語圏(Reddit/HN/ブログ)の対比まとめ

| 観点 | 英語圏の代表的な主張 | 日本語圏の代表的な主張 |
|---|---|---|
| 開始点 | エントリポイント(main/API)から深掘り(HN) | 「外形把握→エントリポイントgrep→縦に1本」の5ステップ(Qiita kotaro_ai_lab) |
| デバッガの位置づけ | "生きたガイドツアー"としてコールスタックを読む(Whole Tomato/UE4記事) | 「動的に追ってから静的に読む」の順序を明言(Qiita masakun1150) |
| 目的意識 | "why does it exist" を先に把握(dev.to/Substack系記事) | 「何をしているかでなく、なぜ今これを読むか」を先に決める(Qiita やむぅ。) |
| 歴史の読み方 | git blame推奨だが「Git Blameだけでは不十分」という批判記事も存在(tekin.co.uk) | git log/blameで「コードに書かれていない"なぜ"」を補う(Qiita/Zenn共通) |
| 可視化 | Sourcetrail後継ツールへの言及は少なく、IDE標準機能中心 | 「地図を作れ」「アーキテクチャ図を先に読め」という比喩表現が複数の独立記事で反復(Zenn naokky, RAKSUL Techblog) |
| CEDEC | — | **「エンジンのソースコード読解方法論」そのものをテーマにしたCEDEC講演は検索で発見できなかった**(後述の限界を参照) |

CEDECに関しては、「ソースコード静的解析の運用」(2011年プログラム系セッション)や「BLUE PROTOCOL: UE4改造によるアニメ表現」(2021年、既存エンジンを読んで機能拡張した実例)など**隣接するテーマの講演は存在するが、「大規模ゲームエンジンのソースコードをどう読み解くか」という方法論そのものを主題にした講演は今回の調査範囲では発見できなかった**。

---

## 調査過程

1. まず英語圏の一般的な「大規模コードベースの読み方」記事群(Sam Jarman、understandlegacycode.com、Whole Tomato)をWebSearchで収集し、上位3件をWebFetchで直接検証。
2. 続けてグラフィックス特有の技術(RenderDoc)をWebSearch→公式Quick Startページと日本語入門記事(Qiita @tsukino_)をWebFetchで検証し、コールスタック→ソース対応の具体手順を確認。
3. Fabien Sanglard氏をWebSearchで確認後、本人サイトのトップページとDOOM3レビュー記事をWebFetchし、記事一覧と解説手法(図解シフト、cloc定量化、frontend/backend分割)を直接検証。
4. 「写経ではなく読解」学習法についてWebSearchで"Learn by Porting"を発見しWebFetchで検証。日本語圏では「図解」系の記事が独立に複数ヒットしたため、代表例(RAKSUL、Qiita やむぅ。、Zenn okmttdhr)をWebFetchで検証。
5. ツール面はSourcetrailの終了状況、Woboq Code Browser、Understand(SciTools)をそれぞれWebSearch→一部WebFetchで検証。
6. CEDECについては複数のクエリ変化(講演、個人開発、静的解析、大規模レガシー)で検索したが、テーマに直接合致する講演は発見できず、隣接する講演のみ確認できた。この点は「見つからなかったもの」として明記。
7. Hacker News上の複数スレッド(Ask HN: 大規模コードベースの読み方関連 全9スレッド発見、うち1件を直接フェッチ)から実務者コメントを収集。

**探して見つからなかったもの**:
- CEDEC/GDCで「ゲームエンジンのソースコード読解方法論」自体をテーマにした講演資料
- Sourcetrailの直接的な後継となる無料・高精度なC++専用可視化ツール(現時点で完全な代替は存在しない)

---

## 確信度と限界

**検証済み(WebFetchで一次情報を直接確認)**:
- Fabien Sanglard氏のサイト構成・DOOM3レビューの解説手法
- RenderDoc公式Quick Startのワークフロー(コールスタック解決にResolve Symbolsが必要な点など)
- Qiita/Zenn各記事の具体的手法(kotaro_ai_lab、masakun1150、shitake4、yamu_official、okmttdhr、RAKSUL Techblog)
- Sam Jarman、understandlegacycode.com、Whole Tomatoの記事内容
- Mark Heathの"Learn by Porting"
- Woboq Code Browser、Understand(SciTools)の機能
- ssloy/tinyrenderer、TheCherno/Hazelの実在とURL

**状況証拠(検索結果のスニペットのみ、一次情報は未フェッチ)**:
- Zenn naokky記事の詳細内容
- Source Insight、CodeLayers、CodeViz、日本語RenderDoc記事(note/はてな等)の詳細
- Hacker News各スレッドの個別コメント全文(要約引用のみ確認)

**推測・不確実な点**:
- 「CEDEC講演でこのテーマがゼロである」という結論は、検索エンジンのインデックス範囲内での不在確認に過ぎず、**CEDiL(CEDEC Digital Library, https://cedil.cesa.or.jp/)の会員限定アーカイブや口頭のみのセッションまでは調査できていない**。過去のCEDEC全講演を横断検索したわけではないため、断定はできない。
- 「Sourcetrailの完全な代替が存在しない」という評価は、Slashdot記事1件の記述に基づいており、他の比較記事との突き合わせは行っていない。
- 日付が古い記事(masakun1150 2020年、Sam Jarman 2017年、Sanglard記事群2008〜2013年)は、ツール名称やIDE機能の細部が現行バージョンと異なる可能性がある。ただし方法論自体(デバッガで追う、図解する等)は普遍的な内容であり陳腐化リスクは低いと判断した。
- RenderDocの操作手順はバージョンにより多少変わる可能性があるため、実際に使う際は公式ドキュメント(https://renderdoc.org/docs/)の最新版を都度確認することを推奨する。

**今回の調査目的との整合性チェック**: 依頼された5項目((1)定番手法(2)ツール(3)著名実例(4)写経でない読解法(5)英語圏/日本語圏の両方)は全て充足した。特に(3)のFabien Sanglard氏については、単なる著書紹介に留まらず本人サイトの記事一覧と実際の解説手法(図解・定量化・機能別ページ分割)まで踏み込んで検証できている。