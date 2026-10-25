# Rime Chinese IME LM — Fcitx5 0.1.3 / 048f581c / arm64-v8a

## 兼容范围 / Compatibility

**此 APK 仅适配以下已验证组合：**

- 官方 Fcitx5 Android：`0.1.3-0-g048f581c`，versionCode `112`。
- 主程序提交：`048f581c652367567b8ee5c28c5163b805288895`。
- 架构：`arm64-v8a`。
- 内含 Rime：1.16.1，匹配该版本官方预编译库及 Android 补丁。

**其他 Fcitx5 Android 版本、Debug 主程序及其他 CPU 架构尚不支持，请勿混装。** Rime 内部 C++ 接口存在版本和补丁依赖，不能只看 Android 版本或“Rime 1.x”判断兼容。

**This APK targets official Fcitx5 Android `0.1.3-0-g048f581c` (versionCode 112), Release, arm64-v8a only. Other host versions, Debug builds and architectures are unsupported.**

## 功能

- 离线 4M INT8 中文模型，权重文件 4,486,280 字节。
- 对 Rime 已有的生成句候选进行重排，保留前缀、补全、选词上屏和后续候选。
- 原版 Fcitx5 Android 主程序无需修改。
- Rime 方案开启 `translator/enable_correction: true` 后，可向模型提供内置纠错产生的候选；它仍不能保证修正所有拼写错误。

## 安装与启用

1. 确认主程序完整版本和 ABI，先备份 Rime 用户目录。
2. 安装本 APK，停用官方 Rime 插件，保留其安装和数据。
3. 将模型滤镜合并到所用方案的 `.custom.yaml`，放在 `uniquifier` 前；需要拼写纠错时再启用内置纠错。保留已有配置和注释。
4. 重新部署 Rime，并重启 Fcitx 实例。

增强插件包名：`org.fcitx.fcitx5.android.plugin.rime.neural`。详细配置和恢复步骤见 [README](README.md)。

## 已验证与限制

- Android 真机原生测试通过：滤镜构造、候选重排、词典保护、候选尾部和选词上屏。
- 开启内置纠错的雾凇方案副本中，`haoyourenma`、`wixihuanni` 得到“还有人吗”“我喜欢你”首选。这是个例验证，不是整体准确率评测。
- 一次原生对照测试中，模型加载和推理后的额外 RSS 约 5.2 MiB；不包含输入法主程序和词库，具体占用随会话和设备变化。

## 授权与源码

新增集成代码：GNU GPL-3.0-or-later。上游 Fcitx5 Android 文件保留 LGPL 声明；推理参考实现和权重保留 Apache-2.0，Rime 头文件保留 BSD-3-Clause。

源码仓库：https://github.com/rD227/fcitx5-android-chinese-ime-lm
