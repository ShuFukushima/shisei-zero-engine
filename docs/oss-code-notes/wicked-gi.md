> 作成: 2026-08-20 / AI読解エージェントによる実ソースコード読解記録
> 対象スナップショット: Wicked Engine (d857d90) / ライセンス: MIT(コード引用は出典明記のうえ)
> 行番号はこのスナップショット時点のもの。最新版ではズレる。代表引用の一部はメインAIが実コードと突き合わせて一致を確認済み(全数検証ではない)。各記録末尾の「確信度と限界」を必ず読むこと。

# Wicked Engine GI実装群 読解記録(DDGI / VXGI / SurfelGI)

対象コミット `d857d90`(upstream turanszkij/WickedEngine, MIT)。以下の行番号は実際にReadした結果に基づく。

---

## 1. 読んだファイル一覧

**CPU側**(`WickedEngine/` 相対)

| ファイル | 全行数 | 読んだ範囲 |
|---|---|---|
| `wiRenderer.cpp` | 19886 | 9295-9845(RefreshEnvProbes)、10039-10375(VXGI群)、12117-12599(SurfelGI群)、12601-12807(DDGI) |
| `wiRenderer.h` | 1450 | 499-564(3手法のリソース構造体と関数宣言) |
| `wiScene.h` | — | 185-246(SurfelGI/DDGI/VXGIのGPUリソース定義) |
| `wiScene.cpp` | 9812 | 489-733、820-837、976-996(リソース生成・クリップマップ更新・shaderscene転記) |
| `wiRenderPath3D.cpp` | — | 509-521、879-890、1059-1061、1258-1262、1442-1444(呼び出し順) |

**シェーダ**(`WickedEngine/shaders/`)

DDGI系: `ShaderInterop_DDGI.h`(447)、`ddgi_rayallocationCS`(81)、`ddgi_indirectprepareCS`(14)、`ddgi_raytraceCS`(515)、`ddgi_updateCS`(257)、`ddgi_updateCS_depth`(2)、`ddgi_raytraceCS_rtapi`(2)、`ddgi_debugVS`(19)
Surfel系: `ShaderInterop_SurfelGI.h`(619)、`surfel_coverageCS`(647)、`surfel_raytraceCS`(607)、`surfel_integrateCS`(387)、`surfel_updateCS`(310)、`surfel_denoiseCS`(88)、`surfel_indirectprepareCS`(48)、`surfel_binningCS`(40)、`surfel_gridoffsetsCS`(15)
VXGI系: `ShaderInterop_VXGI.h`(166)、`voxelConeTracingHF.hlsli`(219)、`voxelHF.hlsli`(45)、`objectVS/GS/PS_voxelizer`(69/92/334)、`vxgi_temporalCS`(128)、`vxgi_offsetprevCS`(47)、`vxgi_sdf_jumpfloodCS`(52)、`vxgi_resolve_diffuseCS`(30)、`vxgi_resolve_specularCS`(40)
比較用: `filterEnvMapCS`(129)、`envMapVS/PS`(17/19)、`envMap_skyVS/PS`(15/20/11)、`shadingHF.hlsli`(110-190を読了)

---

## 2. 機能→実装の対応記録

### 2-1. DDGI(Dynamic Diffuse GI / プローブ方式)

**(a) 主要ファイルと行範囲**
CPU: `wiRenderer.cpp:12601-12807`(`DDGI()`、207行)。リソース生成 `wiScene.cpp:596-682`、shaderscene転記 `wiScene.cpp:976-996`。

