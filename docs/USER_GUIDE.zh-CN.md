# Elemental Reactions C++ 操作说明书

本文面向需要构建、运行或把元素反应核心接入游戏/数值模拟器的开发者。工程版本为 `0.1.0`，要求 C++17，不依赖第三方运行时库。

机制公式、资料依据和拟真边界见 [元素反应机制与工程映射](MECHANICS.zh-CN.md)。本文只说明如何正确操作当前 API。

## 1. 系统能做什么

系统既可以单独结算一次攻击，也可以驱动四人队伍对一个或多个敌人的完整时间轴。它可以处理：

- 普通元素附着、元素量消耗与随时间衰减；
- 蒸发、融化、超载、感电、超导、冻结、碎冰、扩散和结晶；
- 燃烧、绽放、超绽放、烈绽放、原激化、超激化和蔓激化；
- 感电与燃烧周期伤害、冻结和原激化持续时间、草原核到期爆炸；
- 增幅、剧变、激化、结晶盾、防御和抗性计算；
- 常规 `3 hit / 2.5 s` 元素附着 ICD；
- 月感电、月绽放、月结晶的共用伤害公式；
- 四人自动轮转、技能/爆发冷却、元素能量与微粒分配；
- 单目标和全体目标攻击，每个敌人独立保存元素状态。

四人队伍的专门用法见 [队伍战斗模拟说明](BATTLE_SIMULATOR.zh-CN.md)。系统不会管理完整角色资料库、场景坐标、精确碰撞、击退动画、晶片拾取或月反应角色专属资源；这些内容应由上层游戏逻辑或角色配置负责。

## 2. 快速开始

### 2.1 环境要求

构建工程需要：

| 工具 | 最低要求 | 说明 |
|---|---:|---|
| C++ 编译器 | 支持 C++17 | GCC、Clang 或 MSVC 均可 |
| CMake | 3.20 | 用于生成工程 |
| 构建工具 | Ninja 或 Visual Studio | 二选一 |

本工作区已经在 `.tools/w64devkit/w64devkit/bin` 中准备了便携版 GCC、CMake 和 Ninja。该目录被 Git 忽略，只服务于当前机器。

### 2.2 使用工作区内的便携工具链

在 PowerShell 中进入项目根目录，然后执行：

```powershell
$toolchain = (Resolve-Path ".tools\w64devkit\w64devkit\bin").Path
$env:Path = "$toolchain;$env:Path"

g++ --version
cmake --version
ninja --version
```

这只修改当前 PowerShell 会话的 `PATH`，关闭终端后不会影响系统配置。

### 2.3 构建、测试与运行示例

使用 Ninja：

```powershell
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel
ctest --test-dir build-release --output-on-failure
.\build-release\elemental_demo.exe
```

使用 Visual Studio 2022：

```powershell
cmake -S . -B build-vs -G "Visual Studio 17 2022"
cmake --build build-vs --config Release
ctest --test-dir build-vs -C Release --output-on-failure
.\build-vs\Release\elemental_demo.exe
```

测试成功时会显示：

```text
100% tests passed, 0 tests failed
```

### 2.4 可选构建开关

| CMake 选项 | 默认值 | 作用 |
|---|---:|---|
| `ELEMENTAL_BUILD_TESTS` | `ON` | 构建 `elemental_tests` |
| `ELEMENTAL_BUILD_EXAMPLE` | `ON` | 构建 `elemental_demo` |
| `BUILD_TESTING` | `ON` | CTest 总开关 |

只构建库：

```powershell
cmake -S . -B build-lib -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DELEMENTAL_BUILD_TESTS=OFF `
  -DELEMENTAL_BUILD_EXAMPLE=OFF `
cmake --build build-lib --parallel
```

## 3. 接入自己的 CMake 工程

把本项目放在主工程的 `third_party/elemental_reactions` 下，然后在主工程的 `CMakeLists.txt` 中加入：

