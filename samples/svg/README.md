# ImageViewerLite SVG テストサンプル

仕様書 v1.0 の 4.2 節・受入テスト T31〜T33 の確認用。
`_preview_reference.png` はブラウザ相当のレンダラー（librsvg）での描画例。Direct2D では 10 番が異なる（下表）。

| ファイル | 確認内容 | 期待する原寸 | 期待結果 |
|---|---|---|---|
| 01_shapes.svg | rect / circle / ellipse / line / polyline / polygon | 480×320 | 参照画像どおり |
| 02_gradients.svg | 線形・放射グラデーション、stop-opacity、spreadMethod=reflect | 480×320 | 参照画像どおり |
| 03_transparency.svg | 背景なし・半透明の重なり | 400×300 | 背景色／市松模様と合成。Ctrl+C では背景色に合成した24bppで貼り付け |
| 04_paths_clip_use.svg | path（ベジェ・円弧）、clipPath、use、transform、破線 | 480×320 | 参照画像どおり |
| 05_viewbox_only.svg | width/height なし | 360×120 | viewBox から決定 |
| 06_units_mm.svg | mm 単位（Windows 10 1709 以降） | 378×189 | 100mm×50mm を 96dpi で換算 |
| 07_width_only.svg | width のみ指定 | 600×200 | 高さは viewBox の比率で補完 |
| 08_no_size.svg | サイズ指定も viewBox もなし | 300×150 | 既定サイズ |
| 09_huge.svg | 20000×10000 指定 | 16384×8192 | 上限に合わせ縮小描画。展開後約512MiBのため読込に時間がかかる |
| 10_unsupported_elements.svg | text / `<style>` / filter / mask / pattern の混在 | 480×320 | 上段の水色四角と黄色い円だけ参照どおり。文字は出ない、CSS指定の四角は既定色（黒）、ぼかしなし、マスクの穴なし、ドット模様は出ない（または塗りなし）。異常終了しないこと |
| 11_icon_viewer.svg | 小さい原寸の拡大表示 | 64×64 | 拡大するとぼやける（ビットマップ拡大） |
| 12_comment_doctype.svg | XML宣言・DOCTYPE・コメント内の偽 `<svg>` | 240×160 | コメント内の 9999×9999 を拾わない |
| 13_broken.svg | 壊れたXML | — | 「画像を表示できません」。次の画像へ移動できる |
| 14_not_svg.svg | SVGではないテキスト | — | 同上 |
| 15_embedded_image.svg | base64埋め込み画像（対応）と外部参照画像（非対応） | 320×200 | 左の埋め込み画像は表示、右の破線枠内は空 |

原寸は ImageViewerLite のサイズ判定コード（src/svg.c）で算出した値と一致することを確認済み。
ステータスバーの解像度表示、または画像情報（I キー）の「元のサイズ」で確認できる。
