# 調査レポート: Ray Tracing in One Weekend(第1巻)をC++初学者がやる際の実際

## §1 教材マップ(順番つき)

**前提**: 検証の結果、RTiOW全編(第1巻〜第2巻)は**GLM等の外部数学ライブラリに一切依存しない**(自作`vec3`のみ)。生ソース(`raytracing.github.io/books/RayTracingInOneWeekend.html` v4系, 2024〜2025年改訂版)を直接取得して確認した。従って「GLM章をスキップ/書き直す」という懸念は本書には基本的に発生しない。ただし本書の`vec3`は生徒が既に持っている自作`Vec3`とは設計が異なるため、その移植コストは§3で別途扱う。

### 第1巻: Ray Tracing in One Weekend
URL: https://raytracing.github.io/books/RayTracingInOneWeekend.html (Version 4.0.x)

| # | 章 | 主なつまづきポイント |
|---|---|---|
| 1 | Overview | - |
| 2 | Output an Image(PPM形式・進捗表示) | Windowsでの出力確認(§6参照) |
| 3 | The vec3 Class(+Color Utility Functions) | 自作Vec3との統合方針をここで決める必要 |
| 4 | Rays, a Simple Camera, and Background | - |
| 5 | Adding a Sphere(Ray-Sphere Intersection) | 判別式(discriminant)の理解 |
| 6 | Surface Normals and Multiple Objects(Hittable抽象化・front face・Hittable list・Interval class) | ここが本書で最も長く、C++の新機能(`shared_ptr`, `std::make_shared`, ヘッダ分割)が集中投入される章。初学者はここで一度失速しやすい |
| 7 | Moving Camera Code Into Its Own Class | カメラクラス設計 |
| 8 | Antialiasing(乱数ユーティリティ・複数サンプル) | 乱数生成器の選択(§2④) |
| 9 | **Diffuse Materials**(子レイ数制限→shadow acne修正→真のLambertian→ガンマ補正) | 本調査で重点調査した④つまづきの半分がこの1章に集中している(§2参照) |
| 10 | Metal(Material抽象クラス・鏡面反射・fuzzy反射) | - |
| 11 | Dielectrics(屈折・Snellの法則・全反射・Schlick近似) | NaN/黒くなるバグの温床(§2⑤) |
| 12 | Positionable Camera | 座標系・FOV計算 |
| 13 | Defocus Blur(被写界深度) | - |
| 14 | Where Next? | 第2巻・第3巻・三角形メッシュ等への案内 |

前提章の流れは一直線(各章が直前の章の続き)で、分岐はない。

### 第2巻(必要な範囲のみ): Ray Tracing: The Next Week
URL: https://raytracing.github.io/books/RayTracingTheNextWeek.html (Version 4.0.2, 2025-04-25)

生ソースを取得し、章立てを実見出しで確定した(以前流布している「Rectangles and Lights」を1章にまとめた旧版と、現行版で構成が異なる点に注意→§6の限界に後述):

1. Motion Blur
2. Bounding Volume Hierarchies
3. Texture Mapping
4. Perlin Noise
5. **Quadrilaterals**(四角形プリミティブ)
6. **Lights**(`diffuse_light`マテリアル)
7. **Instances**(`translate` / `rotate_y` — Cornell箱の箱を回転させるのに必要)
8. Volumes
9. A Scene Testing All New Features

§4の重点調査(発光マテリアルのみの前倒し)はこの構成を踏まえて後述する。

---

## §2 初学者の典型的なつまづき

### ① Shadow Acne(t_min)
- **症状**: 拡散反射を実装した直後、物体表面に縞模様・斑点状のノイズが出る。
- **原因**: 反射の起点となる交点座標が浮動小数点誤差でわずかに面の内側にずれ、同じ面と再交差してしまう。
- **対処**: 本書は`world.hit(r, interval(0.001, infinity), rec)`のように交差判定の下限を0にせず0.001にすることで対策している(出典: 本文「Fixing Shadow Acne」節)。
- **既知の落とし穴**: この0.001という固定値は万能ではない。GitHub公式Discussionでは「近い方の交点0.0007を無視した結果、遠い方の交点(球の裏側、法線が内向き)を拾ってしまい、無限に近い再帰反射(5万回以上)が起きる」という副作用が報告されており、`front_face`変数によるレイの内外判定の方が根本的な対処になるという意見が出ている。
  出典: https://github.com/RayTracing/raytracing.github.io/discussions/1296

