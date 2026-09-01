> 作成: 2026-08-20 / AI読解エージェントによる実ソースコード読解記録
> 対象スナップショット: Godot (855e9ad) / ライセンス: MIT(コード引用は出典明記のうえ)
> 行番号はこのスナップショット時点のもの。最新版ではズレる。代表引用の一部はメインAIが実コードと突き合わせて一致を確認済み(全数検証ではない)。各記録末尾の「確信度と限界」を必ず読むこと。

# Godot シャドウ実装 読解レポート

対象: `servers/rendering/renderer_rd/` 配下のシャドウアトラス、平行光源シャドウ(CSM)、GLSL側PCF/ソフトシャドウ、バイアス処理。関連する分割距離計算がCPU側の`renderer_scene_cull.cpp`にあるため、そこも合わせて読んだ。

---

## 1. 読んだファイル一覧

| パス | 概算行数 | 読み方 |
|---|---|---|
| `servers/rendering/renderer_rd/storage_rd/light_storage.h` | 1204行 | L380-460, L670-750を実読 |
| `servers/rendering/renderer_rd/storage_rd/light_storage.cpp` | 2890行 | L750-870, L2340-2830を実読 |
| `servers/rendering/renderer_scene_cull.cpp` | 4559行 | L2145-2380を実読、他はGrepで所在確認 |
| `servers/rendering/renderer_rd/forward_clustered/render_forward_clustered.cpp` | 5313行 | L2607-2690を実読、他はGrep |
| `servers/rendering/renderer_rd/forward_clustered/scene_shader_forward_clustered.cpp` | 1069行 | L995-1015を実読 |
| `servers/rendering/renderer_rd/shaders/scene_forward_lights_inc.glsl` | 1453行 | L280-455, L780-825を実読 |
| `servers/rendering/renderer_rd/shaders/forward_clustered/scene_forward_clustered.glsl` | 3130行 | L2300-2530を実読 |
| `servers/rendering/renderer_rd/shaders/forward_clustered/scene_forward_clustered_inc.glsl` | 513行 | Grepのみ(L176, L388確認) |
| `servers/rendering/renderer_rd/renderer_scene_render_rd.cpp` | 1928行 | Grepのみ(所在確認止まり) |

---

## 2. 機能→実装の対応記録

### (1) シャドウアトラス(スポット/オムニ光用)

**主要ファイルと行範囲**
- `light_storage.h` L403-467: `ShadowAtlas`構造体定義
- `light_storage.h` L684-718: `light_instance_get_shadow_atlas_rect()` — UV矩形計算
- `light_storage.cpp` L2341-2733: 生成/サイズ設定/分割設定/割当アルゴリズム

**中心データ構造**
```
ShadowAtlas {
  Quadrant quadrants[4] {          // アトラスを固定4分割(TL/TR/BL/BR)
    uint32 subdivision;             // このクアドラントをさらにNxNに分割
    Vector<Shadow> shadows {RID owner; uint64 version; uint64 alloc_tick;}
  }
  int size_order[4];    // subdivision降順にソートされたクアドラント番号
  uint32 smallest_subdiv;
  RID depth; RID fb;     // アトラス全体で1枚の深度テクスチャを共有
  HashMap<RID,uint32> shadow_owners; // light_instance → (quadrant<<27 | shadow_index)
}
```
4クアドラントは**固定**でアトラステクスチャの4分の1ずつを占め、各クアドラントは独立に「1x1」〜「NxN」のグリッド分割数を持てる(エディタの「Shadow Atlas Quadrant N Subdiv」設定に対応)。つまり「重要なライトは大きい枠、遠くの小さいライトは小さい枠」という異サイズ管理を、4段階の粗い量子化で実現している。

