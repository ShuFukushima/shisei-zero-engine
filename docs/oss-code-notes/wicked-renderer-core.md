> 作成: 2026-08-20 / AI読解エージェントによる実ソースコード読解記録
> 対象スナップショット: Wicked Engine (d857d90) / ライセンス: MIT(コード引用は出典明記のうえ)
> 行番号はこのスナップショット時点のもの。最新版ではズレる。代表引用の一部はメインAIが実コードと突き合わせて一致を確認済み(全数検証ではない)。各記録末尾の「確信度と限界」を必ず読むこと。

# Wicked Engine レンダラー中核構造 読解レポート

## 1. 読んだファイル一覧

| パス(リポジトリルート = `WickedEngine/`) | 実測行数 | 読み方 |
|---|---|---|
| `wiRenderPath3D.h` | 395行 | 全読 |
| `wiRenderPath3D.cpp` | 3296行 | `PreRender()`(342-799)、`Render()`(801-1755)、`RenderTransparents()`(2053-2302)、`RenderPostprocessChain()`(2303-2533)を実読(計約1400行) |
| `wiRenderer.h` | 1450行 | 冒頭200行を実読、残りはGrepで関数宣言を洗い出し |
| `wiRenderer.cpp` | **19886行** | 全関数シグネチャをGrepで一覧化(約230個)。`RenderMeshes()`(3089-3444)、`DrawScene()`(7396-7500)、`UpdateRenderData()`(5075-5274)、エンティティ配列書き込み部(4440-4534)を実読 |
| `wiScene.h` | 726行 | 全読 |
| `shaders/ShaderInterop_Renderer.h` | 1913行 | `ShaderScene`(9-79)、`ShaderMaterial`(388-506)、`ShaderGeometry`(535-590)、`ShaderMeshInstancePointer`/`ObjectPushConstants`(798-840)を実読 |
| `shaders/objectHF.hlsli` | 1151行 | Grepで頂点フェッチ関数群(53-260)を特定・実読 |
| `shaders/objectVS_common.hlsl` / `objectPS.hlsl` | 3行/5行 | 全読 |
| `shaders/globals.hlsli` | 2681行 | バインドレスリソース宣言部(363-495)のみGrep実読 |

参考(担当外だが可視性確認のため行数のみ計測): `wiScene.cpp` 9812行。

---

## 2. 機能→実装の対応記録

### (A) `RenderPath3D::Render()` が呼ぶパスの実際の順序

**主要ファイル**: `wiRenderPath3D.cpp:801-1755`(`Render()`本体956行)、前処理は`PreRender()`342-799行。

**中心データ構造**: `wi::jobsystem::context ctx` と `wi::graphics::CommandList` の集合。各パスは`device->BeginCommandList()`でコマンドリストを取得し、`wi::jobsystem::Execute(ctx, lambda)`でスレッドプールに積む。パス間の依存は`device->WaitCommandList(cmd_A, cmd_B)`で明示し、最後に`wi::jobsystem::Wait(ctx)`で全合流させる。

**実際に読み取ったパス順序**(依存関係つき。太字は同時実行される非同期ブロック):