```cmake
add_subdirectory(third_party/elemental_reactions)

add_executable(my_game main.cpp)
target_link_libraries(my_game PRIVATE elemental::reactions)
```

代码中只需包含公共头文件：

```cpp
#include "elemental/reaction.hpp"
```

公共 API 位于 `include/elemental/reaction.hpp`，实现位于 `src/reaction.cpp`。链接 `elemental::reactions` 后，头文件路径和 C++17 要求会自动传递给调用方。

## 4. 核心操作流程

一次战斗循环遵循以下顺序：

```text
创建 ReactionEngine
        ↓
为每个敌人创建一个 TargetState
        ↓
命中时组装 Attack，并用 apply() 结算
        ↓
读取 Resolution 与 ReactionEvent，驱动伤害和表现
        ↓
经过时间后调用 advance()，结算衰减、周期伤害和核心到期
```

最重要的对象关系是：

- `ReactionEngine`：无目标专属状态，可以复用于多个敌人；
- `TargetState`：保存一个敌人的元素附着和持续状态，每个敌人必须单独持有；
- `Attack`：描述一次命中；
- `Resolution`：描述这次命中的直接结果；
- `ReactionEvent`：描述一个具体反应事件；
- `StandardIcdTracker`：由攻击来源持有，判断某次命中是否施加元素。

## 5. 数值单位与输入约定

所有百分比均使用小数，而不是百分数文本。

| 字段 | 单位/格式 | 示例 | 含义 |
|---|---|---:|---|
| `level` | 整数等级 | `90` | 角色或目标等级 |
| `elemental_mastery` | 点数 | `180.0` | 元素精通 |
| `gauge_units` | U | `1.0` | 攻击的原始元素量，系统内部再乘附着税 |
| `damage_bonus` | 小数 | `0.466` | 46.6% 伤害加成 |
| `critical_multiplier` | 倍率 | `2.2` | 本次暴击最终乘 2.2；未暴击填 1.0 |
| `resistance` | 小数 | `0.10` | 10% 抗性；允许负数 |
| `defense_reduction` | 小数 | `0.30` | 30% 减防，计算时限制在 `[0, 1]` |
| `defense_ignore` | 小数 | `0.20` | 20% 无视防御，计算时限制在 `[0, 1]` |
| `reaction_bonus` | 小数 | `0.15` | 15% 指定反应加成 |
| `base_damage` | 伤害值 | `10000.0` | 天赋倍率与角色属性计算后的基础伤害 |
| 时间参数 | 秒 | `0.016` | 游戏帧或逻辑步长 |

`base_damage` 必须是应用伤害加成、暴击、防御、抗性、蒸发/融化和激化加值之前的数值。若只模拟附着而不计算攻击伤害，把它保持为 `0.0`。

角色反应等级目前支持 `1–90`、`95` 和 `100`。需要等级系数的反应若使用其他 90 级以上等级，会抛出 `std::out_of_range`。

## 6. 枚举速查

### 6.1 元素

| 中文 | C++ 枚举 | 能否作为普通附着保存 |
|---|---|---:|
| 物理 | `Element::physical` | 否 |
| 火 | `Element::pyro` | 是 |
| 水 | `Element::hydro` | 是 |
| 雷 | `Element::electro` | 是 |
| 冰 | `Element::cryo` | 是 |
| 风 | `Element::anemo` | 否 |
| 岩 | `Element::geo` | 否 |
| 草 | `Element::dendro` | 是 |

### 6.2 反应