**処理フロー(実際の呼び出し順)**
1. `shadow_atlas_create()` → RIDだけ発行
2. `shadow_atlas_set_size(atlas, size)` → 2のべき乗に丸め、全クアドラントのshadows配列をクリア
3. 毎フレーム、CPUカリング側(`renderer_scene_cull.cpp` L3637)が各ライトについて`shadow_atlas_update_light(atlas, light_instance, coverage, version)`を呼ぶ。`coverage`はそのライトの影が画面上でどれくらいの割合を占めるかの概算値
4. `shadow_atlas_update_light`内で`desired_fit = min(quad_size/smallest_subdiv, next_pow2(quad_size*coverage))`を計算し、`size_order`を辿って収まる最小クアドラントを探索
5. `_shadow_atlas_find_shadow`(スポット/平行以外)または`_shadow_atlas_find_omni_shadows`(オムニ、隣接2スロット必要)で空きまたはLRU(最終使用パスが最も古いもの)のスロットを確保
6. 描画直前、`light_instance_get_shadow_atlas_rect()`がkeyをデコード: `x=(quadrant&1)*quad_size + (shadow%subdiv)*cell`, `y=(quadrant>>1)*quad_size + (shadow/subdiv)*cell` で正規化UV矩形を算出
7. `render_forward_clustered.cpp`の`_render_shadow_pass`がこの矩形をビューポート/シザーとして使い、共有の`shadow_atlas->depth`テクスチャへ深度のみ描画

**GPU側**: 深度のみのパス(専用フラグメントシェーダは未確認、深度書き込みのみと推測)。サンプリング側は(3)節参照。

**依存サブシステム**: `LightInstance`(ライトの変換行列保持)、`RenderingDevice`抽象(`texture_create`/`framebuffer_create`)、`RendererSceneCull`(coverage計算・毎フレーム呼び出し元)。

---

### (2) 平行光源シャドウ(CSM)

**主要ファイルと行範囲**
- `light_storage.h` L443-451: `DirectionalShadow`構造体
- `light_storage.cpp` L2765-2830: テクスチャ共有・グリッド分割(`_get_directional_shadow_rect`)
- `renderer_scene_cull.cpp` L2145-2367: `_light_instance_setup_directional_shadow` — カスケード分割の本体
- `render_forward_clustered.cpp` L2607-2680: `_render_shadow_pass`内、パス番号→矩形の変換
- `light_storage.cpp` L780-863: GPUユニフォームへの詰め込み(行列・バイアス・split距離)

**中心データ構造**: `DirectionalShadow { RID depth; RID fb; int light_count; int size; int current_light; }` — **全ての平行光源・全カスケードが1枚の正方形深度テクスチャを共有**する(プロジェクト設定`directional_shadow/size`、既定4096)。CSMの分割数はライトごとに`LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL`(1)/`PARALLEL_2_SPLITS`(2)/`PARALLEL_4_SPLITS`(4)から選ぶ。

**処理フロー**
1. `renderer_scene_cull.cpp`が可視の平行光源数を`set_directional_shadow_count(N)`で通知(→ `current_light=0`にリセット)
2. 各ライトについて`_light_instance_setup_directional_shadow`(L2145)を呼ぶ:
   - `distances[]`配列を構築: `distances[0]=near`, `distances[k+1] = near + (SPLIT_1/2/3_OFFSETパラメータ, 既定0.1/0.3/0.6) * (far-near)`, `distances[splits]=far`。**分割距離はログ関数ではなくライトごとに編集可能な0〜1のオフセット値**で決まる
   - 分割ごとに、その深度範囲だけをカバーするカメラ視錐台の8頂点をワールド座標で取得(`camera_matrix.get_endpoints`)
   - ライトのローカル基底(x_vec/y_vec/z_vec)へ8頂点を射影し、外接球(中心=8頂点平均、半径=中心からの最大距離)を計算
   - **テクセルスナップ**によるシャドウの安定化: `unit = radius*4/texture_size; x_max_cam = snapped(x_vec・center + radius, unit)` — カメラが回転してもシャドウ端がテクセル単位でしか動かないようにする、CSM実装の定石トリック
   - 直交投影行列とTransform3Dを`cull.shadows[i].cascades[j]`に格納
