# AI 开发与移植指南

## 项目范围

本分支为官方 Fcitx5 Android 的 Rime 插件加入 chinese-ime-lm 离线候选重排。原版 Android 主程序保持不变；新增功能编译在独立包名的增强版 Rime 插件中。

- 原创神经重排集成代码使用 MIT，许可证位于 `plugin/rime/neural/LICENSE`，不覆盖 `vendor/`。根目录 `LICENSE-MIT` 为方便 GitHub 识别的相同副本，维护时保持两份内容一致。Fcitx5 Android 原文件及其修改保留原 LGPL；保留其他依赖的授权和署名，不得将完整仓库或 APK 统一标为 MIT。授权范围见根目录 `NOTICE`。
- 新 Android/JVM 文件优先 Kotlin，现有 Java 文件保持原语言，避免无关重构。
- 使用简体中文说明实现、验证结果和限制。
- 先确定根因和违反的约束，再修改。不得用猜测偏移、吞异常、空值回退掩盖原生 ABI 问题。
- 构建与启用步骤见 [插件 README](plugin/rime/neural/README.md)，来源见 [UPSTREAM.md](plugin/rime/neural/UPSTREAM.md)。

## 当前验证组合

| 项目 | 固定版本 |
| --- | --- |
| 官方主程序 | `0.1.3-0-g048f581c`，versionCode `112`，Release，`arm64-v8a` |
| 主仓库基线 | `048f581c652367567b8ee5c28c5163b805288895` |
| Fcitx5 子模块 | `16465b04f675105da9fde14e7087984bdc07a146` |
| prebuilt 子模块 | `86ce2c95d42f1132746fbf60c278193aa1f4b758` |
| 预编译库中的 Rime | **1.16.1**，源码 `de4700e9f6b75b109910613df907965e3cbe0567` |
| Android Rime 补丁来源 | `fcitx5-android/prebuilder` 的 `eb156443de3b387089e51f9bd19df4e3ddce1732` |
| 推理实现 | vendored `chinese-ime-lm/reference`，完整提交见其 `UPSTREAM_COMMIT` |
| 权重 | 4M INT8 `sentence-model.safetensors`，SHA-256 见 `UPSTREAM.md` |

其他主程序版本和 ABI 尚未验证。不得把“编译成功”写成“兼容所有版本”。

**不要根据 App CMake 中的 `Rime_VERSION` 选择内部头文件。** 当前该变量仍写着 1.12.0，而实际预编译库为 1.16.1。最初按 1.12.0 编译内部适配层，会在创建滤镜并读取 Schema 配置时发生 SIGSEGV。

即使版本号一致，也必须匹配 Android 补丁。`prebuilder-librime.patch` 将 `switcher_` 移入 `Engine`，改变 `schema_` 等成员的偏移。使用未打补丁的上游头文件仍会出错。

## 关键文件

- `plugin/rime/build.gradle.kts`：`-PrimeNeural=true` 打开功能，使用独立插件包名。
- `plugin/rime/neural/CMakeLists.txt`：Rust 静态库、原生滤镜、模型安装、Rime 初始化钩子和预编译库校验。
- `plugin/rime/neural/neural-reranker.cpp`：候选筛选、上下文提取、重排和翻译流。
- `plugin/rime/neural/candidate-policy.h`：完整输入覆盖和词典来源判断。
- `plugin/rime/neural/src/lib.rs`：Rust C ABI 以及上游 `best_where` 调用。
- `plugin/rime/neural/build-rust.sh`：Rust 构建与产物复制。
- `plugin/rime/neural/tests/rime-filter-smoke.cpp`：Android 真机 Rime 流程回归测试及方案副本探测。
- `build-logic/convention/src/main/kotlin/DataDescriptorPlugin.kt`：资源清单生成。Windows 上文件和目录键都必须用 `/`。

## 移植到其他 Rime / prebuilt / 主程序版本