| 中文 | C++ 枚举 | 主要输出字段 |
|---|---|---|
| 蒸发 | `Reaction::vaporize` | `attack_multiplier` |
| 融化 | `Reaction::melt` | `attack_multiplier` |
| 超载 | `Reaction::overloaded` | `independent_damage` |
| 感电 | `Reaction::electro_charged` | `independent_damage` |
| 超导 | `Reaction::superconduct` | `independent_damage`、`duration_seconds` |
| 冻结 | `Reaction::frozen` | `duration_seconds` |
| 碎冰 | `Reaction::shatter` | `independent_damage` |
| 扩散 | `Reaction::swirl` | `independent_damage`、`damage_element` |
| 结晶 | `Reaction::crystallize` | `shield_strength`、`duration_seconds` |
| 燃烧 | `Reaction::burning` | `independent_damage` |
| 绽放 | `Reaction::bloom` | `core_id`、`duration_seconds` |
| 草原核爆炸 | `Reaction::bloom_explosion` | `independent_damage`、`core_id` |
| 超绽放 | `Reaction::hyperbloom` | `independent_damage`、`core_id` |
| 烈绽放 | `Reaction::burgeon` | `independent_damage`、`core_id` |
| 原激化 | `Reaction::quicken` | `duration_seconds` |
| 超激化 | `Reaction::aggravate` | `additive_base_damage` |
| 蔓激化 | `Reaction::spread` | `additive_base_damage` |
| 月感电 | `Reaction::lunar_charged` | 通过 `DamageModel` 单独计算 |
| 月绽放 | `Reaction::lunar_bloom` | 通过 `DamageModel` 单独计算 |
| 月结晶 | `Reaction::lunar_crystallize` | 通过 `DamageModel` 单独计算 |

`to_string(Element)` 和 `to_string(Reaction)` 返回英文名称，适合日志与调试输出。

## 7. 创建角色和目标

### 7.1 攻击者属性

```cpp
elemental::ActorStats pyro;
pyro.id = "pyro-player-1";
pyro.level = 90;
pyro.elemental_mastery = 180.0;
pyro.set_bonus(elemental::Reaction::vaporize, 0.15);
pyro.set_bonus(elemental::Reaction::overloaded, 0.20);
```

`id` 用于填充事件的 `source_id`，应当能唯一识别伤害归属者。不同反应加成需要分别调用 `set_bonus()`。

读取某项加成：

```cpp
const double bonus = pyro.bonus(elemental::Reaction::vaporize);
```

### 7.2 目标属性和状态

```cpp
elemental::TargetStats enemy_stats;
enemy_stats.level = 90;
enemy_stats.set_resistance(elemental::Element::physical, 0.10);
enemy_stats.set_resistance(elemental::Element::pyro, 0.10);
enemy_stats.set_resistance(elemental::Element::hydro, 0.10);
enemy_stats.freeze_resistance = 0.0;
enemy_stats.defense_reduction = 0.0;

elemental::TargetState enemy(enemy_stats);
```

未设置的抗性默认为 `0.0`。也可以在战斗中更新目标数值：

```cpp
enemy.stats().set_resistance(elemental::Element::pyro, -0.20);
enemy.stats().defense_reduction = 0.30;
```

## 8. 组装并结算一次攻击

下面的完整片段先挂水，再用火攻击触发逆蒸发：

```cpp
#include "elemental/reaction.hpp"

#include <iostream>

int main() {
    using elemental::Element;
    using elemental::Reaction;

    elemental::ReactionEngine engine;
    elemental::TargetState enemy;

    elemental::ActorStats hydro;
    hydro.id = "hydro-applier";
    hydro.level = 90;

    elemental::Attack wet;
    wet.element = Element::hydro;
    wet.gauge_units = 2.0;
    wet.source = hydro;
    (void)engine.apply(enemy, wet);

    elemental::ActorStats pyro;
    pyro.id = "pyro-trigger";
    pyro.level = 90;
    pyro.elemental_mastery = 180.0;
    pyro.set_bonus(Reaction::vaporize, 0.15);

    elemental::Attack hit;
    hit.element = Element::pyro;
    hit.gauge_units = 1.0;
    hit.source = pyro;
    hit.base_damage = 10'000.0;
    hit.damage_bonus = 0.466;
    hit.critical_multiplier = 2.20;

    const elemental::Resolution result = engine.apply(enemy, hit);

    double total_damage = result.attack_damage;
    for (const auto& event : result.events) {
        total_damage += event.independent_damage;
        std::cout << elemental::to_string(event.reaction)
                  << " source=" << event.source_id
                  << " independent=" << event.independent_damage << '\n';
    }

    std::cout << "attack=" << result.attack_damage << '\n';
    std::cout << "total=" << total_damage << '\n';
    std::cout << "hydro aura="
              << enemy.aura_gauge(Element::hydro) << "U\n";
}
```

