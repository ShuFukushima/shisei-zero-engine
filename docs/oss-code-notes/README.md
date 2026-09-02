# oss-code-notes — OSS実ソースコード読解記録(索引)

作成: 2026-08-20。AI読解エージェント10体が Godot / Wicked Engine / raylib / Filament の**実コード**を読み、「どの機能がどのファイルの何行目でどう実装されているか」を記録したもの。PLAN2 v0.3(A-S反映後の第1.5〜第4フェーズ再計画)の根拠資料。ただし、下記の既存記録は歴史資料であり、短縮SHAや可変branchを再現可能な固定版とは扱わない。

## 使い方

- **今すぐ全部読む必要はない**。各マイルストーンに入ったら PLAN2 §8 の対応表で該当する1〜2本だけ読む。
- 行番号は読解時点のスナップショットの補助情報であり、最新版ではズレる。行番号だけを再現性の根拠にしない。
- **各記録末尾の「確信度と限界」を必ず読む**(未読部分・推測部分が正直に列挙してある)。
- コードの扱いは oss-study-guide.md の規律どおり: **読む→閉じる→自分の言葉で書く**。src/ へのコピペはしない。
- **開示規律との橋渡し**: 記録内には OSS の関数がそのまま引用されている。これから自分が実装する機能のコード引用を読んだときは、**disclosure-log に「oss-code-notes/◯◯ の△△を参照」と1行記録する**(白紙再実装カウントには数えない)。推奨の型は「前日に読む→閉じる→翌日自分の言葉で書く」。当日にコード引用を開きながら書くのは写経と同じなのでしない。詳細は PLAN2 §8 末尾。

## 対象スナップショット(旧記録の表記)

| リポジトリ | 旧記録のコミット表記 | ライセンス |
|---|---|---|
| Godot | 855e9ad(2026-08-19時点 master) | MIT |
| Wicked Engine | d857d90 | MIT |
| raylib | f9e2215 | zlib |
| Filament | d8b4a37b | Apache 2.0 |

## 参照版の現行規則(A-S技術訂正、2026-08-21)

- **完全SHA(40桁)を直接確認できた場合だけ固定版と呼ぶ**。固定版には完全SHA・取得日・対象シンボル(関数/型/定数)を併記し、行番号は補助情報にする。
- `master`/`main`などの**可変branch**を参照した場合は、確認日と対象シンボル名を記録する。可変branchと古い行番号だけでは過去の実装を再現できない。
- 上表と既存記録の短縮SHAは歴史資料として残す。新規記録を追加するときは、固定版か可変branchかを明記し、固定版の条件を満たさないものを現行計画の「固定版根拠」と表現しない。

## 記録一覧

| ファイル | 対象 | 中身 | 主に効くM |
|---|---|---|---|
| godot-frame-pipeline.md | Godot Forward+ | 1フレームのパス列・クラスタードライトカリング・描画リストとソート・uniform受け渡し | 全体地図 / M7(ソート) |
| godot-shadows.md | Godot | シャドウアトラス・CSM(分割手順は流用可)・PCF/PCSS・バイアス2段構成 | M12(第3フェーズ。2026-08-25確定でフェーズ1にシャドウは無い) |
| godot-gi.md | Godot | SDFGI/VoxelGI の全構造(実測約1万行)・reflection probe との対比・初学者の壁5項目 | M15 / 射程外の根拠 |
| godot-postprocess.md | Godot | Bloom 2系統・トーンマップ5種・TAA・SSAO(Intel ASSAO)・effects/全ファイルマップ | M7 / M11 |
| godot-rendering-device.md | Godot | GPU抽象化3層・シェーダーコンパイル(SPIR-V)・「真似る所と真似ない所」 | M6(RAII設計) |
| wicked-renderer-core.md | Wicked | RenderPath3D のパス順序・19886行の整理術・バインドレス設計・単独開発の工夫 | 全体地図 / M7(ping-pong) |
| wicked-gi.md | Wicked | DDGI/VXGI/SurfelGI の実装+行数実測比較・3手法の本質的な違い・簡易版の削減見積り | M15 |
| wicked-materials-shadows-skinning.md | Wicked | Cook-Torrance実装(brdf.hlsli 206行)・metallicワークフロー・Vogel PCF・GPUスキニング | M8 / M13(b) |
| raylib-architecture.md | raylib | rlglバッチ・Mesh/Material/Model所有権設計・**hikariが直接真似できる7パターン(C→C++翻訳つき)** | M6〜M7 |
| filament-brdf-ibl.md | Filament | D/V/F項と理論文書の対応・roughness remap・IBL事前計算(SH/prefilter/DFG LUT)・**M8/M9で読むべき関数10個** | M8 / M9 |

## 読解の検証について

代表コード引用のうち3箇所(Godot cluster_render.glsl:114-127 / Wicked brdf.hlsli:8-17 / raylib rmodels.c:1219-1231)をメインAIが実コードと突き合わせ、行番号・内容の一致を確認済み。全数検証ではないため、利用時は引用元パスを実リポジトリで確認するのが確実(GitHubで該当コミットを開けば同じ行が見られる)。
