# 四人队伍战斗模拟说明

队伍模块建立在元素反应核心之上，用于输入四名角色和一个或多个敌人，自动模拟元素战技、元素爆发、换人、冷却、能量、元素微粒、多段脱手攻击和元素反应。

公共头文件是 `include/elemental/battle.hpp`，命名空间为 `elemental`。

## 1. 直接运行

先构建工程：

```powershell
$toolchain = (Resolve-Path ".tools\w64devkit\w64devkit\bin").Path
$env:Path = "$toolchain;$env:Path"

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

运行默认 30 秒、3 个敌人的队伍：

```powershell
.\build\elemental_team_demo.exe
```

显式输入战斗时长、敌人数和四名角色：

```powershell
.\build\elemental_team_demo.exe 30 3 Xiangling Xingqiu Kaeya Sucrose
```

命令行演示名册当前包含以下四名角色，输入顺序同时决定初始轮转顺序：

- `Xiangling`
- `Xingqiu`
- `Kaeya`
- `Sucrose`

示例中的冷却、爆发能量、持续时间、元素量和微粒机制采用游戏机制数据；固定 `base_damage` 与部分命中时刻是用于展示模拟器的场景参数，不代表某一套圣遗物、武器、命座和天赋等级的最终面板。行秋的雨帘剑按每秒存在一次普通攻击触发机会进行演示。

## 2. 模拟器的实际规则

### 2.1 共享场上角色

四名角色共享一个前台位置。只有当前场上角色可以主动施放战技或爆发，但已经生成的脱手攻击会继续执行。切换角色后会进入默认 1 秒换人冷却。

角色选择采用循环公平调度。多个动作同时可用时：

1. 默认优先选择已充满能量且冷却结束的元素爆发；
2. 再选择冷却结束的元素战技；
3. 同优先级从上次行动角色的下一位开始循环查找；
4. 若目标角色不在场，必须先满足换人冷却；
5. 技能动画的 `action_duration_seconds` 会锁住下一次主动操作。

将 `BattleConfig::burst_before_skill` 设为 `false` 可让战技在同一时刻优先。

“冷却好立即使用”是指在以下约束都满足的第一个时刻施放：角色可切换、共享动作锁结束、技能自身冷却结束；爆发还必须拥有不少于能量消耗的能量。

### 2.2 冷却

战技和爆发的冷却在施放瞬间开始，并分别保存为角色运行时状态。可通过以下方法查看：

```cpp
character.skill_ready_at();
character.burst_ready_at();
character.skill_cast_count();
character.burst_cast_count();
```

当前通用模块支持固定冷却。祭礼武器重置、如雷套减冷却、迟滞之水、技能层数以及角色天赋的动态减冷却，应由后续角色规则在施放回调或专用控制器中扩展。

### 2.3 爆发能量

爆发施放条件为：

```text
当前时间 >= 爆发冷却完成时间
且 当前能量 >= 爆发能量消耗
```

施放后扣除 `AbilityDefinition::energy_cost`。能量不会超过该角色的爆发能量上限。

四人队伍吸收一个元素微粒时，100% 充能效率下的基础能量为：

| 接收者 | 同元素微粒 | 异元素微粒 | 无色微粒 |
|---|---:|---:|---:|
| 前台 | 3.0 | 1.0 | 2.0 |
| 后台 | 1.8 | 0.6 | 1.2 |

最终获得量再乘角色自己的 `energy_recharge`。例如后台 200% 充能的火角色吸收一个火微粒，获得 `1.8 × 2.0 = 3.6` 能量。该换算与前后台规则参考 [KQM Energy Mechanics](https://library.keqingmains.com/combat-mechanics/energy) 和 [Energy 数据表](https://genshin-impact.fandom.com/wiki/Energy)。

`AbilityHit::particle_count` 可以是小数，用于确定性模拟“2 或 3 个微粒”等随机产出的期望值。`Element::physical` 在微粒字段中表示无色微粒。微粒只在对应命中至少碰到一个存活敌人时产生，并在 `particle_pickup_delay_seconds` 后由当时的前台角色接取，全队同时获得各自的能量。

直接回复固定能量、敌人血量阈值掉球、武器和命座额外能量暂未自动生成；可以把它们作为新的计划事件扩展。固定能量通常不受充能效率影响，与元素微粒应分开处理。

### 2.4 多段和脱手技能

一个 `AbilityDefinition` 包含多个 `AbilityHit`。每一段拥有独立的：

- 相对施放时间；
- 单体或全体目标方式；
- 伤害元素、元素量和基础伤害；
- 伤害加成、暴击倍率与无视防御；
- 钝击标记；
- ICD 组；
- 微粒产量；
- 草原核触发上限。

因此锅巴、旋火轮、雨帘剑、冰棱等离场后继续攻击的技能，不需要角色继续站场。施放时所有命中都会进入全局计划队列。

### 2.5 元素附着 ICD

启用 `AbilityHit::uses_standard_icd` 后，使用 `3 hit / 2.5 s` 规则。ICD 按以下三项隔离：

```text
来源角色 + 目标敌人 + icd_group
```

因此一个全体攻击对每个敌人的附着计数彼此独立。不同技能需要独立 ICD 时，使用不同的 `icd_group`；需要共享时使用相同字符串。ICD 的标签/组和默认 `2.5 秒 / 3 次命中` 模型可参考 [Internal Cooldown 数据](https://genshin-impact.fandom.com/wiki/Internal_Cooldown/Data)。

### 2.6 多个敌人

每个 `Enemy` 持有自己的：

- 当前生命值；
- `TargetState` 元素附着；
- 感电、冻结、燃烧、激化和超导状态；
- 草原核记录；
- 每目标 ICD 进度。

`TargetingMode::primary` 命中第一个仍存活的敌人；`TargetingMode::all_enemies` 命中所有存活敌人。敌人死亡后不再成为新攻击目标。

## 3. 在代码中输入四人队伍

### 3.1 定义一段攻击

```cpp
elemental::AbilityHit hit;
hit.delay_seconds = 0.25;
hit.targeting = elemental::TargetingMode::all_enemies;
hit.element = elemental::Element::pyro;
hit.gauge_units = 1.0;
hit.base_damage = 5'000.0;
hit.damage_bonus = 0.466;
hit.critical_multiplier = 2.20;
hit.uses_standard_icd = false;
hit.particle_count = 2.5;
hit.particle_element = elemental::Element::pyro;
```

`particle_count` 绑定在该命中上，只会生成一次，不会因为全体攻击命中三个敌人就生成三倍微粒。

### 3.2 定义角色

```cpp
elemental::CharacterDefinition definition;
definition.stats.id = "MyPyroCharacter";
definition.stats.level = 90;
definition.stats.elemental_mastery = 180.0;
definition.element = elemental::Element::pyro;
definition.energy_recharge = 1.80; // 180%
definition.initial_energy = 0.0;

