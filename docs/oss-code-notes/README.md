# oss-code-notes — OSS実ソースコード読解記録(索引)

作成: 2026-08-20。AI読解エージェント10体が Godot / Wicked Engine / raylib / Filament の**実コード**を読み、「どの機能がどのファイルの何行目でどう実装されているか」を記録したもの。PLAN2 v0.2(第2〜第4フェーズ再計画)の根拠資料。

## 使い方

- **今すぐ全部読む必要はない**。各マイルストーンに入ったら PLAN2 §8 の対応表で該当する1〜2本だけ読む。
- 行番号は読解時点のスナップショット(下表のコミット)のもの。最新版ではズレる。
- **各記録末尾の「確信度と限界」を必ず読む**(未読部分・推測部分が正直に列挙してある)。
- コードの扱いは oss-study-guide.md の規律どおり: **読む→閉じる→自分の言葉で書く**。src/ へのコピペはしない。
- **開示規律との橋渡し**: 記録内には OSS の関数がそのまま引用されている。これから自分が実装する機能のコード引用を読んだときは、**disclosure-log に「oss-code-notes/◯◯ の△△を参照」と1行記録する**(白紙再実装カウントには数えない)。推奨の型は「前日に読む→閉じる→翌日自分の言葉で書く」。当日にコード引用を開きながら書くのは写経と同じなのでしない。詳細は PLAN2 §8 末尾。

## 対象スナップショット

| リポジトリ | コミット | ライセンス |
|---|---|---|
| Godot | 855e9ad(2026-08-19時点 master) | MIT |
| Wicked Engine | d857d90 | MIT |
| raylib | f9e2215 | zlib |
| Filament | d8b4a37b | Apache 2.0 |

## 記録一覧

| ファイル | 対象 | 中身 | 主に効くM |
|---|---|---|---|
| godot-frame-pipeline.md | Godot Forward+ | 1フレームのパス列・クラスタードライトカリング・描画リストとソート・uniform受け渡し | 全体地図 / M7(ソート) |
| godot-shadows.md | Godot | シャドウアトラス・CSM(分割手順は流用可)・PCF/PCSS・バイアス2段構成 | M12(+フェーズ1のM5にも) |
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
