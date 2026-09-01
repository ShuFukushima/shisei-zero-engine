> 作成: 2026-08-20 / AI読解エージェントによる実ソースコード読解記録
> 対象スナップショット: Godot (855e9ad) / ライセンス: MIT(コード引用は出典明記のうえ)
> 行番号はこのスナップショット時点のもの。最新版ではズレる。代表引用の一部はメインAIが実コードと突き合わせて一致を確認済み(全数検証ではない)。各記録末尾の「確信度と限界」を必ず読むこと。

# GodotのGPU抽象化層(RenderingDevice)とシェーダーシステム 読解記録

対象リポジトリ: `scratchpad/oss/godot`(commit `855e9ad9`, 2026-08-19, masterブランチ。Godot 4.5系〜次期メジャー相当の最新コード)

**重要な前提の確認**: このバージョンのGodotは、依頼で想定されていた「RenderingDeviceがVulkanを直接呼ぶ」構造ではなく、**RenderingDevice(バックエンド非依存フロントエンド) → RenderingDeviceDriver(抽象インターフェース) → RenderingDeviceDriverVulkan(Vulkan実装)** の3層構造に既に移行済みだった(D3D12/Metal対応のため)。この点は読解の中心的な発見なので先に明記する。

---

## 1. 読んだファイル一覧

| パス | 総行数 | 実読範囲 |
|---|---|---|
| servers/rendering/rendering_device.h | 2072 | 1-1623, 1850-2072(約88%) |
| servers/rendering/rendering_device_driver.h | 961 | 全読 |
| servers/rendering/renderer_rd/shader_rd.h | 286 | 全読 |
| servers/rendering/renderer_rd/shader_rd.cpp | 1239 | 1-260, 380-759, 1140-1238(約56%) |
| servers/rendering/rendering_device.cpp | 10343 | 200-320, 4176-4275(該当関数周辺のみ、約2%) |
| servers/rendering/rendering_shader_container.h | 311 | Grepのみ(宣言把握、本文未読) |
| servers/rendering/rendering_shader_container.cpp | 1039 | 250-350(約10%) |
| drivers/vulkan/rendering_shader_container_vulkan.h/.cpp | 68/117 | 全読 |
| drivers/vulkan/rendering_device_driver_vulkan.cpp | 7564 | 4288-4377(shader_create_from_container冒頭のみ) |
| drivers/vulkan/rendering_device_driver_vulkan.h | 834 | 未読(Grepでシグネチャのみ) |
| drivers/vulkan/rendering_context_driver_vulkan.h | 226 | 30-120 |
| drivers/vulkan/rendering_context_driver_vulkan.cpp | 1094 | 未読 |
| drivers/vulkan/godot_vulkan.h | 44 | 全読 |
| drivers/vulkan/vulkan_hooks.h/.cpp, SCsub | 53/45/- | 未読(存在確認のみ) |
| servers/rendering/rendering_device_commons.h | 1178 | 650-690付近のみ(約40行) |
| servers/rendering/rendering_device_graph.h/.cpp | 966/2868 | .hの31-75のみ(役割把握) |
| modules/glslang/register_types.cpp | 161 | 44-54付近(GLSL→SPIR-Vコンパイラ確認) |
| servers/rendering/renderer_rd/shaders/tex_blit.glsl | 127 | 全読(テンプレート実例) |

---

## 2. 機能→実装の対応記録

### A. RenderingDevice API層とVulkan概念のマッピング

**(a) 主要ファイルと行範囲**
- API本体: `servers/rendering/rendering_device.h` 全体(概念一覧は67-2072行に配置)
- 抽象ドライバIF: `servers/rendering/rendering_device_driver.h` 90-961行(`class RenderingDeviceDriver`、純粋仮想関数のみ)
- Vulkan実装: `drivers/vulkan/rendering_device_driver_vulkan.cpp`(7564行、上記IFを実装)
- Vulkanインスタンス/物理デバイス選択: `drivers/vulkan/rendering_context_driver_vulkan.h` 46-226行

