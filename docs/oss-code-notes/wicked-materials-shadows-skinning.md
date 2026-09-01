> 作成: 2026-08-20 / AI読解エージェントによる実ソースコード読解記録
> 対象スナップショット: Wicked Engine (d857d90) / ライセンス: MIT(コード引用は出典明記のうえ)
> 行番号はこのスナップショット時点のもの。最新版ではズレる。代表引用の一部はメインAIが実コードと突き合わせて一致を確認済み(全数検証ではない)。各記録末尾の「確信度と限界」を必ず読むこと。

# Wicked Engine 読解記録: PBRマテリアル・シャドウ・スキニング

## 1. 読んだファイル一覧

| パス(WickedEngine/配下) | 行数 | 読んだ範囲 |
|---|---|---|
| shaders/brdf.hlsli | 全206行 | 全読 |
| shaders/surfaceHF.hlsli | 全1214行 | L1-340, L600-799, L860-989, L1000-1099(計約730行) |
| shaders/objectHF.hlsli | 全1151行 | L538-687, L890-1019(計約280行) |
| shaders/objectPS.hlsl | 全5行 | 全読 |
| shaders/shadowHF.hlsli | 全461行 | 全読 |
| shaders/skinningCS.hlsl | 全192行 | 全読 |
| shaders/lightingHF.hlsli | 全654行 | L1-145, L160-229(計約215行) |
| shaders/shadingHF.hlsli | 全653行 | L1-229 |
| shaders/globals.hlsli | 全2681行 | L1594-1633(cubemap_to_uv周辺のみ) |
| shaders/ShaderInterop_Renderer.h | 全1913行 | Grepで特定箇所のみ(L393, L463-470) |
| wiRenderer.cpp | 全19886行 | L5160-5299(スキニングディスパッチ) |
| wiScene.cpp | 全9812行 | L3934-3973(ボーン行列計算) |
| wiScene_Components.h | 全2720行 | Grepで特定箇所のみ(L186-389) |
| wiScene_Components.cpp | 全3182行 | L282-351(WriteShaderMaterial) |

---

## 2. 機能→実装の対応記録

### 2-1. BRDF実装(Cook-Torrance)

**(a) 主要ファイルと行範囲**
- `shaders/brdf.hlsli` L8-206(D/V/F項、SurfaceToLight、BRDF_GetSpecular/Diffuse)
- `shaders/surfaceHF.hlsli` L20-36(F_Schlick, EnvBRDFApprox)、L257-318(Surface::update())
- `shaders/lightingHF.hlsli` L58-141(directional light適用箇所)

**(b) 中心データ構造**
- `Surface`(surfaceHF.hlsli L73-134): P/N/V/baseColor/albedo/f0/roughness/occlusion/NdotV/F/R などを保持。SHEEN/CLEARCOAT/ANISOTROPICは`SheenSurface`/`ClearcoatSurface`/`AnisotropicSurface`という入れ子構造体として`#ifdef`ガード付きで追加される設計。
- `SurfaceToLight`(brdf.hlsli L90-151): 1光源ごとに使い捨てで計算するL/H/NdotL/NdotH/LdotH/VdotH/F(Schlick)。`create()`で一括計算し、異方性用のTdotL等も同メソッド内で追加計算。
- `Lighting`(lightingHF.hlsli L17-25): direct/indirectのdiffuse/specularを溜めるだけの薄い集計構造体。

**(c) 処理フロー(実際の呼び出し順)**
1. `objectHF.hlsli main()`(L538)で`Surface surface; surface.init()`後、頂点補間値をSurfaceへ詰める
2. `surface.update()`(objectHF.hlsli L905 → surfaceHF.hlsli L257)が`NdotV`、`F = EnvBRDFApprox(f0, roughness, NdotV)`(環境光・反射用のフレネル近似)、反射ベクトル`R`を計算
3. `TiledLighting()`(shadingHF.hlsli L46)がタイル内の光源をバケット走査し、光源種別ごとに`light_directional`/`light_spot`/`light_point`(lightingHF.hlsli)を呼ぶ
4. 各`light_*`内で`SurfaceToLight surface_to_light; surface_to_light.create(surface, L)`(brdf.hlsli L108)を呼び、その光源固有のH/NdotL/NdotH/`F = F_Schlick(f0, VdotH)`(直接光用の正確なフレネル項)を計算
5. シャドウ(`shadow_2D`/`shadow_cube`)で`light_color`を減衰
6. `lighting.direct.diffuse = mad(light_color, BRDF_GetDiffuse(...), ...)` / `...specular = mad(light_color, BRDF_GetSpecular(...), ...)`(lightingHF.hlsli L140-141)
7. `BRDF_GetSpecular`(brdf.hlsli L156-188)内部で`D_GGX`×`V_SmithGGXCorrelated`×`surface_to_light.F`を計算し、`SHEEN`定義時は`D_Charlie`×`V_Neubelt`項、`CLEARCOAT`定義時は`D_GGX`(クリアコート用)×`V_Kelemen`項を加算(多層マテリアルをレイヤーごとの追加項として合成する設計)
8. `ApplyLighting()`(lightingHF.hlsli L41-55)で`diffuse / PI`と間接光(IBL/GI)を合算して最終色を確定