`apply()` 会立即修改 `enemy`，因此调用之后查询到的是结算后的附着和状态。

## 9. 正确读取结算结果

### 9.1 `Resolution`

| 字段 | 含义 | 是否计入 `attack_damage` |
|---|---|---:|
| `events` | 本次命中产生的所有反应事件 | 视事件类别而定 |
| `attack_multiplier` | 蒸发/融化的最终增幅倍率 | 是 |
| `additive_base_damage` | 超激化/蔓激化加入基础伤害区的数值 | 是 |
| `attack_damage` | 已处理增伤、暴击、防御、抗性、增幅和激化的攻击伤害 | 最终值 |

不要再次把 `attack_multiplier` 或 `additive_base_damage` 乘/加到 `attack_damage`，否则会重复计算。

### 9.2 `ReactionEvent`

| 字段 | 操作建议 |
|---|---|
| `time_seconds` | 事件在目标时间轴上的准确发生时间 |
| `reaction` | 决定反应类型、特效和日志文本 |
| `damage_element` | 用于选择飘字颜色和伤害元素 |
| `source_id` | 归属伤害、击杀、统计或触发者 |
| `independent_damage` | 剧变反应的独立伤害；应与 `attack_damage` 分开结算/显示 |
| `attack_multiplier` | 当前蒸发或融化事件的倍率 |
| `additive_base_damage` | 当前激化事件贡献的基础伤害加值 |
| `shield_strength` | 结晶晶片对应的基础盾值 |
| `duration_seconds` | 冻结、超导、结晶盾或草原核的持续时间 |
| `gauge_consumed` | 本次反应实际消耗的元素量，用于调试 |
| `core_id` | 草原核实体标识；非草原核事件通常为 0 |
| `periodic` | `true` 表示事件来自时间推进，而非当前命中 |
| `damage_suppressed` | `true` 表示伤害受内部伤害 ICD 抑制 |
| `note` | 面向开发者的英文补充说明，不建议直接作为玩家文案 |

一次攻击可能产生多个事件，例如火命中水雷共存目标时可能同时出现蒸发和超载。不要只处理 `events.front()`。

推荐将一次结算的总数定义为：

```cpp
double total = result.attack_damage;
for (const auto& event : result.events) {
    total += event.independent_damage;
}
```

冻结、原激化、绽放创建核心等事件本身的 `independent_damage` 为零，这是正常行为。

## 10. 推进战斗时间

元素衰减、状态倒计时、感电/燃烧周期伤害和草原核到期都只在 `advance()` 中推进。

```cpp
const double delta_seconds = 1.0 / 60.0;
const auto timed_events = engine.advance(enemy, delta_seconds);

for (const auto& event : timed_events) {
    // event.periodic 通常为 true
    apply_damage_to_target(event.independent_damage);
}
```

示例中的 `apply_damage_to_target()` 代表上层项目自己的扣血函数，不属于本库。

有两种常用驱动方式：

1. 每帧调用 `advance(target, deltaTime)`；
2. 在离散事件模拟器中，按两个命中之间的时间差调用一次。

两种方式都会让内部时间累计到 `TargetState::time_seconds()`。同一个目标的时间不能通过负数倒退。

在命中时刻，建议先把目标推进到该时刻，再调用 `apply()`：

```cpp
(void)engine.advance(enemy, hit_time - enemy.time_seconds());
const auto result = engine.apply(enemy, hit);
```

## 11. 元素附着 ICD

元素伤害不一定施加元素。系统不会猜测角色技能的 ICD，而是让调用方决定 `Attack::applies_aura`。