**(b) 中心データ構造**
- `RenderingDevice`(rendering_device.h:67): `RenderingContextDriver *context` + `RenderingDeviceDriver *driver` を保持するフロントエンド。`RID_Owner<Texture,true> texture_owner` のように、リソース種別ごとにRIDプール(`RID_Owner`)を持つ。
- `RenderingDeviceDriver::ID`(961:94-145): `DEFINE_ID`マクロで `BufferID/TextureID/ShaderID/UniformSetID/PipelineID/RenderPassID/FramebufferID...` を型安全な64bit整数として定義。VulkanではハンドルまたはPagedAllocatorのポインタをそのままIDに詰める設計(コメントに明記)。
- `RenderingDevice::Uniform`(1071-1134): OpenGLの「テクスチャユニットに束縛」ではなく、**バインディング番号+RIDの配列**をまとめて1つの記述にする。
- `RenderingDevice::UniformSet`(1151-1176): Vulkanの `VkDescriptorSet` 相当。`uniform_set_create()` で「シェーダーと紐付けて焼く(bake)」設計。

**(c) 処理フロー(呼び出し順)**
1. `RenderingDevice::initialize()`(1897) が `ReningContextDriver`→`RenderingDeviceDriver`を受け取り初期化。
2. リソース作成はすべて「フロントエンドで検証・キャッシュ→ドライバへ委譲」の2段構え。例: `texture_create()`(467)→内部で`driver->texture_create()`相当を呼ぶ(実装はrendering_device.cpp、未深読)。
3. framebuffer_format_create() → 内部で `_render_pass_create()`(679)が `RDD::render_pass_create()` を呼び、`VkRenderPass` 相当を生成してキャッシュ(`framebuffer_format_cache`)。
4. 描画は `draw_list_begin()`→`draw_list_bind_render_pipeline()`→`draw_list_bind_uniform_set()`→`draw_list_draw()`→`draw_list_end()` という命令列としてまず**RenderingDeviceGraph(RDG)に記録**され(rendering_device_graph.h、966行、役割のみ把握・未深読)、実際のVkCommandBuffer記録とバリア挿入は後でグラフを解決する際にまとめて行われる。`buffer_update()`のdocコメント(rendering_device.h:227-269)に「呼び出し順ではなく依存関係順に並べ替えられる」と明記されている。
5. `RenderingDeviceDriverVulkan` 側は `command_begin_render_pass`/`command_bind_render_pipeline`/`command_render_draw` 等(rendering_device_driver.h:682-707)を実装し、Vulkan APIを1:1で叩く。

**(d) GPU側シェーダー**: 本項目自体は抽象化層でありシェーダー実体を持たないため対象外(Bで扱う)。

**(e) 依存する他サブシステム**: `RenderingContextDriver`(サーフェス・物理デバイス選択)、`RenderingDeviceGraph`(自動バリア挿入・コマンド並べ替え)、`WorkerThreadPool`(並列シェーダーコンパイル)。

---

### B. シェーダーコンパイル・バリアント管理パイプライン

**(a) 主要ファイルと行範囲**
- テンプレート解析/バリアント管理: `servers/rendering/renderer_rd/shader_rd.cpp` 44-260, 407-424(`_compile_variant`), 615-759(キャッシュ), 1154-1223(`compile_stages`)
- GLSL→SPIR-V: `modules/glslang/register_types.cpp` 44付近(`compile_glslang_shader`、glslangライブラリ使用)
- SPIR-Vリフレクション: `servers/rendering/rendering_shader_container.cpp` 250-350(SPIRV-Reflectライブラリ`spvReflect*`使用)
- バックエンド固有シリアライズ: `drivers/vulkan/rendering_shader_container_vulkan.cpp` 全体(117行)
- Vulkan側パイプラインレイアウト生成: `drivers/vulkan/rendering_device_driver_vulkan.cpp` 4288-4377

