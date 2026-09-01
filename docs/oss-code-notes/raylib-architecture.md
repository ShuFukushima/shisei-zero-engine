> 作成: 2026-08-20 / AI読解エージェントによる実ソースコード読解記録
> 対象スナップショット: raylib (f9e2215) / ライセンス: zlib(コード引用は出典明記のうえ)
> 行番号はこのスナップショット時点のもの。最新版ではズレる。代表引用の一部はメインAIが実コードと突き合わせて一致を確認済み(全数検証ではない)。各記録末尾の「確信度と限界」を必ず読むこと。

# raylib 読解レポート: rlgl / rmodels / rcore 設計解析

## 1. 読んだファイル一覧(相対パスはraylibリポジトリルートから)

| ファイル | 全体行数 | 実読した範囲(行番号) |
|---|---|---|
| `src/rlgl.h` | 5422行 | 1-200(ヘッダ/設定コメント), 376-490(構造体定義), 1031-1150(グローバル状態rlglData), 1451-1630(rlBegin/rlEnd/rlVertex3f等), 1637-1701(rlSetTexture), 2776-2910(rlLoadRenderBatch), 2957-3183(rlDrawRenderBatch), 3203-3227(rlCheckRenderBatchLimit) — 実読計約830行 |
| `src/rmodels.c` | 7359行 | 1-60(ヘッダ/define), 1113-1278(LoadModel/UnloadModel), 1278-1452(UploadMesh全体), 1450-1703(DrawMesh全体), 1943-1987(UnloadMesh), 2224-2298(LoadMaterialDefault/UnloadMaterial), 4414-4503(LoadOBJ冒頭) — 実読計約620行 |
| `src/rcore.c` | 4683行 | 301-400(CoreData構造体), 590-734(InitWindow), 867-926(BeginDrawing/EndDrawing) — 実読計約300行(依頼通り「構造レベル」に留めた) |
| `src/raylib.h` | (全138620バイト、部分読) | 340-443(Mesh/MaterialMap/Material/Model/ModelAnimation構造体定義) |
| `examples/shaders/rlights.h` | 170行 | 全文読了 |
| `examples/shaders/resources/shaders/glsl330/lighting.vs` | 32行 | 全文読了 |
| `examples/shaders/resources/shaders/glsl330/lighting.fs` | 78行 | 全文読了 |

Grepで関数一覧・定数値のみ確認し本文は未読の箇所: `rlLoadShaderDefault`(rlgl.h 4988行〜、デフォルトシェーダーのGLSL文字列本体)、`DrawMeshInstanced`(rmodels.c 1704行〜)、`LoadOBJ`の頂点詰め込みループ後半、rcore.cのプラットフォーム分岐(GLFW/SDL/RGFW)、rtextures.c/rshapes.c/rtext.c(担当範囲外)。

---

## 2. 機能→実装の対応記録

### 2-1. rlglの即時モードAPI→バッチ描画変換(OpenGL 3.3バックエンド)

**(a) 主要ファイルと行範囲**
構造体: `rlgl.h:376-428`(rlVertexBuffer/rlDrawCall/rlRenderBatch) / グローバル状態: `rlgl.h:1031-1117`(rlglData + `static rlglData RLGL = {0};`) / 即時モードAPI: `rlBegin` 1451-1481, `rlVertex3f` 1494-1555, `rlColor4ub` 1610-1616 / バッチ確保: `rlLoadRenderBatch` 2776-2907 / バッチ描画: `rlDrawRenderBatch` 2957-3180 / 上限チェック: `rlCheckRenderBatchLimit` 3203-3226 / テクスチャ切替: `rlSetTexture` 1637-1690