普通 `3 hit / 2.5 s` 规则可以这样接入：

```cpp
elemental::StandardIcdTracker icd;

elemental::Attack hit;
hit.element = elemental::Element::pyro;
hit.gauge_units = 1.0;
hit.source = pyro;
hit.base_damage = 3'000.0;

const double now = enemy.time_seconds();
hit.applies_aura = icd.can_apply("pyro-normal-attack", now);

const auto result = engine.apply(enemy, hit);
```

同一 ICD 组应使用稳定且一致的键；互相独立的技能使用不同键。规则行为如下：

- 每组第一次命中会附着；
- 上次附着后的第 3 次后续命中返回 `true`，即一段连续序列中的第 4 次命中；
- 距上次附着达到 2.5 秒时也返回 `true`；
- 命中次数与时间条件满足其一即可；
- `reset(group)` 重置一个组；
- `clear()` 清空所有组。

`applies_aura = false` 时，本次攻击仍会计算普通伤害，但 `gauge_units` 不会触发反应或形成附着。

无 ICD、每次命中都附着的技能不需要追踪器，直接保持默认的 `true`。

## 12. 常用反应操作示例

### 12.1 蒸发与融化

先施加底元素，再应用触发攻击。伤害从 `Resolution::attack_damage` 读取：

```cpp
(void)engine.apply(enemy, make_aura(Element::hydro, 2.0, hydro));
const auto vaporize = engine.apply(enemy, make_hit(Element::pyro, 1.0, pyro));
```

上面的 `make_aura()` 和 `make_hit()` 只是项目可自行封装的辅助函数，不属于公共 API。直接使用时按第 8 节组装 `Attack`。

方向决定基础倍率：

| 底元素 | 触发元素 | 反应 | 基础倍率 |
|---|---|---|---:|
| 水 | 火 | 逆蒸发 | 1.5 |
| 火 | 水 | 正蒸发 | 2.0 |
| 火 | 冰 | 逆融化 | 1.5 |
| 冰 | 火 | 正融化 | 2.0 |

### 12.2 感电

```cpp
elemental::Attack wet;
wet.element = elemental::Element::hydro;
wet.gauge_units = 2.0;
wet.source = hydro;
(void)engine.apply(enemy, wet);

elemental::Attack shock;
shock.element = elemental::Element::electro;
shock.gauge_units = 1.0;
shock.source = electro;
const auto first_tick = engine.apply(enemy, shock);

const auto later_ticks = engine.advance(enemy, 2.0);
```

首次有效结算立即发生，之后按配置的周期结算。水、雷任一附着耗尽时状态结束。可用 `enemy.is_electro_charged()` 查询。

### 12.3 冻结与碎冰

```cpp
(void)engine.apply(enemy, wet);

elemental::Attack freeze;
freeze.element = elemental::Element::cryo;
freeze.gauge_units = 1.0;
freeze.source = cryo;
const auto frozen = engine.apply(enemy, freeze);

elemental::Attack heavy;
heavy.element = elemental::Element::physical;
heavy.gauge_units = 0.0;
heavy.source = claymore_user;
heavy.blunt = true;
const auto shattered = engine.apply(enemy, heavy);
```

钝击设置 `blunt = true`。岩攻击默认按钝击处理，无须额外设置。冻结状态用 `is_frozen()` 查询。

### 12.4 原激化、超激化与蔓激化

```cpp
elemental::Attack electro_setup;
electro_setup.element = elemental::Element::electro;
electro_setup.gauge_units = 2.0;
electro_setup.source = electro;
(void)engine.apply(enemy, electro_setup);

elemental::Attack dendro_trigger;
dendro_trigger.element = elemental::Element::dendro;
dendro_trigger.gauge_units = 1.0;
dendro_trigger.source = dendro;
(void)engine.apply(enemy, dendro_trigger); // Quicken

elemental::Attack aggravate_hit = electro_setup;
aggravate_hit.gauge_units = 1.0;
aggravate_hit.base_damage = 8'000.0;
const auto aggravate = engine.apply(enemy, aggravate_hit);
```

