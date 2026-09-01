> 作成: 2026-08-20 / AI読解エージェントによる実ソースコード読解記録
> 対象スナップショット: Godot (855e9ad) / ライセンス: MIT(コード引用は出典明記のうえ)
> 行番号はこのスナップショット時点のもの。最新版ではズレる。代表引用の一部はメインAIが実コードと突き合わせて一致を確認済み(全数検証ではない)。各記録末尾の「確信度と限界」を必ず読むこと。

# Godot Forward+ レンダラー読解記録

## 1. 読んだファイル一覧

| パス(リポジトリルートから) | 概算行数 | 読み方 |
|---|---|---|
| `servers/rendering/renderer_rd/forward_clustered/render_forward_clustered.cpp` | 全5313行 | `_render_scene`(1706-2572)、`_pre_opaque_render`(1519-1685)、`_fill_render_list`(922-1206)、`_fill_instance_data`(807-921)、`_render_list_template`(306-505, 550-629部分)、`setup_added_light/decal/reflection_probe`(1394-1419)を全読。他関数はGrepでシグネチャ一覧のみ把握 |
| `servers/rendering/renderer_rd/forward_clustered/render_forward_clustered.h` | 全855行 | `UNIFORM_SET`列挙(64-67)、`RenderListParameters`(222-259)、`SceneState`(294-413)、`GeometryInstanceSurfaceDataCache`(490-555)、`RenderList`(683-752)を読了。残りは未読 |
| `servers/rendering/renderer_rd/cluster_builder_rd.h` | 全434行 | 全読 |
| `servers/rendering/renderer_rd/cluster_builder_rd.cpp` | 全596行 | 全読 |
| `servers/rendering/renderer_rd/shaders/cluster_render.glsl` | 全164行 | 全読 |
| `servers/rendering/renderer_rd/shaders/cluster_store.glsl` | 全119行 | 全読 |
| `servers/rendering/renderer_rd/shaders/cluster_data_inc.glsl` | 全4行 | 全読 |
| `servers/rendering/renderer_rd/shaders/forward_clustered/scene_forward_clustered.glsl` | 全3130行 | クラスタ消費部のみ部分読(130-230行、2670-2840行、約270行) |

---

## 2. 機能→実装の対応記録

### (A) 1フレームのパス列 — `_render_scene`

**主要ファイル**: `render_forward_clustered.cpp:1706-2572`(単一の巨大関数)

**中心データ**: `RenderDataRD *p_render_data`(このフレームの全入力: カメラ・環境・インスタンス配列・ライト配列など)、`Ref<RenderBufferDataForwardClustered> rb_data`(このビューポート専用のFBO/クラスタビルダー等の永続バッファ)。

**処理フロー(実際に呼ばれる順)**:
1. `_update_sdfgi`(1737) → SDFGIのケード更新
2. クラスタビルダー取得: リフレクションプローブレンダリング中なら`reflection_probe_instance_get_cluster_builder`、通常時は`rb_data->cluster_builder`(1746-1773)
3. `_fill_render_list(RENDER_LIST_OPAQUE, ...)`(1916) → 内部で`RENDER_LIST_MOTION`/`RENDER_LIST_ALPHA`も同時に埋める → 3リストを`sort_by_key()`/`sort_by_reverse_depth_and_priority()`(1917-1919) → `_fill_instance_data`ｘ3(1922-1924)
4. Depth Pre-Pass(条件: `force_depth_pre_pass`かプロジェクト設定、2123-2162): `_post_prepass_render`(GIコンピュートを並列起動、2135)→`_setup_render_pass_uniform_set`(2140)→`_render_list_with_draw_list`(2144、`PASS_MODE_DEPTH`系)→MSAA時は`resolve_effects->resolve_depth/resolve_gi`(2149-2161)
5. Pre Opaque Compositor Effects(2176)
6. `_pre_opaque_render`(2185) — **このフレームの心臓部**。内部で: シャドウパス(`_render_shadow_begin/_render_shadow_pass×N/_render_shadow_process/_render_shadow_end`)、GI(`gi.process_gi`)、SSAO/SSIL(`_process_ssao/_process_ssil`)、SSR(`_process_ssr`)、**クラスタ構築**(`current_cluster_builder->begin()`→ライト/デカール/プローブのコールバック登録→`bake_cluster()`)、ボリュームフォグ(`_update_volumetric_fog`)を実行
7. Opaque Pass(2191-2256): `_setup_environment`→`_setup_render_pass_uniform_set`→`_render_list_with_draw_list`(不透明本体、2226)。モーションベクトル用オブジェクトがあれば追加でMotion Pass(2244-2255)
8. Post Opaque Compositor Effects(2272)、デバッグ描画(VoxelGI/SDFGIプローブ、2275-2296)
9. Sky描画: `sky.draw_sky`(2304)、MSAA resolve(2310-2329)、Post Sky Compositor Effects(2332)
10. Sub-Surface Scattering(`_process_sss`)+ Specular合成(`copy_effects->merge_specular`)(2338-2350)
11. スクリーン/深度テクスチャコピー(不透明パスの結果を透明パスで読むため、2368-2388)、Pre Transparent Compositor Effects(2409)
12. Transparent Pass(2413-2429): `_setup_render_pass_uniform_set(RENDER_LIST_ALPHA,...)`→`_render_list_with_draw_list`
13. MSAA最終Resolve(2437-2447)、SSIL/SSR用フレームバッファコピー(2451-2456)、Post Transparent Compositor Effects(2460)
14. アップスケーリング(FSR2 / MetalFX Temporal / TAA、いずれか排他、2463-2545)
15. `_debug_draw_cluster`→トーンマップ(`_render_buffers_post_process_and_tonemap`、2548-2553)