1. `cmd_prepareframe`(Graphics): `BindCameraCB` → `UpdateRenderData`(シーンGPUバッファ更新、後述)
2. **`cmd_prepareframe_async`(Compute, ①待ち)**: `UpdateRenderDataAsync` / `RefreshWetmaps` / `UpdateRaytracingAccelerationStructures` / `SurfelGI` / `DDGI`
3. `cmd_maincamera_prepass`(Graphics): `RefreshImpostors` → Zプリパス`DrawScene(..., RENDERPASS_PREPASS)`(foreground分・regular分の2回)
4. **`cmd_maincamera_compute_effects`(Compute, ③待ち)**: `Visibility_Prepare` → `ComputeTiledLightCulling` → `Visibility_Surface`/`Visibility_Velocity` → `RenderAO`/`RenderSSR`/`RenderSSGI` → RTシャドウ or スクリーンスペースシャドウ
5. `cmd_occlusionculling`(条件付き): 前フレーム深度でのオクルージョンクエリ発行・解決
6. 平面反射Zプリパス(条件付き)
7. `cmd_ocean`(Compute, 条件付き): 海面シミュレーション
8. `cmd_shadowmap`(②待ち, 条件付き): `DrawShadowmaps`(全ライトのシャドウマップ一括描画)
9. VXGIボクセル化(条件付き)
10. `cmd_updatetextures`(条件付き): `RefreshLightmaps`/`RefreshEnvProbes`/`PaintDecals`
11. 平面反射カラーパス(条件付き): `DrawScene(RENDERPASS_MAIN)`不透明→`DrawSky`→透明
12. **メインカメラ不透明カラーパス(④待ち)**: RTReflection/RTDiffuse解決 → `DrawScene(RENDERPASS_MAIN)`(foreground/regular) → `DrawSky` → `RenderOutline`
13. `cmd_weathereffect`(Compute, 条件付き): ボリュメトリック雲・大気遠近法
14. MSAAリゾルブ/天候ブレンド
15. `RenderTransparents()`: 水しぶき→透明メッシュ描画→デバッグ描画/ワイヤーフレーム→ソフトパーティクル→ガウシアンスプラット→スプライト/フォント→ボリュームライト合成→レンズフレア→ディストーションパーティクル
16. `RenderCameraComponents(ctx)`
17. `RenderPostprocessChain()`(後述、⑫の一部と並行して積まれる)
18. `TextureStreamingReadbackCopy`(Copyキュー)
19. `RenderPath2D::Render()`(2D UI)
20. `wi::jobsystem::Wait(ctx)`

**`RenderPostprocessChain()`内部**(`wiRenderPath3D.cpp:2303-2533`)は`rt_read`/`rt_write`のping-pongで一本のパイプラインとして直線的に実行: FSR2 or TemporalAA → 水中 → カスタム(BeforeTonemap) → 被写界深度 → モーションブラー → 輝度計測 → ブルーム → トーンマップ(HDR→LDR) → カスタム(AfterTonemap) → シャープン → FXAA → 色収差 → CRT → GUI背景ブラー → FSR。

**依存する他サブシステム**: `wiJobSystem`(スレッドプール)、`wiGraphicsDevice`(コマンドリスト/バリア/RenderPass API)、`wiProfiler`(GPU/CPUレンジ計測)。

---

### (B) `wiRenderer.cpp` 単一ファイルの整理方法

**中心データ構造**: クラスは使わず、`namespace wi::renderer { ... }` 1つで19886行全体を囲む(46行目で開始)。グローバル状態はファイルスコープの静的配列で保持:
```
Shader shaders[SHADERTYPE_COUNT]; Texture textures[TEXTYPE_COUNT];
InputLayout inputLayouts[ILTYPE_COUNT]; ... GPUBuffer buffers[BUFFERTYPE_COUNT];
```
(`wiRenderer.cpp:51-58`)。すべてenumインデックスでアクセスする「グローバルレジストリ」方式。

**グルーピングはコメント区切りではなく命名規則で行われている**(Grepで抽出した約230関数の並び順から判明):
- 初期化: `LoadShaders`/`LoadBuffers`/`SetUpStates`/`Initialize`(1-2870行台)
- 描画コア: `RenderMeshes`/`RenderImpostors`/`DrawScene`/`DrawShadowmaps`/`DrawDebugWorld`(3089-8923行台)
- GI/VXGI: `VXGI_Voxelize`/`SurfelGI`/`DDGI`(10039-12809行台)
- ポストプロセス群(**全体の約1/3、12809-19271行台**): 各エフェクトが`CreateXxxResources()` + `Postprocess_Xxx()`の対を成す形で並ぶ(SSAO/HBAO/MSAO/RTAO, RTDiffuse, SSGI, RTReflection, SSR, RTShadow, ScreenSpaceShadow, DepthOfField, MotionBlur, AerialPerspective, VolumetricClouds, TemporalAA, Tonemap, FSR等、20種以上)
- デバッグ描画プリミティブ: `DrawBox`/`DrawSphere`/`DrawLine`等(19385-19560)
- 末尾: グローバルトグルの`SetXxx`/`GetXxx`(19560-19886、単純な状態フラグの集合)

**設計上の要点**: 「1エフェクト = 専用`XxxResources`構造体(状態を`RenderPath3D`側が保持) + `Create`関数 + `Postprocess_`関数」というテンプレートが機械的に反復されている。新しいポストプロセスを足す際、既存コードへの侵襲がほぼゼロで済む構造。