**(b) 中心データ構造**
- `Scene::DDGI`(`wiScene.h:208-223`): `grid_dimensions`(既定 32×8×32 = 8192プローブ)、`grid_min/max`、`ray_buffer`、`variance_buffer`、`raycount_buffer`、`rayallocation_buffer`、`probe_buffer`、`depth_texture`。
- `DDGIProbe`(`ShaderInterop_Renderer.h:1885-1889`): **`SH::L1_RGB::Packed radiance` と `uint2 offset` だけ**。輝度は8×8のオクタ球テクセルではなく **L1球面調和(4係数)に圧縮**して保存する。プローブあたり実質32バイト程度。
- `DDGIRayDataPacked`(`ShaderInterop_DDGI.h:41-61`): `uint4` に direction(half3)+depth(half)+radiance(half3)をパック。
- `DDGIVarianceData`(同31-38): mean/shortMean/variance/vbbr/inconsistency ← 時間積分器の状態。
- 深度は別テクスチャ `depth_texture`(16×16+ボーダー=18テクセル/プローブ、`ShaderInterop_DDGI.h:9-10`)に `(depth, depth²)` の2モーメントで保持。

**(c) 処理フロー**(`DDGI()` 内のディスパッチ列)
1. `frame_index==0` なら全バッファ `ClearUAV`(12621-12636)
2. **Ray allocation** `CSTYPE_DDGI_RAYALLOCATION` を `Dispatch(probe_count,1,1)`(12668)。`ddgi_rayallocationCS.hlsl` はプローブの64テクセル分の `inconsistency` の最大値を取り、`rayCount = saturate(max_inconsistency) * push.rayCount`(:47)。**収束したプローブにはレイを割かない**。さらに視錐台外なら ×0.1(:52-55)。`InterlockedAdd` でグローバルなレイ配列にオフセット確保(:66)、`(probeIndex | rayIndex<<20)` を書き込む(:77-79)。
3. **Indirect prepare** `CSTYPE_DDGI_INDIRECTPREPARE` を `Dispatch(1,1,1)`(12692)。合計レイ数→DispatchIndirect引数へ。
4. **Raytrace** `CSTYPE_DDGI_RAYTRACE` を `DispatchIndirect`(12734)。`ddgi_raytraceCS.hlsl`: `spherical_fibonacci(rayIndex, rayCount)` で球面一様方向を生成し、フレーム毎にランダム回転行列 `g_xTransform` を掛ける(:52、CPU側12712-12721で生成)。TLAS に `TraceRayInline`(:236)→ヒット面で光源1個をランダムサンプル+シャドウレイ→**前フレームのプローブを `ddgi_sample_irradiance` で読んで無限バウンス**(:492-500)→`radiance*albedo+emissive` を ray_buffer へ。
5. **Update** `CSTYPE_DDGI_UPDATE` を `Dispatch(probe_count,1,1)`(12768)。`ddgi_updateCS.hlsl`: 1プローブ=1スレッドグループ(8×8)。全レイをLDSにキャッシュし、各テクセルが `weight = saturate(dot(texel_direction, ray.direction))` で加重平均(:144-158)→`MultiscaleMeanEstimator` で時間積分(:227)→**8×8の結果をL1 SHに射影**して `probe_buffer` へ(:240-251)。
6. **Update Depth** 同シェーダの `DDGI_UPDATE_DEPTH` 版を再ディスパッチ(12792)。weightを64乗して鋭くし(:146)、`(depth, depth²)` を書く。同時に**プローブが壁にめり込んだら押し出す** offset を計算(:136-139、:206-211)。

**(d) GPU側要点**: ライティング適用は `shadingHF.hlsli:179-188` → `ddgi_sample_irradiance`(`ShaderInterop_DDGI.h:147-317`)。周囲8プローブをトライリニア加重し、**Chebyshev不等式による可視性テスト**(:236)で壁抜けを防ぐ。

**(e) 依存**: レイトレーシングAPI(TLAS)または自前BVH(`scene.BVH`)が**必須**(12606)。SH実装、`lightingHF.hlsli`、乱数(RNG)。

---

### 2-2. VXGI(ボクセルコーントレース)

**(a)** CPU: `wiRenderer.cpp:10039-10067`(Create)、`10068-10266`(`VXGI_Voxelize`)、`10267-10375`(`VXGI_Resolve`)。Scene: `wiScene.cpp:684-733`(3Dテクスチャ生成)、`820-837`(クリップマップ中心のスナップ)。

