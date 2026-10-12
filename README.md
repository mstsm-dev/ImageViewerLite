# LiteViewer — Win32 / C11 lightweight image viewer

閲覧専用のWindows画像ビューア。C11、Win32 API、WIC、GDIを使用します。

## ビルド (MSYS2 UCRT64)

MSYS2の **UCRT64** ターミナルで実行:

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-binutils make
make
```

`dist/LiteViewer.exe` が生成されます。`LiteViewer.ini` をEXEの横にコピーしてください。

```bash
cp LiteViewer.ini dist/
./dist/LiteViewer.exe /c/Users/username/Pictures/example.jpg
```

MSYSシェル (`/usr/bin/gcc`) ではなく UCRT64 の GCC を利用してください。`which gcc` が `/ucrt64/bin/gcc` であることを確認してください。

## 主な操作

| キー | 操作 |
|---|---|
| Ctrl+O | 画像を開く |
| Space / PageDown / ホイール下 | 次画像 |
| Backspace / PageUp / ホイール上 | 前画像 |
| Home / End | 最初 / 最後 |
| 1,2,3,4,5 | 原寸、縮小のみ、全体、幅、縦フィット |
| + / - / Ctrl+ホイール | 拡大・縮小 |
| 左ドラッグ / 矢印キー | パン |
| F11 / Enter / ダブルクリック | 全画面切替 |
| F10 | シンプルモード |
| Ctrl+L | ズーム固定 |
| F5 | 一覧・現在画像の再読み込み |
| Esc | 全画面終了／アプリ終了 |

## 注意事項

- スライドショー、漫画モード、編集・保存機能なし。
- GIF/TIFF/ICOは先頭フレームのみ表示。
- WebP/HEIF/AVIFはOS上のWICコーデックに依存します。
- INIはEXEと同じフォルダ。書き込めない場合、設定保存は失敗します。
- 先読みは現在画像の次・前1枚。非同期WICデコードを実行します。
- ファイル選択は標準GetOpenFileNameWダイアログを採用。
- `WM_MOUSEWHEEL`対応。高精度マウスでは差分を累積します。

## 検証状況

コードはLinux作業環境で生成されました。この環境にはMinGW-w64クロスコンパイラがなく、現時点では実機コンパイル・動作検証ができていません。初回MSYS2ビルド時にWindows SDK/MinGWヘッダー差異による修正が必要になる可能性があります。

## 実装上の制限（v1.0ソース）

- キー割り当ての任意編集は未実装（固定ショートカット）。設定GUIは表示関連の5項目のみ。
- キャッシュはパスをキーにし、ファイル更新時の自動無効化はありません（F5で再読込）。
- 現在画像のデコード中は旧画像を維持する代わりに読み込み表示を出します。
- 補間モードの高品質指定は不透明画像でHALFTONEを使用します。透明画像の拡縮はAlphaBlendによります。
- 単一EXEでの動作はWindows標準DLLへの依存を前提とし、Windows実機での依存DLL検証は未実施です。
- 仕様書v1.0の完全実装としてではなく、Windows向けビルドと実機試験が必要な実装初版です。
