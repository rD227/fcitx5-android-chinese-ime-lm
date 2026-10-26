# Rime Chinese IME LM 0.1.4 — host Fcitx5 0.1.3 / 048f581c / arm64-v8a

插件基础版本名为 `0.1.4`，基础版本码从 11 升至 12；按项目 ABI 编码规则，arm64 APK 的 versionCode 从 `112` 升至 `122`。Git 提交后缀和构建环境的显式覆盖仍保留。下文的 `0.1.3 / 112` 指兼容的官方主程序，不是本插件的新版本号。

本次增加跨排邻键、同音节两处误按、两步键位纠错及完整组合候选。修复滤镜默认配置区的读取，使各项功能可在方案 custom.yaml 中独立关闭；开关表见 [README](README.md#每项功能都可独立关闭)。交换搜索合并重复的前缀查找。保留原首选排序；“我再打点字”已进入完整候选，但尚不能稳定排到首位。模型仍为原 4M INT8 权重，未重新训练。

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
- 新增混合简拼词典候选：`shuo` 保留“说”首选，并补充“生活”等真实词条；同类 `shui → 社会`、`fan → 方案` 等 23 项诊断进入前 16 项。可用 `chinese_ime_lm/enable_mixed_abbreviations: false` 独立关闭；不依赖模型或拼写纠错总开关。

## 安装与启用

1. 确认主程序完整版本和 ABI，先备份 Rime 用户目录。
2. 安装本 APK，停用官方 Rime 插件，保留其安装和数据。
3. 将模型滤镜合并到所用方案的 `.custom.yaml`，放在 `uniquifier` 前；需要拼写纠错时再启用内置纠错。保留已有配置和注释。
4. 重新部署 Rime，并重启 Fcitx 实例。

增强插件包名：`org.fcitx.fcitx5.android.plugin.rime.neural`。详细配置和恢复步骤见 [README](README.md)。

## 已验证与限制

- Android 真机原生测试通过：滤镜构造、候选重排、词典保护、候选尾部和选词上屏。
- 开启内置纠错的雾凇方案副本中，`haoyourenma`、`wixihuanni` 得到“还有人吗”“我喜欢你”首选。这是个例验证，不是整体准确率评测。
- 在不含个人手写纠错规则的独立方案副本中，51 组键盘误按音节目标全部可达；`haijeyi` 首选“还可以”。另验证交换路径、原生/邻键/多误按/较远键位/交换/关闭共 6 组配置，24 项开关断言通过。生成了候选不等于一定首选，见 [纠错诊断](CORRECTION.md)。
- 0.1.4 此次验证使用独立 Android 原生程序；插件说明资源通过 Android 资源编译。完整 APK 的打包、安装及界面显示需用实际产物复核。
- 一次原生对照测试中，模型加载和推理后的额外 RSS 约 5.2 MiB；不包含输入法主程序和词库，具体占用随会话和设备变化。

## 授权与源码

原创神经重排集成代码采用 [MIT](LICENSE)。上游 Fcitx5 Android 文件及其修改保留 LGPL 声明；推理参考实现和权重保留 Apache-2.0，Rime 头文件保留 BSD-3-Clause，其他组件和词库保留各自授权。完整 APK 包含多种授权，范围见根目录 [NOTICE](../../../NOTICE)。

源码仓库：https://github.com/rD227/fcitx5-android-chinese-ime-lm
