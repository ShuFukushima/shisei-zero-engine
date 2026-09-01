> 作成: 2026-08-20 / AI読解エージェントによる実ソースコード読解記録
> 対象スナップショット: Godot (855e9ad) / ライセンス: MIT(コード引用は出典明記のうえ)
> 行番号はこのスナップショット時点のもの。最新版ではズレる。代表引用の一部はメインAIが実コードと突き合わせて一致を確認済み(全数検証ではない)。各記録末尾の「確信度と限界」を必ず読むこと。

GodotのGI実装(SDFGI / VoxelGI)読解記録

---

## 1. 読んだファイル一覧

| パス(リポジトリルート相対) | 行数 | 読了範囲 |
|---|---|---|
| `servers/rendering/renderer_rd/environment/gi.h` | 850 | 全読 |
| `servers/rendering/renderer_rd/environment/gi.cpp` | 4321 | 414-540, 1192-1535, 1825-1944, 2061-2260 を精読。他は grep で構造把握 |
| `servers/rendering/renderer_rd/shaders/environment/sdfgi_preprocess.glsl` | 1064 | 227-350, 948-1012 精読 |
| `.../shaders/environment/sdfgi_integrate.glsl` | 621 | 164-303 精読 |
| `.../shaders/environment/sdfgi_direct_light.glsl` | 575 | 179-228 精読 |
| `.../shaders/environment/gi.glsl` | 789 | 288-367, 487-607 精読 |
| `.../shaders/environment/voxel_gi.glsl` | 710 | 13-62 精読 + 関数一覧 |
| `.../shaders/environment/sdfgi_debug.glsl` / `sdfgi_debug_probes.glsl` | 183 / 243 | 行数のみ |
| `.../shaders/environment/voxel_gi_debug.glsl` / `voxel_gi_sdf.glsl` | 184 / 180 | 行数のみ |
| `servers/rendering/renderer_rd/forward_clustered/render_forward_clustered.cpp` | (該当箇所) | 1275, 1536, 1760-1762, 3101-3260, 4068 |
| `servers/rendering/renderer_rd/storage_rd/light_storage.cpp` | (該当箇所) | 1851-1890(reflection probe 対比用) |

検証コミット: `855e9ad9`(2026-08-19)

---

## 2. コード総量の実測(依頼項目4)

```
gi.cpp                     4321
gi.h                        850
gi.glsl                     789   ← 画面解決(SDFGI/VoxelGI共通)
sdfgi_preprocess.glsl      1064   ← Jump Flood + SDF生成
sdfgi_integrate.glsl        621   ← プローブのレイマーチ+SH積分
sdfgi_direct_light.glsl     575   ← ボクセルへの直接光
sdfgi_debug*.glsl           426
voxel_gi.glsl               710
voxel_gi_debug/sdf.glsl     364
─────────────────────────────────
合計                       9720 行
```

これに `_render_sdfgi()`(voxelization描画パス、`render_forward_clustered.cpp:3101` から約160行)と、シーンシェーダ内の `MODE_RENDER_SDF` ブロック(`scene_forward_clustered.glsl:2903` から約90行)が加わる。**実質1万行強**が「GIだけ」に費やされている。

参考として hikari の現状規模感と比べると、GI単体でレンダラー全体を超える分量になる。

---

## 3. 機能→実装の対応記録

### 3-1. SDFGI のデータ構造(依頼項目1)

**(a) 主要ファイル**: `gi.h:556-706`(`class SDFGI`)、`gi.cpp:414-1132`(`SDFGI::create`、**718行の確保処理**)

**(b) 中心データ構造**

定数(`gi.h:560-569`):
```
MAX_CASCADES=8, CASCADE_SIZE=128, PROBE_DIVISOR=16,
ANISOTROPY_SIZE=6, LIGHTPROBE_OCT_SIZE=6, SH_SIZE=16
```
`cascade_size = 128`(`gi.h:646`)。`probe_axis_count = PROBE_DIVISOR + 1 = 17`(`gi.cpp:427`)。

> 注意: `gi.cpp:435` のコメント `// Always 64x64` は**古く実態と合っていない**。実値は128。