---

### (C) シーンデータ(メッシュ/マテリアル/ライト)のGPU転送方式 — 完全なバインドレス設計

**中心データ構造**(`wiScene.h:116-160`、`shaders/ShaderInterop_Renderer.h`):
- `Scene::instanceBuffer` / `geometryBuffer` / `materialBuffer`: それぞれ`ShaderMeshInstance`(256B固定)、`ShaderGeometry`(=Mesh+MeshSubsetを1構造体にまとめたもの)、`ShaderMaterial`(384B)の配列を持つ巨大`StructuredBuffer`。CPU側に対応する`XxxUploadBuffer[フレーム数]`(persistently-mapped)を持ち、毎フレーム`CopyBuffer`でGPU専用バッファへ転送(`wiRenderer.cpp:5121-5158`)。
- `ShaderScene`構造体(`ShaderInterop_Renderer.h:9-79`)が「シーン全体のハンドル集」で、`instancebuffer`/`geometrybuffer`/`materialbuffer`/`TLAS`など**全バッファへのディスクリプタインデックス(int)**だけを保持する。これ1個をシェーダーにバインドすれば、シーン全体にアクセスできる。
- `ShaderGeometry`内のメンバー`ib`/`vb_pos_wind`/`vb_uvs`/`vb_nor`/`vb_tan`/`vb_col`は全部`int`(バインドレスディスクリプタインデックス) — 頂点バッファそのものがバインドレスリソースとして扱われる。
- ライト/デカール/環境プローブなどの「シェーディングエンティティ」は上記の巨大バッファとは別枠で、毎フレームの定数バッファ`FrameCB.entityArray[SHADER_ENTITY_COUNT=256]`に直接パック(`wiRenderer.cpp:4447-4530`)。タイルドライトカリング(32×32ピクセルタイル、`TILED_CULLING_BLOCKSIZE`)で画面タイルごとに使うエンティティを絞り込む。

**処理フロー**(実際に追った呼び出し順):
1. `RenderPath3D::Render()` → `wi::renderer::UpdateRenderData(vis, frameCB, cmd)`(`wiRenderer.cpp:5075`)
2. 内部で `instanceBuffer`/`geometryBuffer`/`materialBuffer`/`skinningBuffer` それぞれについて `device->CopyBuffer(GPU用バッファ, アップロードバッファ, サイズ)` (5121-5171)
3. スキニング(骨アニメーション)はコンピュートシェーダー`CSTYPE_SKINNING`をディスパッチして頂点位置/法線/接線をストリームアウト用バッファに書き込む(5230行台) — CPU側スキニングではなくGPUスキニング
4. 描画時: `RenderMeshes()`(`wiRenderer.cpp:3089`)が`device->AllocateGPU()`でフレーム限りのリングバッファ領域を確保し、そこに`ShaderMeshInstancePointer`(インスタンスへの4byteインデックス+dither情報)を書き込み、`ObjectPushConstants{geometryIndex, materialIndex, instances(バッファのディスクリプタIndex), instance_offset, samplerIndex}`をプッシュコンスタント(16B)としてドローコールごとに送る
5. GPU側(`objectHF.hlsli`)は`push.geometryIndex`から`ShaderGeometry`をロードし、`bindless_buffers_float4[descriptor_index(mesh.vb_pos_wind)][vertexID]`のようにバインドレス配列を経由して頂点シェーダー内で手動フェッチ(固定機能IAを使わない、プログラマブル頂点フェッチ)

**GPU側実装**: `shaders/globals.hlsli`(363-495行台)でAPIごとの実体を分岐 — DX12は`ResourceDescriptorHeap[index]`(SM6.6)、Vulkanは`[[vk::binding(0, DESCRIPTOR_SET_BINDLESS_...)]] Texture2D bindless_textures[]`という無制限記述子配列(descriptor indexing拡張)。両APIの差異を1枚のヘッダーで吸収している。

**依存する他サブシステム**: `wiGraphicsDevice`(`GetDescriptorIndex`/`AllocateGPU`/ディスクリプタヒープ管理)、`wiJobSystem`(非同期コピー)。

---

### (D) 単独開発者がフルエンジンを維持するためのコード上の工夫

