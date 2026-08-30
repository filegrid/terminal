# Workspace Chat Submit 探索方向整理

## 背景

当前问题不是“文本有没有发出去”，而是 **workspace chat 转发到 Copilot/Claude 这类 raw-mode / bracketed-paste CLI 时，目标进程是否把它当成了真实 TTY 前台输入**。

现阶段已经确认：

1. 文本和控制序列可以被送到终端链路。
2. 多种 `Enter` 变体都试过了。
3. 目标 CLI 仍然可能把这些路径都视为“后端注入”，而不是“真实前台键盘输入”。

所以后续探索必须围绕这条主线展开：

> **到底是哪一层把输入变成了“非真实 TTY 交互”，以及是否存在真正可用的前台输入路径。**

## 当前结论

- **已排除**：单纯“没发送到终端”的问题。
- **已排除**：只靠 `BracketedPasteEnabled` 分支就能解决的问题。
- **已排除**：只改 `\r` / `\n` / `\r\n` 组合就能解决的问题。
- **已排除**：仅仅是 workspace chat 输入框自己还持有焦点的问题。
- **已排除**：仅仅是主窗体没回到前台、或 `TermControl` 没拿到 XAML focus 的问题。
- **当前最强假设**：`PasteText` / `SendInput(string)` / `RawWriteChar` / 终端 core/backend 注入类路径，本质上都不等价于 CLI 期待的真实前台 TTY 输入。

### 2026-07-10 新增确认

最新几轮针对多 Copilot tab 的日志对比，又确认了几件重要事实：

1. `windowActivationSucceeded = true`、`controlFocusRequested = true`、`tryFocusAsyncSucceeded = true` 时，后续 tab 仍然可能完全失败。
2. 失败样本里，`focusedIsTermControlReadyToSend = true` 且 `focusedIsWorkspaceChatInputReadyToSend = false`，说明 workspace chat 输入框已经让开焦点。
3. 多个 tab 在发送瞬间命中的 Win32 焦点类名和句柄值一致，都是同一个 `Windows.UI.Input.InputSite.WindowClass`，并不存在“第 4 个 tab 命中了不同 tab 专属 hwnd”这类证据。
4. 终端事件日志显示：失败 tab 也确实收到了 input 和 output，但 output 只是 Copilot 首页 redraw，没有出现真正的 `❯ hi` / `Working` 等进入 prompt 处理态的特征。

因此，这条“共享前台输入口 + 键盘模拟”的方案目前更像是：

> **即使把 Win32/XAML 焦点都做对了，也不能稳定把一次输入绑定成某个 Copilot tab 自己的 prompt 提交。**

## 已探索方向总表

