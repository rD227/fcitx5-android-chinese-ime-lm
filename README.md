# fcitx5-android

[如何配置](https://github.com/rD227/fcitx5-android-chinese-ime-lm/blob/8e198164781e7d98d3680afadb5dd8a06400d3a1/plugin/rime/neural/README.md?plain=1#L89)

或者见下文

**许可证：新增神经重排集成代码采用 [MIT](LICENSE-MIT)；Fcitx5 Android 原代码及其修改保留 LGPL，第三方推理实现和权重保留 Apache-2.0，Rime 头文件保留 BSD-3-Clause。完整仓库和 APK 包含多种授权，范围见 [NOTICE](NOTICE)。**

*此分支增加 Rime 离线中文候选重排插件，构建、启用和授权说明见 [Chinese IME LM](plugin/rime/neural/README.md)。下文保留上游项目介绍和下载链接；上游 APK 不包含本分支的新滤镜*

**AI 协作与移植前请阅读 [AGENT.md](AGENT.md)（[AGENTS.md](AGENTS.md) 为自动发现入口），包含其他 CPU 架构、Rime 版本及预编译库的适配规则。当前仅验证官方主程序 `0.1.3-0-g048f581c` 的 `arm64-v8a` Release；发布说明见 [兼容范围](plugin/rime/neural/RELEASE_NOTES.md)**

**覆盖安装可能遇到错误/同时启用旧的rime组件导致bug，请在ai不生效的时候阅读README**

# 如何配置

### 配置文件怎样写

以下入门说明由 agy 的 Gemini 3.8 Flash 起草，再按实际配置接口核对。

1. 确认当前使用的方案 ID。雾凇是 `rime_ice`，编辑前先备份对应文件：

   ```text
   /storage/emulated/0/Android/data/org.fcitx.fcitx5.android/files/data/rime/rime_ice.custom.yaml
   ```

   其他方案使用 `<方案ID>.custom.yaml`；修改用户目录中的文件，`build/` 内是会被重新生成的编译结果。

2. **已有文件：**在原来的 `patch:` 下合并需要的设置，缩进用两个空格，不用 Tab。根 `patch:` 只写一次；已有同名配置键时直接修改其值，不要重复添加。

3. **新建文件：**可从以下最小示例开始。滤镜已经加入过时，不要再次添加该滤镜行。

   ```yaml
   patch:
     "engine/filters/@before last": chinese_ime_lm
     translator/enable_correction: true
     chinese_ime_lm/enable_mixed_abbreviations: true
   ```

   这里开启原生拼写纠错和混合简拼候选。需要关闭“shuo 补充生活”时，将最后一行改成 `false`；需要关闭字母交换时，在同一个 `patch:` 下设置 `chinese_ime_lm/enable_transposition: false`。更多开关见下表。

4. 保存后重新部署 Rime，并重启 Fcitx 实例使配置生效。

使用 `speller/algebra/+` 追加规则时，基础方案中的规则仍保留；只注释自定义文件内的 `abbrev` 行，并不会删除基础方案已有的简拼。插件功能直接使用下表对应的开关控制。

### 完整开关示例

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
  chinese_ime_lm/enable_mixed_abbreviations: true  # 保留完整音节首选，补充混合简拼词
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
| `enable_mixed_abbreviations` | `true` | 如 `shuo` 补充 `s'huo → 生活`；只查真实双音节词，保留原首选 |
| `aggressive` | `true` | 重新组合词典分词，补充完整候选供选择 |

`enable_mixed_abbreviations` 独立于 `translator/enable_correction`、`aggressive` 和模型重排。它补回“简拼前缀 + 完整音节”的真实词典词，例如 `shuo → 生活`、`shui → 社会`、`fan → 方案`；“说/水/饭”等原首选保留。设为 `false` 后停止补充；显式输入 `s'huo` 仍由原方案解析。测试和限制见 [混合简拼诊断](CORRECTION.md#合法全拼遮住混合简拼shuo-与-shuo)。

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
  chinese_ime_lm/enable_mixed_abbreviations: false
  chinese_ime_lm/aggressive: false
```

以上开关不会移除你在 `speller/algebra` 中手写的 `derive` 等规则；这些规则仍由方案本身执行。自定义滤镜别名如 `chinese_ime_lm@my_lm` 时，滤镜的模型和组句设置写在 `my_lm/` 下；拼写纠错设置仍在 `chinese_ime_lm/` 下。

`n/m`、`y/u`、`i/o`、`j/k` 已在原生同排邻键表中；扩展搜索也覆盖 `j/n`、`s/x` 等跨排近键。`nri → nei` 是 `e` 误打为 `r`。额外搜索只扩展原方案不能接受的片段；两步搜索至少保留一个原字母，且比一步纠错受到更大惩罚。两步不是任意替换两个字母，详见 [纠错实现与实测](CORRECTION.md)。

`haijeyi` 在不含个人手写纠错规则的方案副本中已能首选“还可以”。`suhang` 同时存在合法的 `su'hang` 和交换后的 `shuang` 路径：交换检查会执行，但不能保证“爽/双”排在“苏杭”等解释之前。显式的 `'` 分隔符不会被跨越。

`aggressive` 控制完整组合候选生成，不改变原有首选决定。它复用系统词典和用户词典，保留 4 条中间路径、最多 32 条完整组合，并缓存同一组合中的前缀；提交、取消和删词会清空缓存。老设备上会增加按键耗时，可设为 `false` 关闭。`wozaidadianzi` 已能生成“我再打点字”，但目前仍排得较后，不能保证首选正确。

增强插件的“插件描述”中提供本仓库地址与配置入口，支持中文和英文，点按描述可复制。此次资源已通过 Android 资源编译；完整 APK 中的显示待打包安装复核。


[Fcitx5](https://github.com/fcitx/fcitx5) input method framework and engines ported to Android.

## Download

这里后面的README都没有修改....

[<img src="https://github.com/rubenpgrady/get-it-on-github/raw/refs/heads/main/get-it-on-github.png" alt="Git it on GitHub" width="207" height="80">](https://github.com/fcitx5-android/fcitx5-android/releases/latest)
[<img src="https://fdroid.gitlab.io/artwork/badge/get-it-on.png" alt="Get it on F-Droid" width="207" height="80">](https://f-droid.org/packages/org.fcitx.fcitx5.android)
[<img alt="Get it on Google Play" src="https://play.google.com/intl/en_us/badges/static/images/badges/en_badge_web_generic.png" width="207" height="80">](https://play.google.com/store/apps/details?id=org.fcitx.fcitx5.android)

You can also download the **latest CI build** on our Jeninks server: [![build status](https://img.shields.io/jenkins/build.svg?jobUrl=https://jenkins.fcitx-im.org/job/android/job/fcitx5-android/)](https://jenkins.fcitx-im.org/job/android/job/fcitx5-android/)

> [!NOTE]
> APKs downloaded from GitHub Release/F-Droid/Jenkins have the same signature, which means they're compatible when upgrading, but Google Play's do not.
> <details>
> <summary>(click here for detailed signature info)</summary>
> <ul>
> <li>Package Name: <code>org.fcitx.fcitx5.android</code></li>
> <li>Certificate SHA-256 fingerprint:</li>
> <ul>
> <li>GitHub Release/Jenkins/F-Droid</li>
> <code>E4:DB:1E:9E:DF:F1:36:29:D0:7D:E4:BB:F8:16:5F:E9:BD:85:57:AB:55:09:26:72:DA:8E:40:DB:E4:84:EC:D7</code>
> <li>Google Play</li>
> <code>06:53:6F:F6:E8:76:C0:14:E1:4B:44:6F:61:FA:2B:80:9E:06:67:39:A1:D1:17:0D:0A:7A:89:88:4C:48:00:33</code>
> </ul>
> </ul>
> </details>

In case you want Fcitx5 on other platforms: [macOS](https://github.com/fcitx-contrib/fcitx5-macos), [iOS](https://github.com/fcitx-contrib/fcitx5-ios), [HarmonyOS](https://github.com/fcitx-contrib/fcitx5-harmony), [ChromeOS](https://github.com/fcitx-contrib/fcitx5-chrome), [Windows](https://github.com/fcitx-contrib/fcitx5-windows); or [try Fcitx5 in the browser](https://fcitx-contrib.github.io/online/index.html)

## Project status

### Supported Languages

- English (with spell check)
- Chinese
  - Pinyin, Shuangpin, Wubi, Cangjie and custom tables (built-in, powered by [fcitx5-chinese-addons](https://github.com/fcitx/fcitx5-chinese-addons))
  - Zhuyin/Bopomofo (via [Chewing Plugin](./plugin/chewing))
  - Jyutping (via [Jyutping Plugin](./plugin/jyutping/), powered by [libime-jyutping](https://github.com/fcitx/libime-jyutping))
- Vietnamese (via [UniKey Plugin](./plugin/unikey), supports Telex, VNI and VIQR)
- Japanese (via [Anthy Plugin](./plugin/anthy))
- Korean (via [Hangul Plugin](./plugin/hangul))
- Sinhala (via [Sayura Plugin](./plugin/sayura))
- Thai (via [Thai Plugin](./plugin/thai))
- Generic (via [RIME Plugin](./plugin/rime), supports importing custom schemas)

### Implemented Features

- Virtual Keyboard (layout not customizable yet)
- Expandable candidate view
- Clipboard management (plain text only)
- Theming (custom color scheme, background image and dynamic color aka monet color after Android 12)
- Popup preview on key press
- Long press popup keyboard for convenient symbol input
- Symbol and Emoji picker
- Plugin System for loading addons from other installed apk
- Floating candidates panel when using physical keyboard

### Planned Features

- Customizable keyboard layout
- More input methods (via plugin)

## Screenshots

|拼音, Material Light theme, key border enabled|自然码双拼, Pixel Dark theme, key border disabled|
|:-:|:-:|
|<img src="https://github.com/fcitx5-android/fcitx5-android/assets/13914967/bd429247-62d9-4c78-bab8-70ef3ce47588" width="360px">|<img src="https://github.com/fcitx5-android/fcitx5-android/assets/13914967/3ae969c1-7ed0-4f92-a5df-19dc8c90a8c3" width="360px">|

|Emoji picker, Pixel Light theme, key border enabled|Symbol picker, Material Dark theme, key border disabled|
|:-:|:-:|
|<img src="https://user-images.githubusercontent.com/13914967/202181845-6a5f6bb2-a877-468c-851a-fd7e66e64ed4.png" width="360px">|<img src="https://user-images.githubusercontent.com/13914967/202181861-dd253439-1d5e-4f5f-9535-934f28796a6b.png" width="360px">|

## Get involved

Trello kanban: https://trello.com/b/gftk6ZdV/kanban

Matrix Room: https://matrix.to/#/#fcitx5-android:mozilla.org

Discuss on Telegram: [@fcitx5_android_group](https://t.me/fcitx5_android_group) ([@fcitx5_android](https://t.me/fcitx5_android) originally)

## Build

### Dependencies

- Android SDK Platform & Build-Tools 35.
- Android NDK (Side by side) 25 & CMake 3.22.1, they can be installed using SDK Manager in Android Studio or `sdkmanager` command line.
- [KDE/extra-cmake-modules](https://github.com/KDE/extra-cmake-modules)
- GNU Gettext >= 0.20 (for `msgfmt` binary; or install `appstream` if you really have to use gettext <= 0.19.)

### How to set up development environment

<details>
<summary>Prerequisites for Windows</summary>

- Enable [Developer Mode](https://learn.microsoft.com/en-us/windows/apps/get-started/enable-your-device-for-development) so that symlinks can be created without administrator privilege.

- Enable symlink support for `git`:

    ```shell
    git config --global core.symlinks true
    ```

</details>

First, clone this repository and fetch all submodules:

```shell
git clone git@github.com:fcitx5-android/fcitx5-android.git
git submodule update --init --recursive
```

Install `extra-cmake-modules` and `gettext` with your system package manager:

```shell
# For Arch Linux (Arch has gettext in it's base meta package)
sudo pacman -S extra-cmake-modules

# For Debian/Ubuntu
sudo apt install extra-cmake-modules gettext

# For macOS
brew install extra-cmake-modules gettext

# For Windows, install MSYS2 and execute in its shell (UCRT64)
pacman -S mingw-w64-ucrt-x86_64-extra-cmake-modules mingw-w64-ucrt-x86_64-gettext
# then add C:\msys64\ucrt64\bin to PATH
```

Install Android SDK Platform, Android SDK Build-Tools, Android NDK and cmake via SDK Manager in Android Studio:

<details>
<summary>Detailed steps (screenshots)</summary>

**Note:** These screenshots are for references and the versions in them may be out of date.
The current recommended versions are recorded in [Versions.kt](build-logic/convention/src/main/kotlin/Versions.kt) file.

![Open SDK Manager](https://user-images.githubusercontent.com/13914967/202184493-3ee1546b-0a83-4cc9-9e41-d20b0904a0cf.png)

![Install SDK Platform](https://user-images.githubusercontent.com/13914967/202184534-340a9e7c-7c42-49bd-9cf5-1ec9dcafcf32.png)

![Install SDK Build-Tools](https://user-images.githubusercontent.com/13914967/202185945-0c7a9f39-1fcc-4018-9c81-b3d2bf1c2d3f.png)

![Install NDK](https://user-images.githubusercontent.com/13914967/202185601-0cf877ea-e148-4b88-bd2f-70533189b3d4.png)

![Install CMake](https://user-images.githubusercontent.com/13914967/202184655-3c1ab47c-432f-4bd7-a508-92096482de50.png)

</details>

### Trouble-shooting

- Android Studio indexing takes forever to complete and cosumes a lot of memory.

    Switch to "Project" view in the "Project" tool window (namely the file tree side bar), right click `lib/fcitx5/src/main/cpp/prebuilt` directory, then select "Mark Directory as > Excluded". You may also need to restart the IDE to interrupt ongoing indexing process.

- Gradle error: "No variants found for ':app'. Check build files to ensure at least one variant exists." or "[CXX1210] <whatever>/CMakeLists.txt debug|arm64-v8a : No compatible library found"

    Examine if there are environment variables set such as `_JAVA_OPTIONS` or `JAVA_TOOL_OPTIONS`. You might want to clear them (maybe in the startup script `studio.sh` of Android Studio), as some gradle plugin treats anything in stderr as errors and aborts.

## Nix

Appropriate Android SDK with NDK is available in the development shell.  The `gradlew` should work out-of-the-box, so you can install the app to your phone with `./gradlew installDebug` after applying the patch mentioned above. For development, you may want to install the unstable version of Android Studio, and point the project SDK path to `$ANDROID_SDK_ROOT` defined in the shell. Notice that Android Studio may generate wrong `local.properties` which sets the SDK location to `~/Android/SDK` (installed by SDK Manager). In such case, you need specify `sdk.dir` as the project SDK in that file manually, in case Android Studio sticks to the wrong global SDK.
