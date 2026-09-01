# LearnOpenGL 高度機能学習順序 調査レポート

## 結論サマリー(先に)

- **(1) ユーザーが想定していた章立て順序は、公式サイトを直接確認した結果、完全に正しい**(検証済み)。
- **(2)** HDR→Bloom、Deferred→SSAO、Normal Mapping→Parallax Mapping の依存関係は、いずれも各ページ冒頭の記述で明示的に確認できた(検証済み)。共通の地盤は「Advanced OpenGL」章の Framebuffers。
- **(3)(4)** Reddit(r/GraphicsProgramming)には本環境のツールから一切アクセスできなかった(後述)。代わりに日本語ブログとLearnOpenGL公式の一次情報で代替した。挫折ポイントとしては「シャドウのバイアス調整」「法線マッピングのtangent space理解」「PBRのレンダリング方程式アレルギー」「複数機能を組み合わせた時の原因不明バグ」が確認できた。
- **(5) 重要な発見**: PBR公式ページが明記する前提知識は「framebuffers, cubemaps, gamma correction, HDR, normal mapping」の5つのみで、**Shadows・Parallax・Bloom・Deferred Shading・SSAOはPBRの前提に含まれていない**(検証済み直接引用)。つまりサイドバーの表示順どおりに進む必要はなく、実際に日本語圏の就活ポートフォリオ制作者もPBR→Deferred→Shadowsの順で実装していた実例が見つかった。

---

## 調査の限界(正直な開示)