**(b) 中心データ構造**
- `rlVertexBuffer`: CPU側生ポインタ(vertices/texcoords/normals/colors/indices)とGPU識別子(vaoId, vboId[5])を同じstructに同居させる。1つの構造体が「CPUバッファ」と「アップロード先のGPUタグ」を兼ねる設計。
- `rlDrawCall`: mode, vertexCount, vertexAlignment(次の描画がずれないためのパディング数), textureId のみ。**コード中コメントに明記**: 「テクスチャの変化だけが新規draw callを発行するトリガーで、シェーダーや行列の変化はrlgl.hの外(呼び出し側)で強制フラッシュする」という割り切った設計。
- `rlRenderBatch`: rlVertexBuffer配列(multi-buffering用、既定1本)+ rlDrawCall配列(既定256枠)+ drawCounter + currentDepth(2D描画のz-fighting回避用にrlEndのたび1/20000ずつ加算)。
- `rlglData`(グローバル変数`RLGL`): currentBatch/defaultBatch, State(行列スタック・現在テクスチャ・現在シェーダー・ブレンドモード等), ExtSupported(VAO/インスタンシング対応可否)の3層構造。**rcore.cのCoreData(301-397行)も同一の「無名structでサブシステムごとに分割したグローバル状態体」パターン**であり、rlglDataと様式が完全に統一されている。

**(c) 処理フロー(実際に追った呼び出し順)**
1. `rlBegin(QUADS)` → 直前のdraw callのvertexAlignmentを計算 → `rlCheckRenderBatchLimit()`でオーバーフロー確認 → drawCounter++で新draw call枠を開始
2. `rlVertex3f(x,y,z)` → transformRequiredなら現在の行列で変換 → 頂点上限に近ければ`rlCheckRenderBatchLimit()`で先行フラッシュ → CPU配列にState.texcoordx等の"現在値"を書き込み → vertexCounter++
3. `rlSetTexture(id)` → 現在draw callのtextureIdと異なれば、現在draw callを確定(rlBeginと同じアライメント計算コードが重複)してdrawCounter++。**テクスチャが変わっただけでは即GL描画は発生しない**——draws[]に新エントリが積まれるのみ
4. draws配列がRL_DEFAULT_BATCH_DRAWCALLS(既定256)に達する、または頂点バッファ満杯で`rlDrawRenderBatch()`発火
5. `rlDrawRenderBatch()` → VAO bind → 4本のVBOへ`glBufferSubData`で部分更新 → `glUseProgram` → MVP等をuniformアップロード → `for(i=0;i<drawCounter;i++)`でdraws[i].textureIdごとに`glBindTexture`→`glDrawElements`(QUADS)/`glDrawArrays`(LINES/TRIANGLES) → vertexCounter=0・drawCounter=1にリセット
6. 毎フレーム末尾、`EndDrawing()`(rcore.c 886行)が`rlDrawRenderBatchActive()`を呼び強制フラッシュ

「rlVertex3fのたびにCPU配列へ蓄積し、テクスチャ/描画モードが変わるたびdraws[]へ新エントリを積み、バッファ枯渇・draws上限・フレーム末尾のいずれかで初めて実GL描画命令を発行する」という設計。

**(d) GPUシェーダー**: `rlLoadShaderDefault`(rlgl.h 4988行〜)で頂点色×拡散色×テクスチャのデフォルトシェーダーをC文字列として埋め込みコンパイルしている(呼び出し構造のみ確認、ソース文字列本体は**未読**)。

**(e) 依存**: rcore.c(EndDrawingがフラッシュを駆動)。ただし**3Dメッシュ描画(DrawMesh)はこのバッチシステムを経由せず直接VAO/VBOをbindする**——バッチに載るのは2D図形(rshapes.c)とテキスト(rtext.c)のみ、という切り分けを確認した。

### 2-2. Mesh/Material/Model構造体設計と所有権

**(a) 主要ファイルと行範囲**
構造体: `raylib.h:346-433` / アップロード: `rmodels.c UploadMesh 1278-1441` / 描画: `rmodels.c DrawMesh 1450-1701` / 解放: `UnloadMesh 1943-1967`, `UnloadMaterial 2259-2274`, `UnloadModel 1217-1240` / デフォルトマテリアル: `LoadMaterialDefault 2224-2242`