**(b) 中心データ構造**
- `ShaderRD::StageTemplate::Chunk`(shader_rd.h:115-137): `.glsl`ファイル中の `#VERSION_DEFINES` `#MATERIAL_UNIFORMS` `#GLOBALS` `#CODE:xxx` マーカーをパースし、テキストと差し込み位置の列に分解したもの。
- `ShaderRD::Version`(71-94): 1つの「マテリアル/シェーダーインスタンス」に対応。`variant_data`(コンパイル済みバイナリのキャッシュ)と`variants`(RID配列、バリアント数ぶん)を持つ。
- `RenderingShaderContainer`(rendering_shader_container.h:40): SPIR-V由来のリフレクション情報(uniform set一覧・push constant・spec constant)とバックエンド固有コード片を1つのバイナリblobにシリアライズ/デシリアライズする器。Vulkan/D3D12/Metalで共通の基底クラス。

**(c) 処理フロー(実際の呼び出し順)**
1. `ShaderRD::setup()`(151) が `.glsl`テキストを`_add_stage()`(44)で走査し`StageTemplate`を構築、同時にエンジンバージョン+ソース全文のSHA256を`base_sha256`として保持。
2. マテリアル側が`version_create()`→`version_set_code()`で `#CODE:BLIT` 等のコード片・uniform宣言を注入。
3. 初回`version_get_shader()`(shader_rd.h:200)呼び出し時、`_initialize_version()`→`_compile_version_start()`(713)。
4. `_compile_version_start()`はまずディスクキャッシュを`_load_from_cache()`(625)で確認(ファイルヘッダ`"GDSC"`+バージョン+バリアント数、604-686行)。ヒットしなければ`WorkerThreadPool::add_template_group_task()`で**バリアントごとに並列**`_compile_variant()`(407)を実行。
5. `_compile_variant()`→`_build_variant_stage_sources()`(テンプレートのマーカーに`#define USE_XXX`等を差し込みGLSL文字列を完成)→`compile_stages()`(1154)→各ステージで`RD::shader_compile_spirv_from_source()`を呼ぶ。
6. `RenderingDevice::shader_compile_spirv_from_source()`(rendering_device.cpp:228)→`compile_glslang_shader()`(glslangモジュール、`glslang::TShader`+`GlslangToSpv`)でGLSL→SPIR-Vバイナリ化。
7. `_compile_variant()`は続けて`RD::shader_compile_binary_from_spirv()`(4205)を呼ぶ。ここで`RenderingShaderContainer::set_code_from_spirv()`(rendering_shader_container.cpp:733)→`reflect_spirv()`(250)が**SPIRV-Reflectライブラリ**でdescriptor binding/push constant/spec constant/local sizeを抽出し`ShaderReflection`を構築。続いて仮想関数`_set_code_from_spirv()`がバックエンド固有処理(Vulkanならsmolv圧縮のみ、D3D12/Metalなら別言語へのクロスコンパイルが入ると推測)を行い、`to_bytes()`で1本のバイト列にシリアライズ。
8. このバイト列が`variant_data`としてメモリキャッシュされ、`_save_to_cache()`(688)でディスクにも保存される。
9. `shader_create_from_bytecode_with_samplers()`(rendering_device.cpp:4225)が`from_bytes()`でコンテナを復元し、`driver->shader_create_from_container()`を呼ぶ。
10. Vulkan実装`shader_create_from_container()`(rendering_device_driver_vulkan.cpp:4288)は`ShaderReflection`の`uniform_sets`を1件ずつ走査し`VkDescriptorSetLayoutBinding`を組み立て(4300-4376)、`VkDescriptorSetLayout`→`VkPipelineLayout`を**リフレクション結果のみから自動生成**する。エンジン側コードは一切「set 0 binding 3はサンプラー」のような手書き記述をしない。

**(d) GPU側シェーダーの例**: `servers/rendering/renderer_rd/shaders/tex_blit.glsl`(127行、全読)。`#[vertex]`/`#[fragment]`でステージ分割、`#VERSION_DEFINES`(31行目)がバリアント`#define`の差し込み点、`#MATERIAL_UNIFORMS`(75行目)がマテリアルUBO内容の差し込み点、`#CODE : BLIT`(93行目)が呼び出し側から注入される本体コードの差し込み点。