1. 确定目标官方 App 的完整版本、提交、ABI 和发布渠道。读取对应主仓库的 gitlink，使用它的 Fcitx5 与 prebuilt，避免混用不同版本头文件和 `.so`。
2. 查找该 prebuilt 的构建来源及对应 prebuilder 提交。读取其 `librime` gitlink、`patches/librime.patch` 和构建选项。运行时 `RimeApi::get_version()` 可核实 Rime 版本，但不能代替源码和补丁核对。
3. 从**精确提交并应用对应 Android 补丁**的 Rime 源码更新 vendored 头文件，保留 LICENSE，并更新 `UPSTREAM_COMMIT`、补丁副本和 `UPSTREAM.md`。同时匹配 Boost、glog、NDK/STL 配置。
4. 核对内部类布局及调用约定：`Engine`、`Schema`、`Context`、`Candidate`、`Translation`、`Filter`、`Ticket`、Registry 和 shared_ptr 相关类型。不得靠添加填充成员或手写字节偏移绕过崩溃。
5. 检查 fcitx5-rime 初始化流程。滤镜应在 Rime `initialize()` 后注册，并在每次重新初始化时重新注册；维护和创建会话之前必须可用。CMake 中的源码钩子若失配应明确失败，再根据新源码调整。
6. 审查候选来源、实际输入跨度、纠错标记和补全语义是否改变。不能从汉字长度推断是否覆盖完整输入，也不能把纠错匹配一概当作可信整词。
7. 当前 CMake 校验 arm64 `librime.a` 的 SHA-256。只有完成上面的版本审查后，才能更新校验值；禁止直接删除校验以通过构建。
8. 执行下文的原生测试，再在对应版本的原版主程序中验证插件识别、中文面板、候选选择、重新部署、重启及恢复原插件。把实际验证组合写入 Release。

## 移植到其他 Android ABI

当前构建脚本和 CMake **仅实现 arm64**。架构对应关系：

| Android ABI | Rust target | NDK Clang target（示例 API 23） |
| --- | --- | --- |
| arm64-v8a | aarch64-linux-android | aarch64-linux-android23 |
| armeabi-v7a | armv7-linux-androideabi | armv7a-linux-androideabi23 |
| x86 | i686-linux-android | i686-linux-android23 |
| x86_64 | x86_64-linux-android | x86_64-linux-android23 |

实施顺序：

1. 确认目标 ABI 在同一 prebuilt 中拥有完整依赖。读取其 Rime 版本、补丁及该 ABI 的库校验值。
2. 为 `build-rust.sh` 增加明确的 ABI → Rust target 映射，安装相应 Rust target；输出复制路径也必须随 target 改变。
3. 让 CMake 选择对应 Rust archive，并以 ABI 为键验证 Rime 库。当前 `NEURAL_RUST_ARCHIVE` 是 arm64 路径，不能只移除 ABI 限制。
4. 用 Gradle `BUILD_ABI` 选择目标 ABI，检查 APK 的 `lib/<abi>/` 和 ELF 架构，确认未混入 arm64 库。不要通过重命名 `.a` 或 `.so` 伪装架构。
5. 新架构先使用可移植实现。新增 SIMD 必须根据目标架构编译，并验证结果；不得全局使用构建电脑的 `-march=native`。
6. 在对应真机或模拟器执行原生流程测试，验证 32 位指针、`size_t` / Rust `usize`、候选文本、内存和延迟。模拟器结果不能代表手机速度。
7. 单独发布该 ABI 的产物，准确标注目标主程序和架构；未验证组合继续标记不支持。

## 候选处理约束

