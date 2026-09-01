> 作成: 2026-08-20 / AI読解エージェントによる実ソースコード読解記録
> 対象スナップショット: Godot (855e9ad) / ライセンス: MIT(コード引用は出典明記のうえ)
> 行番号はこのスナップショット時点のもの。最新版ではズレる。代表引用の一部はメインAIが実コードと突き合わせて一致を確認済み(全数検証ではない)。各記録末尾の「確信度と限界」を必ず読むこと。

# Godotポストプロセス実装 読解レポート

## 結論(先出し)

Godotのポストプロセス(`servers/rendering/renderer_rd/effects/`)は、**コンピュートシェーダー版(デスクトップ/Forward+)とラスタ版(モバイル)の2系統が並存**する構成。Bloom/Glowは自作ミップピラミッド(コンピュート版は「ブラー+ダウンサンプルを1パスに融合」、モバイル版はCall of Duty式dual-filter)、SSAO/SSILは**Intel ASSAO 2016をそのまま移植**した本格実装、TAAは外部OSS(Spartan Engine)由来の分散クリッピング方式、トーンマッパーはLinear/Reinhard/Filmic/ACES/AgXの5種切り替え。hikariへの移植難易度は後半に個別評価した(結論: Bloomは2、Tonemapは1〜3、TAAは4〜5、SSAOは簡易版なら3・Godot完全再現は5+)。

---

## 1. 読んだファイル一覧

| パス(godotルートから) | 概算行数 | 読んだ範囲 |
|---|---|---|
| `servers/rendering/renderer_rd/renderer_scene_render_rd.cpp` | 1928行 | 460〜745行(Glow/Tonemap/DOF呼び出し部)のみ |
| `servers/rendering/renderer_rd/storage_rd/render_scene_buffers_rd.cpp` | 865行 | 505〜565行(`allocate_blur_textures`) |
| `servers/rendering/renderer_rd/forward_clustered/render_forward_clustered.cpp` | 5313行 | grep検索のみ(SSAO呼び出し行の特定) |
| `servers/rendering/renderer_rd/effects/copy_effects.cpp` | 1486行 | 736〜945行(gaussian_blur / gaussian_glow系) |
| `servers/rendering/renderer_rd/effects/tone_mapper.h` | 217行 | 全読 |
| `servers/rendering/renderer_rd/effects/taa.cpp` / `.h` | 130行/60行 | 全読 |
| `servers/rendering/renderer_rd/effects/ss_effects.cpp` | 1811行 | 502〜625行、1045〜1420行(SSAO系。SSR/SSSは未読) |
| `servers/rendering/renderer_rd/effects/bokeh_dof.cpp` / `.h` | 503行/120行 | .h全読、.cpp 93〜298行(compute版のみ、raster版未読) |
| `servers/rendering/renderer_rd/shaders/effects/copy.glsl` | 294行 | 1〜225行 |
| `servers/rendering/renderer_rd/shaders/effects/blur_raster.glsl` | 234行 | 全読 |
| `servers/rendering/renderer_rd/shaders/effects/tonemap.glsl` | 955行 | 80〜240行、340〜470行、855〜955行 |
| `servers/rendering/renderer_rd/shaders/effects/taa_resolve.glsl` | 385行 | 全読 |
| `servers/rendering/renderer_rd/shaders/effects/ss_effects_downsample.glsl` | 224行 | 1〜70行 |
| `servers/rendering/renderer_rd/shaders/effects/ssao.glsl` | 481行 | 200〜360行(コアgather関数) |
| `servers/rendering/renderer_rd/shaders/effects/ssil.glsl` | 440行 | 1〜40行+grep(未深読) |
| `servers/rendering/storage/environment_storage.cpp` | (該当部分のみ) | grep(トーンマップパラメータCPU計算の存在確認) |

---

## 2. 機能→実装の対応記録

### 2-1. Bloom / Glow