**(b) 中心データ構造**
- `Scene::VXGI`(`wiScene.h:226-246`): `res=64`、`clipmaps[6]`(各 `voxelsize`, `center`, `offsetfromPrevFrame`)、`radiance`/`prev_radiance`/`render_atomic`/`sdf`/`sdf_temp`。
- `radiance` テクスチャの形状が重要(`wiScene.cpp:686-688`): `width = res * (6 + DIFFUSE_CONE_COUNT)`, `height = res * VXGI_CLIPMAP_COUNT`, `depth = res`。つまり**6面の異方性ボクセル + 16本のコーン方向ごとの事前加重スライス**を横に並べ、6段のクリップマップを縦に並べた1枚の3Dテクスチャ。64³×22×6×8B ≒ **約1.4GB相当…ではなく** R16G16B16A16 で 64*22 × 64*6 × 64 × 8B ≒ 2.2GB。実際は `res` を落として使う想定(注: ここは実測でなく計算)。
- `render_atomic`: `uint` の3Dテクスチャ、`depth = res * VOXELIZATION_CHANNEL_COUNT`(10チャンネル: baseColor RGBA / selfRadiance RGB / normal RG / fragment counter、`ShaderInterop_VXGI.h:78-97`)。

**(c) 処理フロー**
1. `wiScene.cpp:732` で毎フレーム `clipmap_to_update` を1つずつ回す(**6フレームで1周**)。`820-837` でクリップマップ中心をカメラ位置にテクセル単位でスナップし `offsetfromPrevFrame` を算出。
2. `VXGI_Voxelize()`: AABB交差でオブジェクト収集(10088-10099)→ `CSTYPE_VXGI_OFFSETPREV` で前フレームのradianceをカメラ移動分ずらす(10154-10158)→ `render_atomic` をクリア。
3. **ラスタライザによるボクセル化**: `RenderMeshes(vis, renderQueue, RENDERPASS_VOXELIZE, ...)`(10188)。深度テスト無効・res×res のビューポート。`objectGS_voxelizer.hlsl` が三角形の支配軸を選んで投影、`objectPS_voxelizer.hlsl:204-231` が**直接光を計算した上で** `self_radiance = baseColor*(direct.diffuse/PI)+emissive` を作り、`InterlockedAdd` で10チャンネル×該当面に加算(=アトミック平均)。
4. `CSTYPE_VXGI_TEMPORAL` を `Dispatch(res/8, res/8, res/8)`(10210)。`vxgi_temporalCS.hlsl`: counterで割って平均を復元(:42-53)→**そのボクセルから `ConeTraceDiffuse` して間接光を足す**(:63-65、これが多重バウンスの正体)→前フレームと `lerp(prev, cur, 0.5)`(:84)→さらに**16コーン方向の事前加重スライスを計算して書く**(:104-119)。同時に不透明ボクセルの `sdf=0` を記録。
5. `CSTYPE_VXGI_SDF_JUMPFLOOD` を `log2(res)` 回ループ(10229-10251)。ジャンプフラッディングで距離場を作り、コーントレースのステップを飛ばす用途。
6. `VXGI_Resolve()`: `vxgi_resolve_diffuseCS`/`_specularCS` を**半解像度**でディスパッチ(10330/10348)→ `Postprocess_Upsample_Bilateral` で全解像度へ(10365-10369)。

**(d) GPU側要点**: `voxelConeTracingHF.hlsli:95-172` の `ConeTrace()`。距離に応じてコーン直径を広げ `lod = log2(diameter/voxelSize0)` でクリップマップ段を選び、2段をブレンドしてフロント・トゥ・バック合成。`ConeTraceDiffuse`(:177-198)は法線半球の16コーンを cosθ 加重。適用は `shadingHF.hlsli:148-159`。

**(e) 依存**: **ジオメトリシェーダ**(または頂点シェーダのインスタンス3重化)、コンサバティブラスタライゼーション(任意)、`RENDERPASS_VOXELIZE` というレンダーパス種別がオブジェクト描画パイプライン全体(PSO variant, ラスタライザステート, `wiRenderer.cpp:504/524/617/2031/3074/3176` 等)に配線されている。**レイトレAPI不要**。