**(e) 依存する他サブシステム**: `WorkerThreadPool`(並列コンパイル)、`FileAccess`/ディスクキャッシュ、`ShaderIncludeDB`(`#include`解決)、`RenderingShaderContainerFormat`(バックエンドごとのコンテナ生成ファクトリ)。

---

## 3. 代表コード引用

**① 抽象ドライバがVulkan形状に寄せられている証拠**(RenderPass/Framebufferがそのまま概念として存在)
```cpp
// servers/rendering/rendering_device_driver.h:661-663
virtual RenderPassID render_pass_create(VectorView<Attachment> p_attachments, VectorView<Subpass> p_subpasses,
    VectorView<SubpassDependency> p_subpass_dependencies, uint32_t p_view_count,
    AttachmentReference p_fragment_density_map_attachment) = 0;
virtual void render_pass_free(RenderPassID p_render_pass) = 0;
```

**② バリアント並列コンパイルの核**(1関数でGLSL文字列生成→SPIR-V→コンテナ化→RID化まで完結)
```cpp
// servers/rendering/renderer_rd/shader_rd.cpp:407-423
void ShaderRD::_compile_variant(uint32_t p_variant, CompileData p_data) {
    uint32_t variant = group_to_variant_map[p_data.group][p_variant];
    if (!variants_enabled[variant]) return;
    Vector<String> variant_stage_sources = _build_variant_stage_sources(variant, p_data);
    Vector<RD::ShaderStageSPIRVData> variant_stages = compile_stages(variant_stage_sources, dynamic_buffers);
    Vector<uint8_t> shader_data = RD::get_singleton()->shader_compile_binary_from_spirv(variant_stages, name + ":" + itos(variant));
    p_data.version->variants.write[variant] = RD::get_singleton()->shader_create_from_bytecode_with_samplers(shader_data, p_data.version->variants[variant], immutable_samplers);
}
```

**③ バックエンド差し替え可能性の設計**(Vulkanは単にSPIR-Vを圧縮するだけ)
```cpp
// drivers/vulkan/rendering_shader_container_vulkan.cpp:59-67
// Encode into smolv.
Span<uint8_t> spirv = p_spirv[i].spirv().reinterpret<uint8_t>();
smolv::ByteArray smolv_bytes;
bool smolv_encoded = smolv::Encode(spirv.ptr(), spirv.size(), smolv_bytes, smolv::kEncodeFlagStripDebugInfo);
```

**④ リフレクションだけでディスクリプタレイアウトが決まる**
```cpp
// drivers/vulkan/rendering_device_driver_vulkan.cpp:4302-4306
for (uint32_t i = 0; i < shader_refl.uniform_sets.size(); i++) {
    for (uint32_t j = 0; j < shader_refl.uniform_sets[i].size(); j++) {
        const ShaderUniform &uniform = shader_refl.uniform_sets[i][j];
        VkDescriptorSetLayoutBinding layout_binding = {};
        layout_binding.binding = uniform.binding;
```

---

## 4. hikariへの示唆

