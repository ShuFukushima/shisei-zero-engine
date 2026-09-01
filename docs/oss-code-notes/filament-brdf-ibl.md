> 作成: 2026-08-20 / AI読解エージェントによる実ソースコード読解記録
> 対象スナップショット: Filament (d8b4a37b) / ライセンス: Apache 2.0(コード引用は出典明記のうえ)
> 行番号はこのスナップショット時点のもの。最新版ではズレる。末尾の「確信度と限界」を必ず読むこと。

# Filament PBRシェーダー/IBL事前計算 実装読解レポート

## 1. 読んだファイル一覧(リポジトリルート相対・概算行数)

| ファイル | 行数 | 内容 |
|---|---|---|
| shaders/src/surface_brdf.fs | 264 | D/V/F項の全実装と切替マクロ |
| shaders/src/surface_shading_model_standard.fs | 174 | 標準シェーディングモデル(ローブ合成) |
| shaders/src/surface_shading_lit.fs | 364 | PixelParams構築・remap・ライトループ統括 |
| shaders/src/surface_light_indirect.fs | 839 | IBLランタイム(SH・prefiltered cubemap・DFG LUT) |
| shaders/src/surface_light_directional.fs | 155 | 平行光源評価 |
| shaders/src/surface_light_punctual.fs | 275 | 点・スポット光(減衰式を精読) |
| shaders/src/surface_material.fs | 62 | remap系ユーティリティ |
| shaders/src/surface_shading_parameters.fs | 81 | N,V,R,NoV等の共通パラメータ |
| shaders/src/surface_lighting.fs | 先頭80行精読 | Light / PixelParams構造体 |
| libs/ibl/src/CubemapIBL.cpp | 1040 | IBL事前計算(cmgenの中核) |
| libs/ibl/src/CubemapSH.cpp | 776 | 球面調和(SH)計算 |
| tools/cmgen/src/cmgen.cpp | Grepのみ | ↑2つの呼び出し箇所確認 |

## 2. 機能→実装の対応記録

### A. Cook-Torrance直接光BRDF(理論文書 §4.4/§4.5)

**(a)主要ファイル**: surface_brdf.fs(全体)、surface_shading_model_standard.fs:61-174
**(b)中心データ構造**: `PixelParams`(surface_lighting.fs:16)— diffuseColor / f0 / roughness / perceptualRoughness / dfg / energyCompensation を保持。`Light`(同:1)— colorIntensity(wは露出前乗算済み強度)・l・attenuation・NoL。
**(c)処理フロー**: `evaluateLights`(surface_shading_lit.fs:306)→ `getPixelParams`(:285)→ `evaluateDirectionalLight`(surface_light_directional.fs:34)/`evaluatePunctualLights` → 各光源ごとに `surfaceShading`(surface_shading_model_standard.fs:112)→ `specularLobe`(:82)+`diffuseLobe`(:91)→ 最後に `color * lightColor * (intensity * attenuation * NoL * occlusion)`(:172-173)。
**(d)シェーダーの要点** — 文書の式との対応:
- **D項(GGX, §4.4.1)**: `D_GGX`(surface_brdf.fs:54)。文書のGGX式そのものだが、モバイルではLagrange恒等式 `||N×H||²=1-(N·H)²` で fp16 の桁落ちを回避(:68-70)。
- **G/V項(Smith, §4.4.2)**: `V_SmithGGXCorrelated`(:102)= 高さ相関Smith可視性項(V = G/(4·NoV·NoL) を纏めた形)。低品質モードは `V_SmithGGXCorrelated_Fast`(:115、Hammon近似)。切替は :31-39 のマクロ。
- **F項(Fresnel-Schlick, §4.4.3)**: `F_Schlick`(:145,150,155)。`fresnel()`(:177)では文書§4.4.3の「f90をf0の輝度から推定する」トリック `f90 = saturate(dot(f0, vec3(50.0*0.33)))`(:182)を実装。
- **Diffuse(§4.5)**: `Fd_Lambert`(:237)/`Fd_Burley`(:241)。**既定はLambert**(:29)。
- D・V・Fの積は `isotropicLobe`(surface_shading_model_standard.fs:61-72)で `(D * V) * F`。
- **エネルギー補償(§4.7.2)**: `getEnergyCompensationPixelParams`(surface_shading_lit.fs:252)で DFG LUT の y 成分から `1 + f0*(1/dfg.y - 1)`(:259)を計算し、スペキュラに乗算(surface_shading_model_standard.fs:122)。
- **点光源減衰(§5.2)**: `getSquareFalloffAttenuation`(surface_light_punctual.fs:93)= 窓関数付き逆二乗 `(1-(d²·falloff)²)²/d²`(除算は :108)。スポットは `getAngleAttenuation`(:111)。
**(e)依存**: surface_shadowing.fs(シャドウ)、surface_ambient_occlusion.fs(AO)、フレームUBO。

### B. roughness remapping と拡張の分岐点(§4.8/§4.9)

