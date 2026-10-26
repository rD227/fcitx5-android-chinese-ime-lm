# Rime Chinese IME LM（实验版）

为 Fcitx5 Android 的 Rime 增加离线中文候选重排。沿用现有拼音方案和词库，使用 chinese-ime-lm 的 4M 模型比较最多九个整句候选。对于自动组句，可复用同一拼音路径上的词条补充同音整句候选；不处理 Sayboard 的语音识别。

- 只比较覆盖同一段完整输入的候选，允许候选中文字数不同。
- 保留词典整词首选，不改变补全、前缀或非 `abc` 分段。
- 可选的通用相邻字母颠倒纠错见 [纠错方法和实测限制](CORRECTION.md)。当前权重未重新训练。
- `0.1.4` 增加跨排邻键、每音节两步键盘纠错，以及沿真实拼音路径组合完整候选的束搜索；现有首选排序保留，新组合放在其后供选择。
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

上面的初始化命令仅准备独立 Rime 插件。若在 Android Studio 中构建 `app` 主程序，或构建全部模块，还需初始化其余子模块及嵌套依赖：

```powershell
git submodule update --init --recursive --depth 1
```

未初始化时，`fcitx5-lua`、`libime` 等目录为空，会报 `does not contain a CMakeLists.txt file`；`kenlm` 目标不存在是 libime 未加载后的连带错误。

在 Linux/WSL 中：

```sh
rustup target add aarch64-linux-android
sh plugin/rime/neural/build-rust.sh --test
cargo run --manifest-path plugin/rime/neural/Cargo.toml --release --example ffi-smoke -- plugin/rime/neural/model/sentence-model.safetensors
```

在项目根目录的 `local.properties` 配置 `sdk.dir`；使用 SDK 以外的 CMake 时另填 `cmake.dir`，指向包含 `bin/cmake.exe` 的目录。需要 CMake 3.24 以上；本次使用 4.3.1。

Android Studio 同步时，可在该文件保存本机已安装的工具链版本。本项目构建逻辑支持以下两个版本键，避免依赖单独命令行窗口里的环境变量；`local.properties` 不提交到 Git：

```properties
ndkVersion=28.2.13676358
cmakeVersion=4.3.1
```

优先级为环境变量、Gradle 项目属性、本机 `local.properties`、仓库默认版本。`cmake.dir` 必须与指定的 CMake 版本对应。

如果 Android Studio 找不到 Gettext 或 ECM，可在同一个 `local.properties` 中显式指定路径（改为自己的安装位置）：

```properties
gettext.msgmerge=C:/Tools/gettext/bin/msgmerge.exe
gettext.msgfmt=C:/Tools/gettext/bin/msgfmt.exe
ecm.dir=C:/Tools/ecm/share/ECM/cmake
```

前两项指向本机构建工具的可执行文件，`ecm.dir` 指向包含 `ECMConfig.cmake` 的目录；构建逻辑会把这些值传给 CMake。未配置时沿用 CMake 的环境变量和 PATH 查找。Android Studio 进程不会自动继承启动后新增的 Windows 环境变量；日志已成功识别 NDK Clang 时，不需要额外安装或添加主机 Clang。

Android Studio 的 Gradle JDK 可选择 `GRADLE_LOCAL_JAVA_HOME`，在本机 `.gradle/config.properties` 中用 `java.home` 指向完整安装的 JDK 21。IDE 的 JDK 与终端的 `JAVA_HOME` 分别配置；检查所选目录中确实存在 `bin/java.exe` 和 `bin/javac.exe`。

命令行也可继续使用：

```powershell
$env:NDK_VERSION = '28.2.13676358'
$env:CMAKE_VERSION = '4.3.1'
$env:BUILD_ABI = 'arm64-v8a'
# 设置 JAVA_HOME；若未在 local.properties 中配置工具路径，设置 ECM_DIR 并将 gettext/bin 加入 PATH。
.\gradlew.bat :plugin:rime:assembleRelease '-PrimeNeural=true' --max-workers=2
```

