# C++演習ドリル対応表(daily-plan.md の「ドリル(a)〜(e)」の正体)

目的: 教材が「淡泊でつまらない」まま進まないように、**実際に問題を解いてACを取る体験**を混ぜる。ドリルの日(火・木)はここから2問選ぶ。

調査日: 2026-08-20(URLと閲覧可否はすべて実際にアクセスして確認済み)

## 使い方と節約術(重要)

- **主戦場は Paiza「レベルアップ問題集」**(スキルチェックではない方)。問題文はログインなしでも読める。制限時間なし、解答コード例つき
- **学習チケットの心配は不要**: 学校経由の加入なので提出・テストケース閲覧・解答例閲覧はすべて無制限。Paiza上で直接書いて直接提出してよい(無料会員向けのチケット制限はこのアカウントには当たらない)
- ただし**解答コード例を見るのは自力AC後か、30分詰まった後**にすること(見放題でも、先に見たら演習にならない。学習規律は PLAN.md §5 と同じ)
- **AtCoder APG4b** は完全無料・全文公開のC++入門教材。解説→章末のEX問題の順で使う
- **AOJ** は問題文が英語(ChatGPTに翻訳してもらってよい。「英語の仕様を読む」のも実は仕事の練習になる)。URLは必ず下記の旧システム(judge.u-aizu.ac.jp)のものを使う
- **スキルチェック(D/Cランク)は月1回の腕試し**: ランク認定は一発勝負だが、問題自体は登録後なら何度でも練習できる。9月末にDランク、11月末にCランクに挑戦してみるのがおすすめ(取れたら履歴書・ESに書ける)

## (a) 変数・入出力・条件分岐・ループ(9月第1〜2週)

| # | 問題 | URL |
|---|---|---|
| 1 | APG4b 1.04 変数と型 EX4「◯年は何秒?」 | https://atcoder.jp/contests/apg4b/tasks/APG4b_cs |
| 2 | APG4b 1.06 if文 EX6「電卓をつくろう」 | https://atcoder.jp/contests/apg4b/tasks/APG4b_cq |
| 3 | AOJ ITP1_1_A「Hello World」 | https://judge.u-aizu.ac.jp/onlinejudge/description.jsp?id=ITP1_1_A |
| 4 | AOJ ITP1_2_A「Small, Large, or Equal」(条件分岐) | https://judge.u-aizu.ac.jp/onlinejudge/description.jsp?id=ITP1_2_A |
| 5 | Paiza 条件分岐メニュー(C++)から好きな2問 | https://paiza.jp/works/mondai/conditions_branch/problem_index?language_uid=c-plus-plus |

## (b) 関数分割・ヘッダの概念(9月第4週)

| # | 問題 | URL |
|---|---|---|
| 1 | APG4b 1.15 関数(解説を読む) | https://atcoder.jp/contests/apg4b/tasks/APG4b_p |
| 2 | APG4b EX15「三人兄弟へのプレゼント」(関数定義) | https://atcoder.jp/contests/apg4b/tasks/APG4b_ch |
| 3 | 【自作課題】ヘッダ分割はオンラインジャッジでは練習できない(単一ファイル採点のため)。ChatGPTにこう頼む: 「**EX15で書いた関数を .h と .cpp に分割する課題を出して。分割の手順は教えていいけど、コードは書かないで**」 | — |

## (c) クラス・構造体・演算子オーバーロード(9月末〜10月・M2前後)

| # | 問題 | URL |
|---|---|---|
| 1 | Paiza「構造体の作成」 | https://paiza.jp/works/mondai/class_primer/class_primer__make |
| 2 | Paiza「クラスの作成」(メンバ変数+メンバ関数) | https://paiza.jp/works/mondai/class_primer/class_primer__make_class |
| 3 | Paiza「クラスの継承」 | https://paiza.jp/works/mondai/class_primer/class_primer__inheritance |
| 4 | APG4b 3.04 構造体 EX24「時計の実装」(set/to_str/shiftを持つClock構造体。実質クラスの演習) | https://atcoder.jp/contests/apg4b/tasks/APG4b_by |
| 5 | 【自作課題】演算子オーバーロードの外部演習は存在しない(3サービスとも)。ChatGPTにこう頼む: 「**2次元ベクトル(x,y)のクラスに + と * (スカラー倍)を演算子オーバーロードで足す課題を出して。テスト入出力例も付けて。コードは書かないで**」← これがそのままM2のVec3の予行演習になる | — |

## (d) 参照・const参照・ポインタの入り口(10月・M3前後)

| # | 問題 | URL |
|---|---|---|
| 1 | APG4b 2.04 参照(解説を読む)→ EX19「九九の採点」(参照渡しで書き換え) | https://atcoder.jp/contests/apg4b/tasks/APG4b_cd |
| 2 | 【自作課題】ポインタの演習問題も外部には存在しない(APG4b 4.05は解説のみ)。ChatGPTにこう頼む: 「**①2つのintをポインタ経由で入れ替えるswap関数 ②配列をポインタで走査して合計を出す関数、の2課題を出して。図解つきで。コードは書かないで**」 | — |

## (e) 文字列処理・パース(11月・OBJパーサー前)

| # | 問題 | URL |
|---|---|---|
| 1 | Paiza「数式の計算」(1桁の四則演算式をパースして計算=ミニ電卓パーサー) | https://paiza.jp/works/mondai/string_primer/advance_step9 |
| 2 | Paiza 文字列処理メニュー(C++・全30問)から好きな問題 | https://paiza.jp/works/mondai/string_primer/problem_index?language_uid=c-plus-plus |
| 3 | AOJ ITP1_9_A「Finding a Word」(単語分割+大小無視の検索。トークナイズの好例) | https://judge.u-aizu.ac.jp/onlinejudge/description.jsp?id=ITP1_9_A |

## 補足(調査で分かった注意点)

- スキルチェックの問題本文は非ログインでは読めない(登録すれば読める・練習は何度でも可。「ランク認定」だけが一発勝負)
- AOJの新システム(onlinejudge.u-aizu.ac.jp)はAIから読めないことがある。AIに問題を相談するときは上記の旧システムURLを貼ること
- APG4bには「クラス」「演算子オーバーロード」の章がそもそも無い。この2つは上の【自作課題】で補う設計にしてある(むしろ自作数学ライブラリの予行になるので好都合)