**依存サブシステム**: シャドウマップ(`_render_shadow_*`)、GI(SDFGI/VoxelGI、`gi.process_gi`)、SSAO/SSIL/SSR(`ss_effects`)、Sky、Compositor Effects(拡張ポイント)、TAA/FSR2アップスケーラ。

---

### (B) クラスタードライトカリング

**主要ファイル**: `cluster_builder_rd.h/.cpp`(全体)、シェーダ`cluster_render.glsl`・`cluster_store.glsl`、消費側`scene_forward_clustered.glsl:164-177, 2694-2815`。

**中心データ構造**:
- `RenderElementData`(`cluster_builder_rd.h:163-171`): ライト/デカール/プローブ1個分のGPU送信用データ(型、near/far到達フラグ、view空間変換行列、スケール)。32バイトアラインの生配列`render_elements[]`にCPU側で積む。
- `ClusterBuilderSharedDataRD`(h:38-134): 全ビューポート共有の**プロキシメッシュ**(アイコスフィア42頂点/コーン99頂点/ボックス8頂点)と3本のシェーダ(render/store/debug)。
- クラスタ格子: `cluster_size=32`(スクリーン32x32px = 1クラスタ)、`divisor=DIVISOR_4`(=4)で**プロキシ描画はスクリーンの1/4解像度**で行う(h:193-195)。深度方向は固定**32スライス**(線形、view Zを`z_far`で割って0-31にクランプ)。

**処理フロー**:
1. `current_cluster_builder->begin(cam_transform, cam_projection, flip_y)`(cpp:415-436、`_pre_opaque_render`から呼ばれる)がview行列・射影・カウンタをリセット。
2. 可視ライト/デカール/反射プローブごとに、`light_storage->update_light_buffers()`等の内部から**コールバック**として`RenderForwardClustered::setup_added_light/_decal/_reflection_probe`(cpp:1394-1419)が呼ばれ、これが`ClusterBuilderRD::add_light`/`add_box`(h:236-416、インライン)を呼んで`render_elements[]`に1件追加(CPUのみ、GPU送信はまだ)。
3. `current_cluster_builder->bake_cluster()`(cpp:438-546)で初めてGPUに送る。2段構成:
   - **Renderパス**(`cluster_render.glsl`): プロキシメッシュ(球/コーン/ボックス)を¼解像度FBOへ、色出力なしのフラグメントシェーダで描く。各フラグメントは自分の属するクラスタ座標と、深度から求めた0-31のビットを、サブグループ演算で同一クラスタのスレッドをまとめてから`atomicOr`で書き込む(「Practical Clustered Shading」SIGGRAPH2017の手法、コード内コメントで直接言及: `cluster_render.glsl:129`)。
   - **Storeパス**(`cluster_store.glsl`、コンピュートシェーダ、クラスタタイル1個=1スレッド): Renderパスの生ビットマスクを読み、要素タイプ(オムニ/スポット/エリア/デカール/反射プローブ)ごとに深度スライス単位の圧縮ビットマスク+min/maxレンジヒントを`cluster_buffer`に書き出す。