**(d) GPUシェーダーファイルと要点**
- `brdf.hlsli`: D/V/F項の数式そのもの。出典コメントに Walter 2007(GGX)、Heitz 2014(Smith可視性項)、Filamentのbrdf.fsが明記されている
- `surfaceHF.hlsli`: マテリアル読み込みとf0/roughness確定
- `lightingHF.hlsli`: 光源ごとのループ、シャドウ適用、BRDF呼び出し
- `shadingHF.hlsli`: TiledLighting(タイルベース光源カリング+IBL/GI合流の入口)
- `objectPS.hlsl`: 5行のみでobjectHF.hlsliをマクロ付きincludeするエントリポイント

**(e) 依存する他サブシステム**: Forward+(タイル)光源カリング、シャドウアトラス、環境マッププローブ/グローバルスカイ、VXGI/DDGI/SurfelGI(indirect.diffuse/specularの供給元)

---

### 2-2. マテリアルパラメータの流れ

**(a)** `wiScene_Components.h` L186-189・L312-314(MaterialComponentのfloatメンバ)、`wiScene_Components.cpp` L282-351(WriteShaderMaterial)、`ShaderInterop_Renderer.h` L393・L467-470(GPU側パック構造体とアクセサ)、`surfaceHF.hlsli` L236-248(ワークフロー分岐)

**(b) 中心データ構造**
- CPU: `MaterialComponent`(roughness=0.2f, reflectance=0.02f, metalness=0.0f などfloatメンバ)
- GPU: `ShaderMaterial`。4パラメータを`pack_half4`で1つの`uint2`にパックする設計(例: `roughness_reflectance_metalness_refraction`)。読み出しは`GetRoughness()`等の`unpack_half4`アクセサ

**(c) 処理フロー**
1. `WriteShaderMaterial()`(wiScene_Components.cpp L282)が毎フレーム、CPU側floatをhalf精度にパックしてGPU用構造化バッファへ書き込む
2. ピクセルシェーダー側で`material = load_material(materialIndex)`
3. `SURFACEMAP`テクスチャ(R=occlusion, G=roughness, B=metalness, A=reflectance)をサンプリング(surfaceHF.hlsli L667-676)
4. `surfaceMap.g *= material.GetRoughness(); surfaceMap.b *= material.GetMetalness(); surfaceMap.a *= material.GetReflectance();`(L684-686)でテクスチャと定数を乗算合成
5. Metallic-Roughnessワークフローでは `albedo = baseColor*(1-max(reflectance,metalness))`、`f0 *= lerp(reflectance, baseColor, metalness)`(L246-247)というエネルギー保存を考慮した式で最終的なalbedo/f0を確定

**(d)** `surfaceHF.hlsli`(読み込み本体)、`ShaderInterop_Renderer.h`(CPU/GPU共有構造体、HLSL/C++双方からincludeされるヘッダ)

**(e) 依存する他サブシステム**: bindlessディスクリプタ(`descriptor_index`)、テクスチャストリーミング(mip選択)、デカール合成

---

### 2-3. シャドウ

**方式: 単一の共有シャドウアトラス(2Dテクスチャ1枚) + カスケード(directional) + キューブ面アンラップ(point) + Vogelディスクソフトシャドウ(既定16タップ)**

**(a)** `shadowHF.hlsli`全体、`lightingHF.hlsli` L74-132(カスケード選択)・L201-214(point用cube呼び出し)、`globals.hlsli` L1594-1621(`cubemap_to_uv`)