3. `light_instance_set_shadow_transform(...)`で`LightInstance::shadow_transform[j]`に保存
4. `render_forward_clustered._render_shadow_pass`: そのフレームで初回のみ`get_directional_shadow_rect()`(=`_get_directional_shadow_rect(atlas_size, light_count, current_light)`)を呼び、**複数の平行光源が同時に存在する場合はテクスチャをsplit_h×split_vグリッドに分けてライトごとにセルを割り当てる**(1灯なら全面、2灯なら左右半分、…)。さらにそのセル内を、`PARALLEL_4_SPLITS`なら2x2、`PARALLEL_2_SPLITS`なら上下2分割してカスケード用の矩形を作る
5. `light_storage.cpp` L780-863で最終行列を構築: `shadow_mtx = atlas_rect補正 * bias補正 * light_projection * inverse(カメラ相対ライト変換)`。`shadow_bias[j]`と`shadow_normal_bias[j]`もここで計算(詳細は(4))
6. GPU側のカスケード選択とブレンドは(3)節参照

**依存サブシステム**: `RendererSceneCull`(CPU側フラスタムカリング、全レンダラーバックエンド共通コードなので`renderer_rd/`外にある)、`Projection`クラス(`core/math/`、`set_orthogonal`/`get_endpoints`/`set_light_bias`/`set_light_atlas_rect`)、`LightStorage`(GPUユニフォームパッキング)。

---

### (3) GLSL側 PCF/ソフトシャドウ

**主要ファイル**: `servers/rendering/renderer_rd/shaders/scene_forward_lights_inc.glsl` L303-453

- `quick_hash()`(L307): Interleaved Gradient Noise、ピクセル座標+TAAフレーム番号からノイズ角度を生成
- `sample_pcf_shadow()`/`sample_directional_pcf_shadow()`(L312-364): 回転Poissonディスクによる多点PCF
- `sample_omni_pcf_shadow()`(L366-408): オムニ用、キューブ相当のflip-offset処理込み
- `sample_directional_soft_shadow()`(L410-453): PCSS風(ブロッカー探索→可変ペナンブラ)

**サンプリングの仕組み**: `sampler2DShadow`(比較サンプラー)+`textureProj`によりGPUのハードウェアPCF(線形フィルタ+深度比較の2x2バイリニア補間)が1回のテクスチャフェッチで無料で得られる。その上で、`disk_rotation`(ピクセルごとにランダム回転する2x2行列)をPoissonカーネル(`soft_shadow_kernel[i]`、CPU側で生成されユニフォームバッファに格納 — 生成コードは未確認)に掛けて複数回サンプルし平均する = **回転Poissonディスク+ハードウェアPCFの合わせ技**。サンプル数`sc_soft_shadow_samples()`はSpec Constantで可変(0なら中心1点のみ)。

`sample_directional_soft_shadow`はPCSS(Percentage-Closer Soft Shadows)の簡易版: 1パス目でブロッカー(遮蔽物)の平均深度を探索、`penumbra = (-受光点深度 + ブロッカー平均深度)/(1-ブロッカー平均深度)`でペナンブラ幅を決め、2パス目でその幅でPCFする。

**カスケード選択とブレンド**(`scene_forward_clustered.glsl` L2320-2530): `depth_z`(視点からの距離)を`shadow_split_offsets.x/y/z`と順に比較して使うカスケードを決定。`blend_splits`が有効なら境界付近で隣接カスケードも追加サンプルし`smoothstep`+`mix`でクロスフェード、カスケード切替の境目が見えないようにする。

---

### (4) バイアス(シャドウアクネ対策)