4. 消費側(`scene_forward_clustered.glsl:550-552`): `cluster_pos = screen_pixel >> cluster_shift; cluster_z = clamp(-view_z/z_far*32,0,31)` でこのピクセルの属するクラスタを特定 → `cluster_get_item_range()`(164-171)でmin/maxヒントを読んで走査範囲を絞る → `findMSB`でビットを1個ずつ消費しながら実ライト配列(`omni_lights.data[light_index]`等)へジャンプ、`light_process_omni/spot`を呼ぶ。

**特徴**: コンピュートシェーダでAABB対クラスタの総当たり判定を行う一般的な「Tiled/Clustered Forward」実装とは異なり、**GPUラスタライザ自体を使ってライト境界を書き込む**方式。

---

### (C) 描画リストの構築とソート

**主要ファイル**: `_fill_render_list`(cpp:922-1206)、`_fill_instance_data`(cpp:807-921)、`RenderList`構造体(h:683-737)、ソートキー(h:507-531)。

**中心データ**: `GeometryInstanceSurfaceDataCache::sort`(h:507-531)は`uint64_t sort_key1/sort_key2`と、それに重なるビットフィールド(`depth_layer:4, surface_index:8, geometry_id:32, material_id_hi:8` / `material_id_lo:24, shader_id:32, priority:8`)のunion。

**処理フロー**:
1. `_fill_render_list`が全可視`GeometryInstanceForwardClustered`をループし、カメラからの距離で`depth_layer`(0-15、4bit)を算出(cpp:936-969)。
2. サーフェス(サブメッシュ)ごとに`FLAG_PASS_OPAQUE/DEPTH`→`RENDER_LIST_OPAQUE`、`FLAG_PASS_ALPHA`→`RENDER_LIST_ALPHA`、モーションベクトルが必要→`RENDER_LIST_MOTION`へ振り分け(1141-1160)。同一サーフェスが複数リストに登録され得る。
3. `_render_scene`から`render_list[OPAQUE].sort_by_key()`(不透明・sort_key2優先: depth_layer→material→geometry順)、`render_list[ALPHA].sort_by_reverse_depth_and_priority()`(半透明: priority優先、同priorityならカメラから遠い順=奥から手前、h:722-732)を呼ぶ(cpp:1917-1919)。
4. `_fill_instance_data`(807-921)がソート済み配列をGPU用`InstanceData`SSBOへ書き込みつつ、**隣接する要素の`sort_key1/sort_key2`が完全一致**(かつマルチメッシュ/スキンなし)なら`repeats`をカウントし、後段の描画で1回の`draw_list_draw(..., instance_count=repeat)`に畳み込む(878-903)、実質的な自動インスタンシング。

---

### (D) uniform / push constant の受け渡し

**主要ファイル**: `_render_list_template`(cpp:306-625)、`UNIFORM_SET`列挙(h:64-67)、`SceneState::PushConstant/InstanceData`(h:294-413)。

- 4つの固定ディスクリプタセット: `SCENE_UNIFORM_SET=0`(グローバル、フレーム1回)、`RENDER_PASS_UNIFORM_SET=1`(パスごと=cluster_buffer/放射輝度テクスチャ/シャドウアトラス等、`_setup_render_pass_uniform_set`で構築)、`TRANSFORMS_UNIFORM_SET=2`(スケルトン/ブレンドシェイプ、変化時のみ再バインド)、`MATERIAL_UNIFORM_SET=3`(マテリアルパラメータ、変化時のみ再バインド)。3セットとも`_render_list_template`冒頭(313-315)と描画ループ内(564-576)でバインド。
- 位置・回転・GIオフセット等**インスタンス毎の大きなデータはpush constantに乗らない**。`_fill_instance_data`が全インスタンス分を`InstanceData`(h:334-391、transform/prev_transform/gi_offset/flags等)としてSSBOへ事前アップロードし、push constantには**そのSSBOへのインデックス(`base_index`)だけ**を渡す(h:326-332、cpp:356)。
- push constantは`SceneState::PushConstant`(base_index, packed uv_offset, モーションベクトルオフセットx2, ubershader時のみ追加のspecialization定数)で、`draw_list_set_push_constant`はubershaderかどうかでサイズを可変にする(cpp:587-597)。
- ソート済みリストのおかげで頂点/インデックスバッファ・マテリアルセット・トランスフォームセットの再バインドは「前回と違うときだけ」に抑制されている(cpp:517-576の`prev_*`比較群)。