---

### 2-3. SurfelGI(サーフェルキャッシュ方式)

**(a)** CPU: `wiRenderer.cpp:12117-12141`(Create)、`12142-12318`(`SurfelGI_Coverage`, 177行)、`12319-12599`(`SurfelGI`, 281行)。Scene: `wiScene.cpp:489-595`(11本のGPUバッファ生成)。

**(b) 中心データ構造**(`ShaderInterop_SurfelGI.h`)
- `Surfel`(`ShaderInterop_Renderer.h:1894-1905`): `SH::L1_RGB::Packed radiance` + `normal`(oct packed) + `position` + `radius_packed`。**32バイト**。
- `SurfelData`(:103-127): uid, primitiveID, bary, `raydata`(24bitオフセット+8bit本数)、`properties`(life/recycle/seen/redundantのビット詰め)。
- 容量 `SURFEL_CAPACITY=100000`、生存目標 `SURFEL_LIVE_TARGET=80000`、レイ予算 `SURFEL_RAY_BUDGET=100000`/フレーム(:52)。
- **カスケード空間ハッシュグリッド**: `SURFEL_GRID_DIMENSIONS=128×64×128` × `SURFEL_GRID_LEVELS=8` レベル(:10-13)。レベルLのセル/半径は `SURFEL_MIN_RADIUS<<L`。サーフェルの所属レベルは**画面上のフットプリントが一定(32px)になるよう距離から決める**(`surfel_level_continuous`, :242-259)。
- プール管理: `aliveBuffer[2]`(ダブルバッファ)、`deadBuffer`(フリーリスト)、`statsBuffer`(count/deadCount/rayCount等のアトミックカウンタ)、`indirectBuffer`(3種のDispatchIndirect引数)。
- 方向別遮蔽: `momentsTexture`(サーフェルあたり4×4テクセルのアトラス)。

**(c) 処理フロー**(2つの関数に分かれる)

`SurfelGI(res, scene, cmd)`(ワールド空間側、`wiRenderPath3D.cpp:879`で呼ばれる):
1. `gridBuffer` を `ClearUAV`(12358)
2. `CSTYPE_SURFEL_UPDATE` を `DispatchIndirect(indirectBuffer, offsetof(iterate))`(12387)。`surfel_updateCS.hlsl`: 生存サーフェルごとに ①primitiveID+baryから現在位置を再取得(スキニング追従)②`surfel_stable_level` でカスケードレベル再決定 ③重なる最大27セルに `InterlockedAdd(count,1)`(:92)④**関連度ベースのリサイクル**(未visible経過フレーム・カメラ距離・プール圧から確率的に破棄, :124-199)⑤レイ本数を決めて `InterlockedAdd(rayCount, n, offset)` で予算を確保(:265)。距離が遠いサーフェルは本数を減らし、さらに**再トレース周期を最大32フレームに間引く**(:215-256)。
3. `CSTYPE_SURFEL_GRIDOFFSETS` を `Dispatch((SURFEL_TOTAL_TABLE_SIZE+63)/64)`(12420)。セルカウントのプレフィックス和→`cellBuffer` のオフセット確定。
4. `CSTYPE_SURFEL_BINNING` を DispatchIndirect(12453)。各サーフェルを該当セルのスロットに書き込む。
5. `wi::gpusortlib::Sort(...)`(12487)。レイをMortonキーで基数ソートし、BVHトラバースのコヒーレンスを稼ぐ。
6. `CSTYPE_SURFEL_RAYTRACE` を DispatchIndirect(12537)。`surfel_raytraceCS.hlsl`: コサイン半球サンプル(:244)、SHの支配方向へ最大50%をガイド(:253)、TLASトレース、ヒット点で**前フレームのサーフェルキャッシュを読む**(無限バウンス)。
7. `CSTYPE_SURFEL_INTEGRATE` を DispatchIndirect(12583)。`surfel_integrateCS.hlsl`: 4×4テクセル方向ごとにレイを加重平均→`MultiscaleMeanEstimator`(:245)→深度モーメント更新(:264-276)→**L1 SHに射影して `surfelBuffer` に書き戻す**。新生サーフェルは近傍サーフェルのSHから種を貰う(:50-118、ポップ防止)。