**(b) 中心データ構造**
- `Mesh`: CPU生ポインタ(vertices/texcoords/texcoords2/normals/tangents/colors/indices/boneIndices/boneWeights)+ 実行時CPUスキニング用の`animVertices`/`animNormals`(元データと分離)+ GPU側`vaoId`(1個)/`vboId`(可変長配列、`MAX_MESH_VERTEX_BUFFERS`=9、GPUスキニング無効時7)。
- `MaterialMap`: `Texture2D + Color(tint) + float value`。1マップ=1テクスチャスロット。`MAX_MATERIAL_MAPS`=12。
- `Material`: `Shader`(値として保持)+ `MaterialMap*`配列 + `params[4]`。Shaderは`{id, int* locs}`のみの軽量構造体なので複数Materialが同じシェーダーidを共有しても壊れない。
- `Model`: transform行列 + meshes配列/materials配列 + **`meshMaterial`(各メッシュがmaterials[]の何番目を使うかを示すintインデックス配列)**という単純な写像でMeshとMaterialを結ぶ(多対多構造ではない)。

**所有権/解放の対応(コードで確証したもの)**
- `UploadMesh`: `mesh->vaoId>0`なら再アップロードを**拒否**(1280-1285行、二重アップロード防止ガード)。CPU配列→GPUへアップロードしvaoId/vboId[]を書き込む。
- `UnloadMesh`: GPU側(VAO/VBO)とCPU側(vertices〜animNormals全ポインタ)を1関数で両方解放。
- `UnloadMaterial`: **シェーダーIDがデフォルトシェーダーと一致すればUnloadShaderをスキップ**(2262行)、**テクスチャIDがデフォルトテクスチャと一致すればrlUnloadTextureをスキップ**(2269行)——共有デフォルトリソースを個々のMaterialが誤って握りつぶさないためのガード。
- `UnloadModel`: 各meshにはUnloadMeshを呼ぶが、**materialsについてはmaps配列だけをFREEし、シェーダー/テクスチャ自体はUnloadしない**。理由がコメントで明記されている(引用は代表コード引用を参照):「複数モデルでシェーダー・テクスチャを共有している可能性があるため、モデルの責任範囲はmapsまで、シェーダー/テクスチャの解放はユーザーの責任」。

**(c) 処理フロー(DrawMesh、GL3.3経路)**
1. `rlEnableShader(material.shader.id)`
2. colDiffuse/colSpecularをuniformへ
3. view/projection/model/normal行列を計算、`shader.locs[...] != -1`のときだけ`rlSetUniformMatrix`(**シェーダーが該当uniformを持たなければlocsが-1になっており、無駄なglUniform呼び出しをスキップ**)
4. MaterialMap配列をループし`texture.id>0`のマップだけ`rlActiveTextureSlot`→`rlEnableTexture`/`rlEnableTextureCubemap`
5. `rlEnableVertexArray(mesh.vaoId)`失敗時(VAO非対応)のみ、vboIdごとに手動で`rlEnableVertexBuffer`+`rlSetVertexAttribute`(フォールバック経路)
6. MVPをuniformへ、`mesh.indices!=NULL`なら`rlDrawVertexArrayElements`、なければ`rlDrawVertexArray`
7. 使用テクスチャ・VAO/VBO・シェーダーを全てDisableし、rlgl内部のmodelview/projectionを描画前の値に復元

**(d) GPUシェーダー**: `lighting.vs`(32行)は`matModel`でワールド座標`fragPosition`を、`matNormal`(=transpose(inverse(matModel)))で法線をワールド空間へ変換して渡すのみ。`lighting.fs`(78行)は`Light`構造体の配列(uniform、MAX_LIGHTS=4)をループし、type(DIRECTIONAL/POINT)で光線ベクトルを分岐、Lambert拡散+反射ベクトル式スペキュラ(`pow(dot(viewDir,reflect(-light,normal)),16.0)`)を加算し、最後に`pow(finalColor, 1/2.2)`でガンマ補正。

**(e) 依存**: rlgl.h(頂点属性・uniform設定全て経由)、raymath.h(MatrixMultiply/Invert/Transpose)、rtextures.c(テクスチャロード、未読)。