- **perceptualRoughness→roughness(α=pr², §4.8.3)**: 変換関数は `perceptualRoughnessToRoughness`(surface_material.fs:37)。**実際に適用される場所**は `getRoughnessPixelParams`(surface_shading_lit.fs:194-242)。ここで clamp(`MIN_PERCEPTUAL_ROUGHNESS`=0.045、モバイル0.089。surface_material.fs:2-7)→ 2乗、の順。
- **reflectance→f0(f0=0.16·reflectance², §4.8.3)**: `computeDielectricF0`(surface_material.fs:25)、metallicとの合成は `computeF0`(:21)`= baseColor*metallic + reflectance*(1-metallic)`。diffuse側は `computeDiffuseColor`(:17)`= baseColor*(1-metallic)`。呼び出し元は `getCommonPixelParams`(surface_shading_lit.fs:53)。
- **拡張の分岐はすべて `#if defined(MATERIAL_HAS_*)` プリプロセッサ**であり、実行時分岐ではない。クリアコートは `getClearCoatPixelParams`(surface_shading_lit.fs:167、§4.9)+`clearCoatLobe`(surface_shading_model_standard.fs:11、V_Kelemen使用)、異方性は `anisotropicLobe`(同:31)、シーンは `sheenLobe`(同:2)。**これらを全部無視すると本体は isotropicLobe+diffuseLobe+surfaceShading の約60行**。

### C. IBL(事前計算=libs/ibl、ランタイム=surface_light_indirect.fs、§5.3.4+付録9)

**(a)主要ファイル**: CubemapSH.cpp、CubemapIBL.cpp、surface_light_indirect.fs:1-160(ランタイム本流)と :717-839(統合)。
**(b)中心データ**: SH係数9個(3バンド×RGB)、prefiltered specular cubemap(mipごとにroughness増加)、DFG LUT(2ch、128×128程度の2Dテクスチャ)。
**(c)処理フロー(事前計算、cmgen.cpp からの呼び出しで確認)**:
- **Diffuse=球面調和が既定**: `CubemapSH::computeSH(js, cm, 3, true)`(CubemapSH.cpp:511、cmgen.cpp:727)→ 立体角重み付きで係数を積算(:540-547)し、`irradiance=true` なら帯域ごとに truncated cos(§ランバート畳み込み)を乗算(:560-569)→ `preprocessSHForShader`(:617)で多項式係数A[i]と1/πを**事前乗算**。irradiance map版も存在(`CubemapIBL::diffuseIrradiance`、CubemapIBL.cpp:554、コサイン重点サンプリング)が、cmgenでは補助出力。
- **Specular prefilter**: `CubemapIBL::roughnessFilter`(CubemapIBL.cpp:304)。mipレベルごとに呼ばれ、`hemisphereImportanceSampleDggx`(:50)でGGX重点サンプリング→ N=V仮定でLを解析的に導出(:382-385)→ pdfからソースmipを選ぶprefiltered importance sampling(:389-395)→ `Σ L_i·NoL / Σ NoL`(:399-410, 452)。
- **DFG LUT**: `CubemapIBL::DFG`(:1008)→ `DFV`(:748)/`DFV_Multiscatter`(:790)。split-sum のBRDF項をf0の係数(x)と定数項(y)に分離して積分(:781-784)。LUTの縦軸は `coord=sqrt(perceptualRoughness)`ではなく **coord²=linear_roughness つまり縦軸=perceptualRoughness**(:1020-1024とsurface_light_indirect.fs:26の注釈で整合)。
**(d)ランタイムサンプリング**: `evaluateIBL`(surface_light_indirect.fs:717)が本流。Diffuse=`diffuseIrradiance`(:74)→`Irradiance_SphericalHarmonics`(:43、SH係数はUBO `frameUniforms.iblSH`)。Specular=`specularDFG`(:135)×`prefilteredRadiance`(:122)、LOD選択は `perceptualRoughnessToLod`(:115、二次フィット)。合成は `Fd = diffuseColor * irradiance * (1.0 - E)`(:805、Eはspecular DFGでエネルギー保存)と `color += Fr + Fd`(:835)。
**(e)依存**: CubemapUtils(面ごとの並列処理)、utilities.h の `hammersley`。ランタイム版重点サンプリング(:166-343)は`IBL_INTEGRATION`切替用で既定は不使用。

## 3. 代表コード引用(Apache 2.0、Google/Filament)

**GGXのfp16対策(shaders/src/surface_brdf.fs:68-78)**
```glsl
#if defined(TARGET_MOBILE)
    vec3 NxH = cross(shading_normal, h);
    float oneMinusNoHSquared = dot(NxH, NxH);
#else
    float oneMinusNoHSquared = 1.0 - NoH * NoH;
#endif
    float a = NoH * roughness;
    float k = min(roughness / (oneMinusNoHSquared + a * a), 453.5);
    float d = k * (k * (1.0 / PI));
```