`SurfelGI_Coverage(res, scene, depth, debugUAV, cmd)`(スクリーン空間側、`wiRenderPath3D.cpp:1059`):
1. `CSTYPE_SURFEL_COVERAGE` を**半解像度**で `Dispatch(w/16, h/16)`(12211)。`surfel_coverageCS.hlsl`(647行、最大のシェーダ): 16×16グループでLDSにセルのサーフェル列をキャッシュ(:112-129)→各ピクセルが `gather_surfel()` でSH評価・加重平均(:155-244)→カバレッジ不足なら**Poisson-disk間隔判定を通してサーフェルを新規生成**(:567-632、`deadBuffer` から確保)→前フレームをモーションベクタで再投影して時間ブレンド。
2. `CSTYPE_SURFEL_INDIRECTPREPARE` を `Dispatch(1,1,1)`(12239)。次フレームのIndirect引数を作る。
3. `CSTYPE_SURFEL_DENOISE` を3パス(à-trous、ステップ1,2,4)(12263-12299)。
4. `Postprocess_Upsample_Bilateral` で全解像度へ(12305)。

**(d)** 適用は `shadingHF.hlsli:124-146`(スクリーン空間テクスチャを直読み)。二次カメラ/透明面はワールド空間の `SampleSurfelGI`(`ShaderInterop_SurfelGI.h:556`)にフォールバック。

**(e) 依存**: TLAS必須、GPU基数ソート(`wiGPUSortLib`)、モーションベクタ、線形深度、DispatchIndirect、`InterlockedAdd` を多用する GPU-driven プール管理。

---

## 3. 代表コード引用

**(1) VXGIのコーン行進 — 「距離が伸びたらMIPを上げる」だけで軟らかい間接光になる**
`WickedEngine/shaders/voxelConeTracingHF.hlsli:124-137`
```hlsl
float3 p0 = startPos + coneDirection * dist;

float diameter = max(voxelSize0, coneCoefficient * dist);
float lod = clamp(log2(diameter * voxelSize0_rcp), clipmap_index0, VXGI_CLIPMAP_COUNT - 1);

float clipmap_index = floor(lod);
float clipmap_blend = frac(lod);

VoxelClipMap clipmap = GetFrame().vxgi.clipmaps[clipmap_index];
float3 tc = GetFrame().vxgi.world_to_clipmap(p0, clipmap);
if (!is_saturated(tc))
{
    clipmap_index0++;
    continue;
}
```

**(2) DDGIの光漏れ対策 — 深度2モーメントによるChebyshev可視性**
`WickedEngine/shaders/ShaderInterop_DDGI.h:227-241`
```hlsl
half dist_to_probe = length(probe_to_point);

half2 temp = bindless_textures_half4[...].SampleLevel(sampler_linear_clamp, tex_coord, 0).xy;
half mean = temp.x;
half variance = abs(sqr(temp.x) - temp.y);

// http://www.punkuser.net/vsm/vsm_paper.pdf; equation 5
half chebyshev_weight = variance / (variance + sqr(max(dist_to_probe - mean, 0.0)));

// Increase contrast in the weight 
chebyshev_weight = max(pow(chebyshev_weight, 3), 0.0);

weight *= (dist_to_probe <= mean) ? 1.0 : chebyshev_weight;
```

**(3) DDGIの積分核 — レイ方向とテクセル方向の内積だけ**
`WickedEngine/shaders/ddgi_updateCS.hlsl:144-158`
```hlsl
half weight = saturate(dot(texel_direction, ray.direction));
#ifdef DDGI_UPDATE_DEPTH
    weight = pow(weight, 64);
#endif

if (weight > WEIGHT_EPSILON)
{
#ifdef DDGI_UPDATE_DEPTH
    result += half2(depth, sqr(depth)) * weight;
#else
    result += ray.radiance.rgb * weight;
#endif
    total_weight += weight;
}
```