**(a) 主要ファイルと行範囲**
- 呼び出し元: `renderer_scene_render_rd.cpp:580-673`(生成)、`691-721`(トーンマップへの受け渡し)
- 実処理: `effects/copy_effects.cpp:810-946`(`gaussian_glow` / `gaussian_glow_downsample_raster` / `gaussian_glow_upsample_raster`)
- シェーダー: `shaders/effects/copy.glsl`(コンピュート版、`MODE_GLOW`)、`shaders/effects/blur_raster.glsl`(ラスタ版、`MODE_GLOW_GATHER/DOWNSAMPLE/UPSAMPLE`)

**(b) 中心データ構造**
- `RB_TEX_BLUR_0`(フル解像度、ミップ`mipmaps_required`段)と`RB_TEX_BLUR_1`(半解像度、`mipmaps_required-1`段)の2枚を`allocate_blur_textures()`(`render_scene_buffers_rd.cpp:513-535`)で確保。Glowはこのミップチェーンを流用する(専用バッファを別に持たない)。
- `MAX_GLOW_LEVELS = 7`(`tone_mapper.h:117` `glow_levels[7]`)。各レベルの寄与率をユーザーが指定し、`max_glow_index`(有効な最大レベル)まで処理。

**(c) 処理フロー(コンピュート版、実際の呼び出し順)**
1. `renderer_scene_render_rd.cpp` が `environment_get_glow_enabled()` を確認 → `RB_TEX_BLUR_1`のレベル0へ`copy_effects->gaussian_glow(internal_texture, ...)` を呼ぶ(初回のみ`first_pass=true`でHDR輝度キャップ・閾値処理も同時実行)
2. `for i in 1..max_glow_index`: 直前のレベルを`source`、次のレベルを`dest`として`gaussian_glow`を再帰的に呼び、**ミップごとに縮小しながらガウスブラーをかける**(`copy.glsl:96-205`の`MODE_GAUSSIAN_BLUR`ブロックが、宛先サイズでUV正規化してソースをサンプルするため、ブラー+ダウンサンプルが1回のディスパッチで完結)
3. `tone_mapper`側で`gather_glow()`(`tonemap.glsl:371-406`)が全ミップレベルを`glow_levels[i]`の重みで加算合成(=**アップサンプルは行わず、トーンマップパス内でミップ全段を直接サンプルして重み付き加算**する設計)