超激化或蔓激化的加值已包含进 `attack_damage`。用 `is_quickened()` 和 `quicken_remaining_seconds()` 查看状态。

### 12.5 燃烧

```cpp
elemental::Attack dendro_aura;
dendro_aura.element = elemental::Element::dendro;
dendro_aura.gauge_units = 1.0;
dendro_aura.source = dendro;
(void)engine.apply(enemy, dendro_aura);

elemental::Attack ignite;
ignite.element = elemental::Element::pyro;
ignite.gauge_units = 1.0;
ignite.source = pyro;
(void)engine.apply(enemy, ignite);

const auto burning_ticks = engine.advance(enemy, 1.0);
```

重新施加火或草会把后续燃烧伤害归属更新为最后施加者；重新施加草还会覆盖底层草元素量。用 `is_burning()` 查询。

### 12.6 绽放和草原核

水草反应会返回 `Reaction::bloom` 事件并创建核心。该事件的 `core_id` 可用于把内部核心与场景实体关联。

```cpp
const auto bloom = engine.apply(enemy, hydro_on_dendro);

for (const auto& event : bloom.events) {
    if (event.reaction == elemental::Reaction::bloom) {
        spawn_core_visual(event.core_id, event.duration_seconds);
    }
}
```

由于核心在真实游戏中具有位置，火/雷命中哪些核心必须由上层空间查询决定。查询完成后显式触发：

```cpp
const auto hyperblooms = engine.trigger_dendro_cores(
    enemy,
    elemental::Element::electro,
    electro,
    2); // 最多触发两个核心

for (const auto& event : hyperblooms) {
    remove_core_visual(event.core_id);
    apply_damage_to_target(event.independent_damage);
}
```

传入 `Element::electro` 产生超绽放，传入 `Element::pyro` 产生烈绽放；其他元素返回空事件列表。省略最后一个参数时会触发当前目标记录的全部核心。

场上最多保留配置数量的核心。生成超限核心时，最早的核心会先以 `bloom_explosion` 爆炸；未触发的核心到期后也通过 `advance()` 返回 `bloom_explosion`。

### 12.7 扩散与结晶

风或岩攻击只在当前 `TargetState` 上处理反应：

- 扩散事件的 `damage_element` 表示被扩散的元素；上层可据此向附近目标发出新的元素攻击；
- 结晶事件的 `shield_strength` 和 `duration_seconds` 可用于生成晶片；拾取、护盾覆盖和元素吸收由上层处理；
- 同一目标的结晶有 1 秒内部反应冷却；冷却期内不会消费附着。

## 13. 状态查询与调试

`TargetState` 提供以下只读查询：

| 方法 | 返回内容 |
|---|---|
| `time_seconds()` | 当前目标的模拟时间 |
| `aura_gauge(element)` | 指定普通附着的剩余 U |
| `aura_remaining_seconds(element)` | 按当前衰减率估算的剩余秒数 |
| `is_frozen()` | 是否处于冻结 |
| `is_burning()` | 是否处于燃烧 |
| `is_electro_charged()` | 是否处于感电共存 |
| `is_quickened()` | 是否存在原激化 |
| `quicken_remaining_seconds()` | 原激化剩余秒数 |
| `superconduct_remaining_seconds()` | 超导减物抗剩余秒数 |
| `dendro_core_count()` | 当前记录的草原核数量 |

开发阶段建议记录每个事件及结算后的附着：

```cpp
for (const auto& event : result.events) {
    std::cout << "t=" << enemy.time_seconds()
              << " reaction=" << elemental::to_string(event.reaction)
              << " source=" << event.source_id
              << " consumed=" << event.gauge_consumed
              << " damage=" << event.independent_damage
              << " note=" << event.note << '\n';
}
```

## 14. 自定义引擎参数

默认配置：