**(4) Surfelの「生成判定用重み」と「GI用重み」を分ける設計**
`WickedEngine/shaders/surfel_coverageCS.hlsl:194-202`
```hlsl
float radial = saturate(1 - dist2 / sqr(surfel.GetRadius()));
radial *= radial;
coverage += saturate(dotN) * radial;

// Track the nearest same-orientation neighbour (dotN > 0.5 so a
// perpendicular surface in the same cell doesn't count as "too close") for
// the spacing reject.
if (dotN > 0.5)
    nearest_dist2 = min(nearest_dist2, dist2);
```

---

## 4. コード規模の実測比較

| 手法 | CPU側(wiRenderer.cpp) | Scene側(wiScene.cpp) | シェーダ合計 | 合計 | 追加の隠れコスト |
|---|---|---|---|---|---|
| **DDGI** | 207 | 108 | 1337 | **約1650行** | レイトレAPI/BVH必須 |
| **VXGI** | 337 | 68 | 1222 | **約1630行** | `RENDERPASS_VOXELIZE` をオブジェクト描画パイプ全体に配線(数百行が既存コードに散在) |
| **SurfelGI** | 483 | 107 | 2763 | **約3350行** | GPU基数ソート(`wiGPUSortLib`)、モーションベクタ |
| **環境プローブ** | 551(`RefreshEnvProbes` 9295-9845) | — | 211 | **約760行** | ただし551行の大半は「シーンをキューブマップに6面描画する」既存機能の再利用 |

規模の順は **環境プローブ < VXGI ≒ DDGI << SurfelGI(約2倍)**。単一シェーダの最大は `surfel_coverageCS.hlsl` の647行。

---

## 5. アルゴリズムの本質的な違い(初学者向け)

3手法とも「間接光をどこにキャッシュするか」が違うだけ、と読める。

- **DDGI = 空間にグリッド状にプローブを置く**。世界を格子に切り、格子点ごとに「全方向からどれだけ光が来るか」をL1 SH(RGB各4係数=12個の数値)で持つ。ピクセルは周囲8プローブを補間するだけ。**キャッシュの位置がジオメトリと無関係**なので、壁の反対側のプローブを拾って光が漏れる。それを深度の2モーメント(平均と2乗平均)で確率的に遮る、というのが `ddgi_sample_irradiance` の主戦場。データ量は固定(プローブ数だけ)、シーンが複雑でも増えない。
- **VXGI = シーンを3Dテクスチャに焼く**。ジオメトリをラスタライザで3Dテクスチャに書き込み(ボクセル化)、そこに直接光を入れておく。受光点からは円錐(コーン)を伸ばし、遠ざかるほど太いMIPを読むことで「多数のレイを1本で近似」する。**レイトレAPIが要らない唯一の手法**。反面、3Dテクスチャがメモリを食い、細い物体が抜け落ち、鏡面反射も同じ機構で出せる(`ConeTraceSpecular`)。
- **SurfelGI = ジオメトリ表面に円盤を貼る**。画面を見て「まだ円盤が足りない場所」に半径付きの円盤(サーフェル)を動的に生成し、円盤ごとにレイを飛ばして光をL1 SHで蓄える。**キャッシュが表面に貼り付いている**ので原理的に漏れにくく、密度も画面上のサイズが一定になるよう距離で自動調整される(近く=小さく密、遠く=大きく疎)。代わりに「生成・破棄・空間ハッシュへの登録・レイ予算配分」という**GPU上のメモリ管理を丸ごと自作する**ことになり、それが2763行の大半。

**共通する骨格**は3つとも同じで、これが最大の学び:
①方向別の輝度を **L1 SH(4係数)** に圧縮して持つ、②毎フレーム少しずつ更新して `MultiscaleMeanEstimator` で時間平均する、③**前フレームの自分自身を読んで多重バウンスにする**(DDGI:`ddgi_raytraceCS.hlsl:497`、VXGI:`vxgi_temporalCS.hlsl:63`、Surfel:`surfel_raytraceCS`)。