**(b) 中心データ構造**
- `vogel_points[]`(shadowHF.hlsli L14-149): Vogelディスクサンプリングパターン。4/8/16/32/64サンプル版がコメントアウトで用意され、既定で有効なのは**16サンプル版**
- 共有シャドウアトラス`texture_shadowatlas_index` + 半透明シャドウ用の別アトラス`texture_shadowatlas_transparent_index`
- `light.shadowAtlasMulAdd`: 各ライトのシャドウ領域がアトラス内のどこにあるかを表すUVスケール/オフセット

**(c) 処理フロー**
1. Directional: `light_directional()`(lightingHF.hlsli L58)がカスケード0から順に投影し、`shadow_uv`が[0,1]²に収まる最初のカスケードを`best_cascade`として採用(L93-105)
2. `shadow_2D(light, z, uv, cascade, pixel)`(shadowHF.hlsli L238)を呼び、`shadow_uv.x += cascade`でアトラス内の該当カスケード領域へオフセット
3. `sample_shadow()`(L154)がVogelディスク16タップを`spread`(アトラス解像度の逆数×ライト半径)でばらまき、各サンプルを`SampleCmpLevelZero`(ハードウェア比較サンプラーによるPCF)でサンプリングして平均化
4. カスケード境界付近では次のカスケードもサンプルして`cascade_blend`でソフトにブレンド(L107-124)、境界アーティファクトを軽減
5. Point: `light_point()`→`shadow_cube()`(shadowHF.hlsli L245)→`cubemap_to_uv()`(globals.hlsli L1594)が方向ベクトルから6面のうちどれかを`faceIndex`として決定し、**物理的なキューブマップではなく6面を1枚のアトラスの別スライスとして扱い**、directionalと同じ`sample_shadow()`に合流
6. 透明物体の色付きシャドウは別アトラスから色/アルファを追加サンプルして乗算(L212-217)

**(d)** `shadowHF.hlsli`(サンプリング本体)。深度パス自体を描く`shadowVS.hlsl`/`shadowPS_alphatest.hlsl`/`shadowMS.hlsl`等は今回未読。

**(e) 依存する他サブシステム**: ShaderEntity(光源管理)、CPU側のアトラス領域割り当てロジック(未読、後述の限界を参照)、レイトレーシングシャドウ(`rtshadowCS.hlsl`、shadow_maskとして代替可能・未読)

---

### 2-4. スキニング(コンピュートシェーダー)

**(a)** `skinningCS.hlsl`全192行、`wiRenderer.cpp` L5217-5299(ディスパッチ)、`wiScene.cpp` L3934-3973(ボーン行列計算)

**(b) 中心データ構造**
- `SkinningPushConstants`(ルート定数): `vb_pos_wind`/`vb_nor`/`vb_tan`(入力頂点バッファ)、`vb_bon`(ボーンインデックス+ウェイト)、`so_pos`/`so_nor`/`so_tan`(出力先=streamoutバッファ)、`skinningbuffer_index`(ボーン行列+モーフターゲットのバッファ)、`bone_offset`、`morph_offset`/`morph_count`、`influence_div4`、`vertexCount`
- ボーン影響パッキング: 1頂点あたり4影響を`uint4`1個(16byte)にパック。インデックスは下位20bit、ウェイトは上位12bit固定小数(4095分の1)。`influence_div4`回ループして4の倍数の影響数まで対応
- `ShaderTransform`(GPU側ボーン行列): CPU側で`InverseBindMatrix * BoneWorldMatrix * R`を計算しアップロード

**(c) 処理フロー**
1. CPU毎フレーム: `armature.boneData[boneIndex]`に`B * W * R`(逆バインド行列×ワールド行列×補正行列)を計算(wiScene.cpp L3952-3960)、アップロードバッファへmemcpy
2. `wiRenderer.cpp`のバッファ更新段階で`skinningUploadBuffer`→GPUの`skinningBuffer`へ`CopyBuffer`(L5162-5170)
3. 同関数内"Skinning and Morph"ブロック(L5217〜)で`BindComputeShader(CSTYPE_SKINNING)`後、スキン対象/モーフ持ちメッシュごとに`SkinningPushConstants`を設定して`PushConstants`→`Dispatch((vertexCount+63)/64, 1, 1)`(L5291-5293)。**メッシュ単位で毎フレーム1回ディスパッチ**
4. `skinningCS.hlsl main()`(`numthreads(64,1,1)`、1スレッド=1頂点)がまずモーフターゲットをpos/norへ加算(L108-124)
5. ボーンスキニングループ(L127-172): `influence_div4`回ループしてboneBufferから4影響ずつindex/weightをunpack、`skinningbuffer.Load<ShaderTransform>`で対応するボーン行列を取得し、位置・法線・接線を加重合算(`weisum<1.0`の間だけ、最大4影響/イテレーション)
6. 結果を`so_pos`/`so_nor`/`so_tan`(streamoutバッファ)へ書き込み(L175-191)。以降の描画パス(prepass/main pass)はこの「スキン済み頂点バッファ」を参照する