**平行光源**(`scene_forward_clustered.glsl` L2342-2350, `light_storage.cpp` L831-832):
- **定数バイアス**: `頂点位置 += ライト方向 * shadow_bias[cascade]`。CPU側で`shadow_bias[j] = (エディタのShadow Biasパラメータ)/100 * bias_scale`、`bias_scale`はそのカスケードの深度レンジ(zfar-znear)。**カスケードごとに深度レンジが違うので、素のバイアス値をレンジでスケールして「1つの設定値」で全カスケードに対応できるようにしている**
- **法線オフセットバイアス(スロープスケール)**: `base_normal_bias = 法線 * (1 - max(0, dot(ライト方向, -法線)))` — ライトに対して斜めな面ほど大きくオフセット。`shadow_normal_bias[j] = (エディタのNormal Biasパラメータ) * そのカスケードのテクセルサイズ`。テクセルサイズでスケールすることで「Nテクセル分ずらす」という直感的な意味になる。さらに`normal_bias -= ライト方向 * dot(ライト方向, normal_bias)`でライト方向成分を除去し、**深度自体は動かさず横方向にのみサンプル点をずらす**(深度比較のズレを起こさない工夫)

**スポット/オムニ**(`scene_forward_lights_inc.glsl` L813-819): 同じ2段構成だが、定数バイアスは投影後の`splane.z += shadow_bias`という形(クリップ空間深度に直接加算)、法線バイアスはワールド空間頂点に加算してから射影する点が平行光源と異なる。

---

## 3. 代表コード引用

**シャドウアトラスのUV矩形デコード** (`light_storage.h:694-702`)
```cpp
uint32_t atlas_size = shadow_atlas->size;
uint32_t quadrant_size = atlas_size >> 1;

uint32_t x = (quadrant & 1) * quadrant_size;
uint32_t y = (quadrant >> 1) * quadrant_size;

uint32_t shadow_size = (quadrant_size / shadow_atlas->quadrants[quadrant].subdivision);
x += (shadow % shadow_atlas->quadrants[quadrant].subdivision) * shadow_size;
y += (shadow / shadow_atlas->quadrants[quadrant].subdivision) * shadow_size;
```

**複数平行光源のテクスチャ共有グリッド分割** (`light_storage.cpp:2791-2804`)
```cpp
while (split_h * split_v < p_shadow_count) {
    if (split_h == split_v) {
        split_h <<= 1;
    } else {
        split_v <<= 1;
    }
}

Rect2i rect(0, 0, p_size, p_size);
rect.size.width /= split_h;
rect.size.height /= split_v;

rect.position.x = rect.size.width * (p_shadow_index % split_h);
rect.position.y = rect.size.height * (p_shadow_index / split_h);
```

**スロープスケール法線バイアスのマクロ** (`scene_forward_clustered.glsl:2342-2350`)
```glsl
float depth_z = -vertex.z;
vec3 light_dir = directional_lights.data[i].direction;
vec3 base_normal_bias = geo_normal * (1.0 - max(0.0, dot(light_dir, -geo_normal)));

#define BIAS_FUNC(m_var, m_idx) \
    m_var.xyz += light_dir * directional_lights.data[i].shadow_bias[m_idx]; \
    vec3 normal_bias = base_normal_bias * directional_lights.data[i].shadow_normal_bias[m_idx]; \
    normal_bias -= light_dir * dot(light_dir, normal_bias); \
    m_var.xyz += normal_bias;
```

**回転Poissonディスクによる多点PCF** (`scene_forward_lights_inc.glsl:348-361`)
```glsl
mat2 disk_rotation;
{
    float r = quick_hash(gl_FragCoord.xy + vec2(taa_frame_count * 5.588238)) * 2.0 * M_PI;
    float sr = sin(r);
    float cr = cos(r);
    disk_rotation = mat2(vec2(cr, -sr), vec2(sr, cr));
}

float avg = 0.0;

SPEC_CONSTANT_LOOP_ANNOTATION
for (uint i = 0; i < sc_soft_shadow_samples(); i++) {
    avg += textureProj(sampler2DShadow(shadow, shadow_sampler), vec4(pos + shadow_pixel_size * (disk_rotation * scene_data_block.data.soft_shadow_kernel[i].xy), depth, 1.0));
}
```