**最も簡単な環境プローブとの差**は「位置依存性があるか」。環境プローブはキューブマップ1枚(=空間1点の全方向輝度)を粗糙度別にGGXフィルタするだけ(`filterEnvMapCS.hlsl`、129行)で、**位置による変化を持たない**。GIの3手法はいずれも「位置ごとに違う間接光」を持つためのデータ構造を追加しているのであって、光の評価式自体は環境プローブと大差ない。

---

## 6. hikariへの示唆

前提: hikari は現在 OpenGL 3.3 / C++17。**OpenGL 3.3 にはコンピュートシェーダが無い**(4.3以降)。3手法とも実質コンピュートシェーダ前提なので、GI に進むならまず **OpenGL 4.3+ への引き上げ**が事実上の必須条件。これが最大の分岐点。

| 手法 | 簡易版の最小構成 | 簡易難易度 | フル版難易度 | 前提知識 |
|---|---|---|---|---|
| **環境プローブ/IBL** | HDR画像1枚 → 放射照度マップ(コサイン畳み込み)+ 粗糙度別プレフィルタ + BRDF LUT。オフラインで焼いてよい | **2** | 3 | キューブマップ、重点サンプリング、GGX |
| **VXGI** | 静的シーンをCPUで一度だけボクセル化(3Dテクスチャ 64³、RGBA8)→直接光を各ボクセルに書く→MIPを生成→フラグメントシェーダで6〜16コーン行進 | **3.5** | 5 | 3Dテクスチャ、MIP、レイマーチ、アルファ合成 |
| **DDGI** | プローブ格子(例 8×4×8)→各プローブから既存のラスタライザで小さいキューブマップを6面レンダ→オクタ投影して1テクスチャに詰める→トライリニア補間で読む(可視性テストは後回し) | **4** | 5 | 球面調和 or オクタ写像、プローブ補間、テクスチャアトラス |
| **SurfelGI** | (簡易版が成立しにくい) | **5** | 5+ | GPU-driven プール管理、アトミック、空間ハッシュ、Indirect Dispatch |

**判断材料(初学者がまず作るならどれか)**

1. **就活作品の費用対効果で見るなら、順序は「IBL → シンプルVXGI」が最善**と読み取れる。理由は3つ:
   - IBL(第2フェーズ)は Phong→PBR 移行と同時に必要になり、**GIの共通骨格(方向別輝度をどう圧縮して持つか)を最小コストで学べる**。約200行のシェーダで見た目が劇的に変わる。
   - VXGI の簡易版は **レイトレーシングAPIもBVHも要らない**(hikari に自前BVHが無い前提では決定的)。DDGI/Surfel はどちらも `if (!scene.TLAS.IsValid() && !scene.BVH.IsValid()) return;`(`wiRenderer.cpp:12606`, `12325`)で始まっており、レイトレ基盤が前提。
   - VXGI簡易版なら「静的シーン・単一クリップマップ・直接光のみ・MIP生成はglGenerateMipmap任せ」まで削れる。Wicked の1222行のうち、クリップマップ6段(約400行)・異方性6面(約200行)・SDFジャンプフラッド(52行)・時間ブレンド(128行)は**全部後回しにできる**。残り約300〜400行が本質。
2. **DDGIを選ぶ場合の抜け道**: レイトレの代わりに「プローブ位置から6面の小キューブマップをラスタライズする」という古典的手法にすれば、hikari の既存シャドウマップ/キューブマップ基盤を流用できる。ただしプローブ数×6面のレンダは重く、動的更新が現実的でないため「静的GIのベイク」に用途が限定される。難易度は4だが、**シャドウマッピングを終えた直後の次の一歩としては素直**。
3. **SurfelGI は非推奨**。コード量が2倍、しかも本質的な難しさが「レンダリング」ではなく「GPU上での動的メモリ管理」に寄っている。就活作品として説明しづらく、デバッグが極端に難しい(`SURFEL_DEBUG` が7種類も用意されているのが証拠)。
4. **フェーズ配分の具体案**(提案): 第2フェーズ=PBR+IBL(難易度2、確実に完成)、第3フェーズ=Deferred(難易度2.5)、第4フェーズ=静的VXGI(難易度3.5)。第4フェーズを「DDGIベイク版」に差し替えるのも可。