Reddit(www.reddit.com, old.reddit.com)は本環境のWebFetchツールから**アクセス不可**(ドメインブロック)だった。WebSearchツールでも「site:reddit.com」を含む十数通りのクエリを試したが、r/GraphicsProgrammingの投稿本文・コメントを一件も取得できなかった(検索結果はGitHub/O'Reilly/Wikipediaなどにフォールバックし続けた)。したがって**(3)(4)についてRedditからの直接引用はゼロ件**である。この点はユーザーへの報告として重要なので先に明示する。代替として日本語ブログ(Qiita/Zenn/個人ブログ)とLearnOpenGL公式ページの記述、および実在の就活ポートフォリオ制作記録を一次情報として用いた。

---

## (1) LearnOpenGLの実際の章立て順序 [検証済み]

公式サイト https://learnopengl.com/ を直接確認した。

| セクション | 順序 |
|---|---|
| Advanced Lighting | Advanced Lighting(Blinn-Phong) → Gamma Correction → Shadows(Shadow Mapping → Point Shadows) → Normal Mapping → Parallax Mapping → HDR → Bloom → Deferred Shading → SSAO |
| PBR | Theory → Lighting → IBL(Diffuse irradiance → Specular IBL) |

ユーザーの想定通りで完全に一致していた。

---

## (2) 順序の依存関係の理由 [検証済み引用]

各ページの冒頭記述を直接確認した結果:

| 依存関係 | 根拠(直接引用) | 出典 |
|---|---|---|
| Shadow Mapping ← Framebuffers/Depth testing | 深度マップをテクスチャに描くのにFBOが前提 | https://learnopengl.com/Advanced-Lighting/Shadows/Shadow-Mapping |
| Bloom ← HDR | "Bloom works best in combination with HDR rendering." Bloomの輝度抽出は1.0を超える値が必要なためfloat framebuffer(HDR)が前提 | https://learnopengl.com/Advanced-Lighting/HDR , https://learnopengl.com/Advanced-Lighting/Bloom |
| SSAO ← Deferred Shading | "If you've followed along with the previous chapter you'll realize this looks quite like a deferred renderer's G-buffer setup... SSAO is perfectly suited in combination with deferred rendering as we already have the position and normal vectors in the G-buffer." | https://learnopengl.com/Advanced-Lighting/SSAO |
| Parallax Mapping ← Normal Mapping | "getting an understanding of normal mapping, specifically tangent space, is strongly advised before learning parallax mapping" | https://learnopengl.com/Advanced-Lighting/Parallax-Mapping |
| PBR ← 5項目のみ | "Some of the more advanced knowledge you'll need for this series are: framebuffers, cubemaps, gamma correction, HDR, and normal mapping." | https://learnopengl.com/PBR/Theory |

つまり依存関係の実体は「フレームバッファ(Advanced OpenGL章)が全ての土台」「HDRはBloomの前提」「DeferredはSSAOと相性が良い(必須ではないが再利用効率が良い)」「Normal MappingはParallaxの前提」の4つに集約され、それ以外(Gamma CorrectionとShadow Mappingの前後関係など)には技術的な依存は確認できなかった。

---

## (3) 初学者が挫折しやすい単元

Redditは取得不能だったため、確認できたのは以下(出典を都度明記):

| 単元 | 挫折要因 | 根拠の性質 |
|---|---|---|
| Shadow Mapping | シャドウアクネ/Peter Panningのバイアス値調整が「シーンごとに異なり、徐々に増やして試行錯誤するしかない」 | 検証済み(LearnOpenGL公式が明言) https://learnopengl.com/Advanced-Lighting/Shadows/Shadow-Mapping |
| Normal Mapping | tangent/bitangentとテクスチャUV方向のズレでデバッグに苦労したという個人開発者の記述あり | 状況証拠(個人ブログの断片的言及、検索結果のみで元記事URL未特定) |
| PBR全般 | 「ゴツイ数式(レンダリング方程式)が出てきたあたりで嫌悪感を感じてやめてしまう人が多いだろう」/「難しいかもしれない内容」と著者自身が前置き | 推測を含む一次情報(著者本人の観測・予想) https://www.technicalife.net/pbr-ibl-rendering-2025/ , https://qiita.com/kyasbal_1994/items/c81bceb7819f956f15a4 |
| 複数機能の組み合わせ | 「透過テクスチャ用に有効化したブレンドがDeferred Shadingに干渉し、エラーは出ないのに結果だけ壊れる」問題で「かなり苦しみました」 | 検証済み(著者本人の実体験) https://qiita.com/kkkjp/items/cd79704b479d4fdb3e7e |
| Specular IBL | 「環境光を考慮し出すと一気に複雑になってくる」 | 推測混じりの著者所感 https://www.technicalife.net/pbr-ibl-rendering-2025/ |

**傾向として言えること(推測)**: 単一機能の理論より、「複数機能を統合した瞬間にエラーメッセージなしで結果だけ壊れる」系のバグ(状態管理の副作用)が最も心理的に消耗するという証言が最も具体的だった。これは初学者にとって特に厄介(何が原因か特定する手がかりが少ないため)。

---

## (4) 初学者の実所要時間の証言 [状況証拠・要注意]

Reddit証言ゼロのため、絶対時間の一次情報は非常に限定的。唯一発見できた具体的タイムラインは、就活ポートフォリオとしてOpenGL+PBRを実装した個人ブログ https://matcha-choco010.net/2020/03/29/opengl-pbr-map/ の記事公開日(検証済み)。

| 実装内容 | 日付 |
|---|---|
| Blinn-Phongシェーダ | 3/31 |
| テクスチャ・法線マップ | 4/1 |
| PBR理論の学習(コード無し) | 4/4(3日の空白) |
| PBRシェーディング実装〜Disney PBR | 4/5 |
| Deferred(Geometry/Directional Light) | 4/6 |
| Deferred(Exposure/Tonemap/Point/Spot Light) | 4/7 |
| モデル読み込み(Blender) | 4/8 |
| シャドウ(Directional→Spot→Point、各Light種別ごと) | 4/9〜4/11 |
| Sky Pass / 物理ベースカメラ | 4/12〜4/13 |
| Diffuse IBL | 4/15(2日の空白) |
| Specular IBL | 4/18(**3日の空白、全体で最長**) |

**重要な注意**: この人物は明らかに経験者で、window表示から基本シェーディング・PBR・Deferred・全種シャドウ・IBLまでを**わずか3週間強**で駆け抜けている。これは総合演習として異常に速いペースであり、ユーザー(C++をこれから学ぶほぼ初学者)の目安には**使えない**(推測: 実時間は5〜10倍以上かかる可能性が高い)。

ただし「相対的にどこで詰まったか」のシグナルとしては有用: **PBR理論の咀嚼(3日)とSpecular IBL(3日)が、経験者にとってすら最大の時間消費ポイント**だった。これは(3)で確認した「レンダリング方程式・環境光の複雑さ」という定性的証言とも整合する。

初学者の絶対所要時間について確度の高い一次情報は今回得られなかった。この点はユーザーに正直に伝えるべきで、「Reddit等の定量的な体験談は本調査では収集できなかった」という限界を明記した上で、上記の間接証拠のみを参考情報として扱うことを推奨する。

---

## (5) 順序を入れ替えてよい箇所・スキップしてよい箇所 [検証済み+論理的推論]

| 項目 | 判定 | 根拠 |
|---|---|---|
| Gamma Correction ⇔ Shadow Mapping | **順序入れ替え可能** | 両者間の技術的依存を示す記述は確認できず。相互独立(推論) |
| Parallax Mapping | **スキップ候補の筆頭** | LearnOpenGL著者自身が「advanced lightingに直接関連する技法ではなく、normal mappingの論理的な延長として扱っているだけ」と明言。効果も「数cm以上の凹凸があるテクスチャでのみ意味がある」限定的技法 https://learnopengl.com/Advanced-Lighting/Parallax-Mapping |
| Point Shadows(cubemap版) | **工数対効果で後回し候補** | Directional/Spotの基本Shadow Mappingの技術理解を示せれば、就活ポートフォリオとしての説得力の大部分は満たせる可能性(推測)。Point Shadowsは同じ原理の6方向拡張であり技術的新規性は限定的 |
| PBR全体 | **Deferred/Bloom/SSAO/Shadowsを経由せず着手可能** | 公式prereqは「framebuffers, cubemaps, gamma correction, HDR, normal mapping」の5つのみ(検証済み)。実例としてmatcha-choco010氏もNormal Mapping直後にPBRへ進み、Deferred・Shadowsはその後に実装していた(検証済み実例) |
| SSAO ⇐ Deferred Shading | **技術的には必須ではないが実務上推奨** | 「相性が良い」("perfectly suited")という表現であり、Forward+専用G-bufferプリパスでも理論上SSAOは実装可能。ただしDeferredのG-bufferをそのまま流用する方が実装コストが低い(検証済み引用+論理的推論) |
| HDR ⇒ Bloom | **順序固定(入れ替え非推奨)** | 直接的な技術依存(輝度抽出に1.0超の値が必要)が確認済み |
| Normal Mapping ⇒ Parallax Mapping | **順序固定** | tangent space前提が明言されている |

### 就活ポートフォリオ向けの優先順位についての示唆(推測を含む提言)

- 面接での説明のしやすさ・差別化効果を考えると、**Shadow Mapping(Directional)→Normal Mapping→HDR→Bloom→Deferred Shading→PBR**を優先度の高いコアパスとし、**Point Shadows・Parallax Mapping・SSAO・IBL**は時間が余った場合のストレッチゴールに位置づける、という切り分けは技術的根拠(上表)と整合する。
- ただしこれはあくまで今回集めた一次情報からの論理的推論であり、実際の面接官の評価基準についての直接証言は本調査では得られていない。

---

## 主要出典一覧(生URL)

- https://learnopengl.com/
- https://learnopengl.com/Advanced-Lighting/Advanced-Lighting
- https://learnopengl.com/Advanced-Lighting/Shadows/Shadow-Mapping
- https://learnopengl.com/Advanced-Lighting/HDR
- https://learnopengl.com/Advanced-Lighting/Bloom
- https://learnopengl.com/Advanced-Lighting/Deferred-Shading
- https://learnopengl.com/Advanced-Lighting/SSAO
- https://learnopengl.com/Advanced-Lighting/Parallax-Mapping
- https://learnopengl.com/PBR/Theory
- https://qiita.com/kkkjp/items/cd79704b479d4fdb3e7e
- https://qiita.com/kyasbal_1994/items/c81bceb7819f956f15a4
- https://www.technicalife.net/pbr-ibl-rendering-2025/
- https://matcha-choco010.net/2020/03/29/opengl-pbr-map/
- https://andnelly.itch.io/funmi/devlog/303857/shadows-work-to-a-good-enough-point