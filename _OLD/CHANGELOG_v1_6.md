# LiteViewer v1.6 キー割り当て変更

- Left: 前の画像
- Right: 次の画像
- Up: 最初の画像
- Down: 最後の画像
- 矢印キーによるパン操作を廃止。パンは左マウスドラッグを使用。
- 移動キーは `src/resource.rc` の `IDR_ACCELERATORS` で管理。
- Space、Backspace、PageUp、PageDown、Home、Endの画像移動割り当てを解除。
- Win32実機ビルドは未検証。