---

## 3. 代表コード引用

**① クラスタ座標とデプススライスのビット計算(ライトカリングの核心)**
`servers/rendering/renderer_rd/shaders/cluster_render.glsl:114-127`
```glsl
	//find the current element in the list and plot the bit to mark it as used
	uint usage_write_offset = cluster_offset + (element_index >> 5);
	uint usage_write_bit = 1 << (element_index & 0x1F);

	uint aux = 0;

	//find the current element in the depth usage list and mark the current depth as used
	float unit_depth = depth_interp * state.inv_z_far;

	uint z_bit = clamp(uint(floor(unit_depth * 32.0)), 0, 31);

	uint z_write_offset = cluster_offset + state.cluster_depth_offset + element_index;
	uint z_write_bit = 1 << z_bit;
```

**② クラスタ消費側: min/maxヒントで走査範囲を絞る**
`servers/rendering/renderer_rd/shaders/forward_clustered/scene_forward_clustered.glsl:164-177`
```glsl
void cluster_get_item_range(uint p_offset, out uint item_min, out uint item_max, out uint item_from, out uint item_to) {
	uint item_min_max = cluster_buffer.data[p_offset];
	item_min = item_min_max & 0xFFFFu;
	item_max = item_min_max >> 16;

	item_from = item_min >> 5;
	item_to = (item_max == 0) ? 0 : ((item_max - 1) >> 5) + 1; //side effect of how it is stored, as item_max 0 means no elements
}

uint cluster_get_range_clip_mask(uint i, uint z_min, uint z_max) {
	int local_min = clamp(int(z_min) - int(i) * 32, 0, 31);
	int mask_width = min(int(z_max) - int(z_min), 32 - local_min);
	return bitfieldInsert(uint(0), uint(0xFFFFFFFF), local_min, mask_width);
}
```

**③ 描画順ソートキー(なぜdepth_layerが最優先か、コメント付き)**
`servers/rendering/renderer_rd/forward_clustered/render_forward_clustered.h:520-531`
```cpp
			// Sorted based on optimal order for respecting priority and reducing the amount of rebinding of shaders, materials,
			// and geometry. This current order was found to be the most optimal in large projects. If you wish to measure
			// differences, refer to RenderingDeviceGraph and the methods available to print statistics for draw lists.
			uint64_t depth_layer : 4;
			uint64_t surface_index : 8;
			uint64_t geometry_id : 32;
			uint64_t material_id_hi : 8;

			uint64_t material_id_lo : 24;
			uint64_t shader_id : 32;
			uint64_t priority : 8;
		};
	} sort;
```

**④ ソート済み隣接要素を自動インスタンシングへ畳み込む判定**
`servers/rendering/renderer_rd/forward_clustered/render_forward_clustered.cpp:878-892`
```cpp
	const bool cant_repeat = instance_data.flags & INSTANCE_DATA_FLAG_MULTIMESH || inst->mesh_instance.is_valid();

	if (prev_surface != nullptr && !cant_repeat && prev_surface->sort.sort_key1 == surface->sort.sort_key1 && prev_surface->sort.sort_key2 == surface->sort.sort_key2 && inst->mirror == prev_surface->owner->mirror && repeats < RenderElementInfo::MAX_REPEATS) {
		//this element is the same as the previous one, count repeats to draw it using instancing
		repeats++;
	} else {
		if (repeats > 0) {
			for (uint32_t j = 1; j <= repeats; j++) {
				rl->element_info[p_offset + i - j].repeat = j;
			}
		}
		repeats = 1;
```

---

## 4. hikariへの示唆