---

## 4. hikariへの示唆

| 機能 | 簡易版の最小構成 | 難易度(1-5) | フル版難易度 | 前提知識 |
|---|---|---|---|---|
| シャドウアトラス | 1灯・1枚のdepthテクスチャのみ(動的割当なし) | **1**(実質フェーズ1そのもの) | **5**(動的LOD割当+LRU+オムニのデュアルパラボロイド詰め込み) | UVパッキング、RIDベースのリソース管理、LRUキャッシュ |
| CSM(カスケード) | 固定2〜3分割、視錐台をワールド空間AABBで囲むだけ(テクセルスナップなし) | **3**(単発シャドウマッピングと同格)、複数カスケード込みで実質3〜4 | **5**(テクセルスナップ安定化+可変分割距離+クロスフェードブレンド+PCSS拡張) | 透視/正射影行列、視錐台8頂点のワールド座標復元、ライトローカル基底への射影、外接球 |
| PCF/ソフトシャドウ | 3x3/5x5固定オフセットPCF、手動深度比較(比較サンプラー未使用) | **2** | Godot式(比較サンプラー+回転Poisson+可変サンプル数): **3〜4**、PCSS(ブロッカー探索): **4〜5** | `sampler2DShadow`/`textureProj`(OpenGLでは`GL_TEXTURE_COMPARE_MODE`+`glTexParameteri`相当)、乱数/ノイズ関数 |
| バイアス | 定数バイアスのみ | **1** | スロープスケール法線オフセット+カスケードごとのテクセルサイズ正規化: **2〜3** | シャドウアクネ/ピーターパンニングのトレードオフ、法線とライト方向の内積による勾配補正 |

**フェーズ1(単一シャドウマッピング)への直接的な示唆**:
- バイアスは「定数バイアス」だけでなく最初から「法線方向のバイアス」も入れておくと、後で法線バイアスへ切り替える手戻りが少ない(Godotも両方を常時併用)
- PCFは最初は3x3固定オフセットで十分。OpenGL 3.3で`sampler2DShadow`+`GL_COMPARE_REF_TO_TEXTURE`が使えるので、Godot式の「ハードウェア比較+多点」は難易度2〜3程度で真似できる
- CSMはフェーズ2以降の課題として、Godotの「視錐台を分割距離でスライスし、8頂点をライト空間へ射影して外接球を取る」手順はそのまま流用可能な設計。テクセルスナップは見た目の品質向上に効くが後回しでよい

---

## 5. 確信度と限界

**確信度が高い(実コードを直接確認済み)**:
- シャドウアトラスのクアドラント構造とUV矩形計算式
- `DirectionalShadow`が全平行光源・全カスケードで1枚のテクスチャを共有し、グリッド分割で場所取りする仕組み
- CSMの分割距離計算式とテクセルスナップの安定化トリック
- GLSL側のPCF/PCSSサンプリング関数の中身
- バイアスの二重構成(定数+法線オフセット)とカスケード間でのスケーリング方法

**未確認・推測が入っている部分**:
- `soft_shadow_kernel[]`/`directional_soft_shadow_kernel[]`(Poisson点配列)のCPU側生成コード・点の個数・既定値は未読
- シャドウ深度パス自体のフラグメントシェーダ側処理(アルファテスト分岐など)は未確認。深度のみの単純パスと推測しているが検証していない
- オムニライトのデュアルパラボロイド投影シェーダ本体は未読(C++側の2スロット確保ロジックのみ確認)
- `renderer_scene_render_rd.cpp` L1227-1259の`directional_shadow_quality`設定は行の所在のみ確認し、品質レベルとPCFサンプル数の対応関係は中身を読んでいない
- モバイルレンダラー版(`scene_forward_mobile.glsl`)は同様の処理を持つが今回は未調査(clustered版のみ)