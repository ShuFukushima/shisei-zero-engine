# GLM ↔ 自作数学 対応表(M3以降の写経禁止の理由)

LearnOpenGLのTransformations章以降のサンプルコードはすべてGLM(既製の数学ライブラリ)前提。このプロジェクトの自作数学とは**見た目が似ていて意味が違う**ため、単純な置換写経はコンパイルが通っても静かに壊れる。該当章は概念の理解のためだけに読み、コードは自作数学で自分で書くこと(規則の正本: PLAN.md §5)。

## 最重要の違い(これだけは最初に読む)

**GLMの変換関数は「既存の行列を第1引数で受け取り、変換を合成して返す」。自作Mat4の変換関数は「素の変換行列を1個返す」static関数。合成は自分で掛ける。**

```cpp
// GLM(LearnOpenGLの写経コード)
glm::mat4 model = glm::mat4(1.0f);
model = glm::translate(model, pos);        // model に平行移動を合成
model = glm::rotate(model, angle, axis);   // さらに回転を合成

// 自作(このプロジェクト)
Mat4 model = Mat4::translate(pos) * Mat4::rotate(angle, axis);  // 合成は自分で掛ける
```

これを知らずに `glm::rotate(model, angle, axis)` を `Mat4::rotate(angle, axis)` に置換すると、**直前のtranslateが黙って捨てられ**、キューブが自分の中心ではなく世界原点の周りを公転する。コンパイルは通り、一見それらしく動くのが最悪の点(画面を見ても気づきにくい)。

## 対応表

| GLM(LearnOpenGL側) | 自作(このプロジェクト) | 注意 |
|---|---|---|
| `glm::mat4(1.0f)` | `Mat4::identity()` | |
| `glm::translate(m, v)` | `m * Mat4::translate(v)` | 合成は自分で掛ける(上記) |
| `glm::rotate(m, rad, axis)` | `m * Mat4::rotate(rad, axis)` | 同上。どちらも角度はラジアン |
| `glm::scale(m, v)` | `m * Mat4::scale(v)` | 同上 |
| `glm::perspective(fovy, aspect, near, far)` | `Mat4::perspective(fovy, aspect, near, far)` | 同じ引数順。fovyはラジアン |
| `glm::lookAt(eye, center, up)` | `Mat4::lookAt(eye, center, up)` | |
| `glm::value_ptr(m)` | `m.m`(float配列をそのまま渡す) | どちらも列優先 → glUniformMatrix4fv の transpose は常に **GL_FALSE** |
| `glm::vec3` | `Vec3` | |
| `glm::normalize(v)` | `v.normalized()` | **自作は長さ0でゼロ除算する(チェックしない設計)**。カメラでは pitch±89°クランプで長さ0を作らせない |
| `glm::dot(a, b)` / `glm::cross(a, b)` | `a.dot(b)` / `a.cross(b)` | 自作はメンバ関数 |
| `glm::radians(deg)` | `deg * PI / 180.0f`(PIは自前定数) | MSVCの`M_PI`は`_USE_MATH_DEFINES`必須の罠があるため自前定数を推奨 |