definition.skill.name = "My Skill";
definition.skill.cooldown_seconds = 6.0;
definition.skill.action_duration_seconds = 0.55;
definition.skill.hits.push_back(hit);

definition.burst.name = "My Burst";
definition.burst.cooldown_seconds = 15.0;
definition.burst.energy_cost = 60.0;
definition.burst.action_duration_seconds = 0.90;

elemental::AbilityHit burst_hit = hit;
burst_hit.delay_seconds = 0.40;
burst_hit.base_damage = 12'000.0;
definition.burst.hits.push_back(burst_hit);

elemental::Character character(std::move(definition));
```

多段技能只需继续向 `hits` 追加时间点。命中延迟相对于本次施放时刻，不是全局时间。

### 3.3 创建敌人

```cpp
std::vector<elemental::Enemy> enemies;

for (int i = 0; i < 3; ++i) {
    elemental::EnemyDefinition enemy;
    enemy.id = "enemy-" + std::to_string(i + 1);
    enemy.maximum_health = 250'000.0;
    enemy.stats.level = 90;
    enemy.stats.set_resistance(elemental::Element::pyro, 0.10);
    enemy.stats.set_resistance(elemental::Element::hydro, 0.10);
    enemies.emplace_back(std::move(enemy));
}
```

一个和多个敌人使用同一接口。

### 3.4 开始模拟

```cpp
#include "elemental/battle.hpp"

std::vector<elemental::Character> party;
party.emplace_back(make_first_character());
party.emplace_back(make_second_character());
party.emplace_back(make_third_character());
party.emplace_back(make_fourth_character());

elemental::BattleConfig config;
config.duration_seconds = 30.0;
config.initial_active_character = 0;
config.switch_cooldown_seconds = 1.0;
config.particle_pickup_delay_seconds = 0.7;
config.burst_before_skill = true;