### ② 再帰深度の制限
- **症状**: 何もヒットしないレイが続くとスタックオーバーフローの恐れ。
- **対処**: `max_depth`(既定50)で打ち切り、上限に達したら黒(0,0,0)を返す。この対策はガンマ補正・shadow acne修正と同じ「Diffuse Materials」章内で連続して導入されるため、3つセットで実装すべき箇所。
  出典: 本文「Limiting the Number of Child Rays」節、https://raytracing.github.io/books/RayTracingInOneWeekend.html

### ③ ガンマ補正忘れ
- **症状**: 反射率50%のはずの球が、実際より暗く(黒っぽく)見える。
- **原因**: リニア空間の色値をそのままPPMに書き出すと、多くの画像ビューアはガンマ空間として解釈するため暗く表示される。
- **対処**: 書き出し前に各色成分の平方根(γ=2近似)を取る。
  出典: 本文「Using Gamma Correction for Accurate Color Intensity」節

### ④ 乱数の質
- 本書のデフォルト実装は`std::rand() / (RAND_MAX + 1.0)`(`<cstdlib>`)。これはグローバル状態を持つため**マルチスレッド化すると破綻する**うえ、`RAND_MAX`が処理系依存で周期・品質にばらつきがある。
- 本書自身が代替として`std::mt19937` + `std::uniform_real_distribution`への差し替えコードを併記している(「C++は伝統的に標準乱数生成器を持たなかったが、`<random>`で(不完全ながら)対応した」との注記付き)。
  出典: 本文「Some Random Number Utilities」節

### ⑤ NaN / ゼロベクトル問題(「負のゼロ」に近い実例)
検索した限り、文字通りの「負のゼロ(-0.0)」固有のバグ報告は見つからなかった(**推測**: おそらくユーザーが念頭に置いているのは以下のNaN系トラブルとの混同、または一般的な浮動小数点の常識(丸め誤差でわずかに負になった値をsqrtに渡すとNaNになる)を指していると考えられる)。実際に本書・コミュニティで確認できたのは次の2件:

1. **degenerate scatter direction(ゼロベクトル→NaN)**: Lambertian反射で`rec.normal + random_unit_vector()`がちょうど打ち消し合うとゼロベクトルになり、後段で無限大・NaNを生む。本書は`vec3::near_zero()`(全成分が1e-8未満かを判定)でこれを検知し、ゼロなら法線そのものを使う対策を入れている。
   出典: 本文「Catch degenerate scatter direction」コード、https://raytracing.github.io/books/RayTracingInOneWeekend.html
2. **屈折角のsqrt(負の値)対策**: `cos_theta = std::fmin(dot(-unit_direction, rec.normal), 1.0)`のように`fmin`でクランプしてから`sqrt(1 - cos_theta²)`を計算している。これをクランプせずに実装すると、浮動小数点誤差でdot積が1.0をわずかに超え、`sqrt`に負の値が渡ってNaNになる典型パターン(一般的なグラフィックスプログラミングの既知パターン)。
3. **誘電体が真っ黒になるバグ**: GitHub Discussionでは、`bool front_face = glm::dot(...)`のように比較演算子`< 0`を書き忘れてdoubleをboolへ暗黙変換してしまい、0以外の値(負の値含む)が常に`true`と判定される→法線の向き判定が壊れて誘電体球が真っ黒になる、という実例が報告されている。
   出典: https://github.com/RayTracing/raytracing.github.io/discussions/997

---

## §3 所要時間の実例

Reddit(r/GraphicsProgramming等)は本ツール環境から直接アクセスできず(WebFetchが`reddit.com`をブロック)、体験談はブログ・Hacker News・Zenn等に限定される点をあらかじめ断っておく。