| 字段 | 默认值 | 作用 |
|---|---:|---|
| `aura_tax` | `0.8` | 攻击元素量转为附着量的比例 |
| `electro_charged_tick_seconds` | `1.0` | 感电周期 |
| `electro_charged_gauge_cost` | `0.4` | 每次感电从水、雷各消耗的 U |
| `burning_tick_seconds` | `0.25` | 燃烧伤害周期 |
| `burning_dendro_minimum_cost_per_second` | `0.4` | 燃烧时草附着最低每秒消耗 |
| `dendro_core_lifetime_seconds` | `6.0` | 草原核寿命 |
| `maximum_dendro_cores` | `5` | 单个 `TargetState` 最多核心数 |

修改方式：

```cpp
elemental::EngineConfig config;
config.dendro_core_lifetime_seconds = 8.0;
config.maximum_dendro_cores = 8;

elemental::ReactionEngine engine(config);
```

`ReactionEngine` 构造后配置只读。若需切换整套规则，请创建新的引擎。配置应满足：

- `aura_tax` 大于 0 且不大于 1；
- 感电周期、燃烧周期、草原核寿命均大于 0；
- `maximum_dendro_cores` 大于 0。

违反以上条件会抛出 `std::invalid_argument`。

另外，`electro_charged_gauge_cost` 与 `burning_dendro_minimum_cost_per_second` 应保持为非负数；当前版本不会替调用方拒绝这两个负值。

## 15. 月反应公式的使用

月反应不会由 `ReactionEngine::apply()` 自动触发，因为是否将普通反应转换为月反应取决于指定角色、队伍和资源。上层确认转换条件后，调用 `DamageModel`。

### 15.1 间接月感电/月结晶

```cpp
std::vector<elemental::LunarContributor> contributors;
contributors.push_back({electro, 1.80});
contributors.push_back({hydro, 1.00});

const double damage = elemental::DamageModel::lunar_indirect_damage(
    elemental::Reaction::lunar_charged,
    contributors,
    enemy.stats(),
    0.20, // 月反应基础伤害加成
    1.25  // 月兆升格倍率
);
```

贡献者最多取计算后伤害最高的四人，权重依次为 `1`、`1/2`、`1/12`、`1/12`。可用于间接计算的类型只有 `lunar_charged` 与 `lunar_crystallize`。

### 15.2 角色天赋直接月反应

```cpp
elemental::LunarContributor contributor{geo, 1.75};

const double damage = elemental::DamageModel::lunar_direct_damage(
    elemental::Reaction::lunar_crystallize,
    3'500.0, // 天赋指定的攻击/生命/防御等缩放属性
    0.40,    // 天赋倍率
    contributor,
    enemy.stats(),
    0.20,
    1.25
);
```

月反应共用公式忽略普通伤害加成和目标防御，但读取对应元素抗性、精通、月反应加成及传入的暴击倍率。

## 16. 多目标与游戏循环集成

推荐的数据组织方式：

```cpp
struct EnemyCombatState {
    int entity_id;
    elemental::TargetState elements;
    double health;
};

struct SkillRuntime {
    elemental::StandardIcdTracker icd;
};
```

每帧可按以下职责分层：

1. 世界层确定命中目标和命中时刻；
2. 技能层通过 ICD 决定 `applies_aura`，并构造 `Attack`；
3. 元素层调用目标自己的 `apply()`；
4. 伤害层应用 `attack_damage` 和所有 `independent_damage`；
5. 表现层根据 `ReactionEvent` 播放特效、飘字、控制和生成物；
6. 世界层对扩散、范围爆炸和核心命中做空间查询；
7. 对仍在场的每个目标调用 `advance()`。

`ReactionEngine` 的 `apply()` 会修改传入的 `TargetState`。若多线程同时结算同一目标，调用方必须串行化或加锁；不同目标可以各自独立调度。

## 17. 错误处理

公共接口会在明显无效的输入上抛出标准异常：

