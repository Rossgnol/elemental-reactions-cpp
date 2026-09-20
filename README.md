# Elemental Reactions C++

一个无第三方运行时依赖的 C++17 元素反应与队伍战斗模拟器。它把“攻击命中”解析为元素附着、元素量消耗、反应事件、周期伤害、草原核和最终攻击伤害，并支持四人队伍、技能/爆发时间轴、冷却、元素能量、元素微粒和单/多目标战斗。

项目不包含《原神》的美术、音频、文本资源或反编译代码。名称仅用于说明兼容的玩法模型。

## 已实现

- 七元素、物理伤害与 0.8 倍附着税
- 元素量线性衰减、同元素刷新、反应消耗倍率
- 蒸发、融化、超载、感电、超导、冻结、碎冰、扩散、结晶
- 燃烧、绽放、超绽放、烈绽放、原激化、超激化、蔓激化
- 感电、燃烧、冻结、原激化的持续状态和时间推进
- 6 秒草原核、场上至多 5 个、生成第 6 个时引爆最早核心
- 增幅、剧变、激化、结晶盾、抗性与防御公式
- 普通 `3 hit / 2.5 s` 元素附着 ICD 工具
- 月感电、月绽放、月结晶的共用直接/间接伤害公式（角色天赋转换由上层启用）
- 数据驱动的角色、元素战技与元素爆发多段时间轴
- 四人自动轮转、1 秒换人冷却、动作锁、技能就绪即施放
- 爆发冷却与能量双重门槛、前后台/同异色微粒充能
- 单目标与全体目标攻击、每个敌人独立附着和 ICD
- CMake 工程、命令行示例和零依赖测试程序

队伍模拟见 [docs/BATTLE_SIMULATOR.zh-CN.md](docs/BATTLE_SIMULATOR.zh-CN.md)，基础使用和排障见 [docs/USER_GUIDE.zh-CN.md](docs/USER_GUIDE.zh-CN.md)，录制三段真实终端演示见 [docs/RECORDING_GUIDE.zh-CN.md](docs/RECORDING_GUIDE.zh-CN.md)，上传仓库见 [docs/GITHUB_UPLOAD.zh-CN.md](docs/GITHUB_UPLOAD.zh-CN.md)，完整规则、公式、资料依据与复刻边界见 [docs/MECHANICS.zh-CN.md](docs/MECHANICS.zh-CN.md)。

## 构建

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

运行四人队伍示例：

```powershell
.\build\elemental_team_demo.exe 30 3 Xiangling Xingqiu Kaeya Sucrose
```

也可以使用录屏友好的预设和按时间回放：

```powershell
.\build\elemental_team_demo.exe --preset classic --duration 12 --enemies 2 --replay-speed 2 --compact
.\build\elemental_team_demo.exe --preset hyperbloom --duration 10 --enemies 1 --replay-speed 2 --compact
```

位置参数依次是战斗秒数、敌人数和四名角色。演示名册包含 `Xiangling`、`Xingqiu`、`Kaeya`、`Sucrose`、`Nahida`、`KukiShinobu`、`Yaoyao`；公共 API 可以传入任意四个自定义角色定义。

也可以使用 Visual Studio 生成器：

```powershell
cmake -S . -B build-vs -G "Visual Studio 17 2022"
cmake --build build-vs --config Release
ctest --test-dir build-vs -C Release --output-on-failure
```

本工作区的 `.tools/` 内已准备便携式 w64devkit；该目录被 Git 忽略，不属于项目发布内容。

## 最小示例

```cpp
#include "elemental/reaction.hpp"

elemental::ReactionEngine engine;
elemental::TargetState enemy;

elemental::ActorStats hydro{"hydro", 90, 0.0};
elemental::ActorStats pyro{"pyro", 90, 180.0};

elemental::Attack wet;
wet.element = elemental::Element::hydro;
wet.gauge_units = 2.0;
wet.source = hydro;
(void)engine.apply(enemy, wet);

elemental::Attack hit;
hit.element = elemental::Element::pyro;
hit.gauge_units = 1.0;
hit.source = pyro;
hit.base_damage = 10'000.0;

const elemental::Resolution result = engine.apply(enemy, hit);
// result.events[0].reaction == Reaction::vaporize
// result.attack_multiplier 是精通修正后的蒸发倍率
// result.attack_damage 是包含 DEF/RES/反应倍率的确定性伤害
```

周期状态由调用方推进：

```cpp
const auto periodic_events = engine.advance(enemy, 1.0);
```

草原核与目标分离发生在真实游戏世界中；这个单目标核心采用显式接口，便于上层先做范围查询：

```cpp
const auto events = engine.trigger_dendro_cores(
    enemy, elemental::Element::electro, pyro, 2);
```

## 工程结构

```text
include/elemental/reaction.hpp  公共 API
include/elemental/battle.hpp    队伍战斗模拟 API
src/reaction.cpp                状态机和数值公式
src/battle.cpp                  自动轮转、能量和多目标调度
examples/demo.cpp               可按时间回放的单目标蒸发示例
examples/team_simulation.cpp    经典队和超绽放队命令行示例
tests/reaction_tests.cpp        机制回归测试
tests/battle_tests.cpp          队伍模拟回归测试
docs/MECHANICS.zh-CN.md         中文机制说明与实现边界
docs/USER_GUIDE.zh-CN.md        中文操作说明书
docs/BATTLE_SIMULATOR.zh-CN.md  队伍战斗模拟说明
docs/RECORDING_GUIDE.zh-CN.md    三段终端演示录制流程
docs/GITHUB_UPLOAD.zh-CN.md      GitHub 上传步骤与排障
```