`struct Cascade`(`gi.h:571-622`)の重要メンバー:
- `sdf_tex` — R8_UNORM の **128³ 3Dテクスチャ**(符号なし距離場)
- `light_tex` / `light_aniso_0_tex` / `light_aniso_1_tex` — ボクセルの蓄積光。異方性を6方向(±X±Y±Z)に分けて保持
- `solid_cell_buffer` — 「中身が詰まったセル」だけを列挙したリスト。`solid_cell_count = 128³ × 0.25 ≒ 524288`(`gi.cpp:429`)
- `solid_cell_dispatch_buffer_storage` / `_call` — **indirect dispatch 用**。GPU が数えた個数でGPU自身がディスパッチ数を決める
- `lightprobe_history_tex` / `lightprobe_average_tex` — 時間的蓄積(テンポラル)用
- `cell_size`, `position`(カスケードのワールド位置、整数セル単位), `dirty_regions`
- uniform set が **7個**(`sdf_store_`, `sdf_direct_light_static_`, `sdf_direct_light_dynamic_`, `scroll_`, `scroll_occlusion_`, `integrate_`, `lights_buffer`)

SDFGI全体側:
- `lightprobe_data` — **オクタヘドラル(八面体)マップ**。R32_UINT に RGBE パック。サイズ `17×17×(6+2)` × `17×(6+2)` = 2312×136、レイヤ数 = カスケード数×2(`gi.cpp:513-525`)
- `occlusion_texture` — R16_UINT、幅×2・深さ×カスケード数(`gi.cpp:471-476`)
- `render_albedo` / `render_emission` / `render_emission_aniso` / `render_geom_facing` / `render_occlusion[8]` — ボクセル化の一時バッファ
- `render_sdf[2]` / `render_sdf_half[2]` — **Jump Flood の ping-pong 用**(R8G8B8A8_UINT)

GPU側UBO `SDFGIData`(`gi.h:725-759`)は `ProbeCascadeData cascades[8]` を持ち、各々 `position`(カメラ相対に変換して数値誤差を減らす、`gi.cpp:1886`)、`to_probe`、`probe_world_offset`、`to_cell`、`exposure_normalization`。

**(c) カスケードのスクロール(トーラス的更新)** — `SDFGI::update`(`gi.cpp:1192-1247`)

カメラが動くと、カスケード全体を作り直すのではなく「はみ出した帯」だけを dirty にする。

```cpp
// gi.cpp:1212-1223
if (pos_in_cascade[j] < cascade.position[j]) {
    while (pos_in_cascade[j] < (cascade.position[j] - drag_margin)) {
        cascade.position[j] -= drag_margin * 2;
        cascade.dirty_regions[j] += drag_margin * 2;
    }
} else if (pos_in_cascade[j] > cascade.position[j]) {
    while (pos_in_cascade[j] > (cascade.position[j] + drag_margin)) {
        cascade.position[j] += drag_margin * 2;
        cascade.dirty_regions[j] -= drag_margin * 2;
    }
}
```
さらに `gi.cpp:1233-1245` で「dirty体積が安全体積の半分を超えたら、むしろ全再構築のほうが安い」と判定して `DIRTY_ALL` に切り替える。この種のヒューリスティックが随所にある。

---

### 3-2. 毎フレーム何が更新されるか(依頼項目2)

**呼び出し順(実測)** — `render_forward_clustered.cpp` および `renderer_scene_render_rd.cpp`:

| 順 | 関数 | 呼び出し元:行 | 内容 |
|---|---|---|---|
| 1 | `sdfgi->update(env, world_pos)` | `render_forward_clustered.cpp:4068` | カスケードをスクロールし dirty 領域を決定(CPUのみ) |
| 2 | `sdfgi->store_probes()` | 同:1536 | 前フレームのSHをオクタヘドラルへ変換して格納 |
| 3 | `sdfgi->update_cascades()` | 同:1760 | カスケードUBO更新(CPU→GPU転送) |
| 4 | `sdfgi->pre_process_gi()` | 同:1761 | `SDFGIData` UBO更新+動的ライト一覧をカスケードごとに構築 |
| 5 | `sdfgi->update_light()` | 同:1762 | 動的ライトをボクセルに焼く |
| 6 | `sdfgi->update_probes(env, sky)` | `renderer_scene_render_rd.cpp:1360` | プローブからレイを飛ばしてSH積分 |
| 7 | `sdfgi->render_region(...)` | `render_forward_clustered.cpp:1275` | **dirty領域があるときだけ**ジオメトリ再ボクセル化+SDF再構築 |

**使用コンピュートシェーダー一覧(全15バリアント)**

