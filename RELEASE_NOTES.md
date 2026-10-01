# AegisubQT 4.0.2

本次更新改进字幕编辑、Automation、音视频处理，并重新整理了 Windows、Linux 和 macOS 的运行时打包。

## 主要更新

- 修复 ASS/SSA 字幕读取与写回、样式和附件处理、查找替换、编码选择、撤销重做等问题；改进 SRT 导入导出和行时间处理。
- 完善 Automation/Lua 脚本接口、正则表达式、导出过滤器、文件操作、帧率转换和脚本加载，并增加相应回归测试。
- 更新 AstraCore 视频后端，修复重复取帧、回退查找、视频结束时的解码刷新等问题，改进异步解码、视频重开和时间码处理。
- Windows 包现在包含固定版本的完整 AstraCore 媒体运行时及依赖。macOS DMG 包含完整 FFmpeg 媒体运行时；Linux 发布流程部署原生媒体后端。
- 修复 macOS DMG 中 Qt Quick 模块缺失导致应用启动后退出的问题，并在最终 DMG 上验证真实主界面启动和视频解码。
- 固定关键构建依赖和上游版本，在打包检查中验证运行时文件、库依赖、FFmpeg 功能及真实视频会话。

## 平台包

GitHub Release 提供 Windows 安装版和便携版、Linux x86_64 AppImage 与 tar.xz，以及 macOS Apple 芯片 DMG。

## macOS 首次启动

此 macOS 版本未使用 Apple Developer ID 签名和公证。首次打开时，macOS 可能阻止启动；如果你确认下载来源可信，可按系统提示在“系统设置 → 隐私与安全性”中选择“仍要打开”。