| 方向 | 目标 | 是否探索过 | 结果/结论 |
| --- | --- | --- | --- |
| `PasteText(body)` + 单独提交 | 让 body 走 bracketed paste，再补一个执行 Enter | 是 | 文本能到，提交不稳定/无效，Copilot 类 CLI 仍可能吞掉 submit |
| `PasteText(text + "\r")` | 把提交也塞进 paste 体内 | 是 | 不符合目标 CLI 预期；用户已明确这条方向不对 |
| `SendInput(text + "\r")` | 单次字符串注入完成输入和提交 | 是 | 普通 tab 可工作，Copilot/Claude 类场景不可靠 |
| `SendInput(text + "\r\n")` | 试 `CRLF` 替代 `CR` | 是 | 无法解决“没有真实 Enter 效果” |
| `SendInput(ESC[200~ + body + ESC[201~ + "\r")` | 模拟 bracketed paste，并在同一 payload 后接提交 | 是 | 日志确认 payload 发出，但目标 CLI 仍不认 submit |
| `SendInput(ESC[200~ + body + ESC[201~ + "\r\n")` | 同上，改为 `CRLF` | 是 | 日志确认 payload 发出，问题仍存在 |
| `RawWriteChar('\r')` | 直接写提交字符到终端连接 | 是 | 仍偏后端注入语义，没有解决问题 |
| `OnDirectKeyEvent(VK_RETURN)` | 走控件级直接按键路径 | 是 | 未稳定形成目标 CLI 可接受的 submit |
| `RawWriteKeyEvent(VK_RETURN, ...)` | 更贴近按键事件而不是字符串 | 是 | 仍未证明对目标 CLI 等价于真实前台输入 |
| OS 级 `SendInput` Enter | 用系统输入模拟真实键盘 Enter | 是 | 仍未稳定命中目标 CLI 所需路径 |
| `SendInput` scan-code only Enter | 让 OS 注入更像物理键盘 | 是 | 仍未解决 |
| `WM_KEYDOWN/WM_CHAR/WM_KEYUP` | 直接往焦点 hwnd 投递消息 | 是 | 仍未解决 |
| `PostMessage` 异步投递 Enter | 让消息走正常队列再翻译 | 是 | 仍未解决 |
| focus 到 terminal 再发 Enter | 避免消息打到 workspace input 自己 | 是 | 解决了“目标控件可能不对”的怀疑，但没解决 submit 本身 |
| 锁定 TSF / focus hwnd / owning hwnd | 找最接近真实输入的窗口目标 | 是 | 做过多轮尝试，仍未解决 |
| 发送前释放 Ctrl/Shift/Alt/Win | 避免 `Ctrl+Enter` 残留修饰键污染后续输入 | 是 | 解决了误触发 `Ctrl+,` 打开设置的问题，但未解决多 tab 提交不稳定 |
| 文本后短延时再发 Enter | 避免文本和 Enter 同一拍被 Copilot 前端吞掉 | 是 | 无决定性效果 |
| 发送前显式激活 owning/hosting window | 验证“聊天框让开焦点 != 主窗体回到前台” | 是 | 日志确认可成功激活，但提交仍不稳定 |
| 记录 `TermControl.Focus` / `TryFocusAsync` 成功位 | 区分“请求了焦点”和“实际拿到焦点” | 是 | 日志确认 XAML focus 可以成功，但提交仍不稳定 |
| 记录 Win32 `hwndActive` / `hwndFocus` / `hwndCaret` | 直接确认真实前台输入 sink | 是 | 多个 tab 发送时命中的是同一个 `InputSite` 焦点窗口，未能证明存在 tab 专属输入 hwnd |
| 延时后再发 Enter | 避免 paste 和 submit 紧贴导致被吞 | 是 | 无决定性效果 |
| 重复多次发 Enter | 粗暴提高提交成功率 | 是 | 日志证明发了，但仍无效，不是次数问题 |
| 仅靠 `BracketedPasteEnabled` 选分支 | 用终端状态判断 interactive CLI | 是 | 证明不够，跨 tab/new tab 会误判 |
| 加入每 tab 的 CLI 检测与 latch | 保持 Copilot/Claude tab 走特殊路径 | 是 | 修复了分支漂移/回退问题，但没触及根因 |
| 查 workspace diagnostics 日志 | 确认代码命中了哪条发送分支 | 是 | 已证明多条 payload 确实发出 |
| 查 terminal event 日志 | 确认终端视角实际看到了什么 | 是 | 已证明发送链路通，但 submit 语义仍不成立 |

## 已探索方向的归类结论

### 1. 纯 payload 调整

这类方向基本已经覆盖过：

- `CR`
- `CRLF`
- paste body 和 submit 分离
- paste body 和 submit 合并
- 手动拼 bracketed paste 控制序列

当前结论：**继续只在 payload 文本层做小修小补，收益很低。**

### 2. 终端内部注入路径

这类方向也基本覆盖过：

- `PasteText`
- `SendInput(string)`
- `RawWriteChar`
- `OnDirectKeyEvent`
- `RawWriteKeyEvent`

当前结论：**这些路径很可能都还在 terminal/control/backend 注入层，未必能冒充 CLI 期待的前台 TTY 输入。**

### 3. Win32 / OS 层按键模拟

这类方向已经做过多轮：

- OS `SendInput`
- scan-code only
- `SendMessage`
- `PostMessage`
- 聚焦 terminal surface / TSF hwnd / owning hwnd
- 显式前台激活 owning/hosting window
- 记录 `hwndActive` / `hwndFocus` / `hwndCaret`

当前结论：**虽然更接近前台输入，但目前仍没有证据证明真正走到了目标 CLI 认可的那条链。最新日志反而说明：多个 tab 最终共享的是同一个 WinUI TSF/InputSite 输入窗口，这条路无法稳定把输入绑定到某个 Copilot tab 的 prompt。**

### 4. 分支选择与 tab 识别

这部分已经探索出明确结论：