**Wickedから盗むべき小技**(実装難易度は低いが効果が高い)
- GIは**半解像度で計算して深度ガイド付きバイラテラルアップサンプル**(`Postprocess_Upsample_Bilateral`、VXGI/Surfel双方が使用)。これだけで4倍速い。
- 輝度は**L1 SH(4係数)**で持つ。オクタ球テクセルより圧倒的に軽く、実装も短い。
- `MultiscaleMeanEstimator`(`ShaderInterop_DDGI.h:390-443`、54行)は**そのまま移植できる汎用の時間積分器**。ファイアフライ抑制も内蔵。
- half精度のオーバーフロー(sky/太陽で `+Inf` → `Inf-Inf=NaN` → 画面全体が白飛び)が実際に事故になっており、`saturateMediump` やクランプが随所に入っている(`ShaderInterop_DDGI.h:269-280`、`ShaderInterop_SurfelGI.h:58`)。GIを自作するときは**最初からHDR値をクランプする**。

---

## 7. 確信度と限界

**検証済み(実際にReadした)**
- 3手法のCPU側ディスパッチ列と順序、リソース構造体の全メンバー、`ShaderInterop_*.h` の全データ構造、`ddgi_rayallocationCS`/`ddgi_updateCS`/`vxgi_temporalCS`/`voxelConeTracingHF.hlsli`/`voxelHF.hlsli` は全文読了。行番号はすべてRead結果に基づく。
- 行数は `wc -l` の実測値。CPU側の関数行数は関数開始行と `^}` の位置から算出。

**部分的にしか読んでいない(Grepでアウトラインのみ確認)**
- `ddgi_raytraceCS.hlsl` の光源サンプリング部(56-490行の大半)。DDGI/Surfel双方で同型の巨大なライトタイプ分岐が繰り返されており、通読していない。
- `surfel_coverageCS.hlsl` の 246-647行(LDSキャッシュ管理と生成判定の詳細)、`surfel_updateCS.hlsl` のリサイクル確率計算の式、`surfel_raytraceCS.hlsl` のヒット処理。フローは把握したが式レベルでは未検証。
- `objectPS_voxelizer.hlsl` の 1-88行(マテリアル読み出し)と 232-334行(非GS経路の3面書き込み)。
- `surfel_denoiseCS.hlsl`、`vxgi_offsetprevCS.hlsl`、`vxgi_sdf_jumpfloodCS.hlsl`、`vxgi_resolve_*.hlsl` は未読(行数のみ)。
- `objectGS_voxelizer.hlsl`(92行)は未読。支配軸選択をしていると推測。

**推測で書いた部分(明示)**
- 「VXGI の radiance テクスチャが 2.2GB 相当」は `wiScene.cpp:686-694` のdescから**計算した値**で、実測ではない。`res=64` が既定だが実運用値は未確認。この数字は誇張の可能性があるため、hikari の計画に使う際は実測すべき。
- 「VXGI簡易版の本質は300〜400行」は各シェーダの削減可能部分からの**見積もり**であり、実装して確かめてはいない。
- 難易度の1〜5評価は依頼者の基準(3=シャドウマッピング相当)に**私が主観で対応づけたもの**。特にVXGI簡易版の3.5は、3Dテクスチャへの書き込み(OpenGLでは `imageStore` = GL4.2以降)の習得コストをどう見るかで4になり得る。
- 「`RENDERPASS_VOXELIZE` の配線が数百行」は Grep のヒット箇所数(`wiRenderer.cpp` 内で約12箇所)からの**印象値**で、正確に数えていない。

**確認できていない重要事項**
- 3手法の**実行時パフォーマンス比較**はコードからは読めない。プロファイラのレンジ名(`wi::profiler::BeginRangeGPU`)が仕込まれているだけで、実測値は含まれない。
- 各手法の**画質の差**も同様にコードからは判断不能。