elemental::BattleSimulator simulator(
    std::move(party), std::move(enemies), config);
const elemental::BattleResult result = simulator.run();
```

构造函数要求恰好四名角色、至少一个敌人，并要求角色与敌人的 `id` 各自唯一。一个模拟器实例只能调用一次 `run()`。

## 4. 读取结果

`BattleResult` 包含：

| 字段 | 含义 |
|---|---|
| `elapsed_seconds` | 实际模拟时长 |
| `total_damage` | 所有敌人实际损失的生命值，过量伤害不计入 |
| `enemies_defeated` | 被击败的敌人数 |
| `log` | 已按时间排序的完整战斗日志 |

日志类型包括换人、施放、攻击伤害、反应、能量获得和敌人死亡。读取示例：

```cpp
for (const auto& entry : result.log) {
    std::cout << entry.time_seconds << " "
              << elemental::to_string(entry.kind) << " "
              << entry.actor_id << " "
              << entry.target_id << " "
              << entry.detail << " "
              << entry.amount << '\n';
}
```

模拟结束后还可以读取运行时状态：

```cpp
for (const auto& character : simulator.party()) {
    std::cout << character.definition().stats.id
              << " energy=" << character.energy()
              << " E=" << character.skill_cast_count()
              << " Q=" << character.burst_cast_count() << '\n';
}

for (const auto& enemy : simulator.enemies()) {
    std::cout << enemy.definition().id
              << " hp=" << enemy.health() << '\n';
}
```

## 5. 草原核、范围攻击和扩散

火或雷命中是否碰到草原核由技能配置决定。将上限设为大于零即可：

```cpp
hit.dendro_core_trigger_limit = 2;
```

传入 `std::numeric_limits<std::size_t>::max()` 表示触发该敌人状态中记录的全部核心。火产生烈绽放，雷产生超绽放。

当前 `all_enemies` 表示简化的全体命中，不计算敌人坐标、圆形半径、距离衰减和碰撞遮挡。扩散事件会正确生成独立伤害，但把被扩散元素传播到附近敌人的空间查询仍属于世界层；如需严格范围模拟，应加入坐标和形状查询后再构造传播攻击。

## 6. 参数校验

以下输入会抛出异常：

- 队伍不是恰好四人；
- 没有敌人；
- 角色或敌人 ID 重复；
- 冷却不大于 0；
- 动作时间、元素量、基础伤害、暴击倍率、微粒数或能量为负；
- 战技配置了能量消耗；
- 战斗时间或最大步长不大于 0；
- 初始前台角色索引越界；
- 对同一个 `BattleSimulator` 调用第二次 `run()`。

## 7. 当前精度边界

通用调度器已经按游戏思路实现固定冷却、爆发双门槛、四人微粒能量换算、前后台接球、共享前台、换人冷却、动作时间、脱手命中和每目标 ICD。以下内容需要角色专属层继续补充：

- 普通攻击、重击和下落攻击控制器；
- 行秋雨帘剑等“由普通攻击触发”的严格触发检测；
- 角色天赋、命座、武器、圣遗物、增益和快照；
- 技能层数、长按/点按、冷却重置和动态减冷却；
- 敌人攻击、打断、位移、无敌帧和阶段切换；
- 坐标、碰撞体、真实范围、扩散传播和感电跳跃目标；
- 敌人血量阈值掉落微粒/晶球；
- 延迟、命中停滞与逐帧动画取消。

游戏中的四人队伍和 1 秒切换冷却可参见 [Party](https://genshin-impact.fandom.com/wiki/Party)，技能固定或可变冷却的分类可参见 [Cooldown](https://genshin-impact.fandom.com/wiki/Cooldown)。示例角色的公开数据入口包括 [Guoba Attack / Xiangling](https://genshin-impact.fandom.com/wiki/Xiangling)、[Pyronado](https://genshin-impact.fandom.com/wiki/Pyronado)、[Fatal Rainscreen](https://genshin-impact.fandom.com/wiki/Guhua_Sword:_Fatal_Rainscreen)、[Glacial Waltz](https://genshin-impact.fandom.com/wiki/Glacial_Waltz)、[Sucrose Skill](https://genshin-impact.fandom.com/wiki/Astable_Anemohypostasis_Creation_-_6308) 和 [Sucrose Burst](https://genshin-impact.fandom.com/wiki/Forbidden_Creation_-_Isomer_75_/_Type_II)。