同一工作目录不要同时运行 ABI 选择不同的 Gradle 构建。命令行的 `BUILD_ABI` 与 Android Studio 的架构选择应保持一致：各次构建共用按 variant 保存的 Prefab 发布元数据，混用时可能把原生依赖生成为只有头文件的目标，导致链接缺少 `libFcitx5Core.so` 等库。发生后应结束并行构建，再用所需的完整 ABI 集合重新执行 Gradle 构建。

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
  chinese_ime_lm/enable_native_correction: true  # Rime 自带的同排邻键搜索
  chinese_ime_lm/enable_transposition: true  # 本增强版插件：全拼相邻字母颠倒
  chinese_ime_lm/enable_keyboard_neighbors: true  # 通用 QWERTY 邻键，包含跨排近键
  chinese_ime_lm/enable_multiple_typos: true  # 同一音节两处邻键误按，例如 mro → mei
  chinese_ime_lm/enable_distant_neighbors: true  # 一处跨两步键位，例如 vi → xi
  chinese_ime_lm/enable_reranking: true  # 4M 模型比较已有完整候选
  chinese_ime_lm/enable_sentence_alternatives: true  # 同音前缀词条扩展
  chinese_ime_lm/aggressive: true  # 更积极生成完整组合候选，默认开启
```

将这一项合并到已有 `patch:`，不要覆盖原有设置。自定义滤镜顺序时，把 `chinese_ime_lm` 放在 `uniquifier` 前、文字转换滤镜后。随后重新部署 Rime 并重启 Fcitx 实例。可用 `adb logcat -s ChineseImeLm` 检查模型加载和推理。

内置纠错负责产生替代拼写，小模型比较汉字候选；模型本身不直接改写拼音。新增设置需要本增强插件，原版插件不实现它们。当前不支持通用插入、删除或任意多字母换位。现有手写规则可共存，不要整体覆盖或注释配置。

### 每项功能都可独立关闭

在**当前方案**的 `.custom.yaml` 中，将对应项明确写为 `false`，然后重新部署方案并重启 Fcitx 实例。以雾凇为例，文件为 `rime_ice.custom.yaml`；应合并到同一个已有的 `patch:` 中。

下表省略共同前缀 `chinese_ime_lm/`：

| 设置 | 未指定时 | 作用 |
| --- | --- | --- |
| `enable_native_correction` | `true` | 保留 Rime 自带的同排邻键搜索 |
| `enable_transposition` | `false` | 一次相邻字母交换，如 `suhang → shuang` |
| `enable_keyboard_neighbors` | 跟随 `enable_transposition` | 按 QWERTY 位置搜索同排和跨排邻键 |
| `enable_multiple_typos` | `true` | 邻键搜索内允许两处各一步误按，如 `cue → xie`、`shwbg → sheng` |
| `enable_distant_neighbors` | `true` | 邻键搜索内允许一处距离为两步的误按，如 `vi → xi` |
| `enable_reranking` | `true` | 模型重排原有完整候选 |
| `enable_sentence_alternatives` | `true` | 用同音前缀词条补充完整句子，参与原有重排 |
| `aggressive` | `true` | 重新组合词典分词，补充完整候选供选择 |

`enable_multiple_typos` 和 `enable_distant_neighbors` 只控制 `enable_keyboard_neighbors` 的扩展搜索。原生同排纠错是另一条搜索路径；要单独试用新增类型，可同时设置 `enable_native_correction: false`。`enable_keyboard_neighbors` 和 `enable_transposition` 都关、原生纠错仍开时，沿用原版 Rime 搜索行为。

**关闭所有算法拼写纠错：**

```yaml
patch:
  translator/enable_correction: false