**(d)** `skinningCS.hlsl`(単一ファイルで完結)

**(e) 依存する他サブシステム**: bindlessバッファのルートシグネチャ(L5-66、SRV/UAVをすべて`descriptor_index`で動的参照)、モーフターゲット(ブレンドシェイプ)システム、ソフトボディ物理(`bone_offset`を共有)

---

## 3. 代表コード引用

**① D項(GGX法線分布) — brdf.hlsli L8-17**
```hlsl
half D_GGX(half roughness, highp float NoH, const float3 h)
{
	// Walter et al. 2007, "Microfacet Models for Refraction through Rough Surfaces"
	half oneMinusNoHSquared = 1.0 - NoH * NoH;

	half a = NoH * roughness;
	half k = roughness / (oneMinusNoHSquared + a * a);
	half d = k * k * (1.0 / PI);
	return saturateMediump(d);
}
```

**② Metallic-Roughnessワークフローのalbedo/f0分離 — surfaceHF.hlsli L236-248**
```hlsl
// Metallic-roughness workflow:
if (material.IsOcclusionEnabled_Primary())
{
	occlusion *= surfaceMap.r;
}
roughness = surfaceMap.g;
const half metalness = surfaceMap.b;
const half reflectance = surfaceMap.a;
albedo = baseColor.rgb * (1 - max(reflectance, metalness));
f0 *= lerp(reflectance.xxx, baseColor.rgb, metalness);
```

**③ Vogelディスク+ハードウェアPCF比較サンプリング — shadowHF.hlsli L197-210**
```hlsl
[loop] // decent perf win with loop on the "island" level on AMD
for (min16uint i = 0; i < soft_shadow_sample_count; ++i)
{
	float2 sample_uv = mad(vogel_points[i], spread, uv);
	sample_uv = clamp(sample_uv, uv_clamping.xy, uv_clamping.zw);
	half3 pcf = texture_shadowatlas.SampleCmpLevelZero(sampler_cmp_depth, sample_uv, cmp).rrr;
	shadow += pcf;
}
shadow *= soft_shadow_sample_count_rcp;
```

**④ GPUスキニングの加重ブレンドループ — skinningCS.hlsl L155-167**
```hlsl
if (any(wei))
{
	for (uint i = 0; ((i < 4) && (weisum < 1.0)); ++i)
	{
		float4x4 m = skinningbuffer.Load<ShaderTransform>(push.bone_offset + ind[i] * sizeof(ShaderTransform)).GetMatrix();
		half weight = wei[i];

		p += mul(m, float4(pos.xyz, 1)) * weight;
		n += half3(mul((half3x3)m, nor.xyz)) * weight;
		t += half3(mul((half3x3)m, tan.xyz)) * weight;
		weisum += weight;
	}
}
```

---

## 4. hikariへの示唆

### BRDF(Cook-Torrance PBR)
- **簡易版最小構成**: `D_GGX` + `V_SmithGGXCorrelated`(可視性項) + `F_Schlick`の3項だけ実装し、albedo/f0/roughnessの3値でPhongを置き換える。難易度: **2**(数式自体は10数行、既存のライティング関数への追加として実装可能)
- **フル版**(sheen/clearcoat/anisotropicのレイヤー合成込み): 難易度**4**。WickedEngineの「`SurfaceToLight`を光源ごとに1回計算し、各レイヤーがそれを使い回す」設計は、複数マテリアル拡張を後付けする際の参考になる
- **前提知識**: マイクロファセットBRDF理論(D/G/F項の意味)、エネルギー保存則(`diffuse *= 1-F`)。hikariはOpenGL3.3+GLSLなのでhalf精度の概念は不要(floatで代用)