1. **クラス階層を作らずnamespace+命名規約で分類** — 継承やインターフェースの設計コストを払わず、`Postprocess_*`/`Create*Resources`/`Draw*`/`Update*`という一貫したプレフィックスだけで19886行を航行可能にしている。IDEの「シンボル一覧」だけで機能を把握できる。
2. **`XxxResources`構造体パターンの完全な反復** — 新しいポストプロセスを追加する際のテンプレートが機械的(コピペ元がすぐ見つかる)。
3. **ジョブシステム+`WaitCommandList`による明示的な依存グラフ** — スレッド安全性を「暗黙のロック」ではなく「コマンドリスト間の明示的Wait」で表現し、並列化の複雑さを局所化している。
4. **バインドレス設計によるバインディング管理コストの排除** — ディスクリプタセットのレイアウト管理・シェーダーごとのルートシグネチャ調整が(ほぼ)不要になり、新しいバッファ/テクスチャを増やす際の既存コードへの影響が最小。
5. **ECSコンポーネントのバージョン管理**(`wiScene.h:35-68`、例: `componentLibrary.Register<MaterialComponent>("...materials", 11)`)— シリアライズフォーマットにバージョン番号を持たせ、1人でも長期間の後方互換を破壊せずに構造体を進化させられる。
6. **シェーダーpermutationをinclude+define一枚に集約**(`objectVS_common.hlsl`は3行、実体は`objectHF.hlsli`1151行)— 大量のシェーダーバリアント(30種類のobjectVS/PS)を1つの実装ファイルの`#define`分岐で管理し、修正箇所を1箇所に保つ。

---

## 3. 代表コード引用

**① バインドレスのグローバルハンドル集**(`shaders/ShaderInterop_Renderer.h:9-20`)
```cpp
struct alignas(16) ShaderScene
{
    int instancebuffer;
    int geometrybuffer;
    int materialbuffer;
    int meshletbuffer;
    int texturestreamingbuffer;
    int globalenvmap;
    int globalprobe;
    int impostorInstanceOffset;
    int TLAS;
    ...
```

**② ドローコール1回あたりの最小限のプッシュ情報**(`shaders/ShaderInterop_Renderer.h:830-840`)
```cpp
struct ObjectPushConstants
{
    uint geometryIndex;
    uint materialIndex : 24;
    uint samplerIndex : 8;
    int instances;
    uint instance_offset;
};
static_assert(sizeof(ObjectPushConstants) == 16);
```

**③ フレーム毎GPUリングバッファへのインスタンス書き込み**(`wiRenderer.cpp:3126-3128, 3322-3330`)
```cpp
const size_t alloc_size = renderQueue.size() * camera_count * sizeof(ShaderMeshInstancePointer);
const GraphicsDevice::GPUAllocation instances = device->AllocateGPU(alloc_size, cmd);
const int instanceBufferDescriptorIndex = device->GetDescriptorIndex(&instances.buffer, SubresourceType::SRV);
...
ObjectPushConstants push;
push.geometryIndex = mesh.geometryOffset + subsetIndex;
push.materialIndex = subset.materialIndex;
push.instances = instanceBufferDescriptorIndex;
push.instance_offset = (uint)instancedBatch.dataOffset;
```

**④ 頂点シェーダー側の手動バインドレスフェッチ**(`shaders/objectHF.hlsli:55-56, 159`)
```cpp
#define GetMesh() (load_geometry(push.geometryIndex))
#define GetMaterial() (load_material(push.materialIndex))
...
return bindless_buffers_float4[descriptor_index(GetMesh().vb_pos_wind)][GetVertexID()];
```

---

## 4. hikariへの示唆