- 模型只重排已存在的候选。产生拼音纠错候选由 Rime 负责；`translator/enable_correction: true` 是可选方案设置。
- 只比较覆盖同一段完整原始输入的候选，保留前缀和预测补全的原有语义。
- 当前保守策略只在首选为生成句子时重排，保留整词首选。不能未经验证移除这个限制来“提高智能”。
- 重排实际 Candidate 对象，不能只交换显示文字；选词、输入跨度和学习必须一致。
- 同音整句扩展只复用同一音节 ID 路径上的真实 Phrase/DictEntry，保留原 Sentence 的 syllabifier、组件和输入边界；不得拼接显示文字来冒充可学习的候选。见 `neural/sentence-alternatives.h`。
- `neural/sentence-decoder.h` 通过现有 ScriptTranslator 复用词典，在选定的全拼路径上搜索不同分词。不得在 Filter 中再创建 Memory/ScriptTranslator，否则会重复订阅提交事件、重复学习。缓存的 DictEntry 必须绑定当前查询的 syllabifier，并在提交、取消和删词时失效。
- 激进组合只补充完整候选，保留原重排器选定的首选。4M 直接解码实测有退化，未经正常输入对照验证不得改成直接接管首选。跨排近键只扩展方案无法接受的拼写片段；已有完整拼写及用户派生规则优先。
- 相邻字母颠倒搜索和诊断集见 `plugin/rime/neural/CORRECTION.md`；诊断集不能当成总体准确率，模型权重目前未重新训练。
- `typing-corrector.h` 的两步键盘搜索必须保留正常拼写和至少一个未改字母。Rime 的原始图构建忽略纠错距离；`correction-distance.cpp` 在调用原实现后恢复每步代价。其链接符号与 CMake 中的 `--wrap`、固定 ABI/库校验绑定。移植时必须核对符号及 `neural-syllabifier-accessors.patch`，不得用允许重复符号或删除校验绕过链接问题。
- 原生/邻键/两处误按/较远键位/交换，以及重排/两种整句扩展，都有 `.custom.yaml` 开关，见插件 README。新增功能也必须允许用户明确写 `false` 关闭，并验证关闭后的行为。未指定别名的滤镜 Ticket 默认配置区为 `filter`，本集成将它映射为 `chinese_ime_lm`；不能直接沿用默认区而使文档中的开关失效。
- 前九项以后的候选必须继续可访问。缓冲候选不能重复、丢失或破坏去重；滤镜放在 `uniquifier` 前。
- 上下文仅来自当前组合中已选中的分段。不增加剪贴板、其他 App 文本或网络读取。
- 生产日志只记录加载状态、索引、数量、耗时，禁止记录用户输入和候选正文。

## 必须验证的项目

- 先确认报错日志中的 ABI，使用同一 ABI 验证修复；arm64 构建通过不能证明 x86 链接正常。
- 同一检出目录不要并行运行 ABI 选择不同的 Gradle 构建。Android Studio 与命令行共用按 variant 保存的 Prefab 发布元数据；需要同时验证不同 ABI 集合时使用独立检出目录，避免把动态库依赖覆盖为仅含头文件的目标。
- 上游 Rust 测试与真实权重的 `ffi-smoke`。
- `candidate-policy.cpp`：整键、前缀、补全、不同汉字长度和分段边界。
- Android `rime-neural-smoke`：真实 Rime 初始化和滤镜构造、句子提升、词典首选保护、超过九项的候选尾部、选词上屏。
- 真实方案在**独立副本**中测试。`--probe <user副本> <shared目录> <schema> <input>...` 会部署方案，只能传入副本目录。示例输入包括 `haoyourenma`、`wixihuanni`，并对照正确拼写。
- Windows 构建检查 `descriptor.json` 不含反斜杠，且每个文件的所有父目录都有目录记录。否则资源同步会误删刚复制的目录。
- 最终 APK 验签、检查 ABI，并用原版主程序验证。模型文件大小、模型增量内存和整个 App 的内存必须分开报告；测试个例不代表整体准确率。

## 手机与个人配置保护

- 安装和试用前备份原插件 APK、Rime 用户目录及主程序配置。保留原插件，通过禁用/启用切换。
- 改配置前**重新拉取当前文件**并保存备份；写回前比较手机 SHA-256，变化时停止覆盖并重新合并。
- 用户可能同时编辑配置。保留其已有选项、注释和停用状态，不得拿较早的测试副本覆盖新修改。
- “注释旧配置”不等于“注释整份文件”。范围不明确时询问，不自行批量注释。
- 用户表示自行管理配置后，停止修改该配置。继续代码和文档工作即可。
- 不删除原输入法、不清除 App 数据，不把个人配置、词库、设备序列号或本地密钥提交到 Git。
- UI 自动化每次输入前确认当前窗口属于测试界面，避免把测试文字输入其他应用。尽量使用不依赖 UI 的原生测试。

## 发布

使用 [RELEASE_NOTES.md](plugin/rime/neural/RELEASE_NOTES.md) 写明主程序完整版本、提交和 ABI。新版本移植完成后再更新兼容表，不沿用旧的“已验证”结论。签名使用发布者自己的密钥，密钥与口令不进入仓库。