| 情况 | 异常 |
|---|---|
| `Attack::gauge_units < 0` | `std::invalid_argument` |
| `advance()` 传入负时间 | `std::invalid_argument` |
| 引擎配置非法 | `std::invalid_argument` |
| 等级不在 `1–90/95/100` | `std::out_of_range` |
| 公式函数收到不适用的反应类型 | `std::invalid_argument` |

在工具或服务器程序中，建议在数据加载阶段校验角色等级、元素量和规则配置；战斗热路径不应依赖异常来处理正常分支。

## 18. 常见问题

### 攻击有元素伤害，为什么没有反应？

检查 `applies_aura` 是否为 `true`、`gauge_units` 是否大于 0，以及目标是否真的还保留可反应附着。`base_damage` 的元素类型不会代替元素附着判定。

### 为什么第一次挂元素没有 `ReactionEvent`？

第一次命中通常只建立附着，不产生反应，因此事件列表为空。这不是错误。

### 为什么 `attack_damage` 是 0？

通常是 `Attack::base_damage` 保持了默认值 `0.0`。附着模拟允许零基础伤害。

### 为什么普通攻击伤害已经包含蒸发或激化？

`attack_damage` 是完整确定性结果，已经使用 `attack_multiplier` 和 `additive_base_damage`。这两个字段用于解释过程，不需要再次参与运算。

### 为什么燃烧、感电或核心没有后续伤害？

调用方必须持续调用 `advance()`。引擎不会读取真实时钟，也不会创建后台线程。

### 为什么火/雷攻击没有自动触发场上的草原核？

核心命中依赖场景坐标和攻击范围，须先由世界层筛选，再调用 `trigger_dendro_cores()`。

### 为什么扩散没有伤害附近敌人？

当前库是单目标状态机。它返回扩散事件，但传播目标需要世界层查询后自行结算。

### 为什么结晶连续触发失败？

同一目标内置 1 秒结晶反应冷却。用 `advance()` 推进时间后再尝试。

### 能否直接填暴击率和暴击伤害？

API 接收的是本次命中的 `critical_multiplier`，不是两项面板值。确定性战斗中，未暴击传 `1.0`，暴击传 `1 + 暴击伤害`；期望值模拟可由上层先换算成期望倍率。

### 如何重置一个敌人的全部元素状态？

创建新的 `TargetState` 最直接：

```cpp
const elemental::TargetStats stats = enemy.stats();
enemy = elemental::TargetState(stats);
```

若还需恢复目标属性，保存初始 `TargetStats` 并用它重新构造。

## 19. 验收清单

首次接入时，建议逐项确认：

- 工程能以 C++17 编译并通过 `ctest`；
- 每个敌人有独立 `TargetState`；
- 每次命中前已把目标时间推进到命中时刻；
- `base_damage`、增伤、暴击、抗性等均使用正确单位；
- 角色技能按独立 ICD 组设置 `applies_aura`；
- 同时处理 `attack_damage` 和所有事件的 `independent_damage`；
- 不重复应用 `attack_multiplier` 或 `additive_base_damage`；
- 周期性调用 `advance()` 并处理返回事件；
- 草原核、扩散、范围反应和结晶晶片接入了世界层；
- 玩家文案没有直接使用开发者字段 `note`；
- 回归测试覆盖项目自行添加的角色专属规则。

## 20. 工程文件索引

| 路径 | 用途 |
|---|---|
| `include/elemental/reaction.hpp` | 公共 API 与数据类型 |
| `src/reaction.cpp` | 状态机、公式和反应实现 |
| `examples/demo.cpp` | 可直接运行的蒸发示例 |
| `tests/reaction_tests.cpp` | 机制回归测试 |
| `docs/MECHANICS.zh-CN.md` | 规则、公式、资料来源与边界 |
| `docs/USER_GUIDE.zh-CN.md` | 本操作说明书 |
| `docs/BATTLE_SIMULATOR.zh-CN.md` | 四人队伍、技能、能量与多目标说明 |