**① 1フレームのパス構成順序**
- 簡易版(depth prepass→opaque→sky→transparentを固定順でハードコード、compositor拡張点なし): **難易度2**。フレームバッファの複数アタッチメントとブレンド有効/無効の切り替えができれば十分。
- フル版(Godot同等: MSAA resolve・モーションベクトル・compositor effect差し込み・可変アップスケーラまで): **難易度5**。
- 前提知識: 深度プリパスによるearly-Z、複数レンダーターゲット(MRT)、ブレンドステートの管理。

**② クラスタードライトカリング**
- 簡易版(固定数ライトをUBO配列に詰め、フラグメントシェーダで全ライトを線形ループ、カリングなし): **難易度2**。ライト数が数個〜十数個の作品規模なら十分実用的。
- 中間版(CPU側でスクリーンをNxMタイルに分け、球/AABB交差判定でタイルごとのライトインデックスリストをUBOに書く「CPU Tiled Forward」): **難易度4**。
- フル版(Godot同様のGPUラスタライズ+アトミックによるクラスタ構築、32深度スライス): **難易度5**、かつ**OpenGL 3.3では技術的に不可能**(SSBO/imageAtomicはGL4.3以降が前提)。hikariの技術スタック制約と衝突するため、フル版の模倣は非推奨。
- 前提知識: バウンディングボリュームのview空間変換、UBOの配列バインド、(中間版なら)CPU側の球-AABB交差判定。

**③ 描画リストの構築とソート**
- 簡易版(不透明=カメラからの距離で手前から奥へソート、半透明=奥から手前へソート、マテリアルソートは省略): **難易度1**。`std::sort`+比較関数だけで実装可能、費用対効果が非常に高い。
- フル版(ビットパックソートキーでマテリアル/シェーダ/ジオメトリを束ね、連続同一描画をインスタンシングへ畳み込む): **難易度3〜4**。
- 前提知識: 深度ソートの目的(early-Zでの無駄な上書き削減、透明合成の正しい順序)。

**④ uniform / push constantの受け渡し**
- 簡易版(GL3.3標準の`glUniformMatrix4fv`等を描画コールごとに設定、hikariは既にこの形と推測): **難易度1**。
- フル版(全インスタンスデータをSSBOへ一括アップロードし、描画毎はインデックス1個だけ渡す): **難易度4**、GL3.3ではSSBOが使えないため厳密な再現は不可。UBO配列+`glBindBufferRange`で疑似的に近い効果(バインド回数削減)を狙うなら**難易度3**。
- 前提知識: UBO/SSBOの違いと世代要件、ディスクリプタセット(GLでは相当する概念がバインディングポイント)。

---

## 5. 確信度と限界

**検証済み(実コード全読で確認)**: `_render_scene`のパス順序全体(1706-2572行を通読)、`cluster_builder_rd.h/.cpp`全文、`cluster_render.glsl`/`cluster_store.glsl`全文、`_fill_render_list`/`_fill_instance_data`/ソートロジック全体、関連構造体定義(`RenderElementData`, `InstanceData`, `PushConstant`, ソートキーunion)。

**状況証拠・部分確認**:
- `scene_forward_clustered.glsl`(3130行)はクラスタ消費部分(164-230行、2670-2840行、計約270行)のみ読了。PBR計算本体・IBL・SSS・ボリュームフォグ合成などシェーダ全体構造は未確認。
- `_render_scene`を実際にフレーム毎に呼び出す上位関数(`renderer_scene_render_rd.cpp`の`render_scene()`)は担当範囲外のため未読。Viewport側からの呼び出し経路はGodotの一般的な設計知識に基づく推測。
- `GeometryInstanceForwardClustered::_geometry_instance_update`(cpp:4356-4657、約300行)や描画パイプライン生成系(`_mesh_compile_pipeline*`)は関数シグネチャのみGrepで把握、中身は未読。
- サブグループ演算(`subgroupBroadcastFirst`等)によるアトミック競合削減の実測効果は未検証(コード上の意図は読めたが、性能測定はしていない)。

**推測で書いた箇所**: 「hikariへの示唆」の難易度評価は、OpenGL 3.3〜4.3の仕様上の一般論に基づくものであり、hikariの現在の実装コード(未確認)を踏まえた個別診断ではない。