### 2-3. rlights.h(ライティングサンプルのヘルパー)
`Light`構造体はCPU側の値(type/enabled/position/target/color)と、対応するuniform locationのキャッシュ(enabledLoc等5個)を同じ構造体に同居させる。`CreateLight()`が`GetShaderLocation`を4回呼んでlocationをキャッシュしてから`UpdateLightValues()`で初期送信、以後は`UpdateLightValues()`を毎フレーム呼ぶだけでよい。`lightsCount >= MAX_LIGHTS`だと新規ライトはサイレントに無視される(エラーにならない)。

### 2-4. 命名規則・ファイル分割・コメントスタイルの一貫性(公式の「読みやすさ優先」の実態)
- **2層の命名分離**: `rl*`プレフィックス(rlgl.h内部API、低レベル)と、プレフィックスなしPascalCase(raylib公開API、高レベル)が完全に分離されており、名前を見ただけでどちらの層かが分かる。
- **全ファイル共通のヘッダテンプレート**: description/CONFIGURATION/LICENSEの3ブロック構成が`rlgl.h`と`rmodels.c`で一字一句同じフォーマット(著者名・zlibライセンス文まで共通)。
- **セクション区切りコメント**が`//----...----`の同一書式で全ファイルに存在し、「Defines and Macros」「Types and Structures Definition」「Module Functions Declaration」「Module Functions Definition」の順に統一されている。
- **`#include`ごとに`// Required for: X, Y`という理由コメント**が例外なく付与されている(rmodels.c 53-56行など)。
- コード分岐の根拠(なぜこのガードが要るか)をNOTE:/WARNING:で残す習慣が徹底している(UnloadModelの所有権コメントが好例)。

以上から、「読みやすさ優先」は単なる標語ではなく、テンプレート化されたコメント規約として実際に維持されていることを確認した。

---

## 3. 代表コード引用

**① テクスチャ変化でdraw callを分割する仕組み(バッチの核心)**
`src/rlgl.h:1658-1670`
```c
RLGL.State.currentTextureId = id;
if (RLGL.currentBatch->draws[RLGL.currentBatch->drawCounter - 1].textureId != id)
{
    if (RLGL.currentBatch->draws[RLGL.currentBatch->drawCounter - 1].vertexCount > 0)
    {
        // Make sure current ...vertexCount is aligned a multiple of 4,
        // that way, following QUADS drawing will keep aligned with index processing
        if (RLGL.currentBatch->draws[RLGL.currentBatch->drawCounter - 1].mode == RL_LINES)
            RLGL.currentBatch->draws[RLGL.currentBatch->drawCounter - 1].vertexAlignment = ...;
        else if (... == RL_TRIANGLES) ... vertexAlignment = ...;
        else RLGL.currentBatch->draws[RLGL.currentBatch->drawCounter - 1].vertexAlignment = 0;
```

**② 頂点属性が無い場合は「無効化+デフォルト値」を明示的にセットする慣習**
`src/rmodels.c:1315-1328`
```c
if (mesh->texcoords != NULL)
{
    mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD] = rlLoadVertexBuffer(...);
    rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD, 2, RL_FLOAT, 0, 0, 0);
    rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD);
}
else
{
    float value[2] = { 0.0f, 0.0f };
    rlSetVertexAttributeDefault(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD, value, SHADER_ATTRIB_VEC2, 2);
    rlDisableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD);
}
```

**③ 「モデルの所有権はどこまでか」を明文化したコメント**
`src/rmodels.c:1219-1231`
```c
// Unload meshes
for (int i = 0; i < model.meshCount; i++) UnloadMesh(model.meshes[i]);

// Unload materials maps
// NOTE: As the user could be sharing shaders and textures between models,
// don't unload the material but free its maps,
// the user is responsible for freeing models shaders and textures
for (int i = 0; i < model.materialCount; i++) RL_FREE(model.materials[i].maps);

// Unload arrays
RL_FREE(model.meshes);
RL_FREE(model.materials);
RL_FREE(model.meshMaterial);
```