| 所要時間 | 属性 | 詳細 | 出典 |
|---|---|---|---|
| 「週末より少し長い」 | **初心者**(自称"in no way an expert") | 家事等で中断しつつ、1週末を少し超えた程度で完走 | https://lptcp.blogspot.com/2021/02/ray-tracing-in-one-weekend-review.html |
| 数日(「ちまちま進めた」) | 日本人ブロガー、初学者寄り | 「数式の意味を確認しながらちまちま進めたので数日かかった」/「最後まで実装できる確証もないまま始めてしまった」 | https://questbeat.hatenablog.jp/entry/2020/01/13/002548 |
| 明記なし、環境構築が最難関 | ある程度C++経験あり("some experience with C++") | 「By far the hardest part of the project was setting up the environment」(clang/clang++の違いで詰まった) | https://dev.to/csilla_lukacs_4cdb393b0e6/what-i-learned-from-ray-tracing-in-one-weekend-77m |
| 「週末より少し長い」 | Rust初挑戦者(新言語+新分野を同時学習) | 「良い経験だった、ただしプロジェクトとしてはコードよりも数学の理解に時間を取られた」 | Hacker Newsコメント欄、https://news.ycombinator.com/item?id=25244301 |
| 約10時間 | **経験者**(C++→Swiftへ全訳しながら実装) | 参考値として掲載。初学者の目安にはならない | https://github.com/davecom/RayTracingInOneWeekend |

**書籍自身の言明(公式見解)**:
- 第1巻: 「週末でできるはずだが、長くかかっても気にしなくていい」(Overview)。
- 第2巻: 「BVHとPerlinノイズが最も難しい2箇所。だからこそ書名は"週末"ではなく"1週間"を示唆している。ただしBVHとPerlinを後回しにすれば週末プロジェクトにもできる」(Overview、直接引用: “without BVH and Perlin texture you will still get a Cornell Box!”)。

**傾向のまとめ(状況証拠)**: 数時間〜10時間程度で終わらせる例は「別言語への移植をしながらでも本業がプログラマ」という経験者に偏り、初学者・非エキスパートを名乗る人は軒並み「週末を少し超える」「数日かかる」と証言している。数学(内積・外積・スネルの法則等)の理解に時間を取られるというコメントが複数の独立ソースで一致しており、これは確信度が高い。

---

## §4 (重点③) 既存Vec3クラスへの移植の注意点

本書の`vec3`はソースコードから直接確認済み:

```cpp
class vec3 {
  public:
    double e[3];
    vec3() : e{0,0,0} {}
    vec3(double e0, double e1, double e2) : e{e0, e1, e2} {}
    double x() const { return e[0]; }   // 関数呼び出し、フィールドではない
    double y() const { return e[1]; }
    double z() const { return e[2]; }
    double operator[](int i) const { return e[i]; }
    double& operator[](int i) { return e[i]; }
    // += *= /= のみメンバ、+ - * / は非メンバ(フリー関数)
};
using point3 = vec3;   // typedefのみ。継承ではない
using color  = vec3;   // typedefのみ
inline double dot(const vec3&, const vec3&);   // フリー関数
inline vec3 cross(const vec3&, const vec3&);   // フリー関数
inline vec3 unit_vector(const vec3&);          // フリー関数(正規化。in-placeではなくコピーを返す)
```
出典: https://raytracing.github.io/books/RayTracingInOneWeekend.html「The vec3 Class」節(直接引用)

以下、**既存の自作Vec3クラスと食い違いやすい設計判断**を技術的に洗い出す(この節は個別の一次情報源が乏しいため、上記の確認済みAPIと、第1フェーズで想定される自作Vec3の一般的な設計との差分から**推論**したものと明記する):

