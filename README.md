# ImageViewerLite

Windows 10/11 向けの閲覧専用・軽量画像ビューア。C11 / Win32 API / WIC / Direct2D / GDI のみで構成し、インストール不要の単一 EXE で動作します。

- 版数：1.0.0
- 仕様：`ImageViewerLite_仕様書_v1.0.md`
- 変更履歴：`CHANGELOG.md`

## 生成AIによる作成について

本ソフトウェアのソースコード、リソース、仕様書、ドキュメントは、**生成AIを用いて作成**しました。

| 工程 | 使用した AI |
|---|---|
| 仕様書のたたき台、初期ソースコード（旧称 LiteViewer v1.0〜v1.7）、アイコン | OpenAI ChatGPT |
| リファクタリング、不具合修正、SVG 対応、SVG サンプル、本 README・CHANGELOG・仕様書 v1.0 | Anthropic Claude |

要件の決定と成果物の採否は利用者（作成者）が行っていますが、コードは人手による全面的なレビューを経ていません。次の「諸注意」を確認したうえで利用してください。

## ビルド（MSYS2 UCRT64）

MSYS2 の **UCRT64** ターミナルで実行します（`which gcc` が `/ucrt64/bin/gcc` であること）。

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-binutils make
make          # dist/ImageViewerLite.exe を生成
make dist     # INI も dist/ にコピー
make DEBUG=1  # デバッグビルド（-O0 -g、シンボルを残す）
make clean
```

Linux からのクロスビルドは `make CROSS=x86_64-w64-mingw32-` で行えます。

## 主な操作

| キー・マウス | 操作 |
|---|---|
| Ctrl+O / ドロップ / 第1引数 | 画像（またはフォルダ）を開く |
| Left / ホイール上 | 前の画像 |
| Right / ホイール下 | 次の画像 |
| Up / Down | 最初 / 最後の画像 |
| 1〜5 | 原寸、縮小のみ、全体、幅、高さフィット |
| + / - / Ctrl+ホイール | 拡大・縮小（Ctrl+ホイールはカーソル位置中心） |
| 左ドラッグ | パン |
| Ctrl+L | ズーム固定 |
| Enter / F11 / ダブルクリック | 全画面切替 |
| F10 | シンプルモード |
| F5 | 一覧と現在画像の再読み込み |
| Ctrl+C | 表示中画像を原寸で CF_DIB コピー |
| I | 画像の情報 |
| Esc | 全画面なら解除、それ以外は終了 |

## ソース構成

```text
ImageViewerLite/
├── Makefile
├── README.md / CHANGELOG.md / ImageViewerLite_仕様書_v1.0.md
├── ImageViewerLite.ini        既定の設定ファイル（EXE と同じフォルダに置く）
├── include/
│   ├── common.h               Windows ヘッダ設定・共通ユーティリティ
│   ├── version.h              版数（C とリソースで共用）
│   ├── resource.h             リソース ID
│   ├── types.h                FitMode / SortMode / WindowMode
│   ├── app.h                  アプリ全体の状態とモジュール間 API
│   └── (各モジュールのヘッダ)
├── src/
│   ├── main.c                 起動・終了、メッセージループ
│   ├── window.c               ウィンドウプロシージャ、マウス、表示モード、描画呼び出し
│   ├── commands.c             メニュー／ショートカットのコマンド処理
│   ├── app.c                  開く・移動・再読み込み・倍率操作
│   ├── view.c                 倍率・パンの計算（Win32 非依存）
│   ├── render.c               GDI 描画（ダブルバッファ）
│   ├── cache.c                LRU 画像キャッシュ
│   ├── worker.c               デコード用ワーカースレッド
│   ├── decoder.c              WIC デコードと形式ごとの振り分け
│   ├── svg.c                  SVG のラスタライズ（Direct2D）
│   ├── filelist.c             フォルダ列挙・並べ替え
│   ├── formats.c              対応拡張子と形式名の一覧
│   ├── settings.c             INI 読み書き（表駆動）
│   ├── settings_dialog.c      表示設定ダイアログ
│   ├── info_dialog.c          画像情報ダイアログ
│   ├── clipboard.c            クリップボードコピー
│   └── util.c                 文字列・パス・リソース文字列
├── res/
│   ├── resource.rc            メニュー・ダイアログ・ショートカット・文言・版数情報
│   ├── manifest.xml           DPI・長いパス・コモンコントロール v6
│   └── ImageViewerLite.ico
└── samples/svg/               SVG 表示確認用サンプル（期待結果は同フォルダの README.md）
```

### よくある変更の手順

| やりたいこと | 変更箇所 |
|---|---|
| 版数を上げる | `include/version.h` のみ |
| 対応拡張子を追加 | `src/formats.c` の `FORMATS` 表に1行 |
| INI の項目を追加 | `Settings` にフィールド、`src/settings.c` の `KEY_TABLE` に1行 |
| ショートカットを変更・追加 | `res/resource.rc` の `ACCELERATORS`（とメニューの表記） |
| メニューを追加 | `resource.h` に ID、`resource.rc` に項目、`commands.c` の `commands_execute` に処理 |
| 文言を変更 | `res/resource.rc` の `STRINGTABLE` |

## 諸注意

### 品質・検証状況

- **Windows 実機での動作は未検証です。** 検証は Linux 上の MinGW-w64 によるクロスビルド（警告ゼロ）と、Wine 9.0 上での基本操作の確認に限られます。MSYS2 UCRT64 でのビルド、DPI・マルチモニタ、日本語表示、クリップボード貼り付け、EXIF 回転、WebP/HEIF/AVIF、性能、リソースリークは実機で確認してください（仕様書 16.2 節）。
- **SVG の実描画は未確認です。** Wine が Direct2D の SVG 機能を実装していないため、Wine 上では SVG はエラー表示になります。最初に `samples/svg/` のサンプルを開き、`samples/svg/README.md` の期待結果と比べてください。
- MinGW-w64 のヘッダに `ID2D1DeviceContext5` の C 向け定義がないため、`src/svg.c` で必要なメソッドだけを vtable 位置を指定して自前で宣言しています。位置は Windows SDK 準拠のインターフェイス定義から数えコンパイル時に検査していますが、誤りがあると SVG 表示時に異常終了します。SVG で異常終了した場合はまずここを疑ってください。
- 生成 AI の出力には、もっともらしく見えて誤っている箇所が含まれることがあります。重要な用途に使う前に、コードを読んで確認してください。

### 利用上の注意

- 本ソフトウェアは現状のまま提供され、動作や結果について一切保証しません。利用によって生じた損害について作成者は責任を負いません。
- ライセンスは未設定です。第三者に配布・公開する場合は、事前にライセンスを決め、利用した各 AI サービスの利用規約（生成物の扱い）も確認してください。
- EXE にはコード署名をしていないため、初回起動時に SmartScreen の警告が出ることがあります。
- 設定は EXE と同じフォルダの `ImageViewerLite.ini` に保存します。`C:\Program Files` など書き込めない場所に置くと設定を保存できません。
- 本ソフトウェアは画像を読み取るだけで、元ファイルの変更・削除・移動やネットワーク通信は行いません。
- 非常に大きな画像（SVG を含む）は、展開後に最大 1GiB 程度のメモリを使うことがあります。

### 機能上の制限

- 画像の編集・保存、スライドショー、漫画モードはありません。
- GIF / TIFF / ICO は先頭フレームのみ表示します。
- WebP / HEIF / AVIF は OS にインストールされた WIC コーデックに依存します。
- SVG は Windows 10 1703 以降の Direct2D で描画します。テキスト（`<text>`）、`<style>` 要素による CSS、フィルタ、マスク、パターン、マーカー、アニメーションは描画されません。拡大表示は原寸で描画したビットマップの拡大になります。
- INI は EXE と同じフォルダに保存します。書き込めない場合は設定保存に失敗します。
- ファイル更新時のキャッシュ自動無効化はありません。F5 で再読み込みしてください。