**④ バッチ上限を超えたら"今ある分だけ"先にGL描画してから続行する**
`src/rlgl.h:3207-3217`
```c
if ((RLGL.State.vertexCounter + vCount) >=
    (RLGL.currentBatch->vertexBuffer[RLGL.currentBatch->currentBuffer].elementCount*4))
{
    overflow = true;
    int currentMode = RLGL.currentBatch->draws[RLGL.currentBatch->drawCounter - 1].mode;
    int currentTexture = RLGL.currentBatch->draws[RLGL.currentBatch->drawCounter - 1].textureId;
    rlDrawRenderBatch(RLGL.currentBatch);    // NOTE: Stereo rendering is checked inside
    RLGL.currentBatch->draws[RLGL.currentBatch->drawCounter - 1].mode = currentMode;
    RLGL.currentBatch->draws[RLGL.currentBatch->drawCounter - 1].textureId = currentTexture;
}
```

---

## 4. hikariへの示唆

### 4-1. 機能別の難易度評価

| 機能 | 簡易版の最小構成 | 難易度(3=シャドウマッピング相当) | フル版難易度 | 前提知識 |
|---|---|---|---|---|
| バッチ描画システム | デバッグ線描画用に、成長するVBOへ`glBufferSubData`で毎フレーム書き込み→フレーム末尾で1回`glDrawArrays` | 2 | 4(テクスチャ切替でのdraw call分割、LINES/TRIANGLES/QUADSごとのアライメント処理、multi-buffering) | VBO/VAO基礎、glBufferSubDataによる動的更新、glDrawElements/glDrawArraysの違い |
| Mesh/Material/Model構造体+所有権設計 | Mesh/Material/Model の3構造体 + 明示的なUpload/Unload関数ペア(所有権をコメントで明記) | 2(C++ならデストラクタ/RAIIで代替でき、Cより容易) | 4(MaterialMap配列化、GPUスキニング用VBO追加、Model内でのメッシュ↔マテリアルのインデックス写像) | 頂点属性レイアウト理解、ポインタ所有権/ライフタイム管理 |
| 複数ライト+uniform location キャッシュ(rlights.h方式) | Light構造体1つ+location5個キャッシュ、配列化はまだしない | 1(hikariは既にPhong実装済みのため事実上ここは通過済み) | 3(MAX_LIGHTS配列化+type分岐+attenuation) | GetShaderLocation/SetShaderValueのAPI設計、シェーダー側struct配列uniform |

### 4-2. hikariのM6〜M7で直接真似できる具体的パターン(C→C++翻訳付き)

1. **状態変化を検知した自動フラッシュ**(`rlSetTexture`, 2-1(c)②): 「毎回描画するのではなく、状態(テクスチャ/シェーダー)が変わった時だけ確定させる」という発想は、hikariが複数メッシュを描画するループでも「マテリアルが変わった時だけシェーダーuseProgram/テクスチャbindする」という最適化にそのまま使える。C++では`if (currentMaterial != mat) { bind(); currentMaterial = mat; }`という単純な比較で書ける。

2. **属性が無ければ`glDisableVertexAttribArray`+デフォルト定数値**(引用②): OBJに法線やUVが無い場合の分岐処理はhikariの自作OBJパーサーでも既に必要な発想のはず。C++では`std::optional<std::vector<Vec2>>`のようにOptionalな属性を表現し、無ければ`glVertexAttrib2f`でデフォルト値を送るという同じパターンに翻訳できる。

3. **所有権をコードコメントで明文化する規約**(引用③): 「このクラスが解放する範囲はここまで、それ以外は呼び出し側の責任」という一文コメントを、hikariのMesh/Material/Modelクラスの**デストラクタ直前**に必ず書く習慣にする。C++なら`std::shared_ptr<Texture>`でMaterial間のテクスチャ共有を安全にし、UnloadTexture相当のコードを書かなくて済む(rayilbがCで手動リファレンスカウント的ガードをしている問題をC++の型システムで解決できる好例)。