1. **`point3`/`color`は別クラスではなく`using`によるただの型エイリアス**。既存の自作コードで`Vec3`とは別に`Point3`や`Color`という独立クラスを持っている場合、本書のコードをそのまま移植すると「`color`に`point3`を代入してもコンパイルが通ってしまう」設計になる。型安全性を優先するなら、本書の方式をそのまま真似ず、既存の独立クラスのままdot/cross/unit_vectorだけ生やす方が安全。
2. **`double`前提**。既存Vec3が`float`ベースの場合、`std::sqrt`のオーバーロード解決やstb_image由来のfloatデータとの混在で暗黙変換が多発する。書籍自身も「floatでもdoubleでもお好みで」としており(本文中に明記)、必須ではないが、`double`↔`float`混在はwarningの温床になる。
3. **`x()/y()/z()`が関数呼び出し**。既存Vec3が`v.x`のようにpublicフィールド直接アクセスなら、本書コードを写経する際に括弧の付け忘れでコンパイルエラーが頻発しうる(逆に既存側をラップする手もある)。
4. **`dot`/`cross`/`unit_vector`がフリー関数**。既存Vec3が`v.Dot(w)`や`v.Normalize()`(破壊的、in-place)のようなメンバ関数設計だと、書籍コードの`dot(a,b)`呼び出し形式や、`unit_vector()`が新しいベクトルを返す(非破壊的)という前提が食い違う。特に「Normalize()が自分自身を書き換えるか、新しい値を返すか」は、書籍のコードをそのまま移植すると意図せず元のベクトルを壊す/壊さないの取り違えバグを生みやすい。
5. **`operator[]`の有無**。本書のBVH章(第2巻)以降で`e[i]`アクセスが多用される場面がある。既存Vec3にoperator[]がなければ追加が必要。
6. **列優先Mat4との接続はRTiOWには存在しない**。RTiOW第1巻はMat4を一切使わず、カメラも`lookfrom`/`lookat`/`vup`から直接基底ベクトル(u,v,w)を組む方式(本文「Positioning and Orienting the Camera」節)。生徒が持っている自作Mat4は本プロジェクトでは出番がなく、「Mat4との整合」を心配する必要はない(確信度: 高、ソースコード確認済み)。

**推奨方針(提案)**: 既存Vec3を無理に本書のAPIに合わせて改造するより、既存Vec3に`dot`/`cross`/`unit_vector`のフリー関数版と`near_zero()`だけをRTiOW用に追加する「アダプタ」的な最小拡張の方が、既存コードとの互換性を壊さず移植コストも低いと考えられる(これは筆者の設計上の提案であり、出典のある事実ではない)。

## §4' (重点④) 発光マテリアル(diffuse_light)だけの前倒し・Cornell箱風シーンの難易度