- `BracketedPasteEnabled` 只是现象，不是 CLI 类型判据。
- 新 tab / 新会话场景里必须结合 command/runtime state。
- per-tab latch 可以解决“识别漂移”，但只是保证走对试验分支，不是最终修复。

### 5. 焦点与激活验证

这部分现在也有比较明确的结论：

- workspace chat 输入框让开焦点后，`TermControl` 的 XAML focus 请求可以成功。
- 主窗体/hosting window 可以被显式重新激活到前台。
- 但这些成功**并不会自动带来 Copilot prompt 就绪**。
- 所以“是不是没 focus 到主窗体 / `TermControl`”已经不是当前最值得怀疑的点。

## 还没有真正做透的方向

下面这些才是后续值得继续挖的主方向。

| 方向 | 是否探索过 | 说明 |
| --- | --- | --- |
| 明确区分“前台输入路径”和“PTY/backend 注入路径”的完整调用链 | 否，未做透 | 现在只是现象上怀疑，缺少从 XAML/WinUI -> TermControl -> Conpty/connection 的一条完整输入语义图 |
| 找出 Windows Terminal 里真正对应“用户实体敲键”的处理入口，并比较 workspace chat 现在是否绕开了它 | 否，未做透 | 需要从真实键盘事件入口反查，而不是只从 workspace chat 发送端正推 |
| 对比“物理键盘输入 hi + Enter”与“workspace chat 转发 hi + Enter”在 terminal diagnostics / control 回调 / connection 写入上的差异 | 否 | 这是最关键的对照实验 |
| 确认 Copilot/Claude CLI 是卡在 bracketed paste 状态机、raw mode、还是某个 line discipline/TTY 期待上 | 否 | 现在只知道“它不认 submit”，但不知道具体不认哪一步 |
| 搞清 Copilot 多 tab 下“当前 prompt 输入归属”到底由什么内部状态决定 | 否 | 现有证据说明多个 tab 共享同一个前台输入口，但只有个别 tab 会把输入认成自己的 prompt |
| 研究是否需要单独的前台输入桥接层，而不是复用现有 chat -> string send 方案 | 否 | 这可能是架构级答案 |
| 研究能否把 workspace chat 发送拆成“真实按键流”而不是“字符串流” | 部分 | 试过一些按键模拟，但没有从“真实键盘路径对照”角度系统化验证 |

## 建议的后续优先级

## 英文真实键盘链路验证方案

这个方案的目标不是一次性解决中文/IME/多语种输入，而是先用 **英文可直接映射字符** 验证：

> **如果 workspace chat 的文本和 Enter 都走“真实物理键盘流”，Copilot 多 tab 的提交链路到底能不能稳定打通。**

### 范围收敛

本阶段只支持以下输入：

- `a-z`
- `A-Z`
- `0-9`
- 常见 ASCII 符号
- `Enter`

本阶段**不支持**：

- 中文
- 拼音 / 双拼 / 五笔等 IME 输入
- 依赖候选窗的输入法交互
- 无法通过当前键盘布局直接映射的字符

### 核心原则

1. **不要“像键盘”**
   - 不再用 Unicode `SendInput`
   - 不再把文本当成字符串塞进输入口
   - 每个字符都必须落成真实物理按键序列

2. **文本和 Enter 必须同源**
   - 文本走物理键盘流
   - Enter 也走物理键盘流
   - 不允许“文本是字符串注入、回车是物理按键”这种混合语义

3. **不能映射就明确失败**
   - 不做静默降级
   - 不偷偷退回 Unicode 注入
   - 日志里明确记录失败字符

### 实施步骤

#### 第一步：把窗口键盘模式限定为英文物理键盘模式

把当前窗口键盘模式明确定义成：

- **只接受当前键盘布局可直接映射的英文/ASCII 字符**
- 每个字符通过 `VkKeyScanEx` / `MapVirtualKeyEx` 生成物理按键序列
- 必要时显式补 `Shift` 的 down/up
- 最终字符键也走 down/up

如果某个字符：

- `VkKeyScanEx` 返回失败，或
- 需要 IME / 候选窗 / 组合输入才能产生，

则这次发送直接失败，并记录：

- 哪个字符无法映射
- 当前使用的输入模式是 `physical-layout-scancode`

#### 第二步：发送前增加“可发条件”

在真正发键之前，至少确认：

