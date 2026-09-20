# 终端演示视频录制流程

这套流程只录真实 PowerShell 终端，不使用宣传片背景、合成配音或 AI 风格画面。三段视频分别展示：

1. 不使用四人队伍的单目标元素反应核心；
2. 经典反应四人队，对战两个敌人；
3. 纳西妲、行秋、久岐忍、瑶瑶组成的超绽放队。

## 一、录制前只做一次：构建录屏版本

打开 PowerShell，执行：

```powershell
cd D:\codex\mywork\element
$env:Path = "$PWD\.tools\w64devkit\w64devkit\bin;" + $env:Path
cmake -S . -B build-recording -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-recording --parallel
ctest --test-dir build-recording --output-on-failure
```

若最后显示全部测试通过，就可以开始录制。当前工作区已经生成过 `build-recording`，源代码没有再修改时可以跳过本节。

## 二、建议的终端画面

- 将 PowerShell 窗口最大化，避免录入桌面杂物；
- 字号调到 20–24，窗口宽度以每条日志不换行为准；
- 关闭终端透明效果；
- 视频不需要鼠标操作，也不需要录入代码编辑器；
- 建议每段单独录制，后期按顺序拼接。

## 三、Xbox Game Bar 录制方法

1. 先在 PowerShell 中输入对应命令，但暂时不要按回车；
2. 按 `Win + Alt + R` 开始录制；
3. 回到 PowerShell 按回车。脚本会先显示标题并倒数 3 秒；
4. 看到 `Recording segment complete` 后，再按 `Win + Alt + R` 停止录制；
5. 停止录制后，按回车结束脚本。

录制文件默认是 MP4，位于用户“视频”目录下的 `Captures` 文件夹。若快捷键无反应，先按 `Win + G` 打开 Xbox Game Bar 并确认捕获功能已启用。

## 四、三段视频的命令

### 第一段：单目标反应核心，不使用四人队伍

```powershell
.\scripts\record_demo.ps1 -Part 1
```

画面会依次显示水元素附着、火元素攻击、蒸发倍率、最终伤害和剩余水元素量。这一段证明元素反应引擎可以脱离四人队伍单独使用。

### 第二段：经典反应队，对战两个敌人

```powershell
.\scripts\record_demo.ps1 -Part 2
```

队伍为香菱、行秋、凯亚、砂糖。输出会显示自动切人、技能释放、爆发释放以及蒸发、融化、冻结、扩散等反应。标题中的规则会明确说明：元素战技冷却结束后立即尝试施放；元素爆发必须同时满足冷却完成和能量充满。

### 第三段：草元素超绽放队

```powershell
.\scripts\record_demo.ps1 -Part 3
```

队伍为纳西妲、行秋、久岐忍、瑶瑶。重点观察以下日志链：

```text
Bloom: Dendro Core created
Hyperbloom
```

期间也会出现原激化、超激化和蔓激化。该段使用单个高生命敌人，避免多目标重复日志遮住超绽放主线；第二段已经展示多敌人模拟。

## 五、调整速度或手动运行

脚本默认以 `2x` 速度回放战斗时间轴。若希望日志慢一些：

```powershell
.\scripts\record_demo.ps1 -Part 3 -ReplaySpeed 1.5
```

也可以绕过录制脚本直接运行：

```powershell
.\build-recording\elemental_demo.exe --replay-speed 2
.\build-recording\elemental_team_demo.exe --preset classic --duration 12 --enemies 2 --replay-speed 2 --compact
.\build-recording\elemental_team_demo.exe --preset hyperbloom --duration 10 --enemies 1 --replay-speed 2 --compact
```

去掉 `--replay-speed` 后，程序会立即输出全部模拟结果，适合测试，不适合录屏。去掉 `--compact` 后会保留所有反应日志，包括被 ICD 抑制的伤害和短时间内重复触发的反应。

## 六、建议口播

第一段开头：

> 先展示底层元素反应引擎。这里没有角色队伍和自动轮转，只向同一个目标依次施加水元素和火元素，程序计算蒸发倍率、最终伤害与剩余元素量。

第二段开头：

> 接下来进入四人自动战斗。角色会在元素战技冷却后立即使用战技；元素爆发只有在冷却完成且能量充满时才会释放。这里使用两个敌人展示独立附着和范围攻击。

第三段开头：

> 最后换成草元素相关队伍。水与草生成草原核，久岐忍的雷元素攻击会触发超绽放；同一时间轴里还能看到原激化、超激化和蔓激化。

## 七、模型边界

队伍示例使用公开的技能冷却、爆发能量、持续时间、命中间隔和元素附着冷却数据。以下内容属于确定性演示近似：

- 纳西妲的灭净三业在游戏中由反应触发；示例把一次触发排入技能时间轴，以便无需场景脚本也能稳定复现；
- 久岐忍和瑶瑶的微粒生成带概率；示例使用期望值形式的分数微粒，保证每次录制结果一致；
- 治疗、角色生命、移动距离、真实碰撞盒、草原核空间位置和玩家普通攻击输入尚未纳入此命令行演示。

因此，视频中应表述为“按公开机制参数构建的确定性模拟”，不要表述为逐帧复现游戏客户端。
