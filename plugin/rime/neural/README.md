# Rime Chinese IME LM（实验版）

为 Fcitx5 Android 的 Rime 增加离线中文候选重排。沿用现有拼音方案、词库和候选对象；使用 chinese-ime-lm 的 4M 模型，在生成整句时比较前九个候选。它不会生成新的候选，也不处理 Sayboard 的语音识别。

- 只比较覆盖同一段完整输入的候选，允许候选中文字数不同。
- 保留词典整词首选，不改变补全、前缀或非 `abc` 分段。
- 使用当前组合中已选中的前文作为上下文，不读取剪贴板或其他应用的文字。
- 日志标签 `ChineseImeLm` 记录模型加载、重排索引和推理耗时，不记录输入文本。
- 暂支持 `arm64-v8a`，原版主程序无需修改。插件基于 `048f581c`，应搭配同版本主程序；升级原生库时需重新核对 Rime 头文件和 Fcitx ABI。

## 构建

需要上游 Android 构建环境（JDK 21、SDK 36、Build Tools 36.1.0、NDK、ECM 和 gettext），另需 Rust 的 `aarch64-linux-android` 目标。Windows 可在 WSL 中编译 Rust，之后使用 Windows NDK 链接。

初始化本插件需要的子模块：

```powershell
git submodule update --init --depth 1 lib/fcitx5/src/main/cpp/fcitx5 lib/fcitx5/src/main/cpp/prebuilt plugin/rime/src/main/cpp/fcitx5-rime plugin/rime/src/main/cpp/rime-prelude plugin/rime/src/main/cpp/rime-essay plugin/rime/src/main/cpp/rime-luna-pinyin plugin/rime/src/main/cpp/rime-stroke
pwsh -File plugin/rime/neural/download-model.ps1
```

在 Linux/WSL 中：

```sh
rustup target add aarch64-linux-android
sh plugin/rime/neural/build-rust.sh --test
cargo run --manifest-path plugin/rime/neural/Cargo.toml --release --example ffi-smoke -- plugin/rime/neural/model/sentence-model.safetensors
```

在项目根目录的 `local.properties` 配置 `sdk.dir`；使用 SDK 以外的 CMake 时另填 `cmake.dir`，指向包含 `bin/cmake.exe` 的目录。需要 CMake 3.24 以上；本次使用 4.3.1。PowerShell 示例：

```powershell
$env:NDK_VERSION = '28.2.13676358'
$env:CMAKE_VERSION = '4.3.1'
$env:BUILD_ABI = 'arm64-v8a'
# 设置 JAVA_HOME、ECM_DIR，并将 gettext/bin 加入 PATH。
.\gradlew.bat :plugin:rime:assembleRelease '-PrimeNeural=true' --max-workers=2
```

APK 位于 `plugin/rime/build/outputs/apk/release/`，需使用自己的密钥签名。独立包名为 `org.fcitx.fcitx5.android.plugin.rime.neural`。必须构建 Release：Debug 插件会寻找 Debug 版主程序。

## 启用与恢复

当前适配的官方主程序为 `0.1.3-0-g048f581c`（versionCode 112），Release，arm64-v8a。其他版本和架构需先移植验证，参见 [AGENT.md](../../../AGENT.md) 和 [Release 说明](RELEASE_NOTES.md)。

先备份 Rime 用户目录。安装新插件后，禁用原版 Rime 插件，避免两者提供同名原生插件和资源；保留原版安装及数据。

```sh
adb install <signed-apk>
adb shell pm disable-user --user 0 org.fcitx.fcitx5.android.plugin.rime
```

在使用的方案 `.custom.yaml` 的现有 `patch:` 下加入滤镜。以原版雾凇方案为例，最后一个滤镜是 `uniquifier`：

```yaml
patch:
  "engine/filters/@before last": chinese_ime_lm
  translator/enable_correction: true  # 可选：让 Rime 生成邻键误击的纠错候选
```

将这一项合并到已有 `patch:`，不要覆盖原有设置。自定义滤镜顺序时，把 `chinese_ime_lm` 放在 `uniquifier` 前、文字转换滤镜后。随后重新部署 Rime 并重启 Fcitx 实例。可用 `adb logcat -s ChineseImeLm` 检查模型加载和推理。

内置纠错负责产生替代拼写的候选，小模型负责比较可比候选；模型本身不直接将拼音改写成汉字。现有手写规则可与其共存，按使用者需要逐项调整，不要整体覆盖或注释配置。

## 验证记录

Rust 上游测试、真实权重 FFI 测试和完整输入覆盖测试通过。Android 原生 `rime-neural-smoke` 通过滤镜 ABI、生成句子重排、词典首选保护、九项之后候选保留及选词上屏测试。一次真机对照测得模型增量 RSS 约 5.2 MiB，这不等于整个输入法的内存占用。

该原生测试可通过 CMake 的显式目标 `rime-neural-smoke` 构建，产物不进入 APK。使用独立临时用户目录运行，不使用个人 Rime 用户目录。更多要求见仓库根目录 `AGENT.md`。

恢复原版：移除刚加入的方案补丁，重新部署，禁用新插件、启用原插件，再重启 Fcitx。

```sh
adb shell pm disable-user --user 0 org.fcitx.fcitx5.android.plugin.rime.neural
adb shell pm enable org.fcitx.fcitx5.android.plugin.rime
```

## 授权

本项目新增集成代码采用 GNU GPL-3.0-or-later，原 Fcitx5 Android 文件保留原 LGPL 声明。chinese-ime-lm 的训练工程为 GPLv3，但作者将 `reference/` 推理实现和发布的模型单独授权为 Apache-2.0；本项目使用这两部分并保留其 LICENSE、NOTICE。Rime 内部头文件为 BSD-3-Clause。来源、版本与模型校验值见 [UPSTREAM.md](UPSTREAM.md)。