1. 主窗体已回到前台
2. `TermControl.Focus(FocusState::Programmatic)` 返回成功
3. `TryFocusAsync(...)` 成功
4. Win32 焦点窗口稳定
5. workspace chat 输入框不再持有焦点

如果这些条件不满足，就不要继续发键。

#### 第三步：明确只验证英文链路

先固定一组简单输入做验证：

- `hi`
- `go`
- `test`

不要一开始就混入：

- 中文
- 空格很多的长句
- 多行文本
- 特殊符号密集输入

先回答“短英文 prompt 能否稳定命中 Copilot prompt”这个最小问题。

### 验收标准

英文物理键盘链路通过，不是看“terminal 收到了 input”，而是看下面这些特征是否稳定出现：

1. terminal event store 里有对应 input
2. output 里出现 `❯ hi` / `❯ go` / `❯ test`
3. output 进入 `Working`
4. 不只是 Copilot 首页 redraw
5. 多个 Copilot tab 下，当前目标 tab 至少能重复稳定命中

如果只看到：

- input 有记录
- output 只是首页整屏刷新
- 没有 `❯ ...`
- 没有 `Working`

那就算**链路未打通**。

### 结果判定

#### 如果英文真实键盘链路打通

说明至少有一部分问题确实来自：

- Unicode 文本注入不像真实键盘
- 文本/Enter 混合语义不够纯

这时再继续扩大输入能力才有意义。

#### 如果英文真实键盘链路仍然打不通

那就可以更明确地下结论：

- 问题不是中文/IME
- 问题也不只是“键盘模拟不够像”
- 根因更可能是：**Copilot 多 tab 下的 prompt 归属，本来就不能靠这条共享前台输入口稳定绑定**

到那一步，就不应继续只在按键模拟层补丁，而要转向：

- 真实物理键盘路径对照实验
- Copilot prompt 归属机制分析
- 是否需要单独输入桥接层

### 第一优先级：做真实输入对照实验

先不要继续堆新的 `\r` 变体，先回答下面两个问题：

1. 用户手工在目标 Copilot tab 里敲 `hi` 再按 Enter，Terminal 内部到底走了哪些回调/事件？
2. workspace chat 转发同样内容时，和上面相比差在哪一层？
3. 多个 Copilot tab 同时存在时，哪个内部状态决定“这次输入归属当前 tab 的 prompt”？

如果这一步不做，后面所有试错都还是盲打。

### 第二优先级：画输入路径分层图

建议把输入路径拆成 4 层来分析：

1. **前台 UI 输入层**：XAML/WinUI/焦点/TSF/消息循环
2. **控件事件层**：TermControl 的键盘事件处理入口
3. **terminal core 层**：控制序列、paste、string send、raw write
4. **connection / PTY 层**：最终写给 conpty / shell / CLI 的内容

后续每个实验都标注自己落在哪一层，避免把不同层的尝试混在一起。

### 第三优先级：决定是否要架构换路

如果对照实验最终证明：

- workspace chat 当前所有可用 API 都只会落到“字符串/后端注入”层，
- 而目标 CLI 必须吃“真实前台键盘输入”链路，

那就不该再继续在当前接口上修修补补，而应该改成：

- 单独的前台输入桥接方案，或
- 明确把这类 CLI 标记为不能走当前 workspace chat 注入模型。

## 暂不建议继续投入的方向

以下方向已经试得比较充分，优先级应明显降低：

- 继续切换 `\r` / `\n` / `\r\n`
- 再加更多次重复 Enter
- 再加更多固定延时
- 继续只围绕“主窗体是否前台 / `TermControl` 是否拿到 XAML focus”打转
- 继续只围绕 `BracketedPasteEnabled` 调分支
- 继续只改 payload 拼接顺序

这些方向现在更像“参数扫表”，不是根因分析。

## 总结

当前探索结果已经足够说明：

1. **发送链路不是主矛盾。**
2. **分支判断不是主矛盾。**
3. **单纯焦点/激活成功也不是主矛盾。**
4. **核心矛盾是：workspace chat 现在拿到的发送能力，是否真的是目标 CLI 所需的前台 TTY 输入能力，以及它能否稳定绑定到目标 Copilot tab 自己的 prompt。**

后续探索不应再以“再试一个 Enter 组合”为主，而应转向：

- **真实键盘路径对照**
- **输入链路分层**
- **确认是否需要架构换路**