`preprocess`(9モード、`gi.cpp:3634-3646` 相当):
`MODE_SCROLL` / `MODE_SCROLL_OCCLUSION` / `MODE_INITIALIZE_JUMP_FLOOD` / `MODE_INITIALIZE_JUMP_FLOOD_HALF` / `MODE_JUMPFLOOD` / `MODE_JUMPFLOOD_OPTIMIZED` / `MODE_UPSCALE_JUMP_FLOOD` / `MODE_OCCLUSION` / `MODE_STORE`

`direct_light`(2モード): `MODE_PROCESS_STATIC` / `MODE_PROCESS_DYNAMIC`

`integrate`(4モード): `INTEGRATE_MODE_PROCESS` / `_STORE` / `_SCROLL` / `_SCROLL_STORE`

画面解決 `gi.glsl`: 5モード × 8スペシャライゼーション(`gi.h:807-821`)

**`render_region` の内部フロー**(`gi.cpp:2061-2422`、最も重い経路):

1. `RendererSceneRenderRD::_render_sdfgi(...)` — ジオメトリをラスタライズしてボクセル化(albedo/emission/facing を3Dテクスチャへ)
2. dirty がスクロール範囲なら `PRE_PROCESS_SCROLL` → `PRE_PROCESS_SCROLL_OCCLUSION` → `INTEGRATE_MODE_SCROLL` → `INTEGRATE_MODE_SCROLL_STORE`(既存データをずらして再利用)
3. `PRE_PROCESS_JUMP_FLOOD_INITIALIZE_HALF` — 半解像度(64³)で種を撒く
4. `PRE_PROCESS_JUMP_FLOOD` を `s /= 2` しながらループ(`gi.cpp:2244-2256`)
5. `PRE_PROCESS_JUMP_FLOOD_OPTIMIZED` — 小ステップは共有メモリ版に切替
6. `PRE_PROCESS_JUMP_FLOOD_UPSCALE` — 64³→128³ へ拡大
7. `PRE_PROCESS_OCCLUSION` — 遮蔽情報を8方向ぶん計算
8. `PRE_PROCESS_STORE` — SDF確定+solid cell リストを atomic で構築

**(d) GPU側の要点**

Jump Flood Algorithm(JFA)は「最近傍の種の座標」を伝播させる手法。ステップ幅を n/2, n/4, ... と半減させるので **O(log n) パス**で距離場が求まる。1パスあたり26近傍を読む(`sdfgi_preprocess.glsl:301-346`)。

**(e) 依存する他サブシステム**: シーン描画パイプライン(ボクセル化のためのラスタライズパス)、`LightStorage`(ライト情報)、`SkyRD`(空のirradiance)、`RenderSceneBuffersRD`、`UniformSetCacheRD`、`RenderingDevice` 抽象層全体。

---

### 3-3. VoxelGI との構造の違い(依頼項目3)

| | SDFGI | VoxelGI |
|---|---|---|
| データ構造 | **カスケード化された密な3Dテクスチャ**(128³×最大8段)+ SDF | **スパースなオクツリー**(`CellChildren{uint children[8]}` + `CellData`) |
| 生成タイミング | **実行時**にカメラ追従で継続的に更新 | **事前ベイク**(`voxel_gi_allocate_data` が octree_cells / data_cells / distance_field を受け取る、`gi.cpp:67-194`) |
| 空間範囲 | 無限(カスケードがカメラに追従) | 有限のAABB。最大8インスタンス(`MAX_VOXEL_GI_INSTANCES`) |
| 光の追跡 | **スフィアトレーシング**(SDFの距離ぶん進む) | **ボクセルコーントレーシング**(ミップマップ済み3Dテクスチャを円錐で積分) |
| 間接光の格納 | プローブ格子にSH(16係数)→ オクタヘドラルマップ | 3Dテクスチャのミップチェーンそのもの |
| GPUパス数 | 15バリアント | 8バリアント(`COMPUTE_LIGHT` / `COMPUTE_SECOND_BOUNCE` / `COMPUTE_MIPMAP` / `WRITE_TEXTURE` / `DYNAMIC_*` 4種) |

`CellData` は32bit×4に極限までパックされている(`voxel_gi.glsl:26-31`):
```glsl
struct CellData {
    uint position;  // xyz 10 bits
    uint albedo;    // rgb albedo
    uint emission;  // rgb normalized with e as multiplier
    uint normal;    // RGB normal encoded
};
```