**(c') 処理フロー(ラスタ版=モバイル、CoD式dual-filter)**
1. `gaussian_glow_downsample_raster`(first_pass): 4タップ閾値抽出しつつ1/4解像度へ直接ダウンサンプル(1レベル飛ばす)
2. 以降のレベルは`BloomDownKernel4`(`blur_raster.glsl:74-90`)で4タップボックスフィルタ縮小を繰り返す
3. 最奥レベルから`gaussian_glow_upsample_raster`を呼び、`BloomUpKernel4`(`blur_raster.glsl:97-133`)のテントフィルタで拡大しながら**1段上のダウンサンプル結果と加算ブレンド**(`use_blend_color`)を繰り返し、最終的にフル解像度へ戻す
4. こちらは**明示的なアップサンプル+加算パス**を持つ古典的Bloom(Call of Duty: Advanced Warfare由来、コメントに明記)

**(d) GPUシェーダー**
- `shaders/effects/copy.glsl`(コンピュート、`MODE_GLOW`定義部、共有メモリ16x16タイルで9タップ分離ガウス)
- `shaders/effects/blur_raster.glsl`(フラグメントシェーダー、`MODE_GLOW_GATHER/DOWNSAMPLE/UPSAMPLE`)

**(e) 依存サブシステム**
- 自動露出(`Luminance`クラス、`luminance->get_current_luminance_buffer`)とHDR輝度キャップ・しきい値でオートエクスポージャー連携
- `ToneMapper`(最終合成)、`RenderSceneBuffersRD`(バッファ管理)

**hikariへの示唆(Bloom)**: 簡易版=**難易度2**(単一閾値抽出→4〜6段ミップのバイリニアダウンサンプル→テントフィルタでアップサンプル加算、専用シェーダー2〜3本で済む)。フル版(HDR輝度キャップ、5種ブレンドモード、ビキュービックアップサンプル、auto exposure連携)は**難易度4**。前提知識: 浮動小数点HDR FBO、複数解像度のFBO/RTチェーン管理、加算ブレンド。

---

### 2-2. トーンマッピング

**(a) 主要ファイルと行範囲**
- C++設定: `effects/tone_mapper.h`(全217行、`TonemapSettings`構造体)、`environment_storage.cpp:276-343`(`environment_get_tonemap_parameters` — CPU側でカーブ係数を事前計算)
- シェーダー: `shaders/effects/tonemap.glsl`(955行、デスクトップ用)、`tonemap_mobile.glsl`(未読、サブパス統合版)

**(b) 中心データ構造**
- `TonemapPushConstant`(`tone_mapper.h:104-125`): `tonemapper`(種類ID)、`tonemapper_params[4]`(カーブごとの係数、CPU計算済み)、`glow_levels[7]`、`glow_mode`、`output_max_value`等
- `TONEMAPPER_LINEAR/REINHARD/FILMIC/ACES/AGX`の5種(`tonemap.glsl:240-244`)

**(c) 処理フロー(`tonemap.glsl:855-954`、フラグメントシェーダー1本の中の実行順)**
1. ソースカラー取得 → `luminance_multiplier`適用
2. 露出適用(手動 or `source_auto_exposure`テクスチャから自動)
3. FXAA(**Glowより先** — 「bleed効果を保持するため」とコメントあり)
4. Glow適用(pre-tonemap。ただし`GLOW_MODE_SOFTLIGHT`のみ後述の理由でここではスキップ)
5. `apply_tonemapping()` → 5種のいずれかのカーブ関数を呼ぶ(`tonemap.glsl:247-262`の分岐)
6. Glow適用(post-tonemap、**SOFTLIGHTモードのみ**。理由: 1.0超で不連続になる問題をトーンマップ後に回すため)
7. BCS(明度/コントラスト/彩度)+ カラー補正LUT(1D or 3D)
8. sRGB変換
9. 8bitデバンディング(ディザ)
10. 出力

**(d) GPUシェーダーの各カーブ実装(`tonemap.glsl`)**
- Reinhard拡張: 92-97行、`color*(1+color/white²)/(1+color/max_value)`
- Filmic(Uncharted2系): 99-115行、定数A〜F+exposure_bias=2.0
- ACES: 117-144行、RRT/ODT行列2つ+MJP版係数
- AgX: 175-228行、Rec.709→Rec.2020インセット行列 → 独自「allenwp curve」(149-173行、Godot独自開発、2025年5月のブログ記事が出典)→ アウトセット行列で戻す

**(e) 依存サブシステム**: Glow(2-1)、Luminance(自動露出)、Environment(RenderingServer側のトーンマップ設定API)

**hikariへの示唆(Tonemap)**: Reinhard拡張 or ACES近似1種類のみなら**難易度1**(数式10行程度)。5種切り替え+AgXの行列変換まで再現するなら**難易度3**。前提知識: HDR→LDRの概念とsRGBガンマ補正のみで、シェーダーコード自体は短いため**費用対効果が非常に高い機能**(hikariのPhongパイプラインにReinhard拡張だけでも入れる価値は大きい)。

---

### 2-3. TAA(Temporal Anti-Aliasing)

**(a) 主要ファイルと行範囲**
- C++: `effects/taa.cpp`(全130行、非常にコンパクト)
- シェーダー: `shaders/effects/taa_resolve.glsl`(全385行、実質的にTAAの全ロジックがここに集約)
- 出典: ファイル冒頭コメントに明記の通り、Panos Karabelas氏の Spartan Engine の`temporal_antialiasing.hlsl`を移植したもの(MITライセンス)

**(b) 中心データ構造**
- `taa`スコープに`history`(前フレーム確定色)、`temp`(作業用)、`prev_velocity`(前フレーム速度)の3テクスチャ(`taa.cpp:98-107`)
- 共有メモリ`tile_color[10][10]` / `tile_depth[10][10]`(8x8スレッドグループ+境界1px、`taa_resolve.glsl:76-93`)

**(c) 処理フロー(実際の関数呼び出し順)**
1. `TAA::process()`(`taa.cpp:90`)が毎フレーム呼ばれ、初回のみバッファ確保。2フレーム目以降のみ`resolve()`を実行(初回は履歴が無いためコピーのみ)
2. `resolve()`(`taa.cpp:51`)がコンピュートシェーダー`taa_resolve.glsl`をディスパッチ
3. シェーダー内`main()`→`populate_group_shared_memory()`(近傍9x9タイルを共有メモリへロード)→`temporal_antialiasing()`を呼ぶ
4. `temporal_antialiasing()`内部: `get_closest_pixel_velocity_3x3()`(3x3で最も手前の深度の速度ベクトルを採用=Catmull-Rom版TAAの定番テク)→ `sample_catmull_rom_9()`(履歴バッファをCatmull-Romで再投影サンプル、ブラー低減)→ `clip_history_3x3()`(**分散クリッピング**、NVIDIA GDC2016論文の手法)→ 輝度差でブレンド係数を動的調整 → Reinhardトーンマップ空間で線形補間(HDRでのゴースト低減)→ 逆トーンマップ
5. `TAA::process()`に戻り、`copy_effects->copy_to_rect()`で結果を`internal_texture`へ書き戻し、`history`と`prev_velocity`を更新

**(d) GPUシェーダー**: `shaders/effects/taa_resolve.glsl`(コンピュートのみ、ラスタ版なし=TAAはモバイルでは提供されない)

**(e) 依存サブシステム**: モーションベクターバッファ(`RB_TEX_VELOCITY`、別途`motion_vectors_store`が書き込み、今回未読)、深度バッファ

**hikariへの示唆(TAA)**: ジッターのみ+単純指数移動平均なら**難易度2**だがゴースト過多で実用性は低い。Godot相当(分散クリッピング+速度再投影+Catmull-Rom履歴サンプル)まで到達するには**難易度4〜5**。最大の前提: **モーションベクターパス自体が別途必要**(前フレームMVPで頂点を再投影する追加シェーダーパス)。これがTAA実装の真のボトルネックで、TAA本体より前提のGバッファ拡張の方が工数がかかる可能性が高い。

---

### 2-4. SSAO(Screen Space Ambient Occlusion)

**(a) 主要ファイルと行範囲**
- C++: `effects/ss_effects.cpp:502-625`(深度ダウンサンプル)、`1045-1420`(SSAO本体、`generate_ssao`)
- シェーダー: `shaders/effects/ss_effects_downsample.glsl`、`ssao.glsl`(481行)、`ssao_blur.glsl`、`ssao_importance_map.glsl`、`ssao_interleave.glsl`
- **出典が明記**: `ssao.glsl`冒頭コメントに`Copyright (c) 2016, Intel Corporation` `filip.strugar@intel.com` — **Intel の ASSAO(Adaptive Screen Space Ambient Occlusion)をそのまま移植**したもの

**(b) 中心データ構造**
- `RB_LINEAR_DEPTH`: 半解像度×4スライス(2x2デインターリーブ=チェッカーパターン分割)×最大5ミップの深度バッファ(`ss_effects.cpp:514`)
- `RB_DEINTERLEAVED` / `RB_DEINTERLEAVED_PONG`: AO値の作業バッファ(4スライス×ビュー数)
- `RB_IMPORTANCE_MAP`: Ultra品質時のみ生成される「適応サンプル数マップ」
- `sample_pattern[]`(`ssao.glsl`): 品質レベル別サンプル数`num_taps[5] = {3, 5, 12, 0, 0}`(片側。左右対称サンプルで実質2倍)

**(c) 処理フロー(実際の関数呼び出し順、`_pre_opaque_render`から)**
1. `render_forward_clustered.cpp:1629`: 深度+法線プリパスの直後に`ss_effects->downsample_depth()`を呼ぶ(`ss_effects.cpp:502`)。品質設定に応じ「半解像度のみ」「3ミップ」「5ミップ(遠距離大半径サンプル用)」を切替生成
2. `render_forward_clustered.cpp:1633`: `_process_ssao()` → `ss_effects->generate_ssao()`(`ss_effects.cpp:1127`)
3. (Ultra品質のみ)ベースパスで粗くAOを計算 → `importance_map`生成(2段階の平滑化処理A/B) → ロードカウンタで適応サンプル数を決定
4. `gather_ssao()`(`ss_effects.cpp:1054`)を4回(4スライス分)呼び、`ssao.glsl`の`generate_SSAO_shadows_internal()`(247行〜)を実行。**法線でチルトした円盤上に最大12点(×2で24)を配置**し、各点で`SSAOTap()`(207行)→ 深度差から遮蔽度計算。エッジは深度ベースと法線ベースの2種で検出し、ブリード防止
5. `ssao_blur.glsl`でエッジアウェアブラー(通常2パス、`SSAO_BLUR_PASS_WIDE`→`_SMART`)
6. `ssao_interleave.glsl`で4スライスをフル解像度へ再インターリーブし`RB_FINAL`へ格納

**(d) GPUシェーダー**: 上記5ファイル(いずれもIntel ASSAO 2016の直接移植、`clayjohn`によるVulkan/Godot移植コメントあり)

**(e) 依存サブシステム**: 法線バッファ(Gバッファ的な法線プリパスが前提)、深度バッファ

**hikariへの示唆(SSAO)**: **Godotのフル版(適応サンプル数、5ミップ長距離サンプル、法線チルト、2種エッジ検出)は難易度5+** — これは研究レベルの実装で、多くの商用エンジンもここまではやらない。一方、**一般的な「半球サンプリングSSAO」(Crysis/LearnOpenGL方式: 8〜16サンプル+ランダム回転ノイズ+ブラー)なら難易度3(シャドウマッピング相当)**で、hikariが目指すべきはこちらが現実的。前提知識: ビュー空間深度復元、法線バッファ、TBN的な回転ノイズ生成。

**補足(SSIL)**: `ssil.glsl`冒頭コメントに`2021-05-27: clayjohn: convert SSAO to SSIL`とあり、**SSAOと全く同じデインターリーブ基盤を再利用**し、遮蔽度の代わりに前フレームカラーバッファ(`last_frame`、`ssil.glsl:85`)をリプロジェクションしてサンプルする设計(間接光近似)。SSAO基盤があれば追加難易度は+1程度と推測(本体アルゴリズムは未深読のため状況証拠)。

---

### 2-5. DOF(被写界深度、参考)

**(a)** `effects/bokeh_dof.cpp:93-298`(compute版)、`.h`(全121行)
**(b)** `BokehBuffers`構造体(base/secondary/half×2のテクスチャ)。`BokehMode`列挙で`GEN_BLUR_SIZE→GEN_BOKEH_BOX/HEXAGONAL/CIRCULAR→COMPOSITE`
**(c)** 1st: `BOKEH_GEN_BLUR_SIZE`でアルファチャンネルにCoC(ボケ円サイズ)を書き込み(遠側+/近側-の符号)。2nd/3rd: box/hexagonは分離2パスブラーをCoC重み付きで実行(品質により半解像度)、circleは単一パスのポワソン風サンプリング。4th: `BOKEH_COMPOSITE`でアップスケール合成
**(d)** `bokeh_dof.glsl`(未深読)、`bokeh_dof_raster.glsl`(モバイル版、未読)
**(e)** 深度バッファ、カメラのnear/far

**hikariへの示唆(DOF)**: CoCベースの単純ガウスぼかし(near/farのみ、形状無視)なら**難易度2**。box/hex/circleのボケ形状+half-res最適化まで含めるなら**難易度4**。

---

### 2-6. `effects/`ディレクトリ全体マップ(概観、名前とヘッダーのclass宣言から)

| ファイル | 役割 |
|---|---|
| `copy_effects.*` | 汎用コピー・ガウスブラー・Glow・ミップ生成(1486行、最大のユーティリティ集) |
| `tone_mapper.*` | トーンマップ+Glow合成+FXAA等の最終パス |
| `taa.*` | TAA |
| `ss_effects.*` | SSAO/SSIL/SSR(スクリーンスペース反射)/SSS(未読) |
| `bokeh_dof.*` | DOF |
| `smaa.*` | SMAA(空間AA、未読) |
| `fsr.*` / `fsr2.*` | AMD FSR1/FSR2アップスケーラー(未読) |
| `metal_fx.*` | Apple MetalFXアップスケーラー(未読) |
| `luminance.*` | 自動露出用の輝度リダクション(未読) |
| `roughness_limiter.*` | 鏡面エイリアシング対策(未読) |
| `motion_vectors_store.*` | モーションベクター書き込み(未読) |
| `resolve.*` | MSAA深度/法線リゾルブ(未読) |
| `sort_effects.*` | GPUソート(未読) |
| `debug_effects.*` | デバッグ表示(未読) |
| `vrs.*` | Variable Rate Shading(未読) |
| `spatial_upscaler.h` | アップスケーラー共通インターフェース |

---

## 3. 代表コード引用

**① Bloomのしきい値+HDR輝度キャップ(コンピュート版共通処理)**
`servers/rendering/renderer_rd/shaders/effects/copy.glsl:188-200`
```glsl
color *= params.glow_strength;

if (bool(params.flags & FLAG_GLOW_FIRST_PASS)) {
    color *= params.glow_exposure;

    float max_value = max(color.r, max(color.g, color.b));
    float feedback = max(smoothstep(params.glow_hdr_threshold, params.glow_hdr_threshold + params.glow_hdr_scale, max_value), params.glow_bloom);

    color = min(color * feedback, vec4(params.glow_luminance_cap));
}
```
学び: 閾値カットではなく`smoothstep`で滑らかにフェードインさせ、`glow_bloom`(常時ブレンド量)と`max()`を取ることでバニラの「しきい値以下は完全ゼロ」による帯状アーティファクトを回避している。

**② モバイル版Bloomダウンサンプル(Call of Duty式dual-filter)**
`servers/rendering/renderer_rd/shaders/effects/blur_raster.glsl:75-89`
```glsl
vec2 RcpSrcTexRes = blur.source_pixel_size;
vec2 tc = (uv0 * 2.0 + 1.0) * RcpSrcTexRes;
float la = 1.0 / 4.0;
vec2 o = (0.5 + la) * RcpSrcTexRes;

vec4 c = vec4(0.0);
c += textureLod(Tex, tc + vec2(-1.0, -1.0) * o, 0.0) * 0.25;
c += textureLod(Tex, tc + vec2(1.0, -1.0) * o, 0.0) * 0.25;
c += textureLod(Tex, tc + vec2(-1.0, 1.0) * o, 0.0) * 0.25;
c += textureLod(Tex, tc + vec2(1.0, 1.0) * o, 0.0) * 0.25;
```
学び: 4タップだけで蚊柱アーティファクトを抑えたボックスダウンサンプル。hikariで最初に実装すべきBloomはこのレベルで十分。

**③ Glow SCREENブレンドの数式簡略化**
`servers/rendering/renderer_rd/shaders/effects/tonemap.glsl:424,433-436`
```glsl
glow.rgb = clamp(glow.rgb, 0.0, white);

// The following is a mathematically simplified version of the above.
color.rgb = color.rgb + glow.rgb - (color.rgb * glow.rgb / white);

return color;
```
学び: `screen(a,b) = a+b-a*b`を「white」でレンジ正規化した形。コメントに残された簡略化前の式(`/white`→合成→`*white`)も含め、シェーダー最適化の実例として学びが多い。

**④ TAAの分散クリッピング(ゴースト抑制の核心)**
`servers/rendering/renderer_rd/shaders/effects/taa_resolve.glsl:277-289`
```glsl
vec3 color_avg = (s1 + s2 + s3 + s4 + s5 + s6 + s7 + s8 + s9) * RPC_9;
vec3 color_avg2 = ((s1*s1)+(s2*s2)+(s3*s3)+(s4*s4)+(s5*s5)+(s6*s6)+(s7*s7)+(s8*s8)+(s9*s9)) * RPC_9;
float box_size = mix(0.0f, params.variance_dynamic, smoothstep(0.02f, 0.0f, length(velocity_closest)));
vec3 dev = sqrt(abs(color_avg2 - (color_avg * color_avg))) * box_size;
vec3 color_min = color_avg - dev;
vec3 color_max = color_avg + dev;

vec3 color = clip_aabb(color_min, color_max, clamp(color_avg, color_min, color_max), color_history);
color = clamp(color, FLT_MIN, FLT_MAX);
```
学び: 3x3近傍の平均と二乗平均から分散(標準偏差)を求め、それをAABBの箱サイズとして履歴色をクリップする「NVIDIA GDC2016」方式。単純なmin/maxクランプよりゴーストとちらつきのバランスが良い。

---

## 4. hikariへの示唆(まとめ表)

| 機能 | 簡易版 最小構成 | 簡易版難易度(1-5) | フル版難易度 | 前提知識 |
|---|---|---|---|---|
| Bloom/Glow | 閾値抽出→ミップ4-6段ダウンサンプル→テントアップサンプル加算 | **2** | 4 | HDR FBO、ミップ管理 |
| Tonemap(1種) | Reinhard拡張 or ACES近似の数式のみ | **1** | 3(5種+AgX) | HDR→LDR、sRGB補正 |
| TAA | ジッター+履歴ブレンドのみ(ゴースト多発) | 2(実用性低) | **4〜5** | モーションベクターパス(前提が別途重い) |
| SSAO | 半球サンプリング(8-16サンプル+ノイズ+ブラー) | **3**(シャドウマッピング相当) | 5+(Intel ASSAO相当) | ビュー空間深度復元、法線バッファ |
| SSIL | (SSAO基盤流用+前フレームカラー再投影) | SSAO+1 | 5+ | SSAO実装済みが前提 |
| DOF | CoCベース単純ガウスぼかし | **2** | 4(ボケ形状対応) | 深度線形化 |

**優先度の提案**(あくまで参考、判断はユーザー): 費用対効果で見るとTonemap(Reinhard拡張)が最も低コスト・高見栄えで、次点でBloom簡易版。TAAはモーションベクターという別の大きな前提機能を要求するため、第2〜3フェーズでは後回しにしてSSAO(半球サンプリング版)を優先する方が学習曲線として自然に見える。

---

## 5. 確信度と限界

**検証済み(コードを直接読み、呼び出し順を実際に追った)**
- Bloom/Glowのミップ生成方式(2系統とも)、トーンマップの処理順序(FXAA→Glow→カーブ→BCS→sRGB→デバンディング)、TAAの分散クリッピング・履歴サンプリング全体像、SSAOの「深度デインターリーブ→gather→blur→interleave」のパイプライン構造とその呼び出し元(`_pre_opaque_render`)。

**状況証拠(呼び出し側や冒頭コメントから推測、本体シェーダーは未深読)**
- SSILの詳細アルゴリズム(`ssil.glsl`本体420行のうち読んだのは40行のみ。前フレーム色のリプロジェクション計算式自体は未確認)
- `ssao_importance_map.glsl` / `ssao_blur.glsl` / `ssao_interleave.glsl`の中身(C++側の呼び出しパターンから役割は分かるが、シェーダーコード自体は未読)
- モーションベクター生成箇所(`motion_vectors_store.*`、`motion_vectors.glsl`)は未読。TAAの前提となる「ジッターがどこで計算されるか」は`renderer_scene_render_rd.cpp`内で`taa_jitter`が代入される箇所のみ確認し、Halton系列の生成元は未追跡
- `bokeh_dof_raster.cpp/.glsl`(モバイル版DOF)は未読。DOFはcompute版のみ確認

**未読・スコープ外として明示的に除外したもの**
- SSR(スクリーンスペース反射)、SSS(サブサーフェス散乱) — `ss_effects.cpp`に同居しているが依頼スコープ外のため未読
- `tonemap.glsl`のFXAA実装本体(469行以降)、`tonemap_mobile.glsl`、SMAA/FSR/FSR2/MetalFX/VRS/sort_effects/debug_effects/resolve/roughness_limiter/luminanceの各実装 — クラス宣言(ヘッダーのclass名)のみ確認、内部は未読
- `renderer_scene_render_rd.cpp`は1928行中約280行、`render_forward_clustered.cpp`(5313行)はgrepのみで中身は未読

**覆り得る条件**: 上記「状況証拠」区分の内容は、実際にファイルを読めば処理順序や重み付けの細部が変わる可能性がある。特にSSILとモーションベクター生成は、hikariへの移植時に前提知識として必要になった場合は追加調査を推奨する。