第2巻の生ソースを直接確認した結果(https://raytracing.github.io/books/RayTracingTheNextWeek.html、Version 4.0.2):

- **必須**: 「Quadrilaterals」章(四角形プリミティブ`quad`クラス、平面との交差判定)→「Lights」章(`diffuse_light`マテリアル、`material::emitted()`の追加、`camera::ray_color()`への背景色パラメータ追加)。
- **箱を回転させて本物のCornell箱にするなら追加で必須**: 「Instances」章(`box()`ヘルパー関数、`rotate_y`/`translate`)。ただし壁と光源だけの「空のCornell箱」ならInstances章は不要。
- **回避可能(本書公式見解)**: BVHの木構造そのものとPerlinノイズは実装しなくてよい。本文に直接引用できる記述がある:
  > “without BVH and Perlin texture you will still get a Cornell Box!”
  出典: RayTracingTheNextWeek.html Overview節
- **ただし見落としがちな隠れコスト**: 第2巻から`hittable`基底クラスに`virtual aabb bounding_box() const = 0;`という**純粋仮想関数**が追加されている(BVH章で導入)。これは「BVHの木を組む/辿る」処理そのものではないが、`aabb`クラス定義と、`sphere`・`quad`・`hittable_list`など**すべての具象クラスに`bounding_box()`の実装を追加する**作業は、BVHを使わなくても構造上避けられない。第1巻だけで止まっている場合、この地味な配線変更が「BVHは要らないはずなのに追加作業が発生する」という誤算になりやすい。

**実装量の感触(検証済みコードから確認)**: 空のCornell箱の`main()`は壁5枚+光源1枚=`quad`の生成コード6行程度(下記に本書の実コードを引用)で足りる:

```cpp
world.add(make_shared<quad>(point3(555,0,0), vec3(0,555,0), vec3(0,0,555), green));
world.add(make_shared<quad>(point3(0,0,0), vec3(0,555,0), vec3(0,0,555), red));
world.add(make_shared<quad>(point3(343,554,332), vec3(-130,0,0), vec3(0,0,-105), light));
world.add(make_shared<quad>(point3(0,0,0), vec3(555,0,0), vec3(0,0,555), white));
world.add(make_shared<quad>(point3(555,555,555), vec3(-555,0,0), vec3(0,0,-555), white));
world.add(make_shared<quad>(point3(0,0,555), vec3(555,0,0), vec3(0,555,0), white));
```
出典: RayTracingTheNextWeek.html「Creating an Empty "Cornell Box"」節(直接引用)

つまり「Quadrilaterals+Lights」の2章分(コード量としては`quad`クラス本体+平面交差判定+`diffuse_light`クラス+`ray_color`のわずかな変更)を読めば、卒制のポートフォリオ用にコーネルボックス風の絵は十分作れる。総じて「第2巻を丸ごとやる」より大幅に軽い、というのは確信度高く言える。

---

## §5 日本語の補助資料

| 資料 | 種類 | 評価コメント |
|---|---|---|
| **inzkyk.xyz「週末レイトレーシング(翻訳)」** https://inzkyk.xyz/ray_tracing_in_one_weekend/ | 個人による無料Web完訳(第1巻・第2巻・第3巻すべて、コード例含む)。CC0ライセンスの原著を正規に翻訳。PDF版はBOOTHで有料販売(学生無料枠あり) | サンプルページを直接確認したところ翻訳品質は高く、「shadow acne」に訳注を付けるなど丁寧。**ただし**、公開・更新日は2020/6/6で止まっており、現行(2024〜2025年改訂)の英語版とは章立て・コード構成が異なる旧版ベースの可能性が高い(第2巻が11章立てと表示され、現行版の9章立てと不一致)。加えて独立した2つの日本語ブログが「屈折(dielectric)の導出部分に誤りがあり、原文コードで確認し直した」と証言している(下記Zenn記事)。**丸写しせず、屈折の章だけは必ず英語原文と突き合わせることを推奨**。 |
| **週末レイトレーシング(達人出版会)** https://tatsu-zine.com/books/ray-tracing-part1 | あんどうやすし氏による商業電子書籍訳(2017年6月20日刊、PDF/EPUB)。第1巻のみ | 出版社を通した正式な翻訳で信頼性は高いと推測されるが、2016年当時の英語版がベース(現行版とは差異がある可能性)。有料。 |
| **Zenn: 「Rustを学びつつ週末レイトレーシング〜」** https://zenn.dev/kjumanenobikto/articles/e01b520d57ba51 | 個人の実装記録(Rust)。体系的な解説記事ではない | 「日本語版に間違いがあり原文コードを参照した」との言及あり(上記inzkyk訳の誤り疑いの一次証言)。C++ではなくRustだが、つまづきポイントの言語化は参考になる。 |
| **すらりん日記(techlab-xe.net)** https://blog.techlab-xe.net/try-raytracing-one-weekend-part1/ | 個人ブログ、C++での実装記録+独自の意訳・解説 | 「本家のコードとは少し違う部分がある」と明記された独自解説型。Part1公開時点でシリーズ未完結(Part2への言及はあるが全章完走したかは未確認)。 |
| **Zenn: 「ZigでRay Tracing in One Weekendをやってみた」** https://zenn.dev/ryoppippi/articles/4fc7570643339d | 実装記録(Zig) | 言語が特殊なため直接の参考度は低いが、マテリアル設計の考え方は流用可。 |
| **Qiita: kidach1「シンプルなレイトレーサーを実装してみる」** https://qiita.com/kidach1/items/c3d04f31ccc10504a79d | 独自実装記事、RTiOWは参考文献としての言及に留まる | RTiOW自体の解説ではないため参考度は低い。 |

---

## §6 (重点⑥) レンダリング結果(PPM)をWindowsで確認する方法

GitHub公式Issueを直接調査し、**根本原因まで特定できた**:

**根本原因**: Windows PowerShell(生徒の主シェルと同じ環境)で`main.exe > image.ppm`のようにリダイレクトすると、PowerShellの既定動作により出力が**UTF-16(BOM付き)**でファイルに書き込まれる。PPMはASCII/UTF-8前提のテキスト形式のため、画像ビューアが「Improper image header」等のエラーで開けなくなる。cmd.exeで同じコマンドを実行すると問題なく開けることも実例で確認されている。
出典: https://github.com/RayTracing/raytracing.github.io/issues/636 (Issueコメント欄を直接取得して確認)

**対処法(確認された選択肢、実用的な順)**:
1. **C++コード側でファイルに直接書き込む**(最も確実): `std::ofstream`を`std::ios::binary`で開いてPPMを書き出せば、シェルのリダイレクト方式に依存しなくなる。標準出力へのリダイレクトをそもそもやめる方式。
2. **PowerShellのまま回避**: `.\main.exe | Set-Content image.ppm -Encoding String` (`-Encoding UTF8`/`-Encoding Unicode`ではなく`String`を指定する必要があると複数ユーザーが報告)。
3. **cmd.exeで実行する**: VSCode内蔵ターミナルやスタートメニューから通常のコマンドプロンプトを開いて`main.exe > image.ppm`を実行。
出典: https://github.com/RayTracing/raytracing.github.io/discussions/1114

**画像を見るツール(複数の実例で動作確認されているもの)**:
- **VSCode拡張機能**(開発環境と統合できて一番楽): 「PBM/PPM/PGM Viewer for Visual Studio Code」(`ngtystr.ppm-pgm-viewer-for-vscode`)、他に`jtlehtinen.vscode-ppm-view`、`martingrzzler.simple-ppm-viewer`などが公開されている。ビルド→保存→VSCode上でそのままプレビュー、というループが組めるため、レンダリングのたびにビューアを開き直す手間がない。
  出典: https://marketplace.visualstudio.com/items?itemName=ngtystr.ppm-pgm-viewer-for-vscode
- **ImageMagick**: `magick convert image.ppm image.png`のようにPNG変換もできる定番ツール(ただしIssue上ではImageMagickだけでは前述のUTF-16問題自体は解決しないと報告されている=文字コード問題を先に直す必要がある)。
- **FastStone Image Viewer / XnView / IrfanView**: いずれも実例でPPM表示が確認できている軽量Windows専用ビューア。
出典: https://github.com/RayTracing/raytracing.github.io/issues/636

**結論としての推奨(提案)**: 開発ループの効率を考えると、①最初からVSCode拡張機能を1つ入れておく、②C++側は`std::ofstream(..., std::ios::binary)`でファイルへ直接書く実装にしてPowerShellのリダイレクト問題自体を最初から踏まない、の2点をセットで最初にやっておくのが最も手間が少ない。

---

## §7 確信度と限界

- **確信度が高い(一次情報源=書籍のソースコード/GitHub公式Issue・Discussionで直接確認済み)**: §2の5項目すべて、§4'のCornell箱前倒しの必要章、§6のPowerShell UTF-16問題と対処法、§1の章立て。
- **確信度が中程度(複数の独立した個人ブログ・Zenn記事による状況証拠)**: §3の所要時間の傾向、§5の日本語訳の品質評価(特に「屈折章に誤訳がある」という指摘は2件の独立ブログから出ているが、具体的にどの行のどんな誤りかまでは特定できなかった)。
- **確信度が低い/推測にとどまる項目**:
  - 「負のゼロ」固有の既知バグは検索上見つからず、NaN/ゼロベクトル系トラブルとの混同ではないかという**推測**にとどめた(§2⑤)。
  - §4(既存Vec3への移植注意点)は、書籍APIとの技術的な差分から論理的に導出した**推論**であり、実際にRTiOWを既存エンジンのVec3へ移植した人の一次体験談は発見できなかった。
- **調査の限界**: このツール環境からは`reddit.com`(www/old双方)への直接アクセスができず、r/GraphicsProgramming等での一次体験談は確認できていない。Web検索経由でReddit上の投稿が要約として拾われることもあったが、具体的なコメント本文までは検証できていない。所要時間のサンプル数も6件程度と少なく、統計的な代表性はない。
- **バージョンに関する注意**: 本レポートの章立て・コード引用はすべて2024〜2025年に改訂された現行版(第1巻: 未記載、第2巻: Version 4.0.2, 2025-04-25)に基づく。日本語訳(特にinzkyk.xyz、2020年公開)や一部の英語ブログ・GitHubリポジトリは2016〜2020年頃の旧版を参照している可能性があり、コードの細部(特にカメラクラスの構造、`interval`クラスの有無、乱数生成の実装)が現行版と食い違うことがある点は、学習者自身が都度確認すべき注意点として明記しておく。