```

**同时关闭模型重排和两种整句扩展：**

```yaml
patch:
  translator/enable_correction: false
  chinese_ime_lm/enable_reranking: false
  chinese_ime_lm/enable_sentence_alternatives: false
  chinese_ime_lm/aggressive: false
```

以上开关不会移除你在 `speller/algebra` 中手写的 `derive` 等规则；这些规则仍由方案本身执行。自定义滤镜别名如 `chinese_ime_lm@my_lm` 时，滤镜的模型和组句设置写在 `my_lm/` 下；拼写纠错设置仍在 `chinese_ime_lm/` 下。

`n/m`、`y/u`、`i/o`、`j/k` 已在原生同排邻键表中；扩展搜索也覆盖 `j/n`、`s/x` 等跨排近键。`nri → nei` 是 `e` 误打为 `r`。额外搜索只扩展原方案不能接受的片段；两步搜索至少保留一个原字母，且比一步纠错受到更大惩罚。两步不是任意替换两个字母，详见 [纠错实现与实测](CORRECTION.md)。

`haijeyi` 在不含个人手写纠错规则的方案副本中已能首选“还可以”。`suhang` 同时存在合法的 `su'hang` 和交换后的 `shuang` 路径：交换检查会执行，但不能保证“爽/双”排在“苏杭”等解释之前。显式的 `'` 分隔符不会被跨越。

`aggressive` 控制完整组合候选生成，不改变原有首选决定。它复用系统词典和用户词典，保留 4 条中间路径、最多 32 条完整组合，并缓存同一组合中的前缀；提交、取消和删词会清空缓存。老设备上会增加按键耗时，可设为 `false` 关闭。`wozaidadianzi` 已能生成“我再打点字”，但目前仍排得较后，不能保证首选正确。

增强插件的“插件描述”中提供本仓库地址与配置入口，支持中文和英文，点按描述可复制。此次资源已通过 Android 资源编译；完整 APK 中的显示待打包安装复核。

## 验证记录

Rust 上游测试、真实权重 FFI 测试和完整输入覆盖测试通过。Android 原生 `rime-neural-smoke` 通过滤镜 ABI、生成句子重排、词典首选保护、九项之后候选保留及选词上屏测试。一次真机对照测得模型增量 RSS 约 5.2 MiB，这不等于整个输入法的内存占用。

该原生测试可通过 CMake 的显式目标 `rime-neural-smoke` 构建，产物不进入 APK。使用独立临时用户目录运行，不使用个人 Rime 用户目录。更多要求见仓库根目录 `AGENT.md`。

恢复原版：移除刚加入的方案补丁，重新部署，禁用新插件、启用原插件，再重启 Fcitx。

```sh
adb shell pm disable-user --user 0 org.fcitx.fcitx5.android.plugin.rime.neural
adb shell pm enable org.fcitx.fcitx5.android.plugin.rime
```

## 授权

原创神经重排集成代码采用 [MIT](LICENSE)，包括本目录内新增的适配层、Rust C ABI、构建脚本、测试和文档，不覆盖 `vendor/`。Fcitx5 Android 原文件及其修改保留原 LGPL 声明，根目录 `LICENSE` 保留上游 LGPL 文本。

chinese-ime-lm 的训练工程为 GPLv3，但作者将本项目使用的 `reference/` 推理实现和发布的模型单独授权为 Apache-2.0；其 LICENSE、NOTICE 和模型内的语料署名保持不变。Rime 头文件保留 BSD-3-Clause，Android prebuilder 补丁保留原始来源。其他上游组件和词库继续适用各自的许可证。

完整仓库和 APK 包含多种授权，不能统一声明为 MIT。范围见根目录 [NOTICE](../../../NOTICE)，来源、版本与模型校验值见 [UPSTREAM.md](UPSTREAM.md)。构建时将本集成的 MIT LICENSE、总 NOTICE、LGPL 文本和 Rime BSD 文本一并放入 APK 资源。