| 機能 | 簡易版の最小構成・難易度 | フル版の難易度 | 前提知識 |
|---|---|---|---|
| RD/RDDのような**フロントエンド/バックエンド分離** | hikariはOpenGL専用なので分離不要。**真似すべきでない過剰設計**。ただし「Shader/Texture/FBOをRAIIクラスにラップし、生成関数の戻り値をIDとして扱う」設計思想だけは踏襲価値あり(難易度1) | D3D12/Metal対応するなら必須級(難易度5、就活作品の範囲外) | インターフェース分離の原則 |
| **シェーダーのuniform/samplerバインディングを自動決定(リフレクション)** | glGetActiveUniform/glGetUniformLocationで自前リフレクションし、バインディング表を自動生成(難易度2〜3)。ただしOpenGL3.3では`layout(binding=)`が使えないサンプラーもあるため恩恵は限定的 | SPIRV-Reflect相当を自作するのは過剰(難易度5)。学ぶべきは「手書きでlocation/bindingを管理しない」という発想であり、実装そのものを真似る必要はない | GLSLのuniform block / サンプラーユニットの仕組み |
| **シェーダーバリアント管理(#define組み合わせをキャッシュ付きで並列コンパイル)** | Phong/Shadowの`#define USE_SHADOW`程度なら、素朴に「文字列連結+glCompileShaderをmapでキャッシュ」で十分(難易度2)。ShaderRDの並列コンパイル・ディスクキャッシュ・グループ有効化機構はオーバースペック | フル機能再現は不要(難易度5相当だが投資対効果が低い) | プリプロセッサ的な文字列置換 |
| **コマンド記録の遅延実行・自動バリア(RDG)** | hikariはOpenGLの即時実行モデルなので**丸ごと不要**。ただし「テクスチャの状態(読み取り中/書き込み中)を自前で追跡してバグを防ぐ」という問題意識は、将来Vulkan/D3D12学習時に効いてくる(今回は難易度対象外) | 就活作品の範囲を大きく超える | Vulkanのパイプラインバリア概念 |
| **ディスクリプタセット(UniformSet)を「焼く(bake)」設計** | OpenGL相当は無い概念だが、「uniform群をまとめて1つのオブジェクトにして使い回す」考え方自体は、マテリアルクラスの設計(Phong用のuniform一式をひとまとめにする)に応用できる(難易度2) | — | — |

**最も重要な示唆**: RenderingDeviceの価値は「Vulkanを隠す」ことより「**リソース作成の検証・キャッシュ・依存関係管理をエンジン共通コードに集約し、バックエンド固有コードを薄く保つ**」という分離規律にある。hikariでは、Shader/Texture/Framebufferをそれぞれ「作成時にOpenGLエラーをチェックし、デストラクタでglDelete*を呼ぶ」RAIIクラスにするだけで、この設計思想の主要な恩恵(リソースリーク防止・生成失敗の一元処理)は得られる。ディスクリプタセットやレンダーグラフのような高度な仕組みは模倣対象外でよい。

---

## 5. 確信度と限界

- **検証済み(コード実読に基づく)**: RD/RDDの2層分離構造、シェーダーコンパイルフロー(GLSL→glslang→SPIR-V→SPIRV-Reflect→RenderingShaderContainer→バックエンド保存→ドライバでVkDescriptorSetLayout生成)、ShaderRDのバリアント管理とディスクキャッシュの仕組み。
- **状況証拠(一部のみ読み、構造から推測)**: `rendering_device.cpp`は10343行中約220行しか読んでおらず、`texture_create`や`draw_list_*`の内部実装(RDGへの記録処理そのもの)は未確認。「RDGが命令を並べ替えて実行する」という主張は、rendering_device.hのコメント(buffer_updateのdocコメント等)と`rendering_device_graph.h`冒頭45行のみに基づく推測であり、実際のグラフ解決アルゴリズムは未読。
- **未読・推測で書いた部分**: `drivers/vulkan/rendering_device_driver_vulkan.h`(834行、シグネチャのみGrep)、`rendering_context_driver_vulkan.cpp`(1094行、デバイス選択ロジック未確認)、`vulkan_hooks.*`(役割はファイル名からの推測のみ)、D3D12/Metalバックエンドでの`_set_code_from_spirv`実装(本リポジトリにdrivers/d3d12やdrivers/metalが存在するか自体未確認、Vulkan版の実装から類推しただけ)。
- **確信度が低い箇所**: 「(d)GPU側シェーダー」でVulkan/D3D12双方の`_set_code_from_spirv`差異について「クロスコンパイルが入ると推測」と書いた部分は、Vulkan版(smolv圧縮のみ)しか実読しておらず未検証の推測。
- 依頼の「OpenGLしか知らない学習者への示唆」は、hikariの実装内容(Phong/シャドウマッピング予定、自作数学、自作OBJパーサー)をSTATUS.md等から把握していない前提での一般論であり、hikariの現在の実コードは未参照。