VoxelGI の更新(`VoxelGIInstance::update`、`gi.cpp:2615-3400`、**785行**)はミップレベルを下から上へ回して `COMPUTE_LIGHT` → `COMPUTE_SECOND_BOUNCE` → `COMPUTE_MIPMAP` → `WRITE_TEXTURE` を実行する。動的オブジェクトは別経路(`DYNAMIC_*`)で毎フレーム描き足す。

**構造的な差の本質**: SDFGI は「動的シーンでも破綻しない代わりに、毎フレーム大量の再構築を回す」設計。VoxelGI は「事前ベイクで品質と速度を取る代わりに、静的ジオメトリしか正しく扱えない」設計。

---

### 3-4. Reflection Probe(対比、はるかに簡単)

`light_storage.cpp:1851-1881`。処理は驚くほど短い:

1. 6面のキューブマップとしてシーンを描画(通常のフォワード描画を6回)
2. `create_reflection_fast_filter()`(リアルタイム更新時、全ラフネスを1パス)
   または `create_reflection_importance_sample()`(1フレームに1ミップずつGGX重点サンプリング、`processing_layer` で進行管理)

つまり**「キューブマップを撮ってラフネス別にぼかすだけ」**。GI と違いボクセル化もSDFもプローブ格子も不要。IBL の実装経験があればほぼそのまま延長線上にある。

---

## 4. 代表コード引用

### (1) JFA の結果から距離場を作る — 最も学びが大きい12行
`servers/rendering/renderer_rd/shaders/environment/sdfgi_preprocess.glsl:953-968`
```glsl
uvec4 p = imageLoad(src_positions, pos);

bool solid = false;
float d;
if (ivec3(p.xyz) == pos) {
    //solid block
    d = 0;
    solid = true;
} else {
    //distance block
    d = 1.0 + length(vec3(p.xyz) - vec3(pos));
}

d /= 255.0;
imageStore(dst_sdf, pos, vec4(d));
```
JFA が各ボクセルに「最も近い実体ボクセルの座標」を配ってくれるので、距離は**その座標との単純な `length()`** で出る。R8 に収めるため255で割っている(=最大距離254セル)。

### (2) SDF に対するスフィアトレーシング — SDFGI の心臓部
`servers/rendering/renderer_rd/shaders/environment/sdfgi_integrate.glsl:223-235`
```glsl
while (advance < max_advance) {
    //read how much to advance from SDF
    uvw = (pos + ray_dir * advance) * pos_to_uvw;

    float distance = texture(sampler3D(sdf_cascades[j], linear_sampler), uvw).r * 255.0 - 1.0;
    if (distance < 0.05) {
        //consider hit
        hit = true;
        break;
    }

    advance += distance;
}
```
「その地点から最も近い物体までの距離」ぶんだけ一気に進めるので、空間が空いているほど高速。レイマーチの固定ステップと違い、**衝突を飛び越さないことが数学的に保証される**のが要点。ヒットしなければ次のカスケードへ持ち越す(`:206-247`)。

### (3) ボクセルコーントレーシング — VoxelGI の心臓部
`servers/rendering/renderer_rd/shaders/environment/gi.glsl:491-503`
```glsl
while (dist < max_distance && color.a < 1.0) {
    float diameter = max(1.0, 2.0 * tan_half_angle * dist);
    vec3 uvw_pos = (pos + dist * direction) * cell_size;
    float half_diameter = diameter * 0.5;
    //check if outside, then break
    if (any(greaterThan(abs(uvw_pos - 0.5), vec3(0.5f + half_diameter * cell_size)))) {
        break;
    }
    vec4 scolor = textureLod(sampler3D(probe, linear_sampler_with_mipmaps), uvw_pos, log2(diameter));
    float a = (1.0 - color.a);
    color += a * scolor;
    dist += half_diameter;
}
```
距離に比例して円錐が太くなり、その太さを **`log2(diameter)` でミップレベルに直結**させている。ミップマップが「ぼかし済みの積分結果」として機能する仕組み。拡散光は6本(高品質)または4本(低品質)のコーンで近似(`gi.glsl:558-588`)。