### マテリアルパラメータの流れ
- **簡易版**: metallic/roughnessをuniform float 2個で渡すだけ。難易度**1**
- **フル版**(テクスチャ×定数の乗算合成、複数ワークフロー切替): 難易度**2〜3**。glTF 2.0のmetallic-roughnessテクスチャ規約(R=AO, G=roughness, B=metallic)をそのまま踏襲すればWickedEngineとほぼ同じ設計になる
- **前提知識**: glTF PBRマテリアル仕様、テクスチャのsRGB/Linear変換

### シャドウマッピング
- **簡易版**(hikariの計画通り): 単一シャドウマップ+固定3x3〜5タップPCF。難易度: ユーザー基準の**3**そのもの
- **フル版**(WickedEngine相当: カスケード+共有アトラス+Vogelディスクソフトシャドウ+キューブアンラップ): 難易度**5**。特にCPU側でのアトラス動的パッキング(複数ライトの深度マップを1枚のテクスチャへ管理)は実装量が大きい
- **前提知識**: 単一シャドウマップの原理を先に習得した上で、カスケード分割(視錐台をZ距離で分割し各カスケードへ直交投影)、PCFソフトシャドウ(サンプリングパターン、比較サンプラー`GL_TEXTURE_COMPARE_MODE`相当)。いきなりアトラス方式を真似る必要はなく、まず固定テクスチャスロット1枚での実装で十分

### GPUスキニング(コンピュートシェーダー)
- **重要な制約**: hikariはOpenGL 3.3が前提だが、**コンピュートシェーダーはOpenGL 4.3以降でしか使えない**。WickedEngineの`skinningCS.hlsl`方式(GPUストリームアウトスキニング)をOpenGL3.3のまま再現することはできない
- **簡易版の代替**: (a) 頂点シェーダーでボーン行列配列をuniformで渡すGPUスキニング(コンピュート不要、OpenGL3.3で可能)、または (b) CPUでのスキニング計算。難易度**2**(頂点シェーダー版)/**1**(CPU版)
- **フル版**(WickedEngineのようにモーフターゲット・ソフトボディ共有・bindless無制限影響数まで含む): 難易度**5**、かつOpenGL4.3+へのアップグレードが前提
- **前提知識**: 頂点スキニング数学(逆バインド行列×ワールド行列×加重平均)自体はGPU/CPUどちらでも共通。コンピュートシェーダー方式を将来採用するならOpenGL4.3+ (またはVulkan)への移行検討とディスパッチモデルの学習が必要

---

## 5. 確信度と限界

- **検証済み(コードを実際に読んで確認)**: BRDFのD/V/F項の対応、diffuse/specularの合成順序、metallic-roughnessワークフローのalbedo/f0分離式、シャドウのVogelディスク16タップ+ハードウェアPCF、カスケード選択ロジック、point光のキューブ面アンラップ、GPUスキニングのディスパッチ・重み付きブレンドループ、CPU側のボーン行列計算式(`InverseBind×World×R`)とマテリアルパラメータのCPU→GPUパッキング経路
- **状況証拠(読んだが完全には裏取りしていない)**: `diffuse / PI`が`ApplyLighting()`側でdirect/indirect共通に一括適用される点の意味論(IBL側が既に正規化済みかどうかまでは未確認)。`attenuation_pointlight()`は呼び出し箇所のみ読み、内部の減衰式は未読
- **未読・推測を含む箇所**:
  - シャドウアトラスの**CPU側の動的領域割り当てロジック**(`shadowAtlasMulAdd`がどう計算されるか)は`wiRenderer.cpp`内のどこかにあると推測されるが未確認
  - PCSS(`SHADOW_SAMPLING_PCSS`)は既定でコメントアウトされているため無効という点は確認済みだが、実際のプロジェクト設定でどのマクロ組み合わせがデフォルトビルドで使われるかは未確認
  - レイトレーシングシャドウ(`rtshadowCS.hlsl`)、カプセルシャドウ(`capsuleShadowHF.hlsli`)、シャドウ深度パス自体(`shadowVS.hlsl`等)は今回対象外・未読
  - `objectHF.hlsli`(1151行)・`surfaceHF.hlsli`(1214行)はそれぞれ6割程度の精読に留まる。特にデカール合成ループ全体やVXGI関連分岐は概観のみ
  - `wiRenderer.cpp`(19886行)・`wiScene.cpp`(9812行)は共に大規模ファイルであり、今回読んだのはGrepでピンポイントに特定した箇所のみ。カリングや描画コマンド生成などCPU側の他ロジックは未調査