**高さ相関Smith可視性項(shaders/src/surface_brdf.fs:102-109)**
```glsl
float V_SmithGGXCorrelated(float roughness, float NoV, float NoL) {
    // Heitz 2014, "Understanding the Masking-Shadowing Function..."
    float a2 = roughness * roughness;
    float lambdaV = NoL * sqrt((NoV - a2 * NoV) * NoV + a2);
    float lambdaL = NoV * sqrt((NoL - a2 * NoL) * NoL + a2);
    float v = PREVENT_DIV0(0.5, lambdaV + lambdaL, 0.0000077);
```

**Cook-Torranceの核(shaders/src/surface_shading_model_standard.fs:61-72)**
```glsl
vec3 isotropicLobe(float roughness, vec3 f0, float f90, const vec3 h,
        float NoV, float NoL, float NoH, float LoH) {
    float D = distribution(roughness, NoH, h);
    float V = visibility(roughness, NoV, NoL);
    vec3  F = fresnel(f0, LoH);
    return (D * V) * F;
}
```

**split-sumのランタイム側再構成(shaders/src/surface_light_indirect.fs:755-758、:142)**
```glsl
vec3 E = specularDFG(pixel);   // = mix(pixel.dfg.xxx, pixel.dfg.yyy, pixel.f0)
vec3 r = getReflectedVector(pixel, shading_normal);
Fr = E * prefilteredRadiance(r, perceptualRoughnessToLod(pixel.perceptualRoughness));
```

## 4. hikariへの示唆

| 機能 | 最小構成 | 簡易版難易度 | フル版 | 前提知識 |
|---|---|---|---|---|
| M8: Cook-Torrance直接光 | D_GGX+V_SmithGGXCorrelated+F_Schlick+Fd_Lambert+remap2関数。合計約50行のGLSL | **2**(数式の写経で動く。シャドウマッピングより簡単) | 3(エネルギー補償・fp16対策込み) | 半球積分の意味、NoL/NoV/NoH の幾何、線形色空間 |
| M9a: IBL Diffuse(SH) | CPU側computeSH相当(3バンド9係数、キューブマップ全画素×立体角重み)+シェーダー9行(Irradiance_SphericalHarmonics) | **3**(SHの数式理解が山。コード量は少ない) | 4(windowing等) | 球面調和の直感、立体角、truncated cos畳み込み |
| M9a代替: irradiance map | コサイン重点サンプリングで畳み込むだけ(CubemapIBL::diffuseIrradiance方式) | **2** | - | モンテカルロ積分の初歩。**SHより先にこちらを推奨** |
| M9b: IBL Specular | prefilter(GGX重点サンプリング+mipごと畳み込み)+DFG LUT(DFV移植)+ランタイム3行 | **4**(重点サンプリング+split-sum理解が必要) | 5(prefiltered importance sampling・multiscatter) | pdf/重点サンプリング、split-sum近似、mipmapとLOD |

**M8・M9で「これだけは読むべき」最小リスト(結論)**:
1. `D_GGX` — shaders/src/surface_brdf.fs:54
2. `V_SmithGGXCorrelated` — shaders/src/surface_brdf.fs:102
3. `F_Schlick`+`fresnel` — shaders/src/surface_brdf.fs:145,177
4. `isotropicLobe`+`surfaceShading` — shaders/src/surface_shading_model_standard.fs:61,112
5. `computeDiffuseColor`/`computeF0`/`perceptualRoughnessToRoughness` — shaders/src/surface_material.fs:17,21,37
6. `getRoughnessPixelParams` — shaders/src/surface_shading_lit.fs:194(clamp→2乗の順序)
7. `CubemapIBL::roughnessFilter` — libs/ibl/src/CubemapIBL.cpp:304(+`hemisphereImportanceSampleDggx` :50)
8. `DFV` — libs/ibl/src/CubemapIBL.cpp:748(コメント内の式導出が教材として秀逸)
9. `CubemapSH::computeSH` — libs/ibl/src/CubemapSH.cpp:511(SH派の場合)
10. `evaluateIBL` — shaders/src/surface_light_indirect.fs:717(全体の組み立て方)

## 5. 確信度と限界

- **検証済み**: 上記の全行番号・関数名はReadした実コードに基づく。cmgenからの呼び出し関係はGrepで確認(cmgen.cpp:684,727,737,1157)。
- **状況証拠**: 理論文書の節番号は§4.4.1-4.4.3、§4.5、§4.8.3、§4.9をWebで確認済み。**IBL関連の節番号(§5.3.4、付録9)は確認が部分的**で、「Distant light probes」節とAnnexに分かれている、という粒度までしか裏取りできていない。式番号単位の対応は文書側が式番号を振っていないため節名対応とした。
- **読み切れなかった部分**: surface_light_punctual.fs のライトインデックス取得(Froxel化)、surface_shadowing.fs、SSR・屈折・分散(surface_light_indirect.fs:449-714は流し読み)、CubemapSH.cpp の windowing(:315-334、リンギング対策と推測)、cmgen.cpp のCLI全体。
- **推測**: 「DFG LUTの縦軸=perceptualRoughness」はCubemapIBL.cpp:1020-1024のコメントとsurface_light_indirect.fs:26,35の注釈の整合から導いた解釈(両者は一致しており確度は高い)。