### (4) プローブのレイ方向 — Vogel球面 + テンポラル分散
`servers/rendering/renderer_rd/shaders/environment/sdfgi_integrate.glsl:184-192`
```glsl
//for a more homogeneous hemisphere, alternate based on history frames
uint ray_offset = params.history_index;
uint ray_mult = params.history_size;
uint ray_total = ray_mult * params.ray_count;

for (uint i = 0; i < params.ray_count; i++) {
    vec3 ray_dir = vogel_hemisphere(ray_offset + i * ray_mult, ray_total, offset);
    ray_dir.y *= params.y_mult;
    ray_dir = normalize(ray_dir);
```
1フレームあたり4〜128本しか飛ばさないが(`gi.cpp:1322`)、**フレームをまたいで異なる方向を担当**させ、履歴バッファで積算して収束させる。これが「30フレームで収束」設定の正体。

---

## 5. hikari への示唆(依頼項目5含む)

### 最重要の前提: OpenGL 3.3 では上記は一切実装できない

これが調査で判明した中で**計画に直結する最大の事実**です。

- コンピュートシェーダは **OpenGL 4.3以降**
- `imageStore` / `imageLoad`(image load/store)は **OpenGL 4.2以降**
- `atomicAdd` によるバッファ書き込みは **4.2以降**
- indirect dispatch は **4.3以降**

Godot の GI は全パスがこれらに依存しています。hikari が GL3.3 のままなら、SDFGI/VoxelGI のような手法は**移植ではなく別設計が必要**です。第2フェーズ以降で GI に踏み込むなら、**GL 4.3 への引き上げをフェーズ計画の分岐点として明示的に決める**必要があります(就活作品としては「4.3に上げてコンピュートを使った」と説明できること自体が加点材料になり得ます)。

### 機能別の難易度評価(3 = シャドウマッピング相当)

| 機能 | 簡易版の最小構成 | 簡易版難易度 | フル版難易度 | 前提知識 | 必要GL |
|---|---|---|---|---|---|
| **Reflection Probe / IBL** | キューブマップ1枚を6面レンダ → ミップごとにGGX重点サンプリングで畳み込み → 反射ベクトルでサンプル | **3** | 4 | キューブマップ、ラフネスとNDFの関係、重点サンプリング | 3.3で可 |
| **Irradiance Probe(拡散のみ)** | プローブ位置でキューブマップを撮り、SH L1(4係数)に投影して定数バッファに持つ。プローブ間は三線形補間 | **3** | 4 | 球面調和関数の基礎、法線での再構成 | 3.3で可 |
| **VoxelGI(簡易)** | 静的シーンを64³の3Dテクスチャにボクセル化 → 手動ミップ生成 → 4本コーントレース | **4** | 5 | 保守的ラスタライズ、3Dテクスチャへの書き込み、コーン積分 | **4.2+** |
| **SDFGI(簡易)** | 単一カスケード128³ → JFAでSDF生成 → プローブからスフィアトレース → SH積分 | **5** | 5+ | 上記すべて + JFA + オクタヘドラル符号化 + テンポラル蓄積 | **4.3+** |
| **SSGI/SSAO(代替案)** | 深度+法線バッファから半球サンプリング | **2〜3** | 4 | 深度からの位置復元、ノイズとブラー | 3.3で可 |

### 推奨する段階(hikari 第2〜第4フェーズ案)

依頼者は C++ 初学者・週10時間・就活作品という制約があります。素直な積み上げ順は:

1. **第2フェーズ: PBR + IBL** — GL3.3のまま可能。Phong から Cook-Torrance へ移行し、HDR環境マップ→irradiance map + prefiltered specular + BRDF LUT。ここまでで見た目のインパクトは最大級に上がる。難易度3。
2. **第3フェーズ: Deferred + SSAO** — G-Buffer 化。GL3.3で可能。難易度3。
3. **第4フェーズ: ここで分岐**
   - 安全策: **Reflection Probe の複数配置とブレンド**(難易度3、GL3.3)
   - 挑戦策: **GL4.3に上げて簡易VoxelGI**(難易度4)

SDFGI 相当を丸ごと自作するのは、週10時間の制約下では現実的な射程外です。「JFAで距離場を作る」部分だけを単体デモとして切り出すのは学習価値が高く(難易度3程度)、作品の技術デモとしては成立します。

### 「初学者が同じものを作るのは何が壁か」(依頼項目5)

コードを実際に読んで見えた壁を、抽象論ではなく具体的に挙げます。