| 機能 | 簡易版の最小構成 | 簡易版難易度(1-5) | フル版難易度 | 前提知識 |
|---|---|---|---|---|
| マルチパスのレンダーパス整理(RenderPath3D的な構成) | `Renderer::render()`内でZプリパス→不透明→透明→ポストの順に固定関数を直列呼び出し | 2 | 4(非同期ジョブ化・依存グラフ管理まで含めると) | フレームバッファ/深度テストの基礎知識のみ |
| GPUバインドレス的な設計 | OpenGL3.3では`ARB_bindless_texture`は非対応拡張なので**再現不可**。素直に「マテリアルごとにテクスチャユニットbind + glDrawElements」で十分 | — (該当なし) | 5(DX12/Vulkanでのみ現実的、GL3.3では原理的に無理) | ディスクリプタヒープ、SM6.6等 |
| SoA構造体バッファへのシーンデータ集約(ShaderMaterial/ShaderGeometry的発想) | マテリアルをUBO 1本にまとめ、`glUniformBlockBinding`でマテリアルインデックスをuniformで渡す程度で十分 | 2 | 4 | UBO/SSBOの基礎 |
| タイルドライトカリング(entityArray+タイル分割) | ライト数が数個〜十数個規模のhikariでは**不要**。全ライトをuniform配列で総当たりループするフォワードシェーディングで十分 | 1(=実装しない選択が妥当) | 4 | コンピュートシェーダー |
| ポストプロセスチェーン(ping-pong FBO) | rt_read/rt_writeを2枚のFBOで交互に切り替え、トーンマップ→FXAA程度を直列適用 | 2(シャドウマッピング相当かそれ以下) | 4(20種類のエフェクトを条件分岐で差し替え可能にする設計まで含めると) | FBO、フルスクリーンクアッド描画 |
| ECSによるシーン管理(wiScene::ComponentManager) | hikariの現在のOBJパーサー+単純なオブジェクトリストのままで当面問題ない。将来スケールする場合のみSoA配列+バージョン管理を検討 | 3 | 5 | データ指向設計 |

**最も重要な示唆**: Wicked Engineの「バインドレス+ジョブシステムの非同期コマンドリスト」は、DX12/Vulkan World（かつ複数人・長期運用）を前提にした設計であり、**OpenGL3.3・単独短期開発のhikariにそのまま輸入する価値は低い**。hikariのPhase2-4(PBR/IBL/シャドウマッピング/Deferred)で参考にすべきは、むしろ「①ShaderMaterial/ShaderGeometryのような**GPU側データレイアウトをC++側の構造体と1対1対応させ、static_assertでサイズを固定する**」「②`XxxResources`構造体+`CreateXxx`/`RenderXxx`関数対のような**エフェクトごとの状態カプセル化パターン**」「③ポストプロセスのping-pongバッファチェーン」の3点。Deferredレンダリングへの移行時は、WickedEngineの「Visibility Buffer(`Visibility_Prepare`/`Visibility_Surface`/`Visibility_Shade`)」というG-Buffer代替方式も参考になりうるが、これは未読(下記限界を参照)。

---

## 5. 確信度と限界

- **検証済み(実コードを読んで確認)**: `Render()`のパス順序と依存関係(セクションA)、`wiRenderer.cpp`の関数命名パターン(セクションB、Grepで230関数を実際に列挙)、`ShaderScene`/`ShaderMaterial`/`ShaderGeometry`/`ObjectPushConstants`のレイアウトとバインドレス転送経路(セクションC)、シェーダー側のバインドレスフェッチ実装。
- **状況証拠(周辺コードから妥当と判断したが、定義本体は未読)**: `RenderCameraComponents()`の中身(`wiRenderPath3D.cpp:2535`以降)は関数シグネチャのみ確認し、実装は未読 — 複数カメラ(反射・監視カメラ等)を回すループと推測しているが未検証。`Visibility_Shade`(Visibility Buffer方式のコンピュートシェーディング)は関数の呼び出し箇所のみ確認し、実装本体(`wiRenderer.cpp:12039`付近)は未読。`DrawShadowmaps`本体(6671-7396行、700行超)も呼び出し関係のみ確認し中身は未読。
- **推測**: 「単独開発者向けの工夫」(D)は、コードの構造的特徴から演繹した解釈であり、開発者本人(turanszkij氏)の設計意図を裏付ける一次情報(コミットログ・設計ドキュメント)には当たっていない。
- **読み切れなかった範囲**: `wiRenderer.cpp`の19886行のうち実際にRead toolで内容を読んだのは約900行(約4.5%)。残りは関数シグネチャ一覧(Grep)による構造把握のみで、個々の実装ロジック(特にレイトレーシング関連10997-11422行、VXGI 10039-10450行、SurfelGI/DDGI 12117-12809行)は未検証。これらの機能について詳しく知りたい場合は追加調査が必要。
- **覆り得る条件**: Wicked Engineは開発が活発なため、リポジトリのコミット時点によって行番号・API名が変わっている可能性がある(このレポートはユーザー提供のローカルclone時点のスナップショットに基づく)。