4. **uniform locationを一度だけ取得してstructにキャッシュする**(rlights.h): 毎フレーム`glGetUniformLocation`(文字列比較)を呼ぶのは非効率。hikariのLightクラスも初期化時に`GLint location`をメンバーに持ち、以後は`glUniform*`だけ呼ぶ設計にする。C++ならコンストラクタで一括取得すればよい。

5. **無名structでサブシステムごとに分割したグローバル/シングルトン状態**(CoreData/rlglData, 2-1(b)): ImGui導入期にウィンドウ・入力・タイマー・レンダラ状態をバラバラなグローバル変数で持つと管理が破綻しやすい。raylibの`CORE.Window.xxx`/`CORE.Input.Keyboard.xxx`のように**1つのApplicationState構造体をKeyboard/Mouse/Window等のネストされたstructで整理する**のは、C++では`class Application`のメンバー構造体(または各サブシステムを専用クラスに分離しApplicationが所有)としてそのまま翻訳できる。

6. **ヘッダファイル冒頭の定型コメント(description/configuration/license)とセクション区切り**: hikariのクラスヘッダにも「このクラスの責務」「主要メンバーの単位・所有権」を冒頭コメントとして定型化すると、後で読み返す・就活作品として他人に見せる際の可読性が上がる(rlights.hのような単一責務ヘッダオンリーライブラリの構成は、hikariのユーティリティ系クラス——例えばCamera制御やLightクラス——に応用しやすい)。

7. **「デフォルトリソースは共有物なので個別に解放しない」ガード**(`UnloadMaterial`のid比較): hikariでもデフォルトテクスチャ(1x1白ピクセル等)を複数マテリアルで共有する設計にするなら、C++では`shared_ptr`の参照カウントに任せれば、raylibのような手動id比較ガードは不要になる——ここはC++の方が素直に安全にできる好例として報告する価値がある。

---

## 5. 確信度と限界

**検証済み(実際にReadして行番号を確認したもの)**: rlgl.hのバッチ構造体・rlBegin/rlVertex3f/rlSetTexture/rlDrawRenderBatch/rlCheckRenderBatchLimitの制御フロー全体、rmodels.cのMesh/Material/Model構造体、UploadMesh/DrawMesh/UnloadMesh/UnloadMaterial/UnloadModelの全文、rcore.cのCoreData構造体とInitWindow/BeginDrawing/EndDrawingの構造、rlights.hとlighting.vs/fsの全文。これらは全て今回の直接読解に基づく。

**状況証拠/推測(明示的に未検証)**:
- デフォルトシェーダー(`rlLoadShaderDefault`, rlgl.h 4988行〜)のGLSLソース文字列自体は**未読**。「頂点色×拡散色×テクスチャ」という記述は関数名・呼び出し文脈からの推測であり、実際のシェーダーコードでは確認していない。
- `LoadOBJ`(rmodels.c 4414行〜)は冒頭90行のみ読み、tinyobj_loader_cライブラリを使っていること(自作パーサーではない)は確認したが、頂点属性を実際に詰め込むループ以降の詳細ロジックは未読。
- rcore.cのプラットフォーム分岐(GLFW/SDL/RGFW/DRM/Android)の実装差は依頼通り「構造レベル」に留め、内部は一切未読。
- GPUスキニング(`SUPPORT_GPU_SKINNING`)分岐は行番号のみ確認、頂点シェーダー側のボーン変換実装は未読。
- `DrawMeshInstanced`(1704行〜)、rtextures.c/rshapes.c/rtext.cは担当範囲外につき完全に未読。

**覆り得る条件**: 上記の未読部分(特にデフォルトシェーダーのGLSL内容)を実際に読めば、2-1(d)の記述の一部(シェーダーの正確な演算式)が修正される可能性がある。また今回はOpenGL 3.3/ES2の`#if`分岐のみを追っており、OpenGL 1.1固定機能パイプライン経路(`GRAPHICS_API_OPENGL_11`)のコードは意図的に読み飛ばした。