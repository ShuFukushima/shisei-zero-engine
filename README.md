# 試製零号発動機

C++ と OpenGL で「GPUはどうやって絵を出すのか」を理解しながら、Scene A(タイトル)からScene B(プレイ)へ切り替わる収集型ミニゲームを載せる、グラフィックス中心の最小ランタイムゲームエンジンです。

## スクリーンショット / GIF

未作成（M5のRelease版完成時に追加予定）。現在は準備中です。

## 現在地 / ロードマップ

- [ ] M0: 計画策定・環境準備（公開前の検証と封印を含む。現在は未完）
- [ ] M1: 三角形・シェーダー・テクスチャ — まず絵を出す (2026-09)
- [ ] M2: 自作3D数学ライブラリ (2026-09〜10)
- [ ] M3: 透視投影カメラで3D空間を飛び回る (2026-10〜11)
- [ ] M4: Phongライティング → OBJモデル読み込み(パーサー自作) (2026-11〜12)
- [ ] M5: Scene A(タイトル)→Scene B(プレイ)の収集型ミニゲーム、距離ベース当たり判定、ゲーム状態、画像/アイコン方式のUI、構成図・Release配布パッケージ (2026-12)
- [ ] 第2フェーズ以降の正式項目（初実装はM12）: シャドウマッピング

現在地の正本は [STATUS.md](STATUS.md) です。最終公開前に、検証結果・スクリーンショット/GIF・実測FPS・Release内容を更新します。

## ビルド・操作方法

Windows / Visual Studio(C++ワークロード。同梱のCMake+Ninjaを使用) / Git が必要です。

```text
ビルド: powershell -ExecutionPolicy Bypass -File scripts\build.ps1 -Path . -Run
操作: M5実装前のため未確定。M5完成時に記載します。
```

または Visual Studio の「フォルダーを開く」でこのフォルダを開いてもビルドできます。
cmake がPATHにある環境なら `cmake -S . -B build && cmake --build build` でも可。
依存ライブラリ(GLFW 3.4)は CMake の FetchContent が自動取得します。

## このプロジェクトの約束事(AI使用ポリシー)

生成AIは「家庭教師」として使い、「代筆者」としては使いません。

- `src/` 以下のエンジン本体は本人が概念を理解してから自分の手で書く
- 作業台（ビルド設定・依存取得・梱包・検証の配管）はAI提供可。ゲームロジックや `src/` へ取り込むコードは作業台に含めない
- AIにはまず概念・手順・APIの名前だけを教わる。コードを書いてもらうのは行き詰まったときに関数単位まで
- 理解した内容は [docs/interview-qa.md](docs/interview-qa.md) に自分の言葉で記録する
- AIの関与は [docs/disclosure-log.md](docs/disclosure-log.md) のコミット単位台帳に、対象ファイルと箇所が分かる粒度で記録する
- 運用ルールの全文は [AGENTS.md](AGENTS.md)

コミット履歴そのものが学習の記録です。

```text
Main Loop
  ├─ Input / Time → Camera / Transform
  ├─ Scene（少数オブジェクトの配列）
  ├─ Asset（shader / texture / OBJ）
  ├─ Mesh / Material
  └─ Renderer（固定順の描画パス）
```

M1〜M5の統合方針:

- M1: `poll input → update → render → present` の1本のフレーム経路を理解する
- M2: 自作Vec3/Mat4を零号機全体の座標の共通言語にする
- M3: `InputState → Camera → SceneObject配列 → 1つの描画入口` に統合する
- M4: キューブとOBJを `MeshData → GpuMesh → Material → Scene` の同じ描画入口へ通す
- M5: Scene A(タイトル)からScene B(プレイ)へ切り替え、距離ベース当たり判定とゲーム状態を使う収集型ミニゲームとしてRelease版にまとめる

2027年以降は零号機をゲーム制作基盤へ拡張する構想です。壱号機・弐号機は将来構想として名前だけを置いており、今回の計画では機能範囲や対応時期を確定しません。[ROADMAP.md](ROADMAP.md) を参照してください。

## 既知の限界と次にやること

(M5で記入する。現状の限界を自覚として明記する節 — 例: ガンマ補正/リニアワークフロー未対応、視錐台カリング等の描画最適化は未実装(全オブジェクトを毎フレーム描画)、背面カリング未使用。実測FPSと測定環境の1行はM5の梱包で必須(PLAN.md §4)。「次フェーズで何を入れるか」とセットで書く)