1. **リソース確保が実装の過半を占める。** `SDFGI::create` は718行あり、その大半がテクスチャフォーマット指定と uniform set の構築です。アルゴリズム本体より「GPUに正しくデータを並べる作業」のほうが長い。ここは面白くないのに絶対に飛ばせない部分で、初学者が最も脱落しやすい。

2. **1つの機能が15個のシェーダーバリアントに分裂している。** 「SDFGIを作る」は単一のシェーダーを書くことではなく、9+2+4個のコンピュートパスを正しい順序と正しいバリアで繋ぐことです。1つでも順序を間違えると、絵は出るが少しおかしい、という最悪のデバッグ状況になります。

3. **ビット詰めが常態。** `CellData` の position は10bit×3、solid cell の position は7bit×3(`sdfgi_preprocess.glsl:998`)、光はRGBEやR8G8B8A8_UINTにパック。デバッガで覗いても数値の意味がわからないため、可視化シェーダー(`sdfgi_debug*.glsl` の426行、`voxel_gi_debug.glsl` の184行)を**先に**書く必要がある。Godotがデバッグ用に610行を割いているのは偶然ではありません。

4. **正しさの基準が存在しない。** シャドウマッピングは「影が出れば正解」が目視で判定できます。GIは「この壁の間接光がやや暗いのは、レイ数不足か、SHの次数不足か、SDFのバイアス過大か、テンポラル未収束か」が切り分けられない。リファレンス(オフラインパストレーサ)なしでは検証不能で、これが本質的に最大の壁です。

5. **性能とのトレードオフが設計に埋め込まれている。** `bounce_feedback`、`solid_cell_ratio=0.25`、「半解像度JFAで十分」(`gi.cpp:2219` の `bool half_size = true; //much faster, very little difference`)、「dirty体積が半分超なら全再構築」など、経験的な定数とヒューリスティックが至る所にあります。これらは論文には書かれておらず、実装して測って初めて出てくる知見です。初学者が一次資料だけから同じ判断に到達するのは困難。

---

## 6. 確信度と限界

**確信度が高い(実際に読んで確認)**
- 全ファイルの行数(`wc -l` による実測)
- `gi.h` のデータ構造定義すべて(全850行を読了)
- SDFGI の毎フレーム呼び出し順(呼び出し元ファイル:行番号を grep で特定済み)
- シェーダーバリアントの一覧と数(`gi.cpp` の `init()` 内 `push_back` を確認)
- 引用した4箇所のコード(すべて Read の出力に基づく。行番号は実測)
- JFA・スフィアトレーシング・コーントレーシングの各アルゴリズム本体
- reflection probe の後処理が2種類しかないこと

**推測・未確認(正直に列挙)**
- **`VoxelGIInstance::update`(785行)の本体は未読。** grep によるパス順の抽出のみで、ミップマップ伝播の詳細やdynamic objectの扱いは推測を含みます。
- **`GI::process_gi`(`gi.cpp:3968-4277`、309行)は未読。** 画面解決の CPU 側ディスパッチ処理。
- **`render_static_lights`(`gi.cpp:2422-2615`)は未読。**
- **`MODE_JUMPFLOOD_OPTIMIZED` の共有メモリ実装(`sdfgi_preprocess.glsl:351-444`)は未読。** 「共有メモリで小ステップを高速化」というのはモード名と `GROUP_SIZE 8` の定義からの推測です。
- **`MODE_OCCLUSION` パス(`sdfgi_preprocess.glsl:484-581`)の内部は未読。** 遮蔽を8方向ぶん計算していることは呼び出し側から確認済みですが、計算内容は未確認。
- **`sdfgi_debug*.glsl` / `voxel_gi_debug.glsl` は行数のみ確認、中身は未読。**
- **`_render_sdfgi()` のボクセル化の詳細は未読。** 保守的ラスタライズを使っているかどうかは未確認です(`scene_forward_clustered.glsl` の `MODE_RENDER_SDF` ブロックの存在は確認済み)。
- **難易度評価(1〜5)は私の判断であり、コードから導かれる客観値ではありません。** 依頼者の実力・利用可能時間によって上下します。
- **OpenGLバージョン要件は一般的な仕様知識に基づくもので、このリポジトリのコードから直接検証したものではありません**(Godot は RenderingDevice 抽象層経由で Vulkan/D3D12 を使うため、GL版の制約はコード内に現れません)。ARB拡張(`ARB_shader_image_load_store` 等)を使えば3.3でも一部可能ですが、ドライバ依存になります。