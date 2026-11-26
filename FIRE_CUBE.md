# 火焰方块设计规格

> **范围**：火焰方块本体 + 可复用的单侧火舌 Actor。
> **不包含**：联动效果（火+雷=爆炸、火+自然=灼烧）、玩家侧属性效果、火焰 / 火星特效、UI。
> **状态**：已落地（2026-11-24），编译通过；属性系统与联动仍未接入。

---

## 0. 术语

| 术语 | 含义 |
| --- | --- |
| 初始方块 | `ACS_CubeProjectile`，被投掷的基础方块本体 |
| 火焰方块 | `ACS_FireCubeProjectile : ACS_CubeProjectile` |
| 面组 | 一对相对面（上下 / 左右 / 前后）；同一组两面完全相同 |
| 火焰轴 | 火焰面组当前的世界轴向，决定两条火舌的喷射方向 |
| 火舌 | `ACS_FlameTongue`，一条单侧火焰判定体（一个 Actor） |
| 火焰属性 | 挂在 Actor 上的临时标记，由属性系统维护（当前未接入） |
| 火星 | 火焰属性在敌人身上的表现挂点，不带玩法 |
| 联动 | 两种属性同时存在时触发的效果（爆炸 / 灼烧 / 导电），当前不处理 |

---

## 1. 定位与边界

### 1.1 需求来源

- 图 1：初始方块三个开关；子类方块「选中变色 / 发射挂子特效 / 给敌人挂属性」；火焰分支「发射火焰（相对面发射）→ 撞到敌人保留火焰 → 火焰 DOT」。
- 图 2：火焰方块 → 爆炸 / 灼烧；雷电 → 爆炸 / 导电；自然 → 灼烧 / 导电；「属性给到玩家触发不同效果」。
- 群聊：先做伤害和特效；属性附加统一由一个人写。

### 1.2 当前实现范围

已实现：

- 火焰方块固定占用「上下」面组，沿该面组当前轴的双侧各生成一条火舌；
- 火舌按 `DamageInterval` 检测，范围内敌人结算火焰伤害；敌人只需要进入范围；
- 命中后广播「火焰属性 / 火星」事件，并打 `LogCS_Fire` 日志；
- 方块本体不推敌、不进入模拟物理（两个开关固定关闭）；
- 命中 / 收回完全沿用基类，火焰侧不覆写。

未实现：

- 火焰属性的存储、刷新、移除（属性系统未定）；
- 灼烧 DOT、爆炸、连锁、导电、中毒、减速等联动；
- 玩家侧属性效果；
- 火舌 / 火星特效（由 `BP_FlameTongue` 与表现层负责）。

### 1.3 职责边界

| 模块 | 负责 | 不负责 |
| --- | --- | --- |
| 火焰方块 | 算轴、生成 / 销毁火舌、转发事件、关两个开关 | 属性存续、联动结算、命中收回（基类） |
| 火舌 Actor | 单条范围判定、按间隔伤害、`OnFlameHit` | 属性系统、视觉 |
| 基类 | 飞行、碰撞、命中、收回 | 火焰判定 |
| 属性系统 | 火焰属性 / 火星的存续与刷新（未接入） | 谁触发 |
| 联动系统 | 火+雷 / 火+自然 / 雷+自然（未实现） | 属性挂载 |
| 表现层 | 选中变色、火舌 / 火星 Niagara | 判定 |
| UI | 方块与面组颜色 | 方向计算 |

---

## 2. 面组与方向

### 2.1 占用面组

火焰固定占用「上下」组（±Z），两面都是火焰面。等旋转数据层的 `GetFaceAxis(UpDown)` 就绪后，直接用它替换回退逻辑。

### 2.2 方向的唯一来源

```cpp
// 有覆盖值时用覆盖值（控制器 / 旋转系统设置）
// 否则取方块世界 +Z 的主轴，保证结果是 ±X / ±Y / ±Z
FVector FireAxis = FireAxisOverride.IsNearlyZero()
    ? SnapToDominantAxis(GetActorUpVector())
    : FireAxisOverride.GetSafeNormal();
```

- 当前 HEAD 没有旋转组件，所以先用「方块上轴 + `SetFireAxisOverride`」兜底；
- 旋转系统就绪后，控制器在发射前把 `GetFaceAxis(UpDown)` 传给 `SetFireAxisOverride` 即可，火焰侧不用改；
- 取值只可能是 ±X / ±Y / ±Z 之一（90° 整数旋转，无浮点误差）。

### 2.3 六种姿态下的火焰轴向

| 姿态 | 到达方式 | 火焰面组所在轴 | 火焰轴 |
| --- | --- | --- | --- |
| S0 | 起始 | +Z | 上下 |
| S1 | Q → E → Q | +Z | 上下 |
| S2 | E | +Y | 左右 |
| S3 | Q → E | +X | 前后 |
| S4 | E → Q | +Y | 左右 |
| S5 | Q | +X | 前后 |

> 双侧喷火，表中只关心轴，不需要正负号。

---

## 3. 行为

### 3.1 总览

```text
选中火焰 → 投掷 → 方块飞行 + 生成两条火舌 → 范围内敌人按间隔受伤 + 火星事件 → 基类收回
```

### 3.2 投掷与命中

- 由 `ACS_PlayerController` 的左键 → 攻击蒙太奇 → `OnAttackThrowNotify` 生成，与初始方块相同；
- 火焰方块在构造和 `BeginPlay` 里强制：

```cpp
bEnablePhysicsOnImpact = false;   // 进入模拟物理
bEnableEnemyKnockback = false;    // 是否弹开敌人
```

- **命中后的收回 / 穿透完全由基类决定**，火焰侧不覆写 `ProcessEnemyHit`、不调用 `StartRecall`；
- 当前基类代码撞敌是穿透继续飞；如果基类侧以后改成「命中收回」，火焰侧无需改动。

### 3.3 火舌（已定：一个 Actor = 一条单侧火舌）

- 方块在 `BeginPlay` 生成 **两条** `ACS_FlameTongue`：
  - 方向 `+FireAxis` 与 `-FireAxis`；
  - 生成点 = 方块中心 ± `FlameMuzzleOffset`（0 时自动取 `CubeSize / 2`），从方块表面喷出；
  - 两条都 `AttachToActor(方块)`，跟随方块移动。
- 每条火舌：
  - 本地 `+X` 为喷射方向，喷口在 Actor 原点；
  - 一个胶囊作几何体（`NoCollision`），每 `DamageInterval` 用 `OverlapMultiByObjectType(ECC_Pawn)` 查一次；
  - 检测到的 `ACS_EnemyCharacter` 结算 `FlameDamage`，`DamageCauser = 火舌`；
  - 同一条火舌内，同一敌人每跳只结算一次（`TSet` 去重）。
- 喷火时长 `Duration`（<= 0 表示一直喷到外部停）；方块进入 `Returning / Recalled` 时停止（`bSprayWhileReturning` 可放开），方块销毁时一并销毁。

### 3.4 伤害、属性与火星

- **伤害归属**：方块本体不结算伤害；伤害由火舌结算，`DamageCauser` 是火舌 Actor，不是方块碰撞。
- **伤害节奏**：每 `DamageInterval` 对范围内每个敌人结算一次 `FlameDamage`；间隔与伤害都可在蓝图里调。
- **命中事件**：`OnFireHit(Enemy, bFirstHit, Damage)`；同一敌人第一次被本次投掷命中时 `bFirstHit = true`。
- **属性 / 火星**：每次命中广播 `OnEmberAttached(Enemy, bFirstHit)`；表现层据此挂 / 刷新火星。属性系统就绪后，在 `ApplyFireAttribute` 里替换成真正的 `ApplyElement` 调用。
- **日志**：`LogCS_Fire` 输出命中、首次标记、`AttributeDuration`，方便联调。
- 属性系统不存在时：只打日志，伤害照常结算，不崩溃。

### 3.5 方块本体

- 只改两个开关：`bEnablePhysicsOnImpact = false`、`bEnableEnemyKnockback = false`；
- 其余（碰撞、飞行、命中、收回）全部沿用基类；
- 方块本体不结算伤害；收回时销毁两条火舌。

---

## 4. 代码关系

### 4.1 `ACS_FlameTongue`（`Source/.../Public/Actor/CS_FlameTongue.h`）

- 一个 Actor = 一条单侧火舌，不依赖 `ACS_FireCubeProjectile`，可被陷阱 / 武器 / Boss 复用；
- 对外：`InitFlameTongue(Source, Instigator, Direction, Settings)`、`SetDirection`、`StartFlame`、`StopFlame`、`OnFlameHit`；
- 视觉：C++ 只有胶囊判定；`BP_FlameTongue` 子类挂 Niagara / 音效；
- 参数：`FCSFlameTongueSettings`（间隔、伤害、长度、半径、持续、属性时长）。

### 4.2 `ACS_FireCubeProjectile`（`Source/.../Public/Projectile/CS_FireCubeProjectile.h`）

- 继承 `ACS_CubeProjectile`，只新增火舌生成 / 销毁 / 事件转发；
- 不覆写基类的命中 / 收回；
- 生成两条火舌（±轴），`FlameTongueClass` 为空时用 C++ 的 `ACS_FlameTongue`；
- 基类进入 `Returning / Recalled` 时停火；`EndPlay` 销毁两条火舌。

### 4.3 与基类钩子的关系

- 基类的 `OnEnemyHitEffect` 是给雷劈 / 自然方块用的；火焰**不使用**它；
- 火焰的伤害由独立火舌结算，和方块的命中流程解耦；
- 基类已有的 `bEnablePhysicsOnImpact`、`bEnableEnemyKnockback` 由火焰子类关闭。

---

## 5. 参数（全部蓝图可调，数值由你调）

| 参数 | 说明 |
| --- | --- |
| `FlameTongueClass` | 火舌类；建议指向 `BP_FlameTongue` |
| `FlameSettings.DamageInterval` | 伤害检测间隔 |
| `FlameSettings.FlameDamage` | 每次检测的单次火焰伤害 |
| `FlameSettings.Length` | 单条火舌长度（cm） |
| `FlameSettings.Radius` | 火舌半径（cm） |
| `FlameSettings.Duration` | 单次喷火持续时间；<= 0 表示一直喷到停 |
| `FlameSettings.AttributeDuration` | 火焰属性 / 火星持续时间 |
| `FlameMuzzleOffset` | 火舌生成点离方块中心的距离；0 = 自动取 `CubeSize / 2` |
| `bSprayWhileReturning` | 收回期间是否继续喷火 |
| `bEnablePhysicsOnImpact` | **固定 false** |
| `bEnableEnemyKnockback` | **固定 false** |

> 伤害只由火舌结算，方块本体不造成伤害；敌人进入范围即可受伤。

---

## 6. 接口与事件

| 名称 | 方向 | 用途 |
| --- | --- | --- |
| `GetFireAxis()` | 输出 | 当前火焰轴（UI / 调试 / 特效） |
| `SetFireAxisOverride(Axis)` | 调用 | 旋转系统 / 控制器设置火焰轴，并同步已有火舌 |
| `OnFireSprayStarted` | 广播 | 两条火舌生成并开始喷火 |
| `OnFireHit(Enemy, bFirstHit, Damage)` | 广播 | 每次火舌伤害 |
| `OnEmberAttached(Enemy, bFirstHit)` | 广播 | 火星挂点 / 刷新（表现层） |
| `ACS_FlameTongue::OnFlameHit(Enemy, bFirstHit, Damage)` | 广播 | 单条火舌的命中，可单独复用 |
| `ApplyElement(...)` | 调用 | 属性系统接入点（待定） |

---

## 7. 联动对照表（只记录，当前不实现）

| 已有属性 | 新属性 | 联动 | 来源 |
| --- | --- | --- | --- |
| 雷 | 火 | 爆炸（范围内连锁） | 图 2 |
| 自然 | 火 | 灼烧（火焰 DOT） | 图 2 |
| 火 | 雷 | 爆炸 | 图 2 |
| 自然 | 雷 | 导电（路径僵直） | 图 2 |
| 火 | 自然 | 灼烧 | 图 2 |
| 雷 | 自然 | 导电 | 图 2 |

> 火焰方块只负责广播火焰属性 / 火星事件；反应由联动系统单独做。

---

## 8. 验收标准与测试用例

| # | 用例 | 预期 |
| --- | --- | --- |
| 1 | 编译 | `Result: Succeeded`（2026-11-24 已通过） |
| 2 | 投掷 | 生成两条火舌，方向 ± 火焰轴，跟随方块 |
| 3 | 进入范围 | 按 `DamageInterval` 持续掉 `FlameDamage`，离开停止 |
| 4 | 参数调整 | 改间隔 / 伤害 / 长度后下一跳生效 |
| 5 | 重复命中 | `OnFireHit` / `OnEmberAttached` 照常广播，`bFirstHit` 只在首次为 true |
| 6 | 收回 | 火舌停止，方块销毁时一并销毁，无残留 timer |
| 7 | 两个开关 | 不推敌、撞墙不转刚体（走基类 Blocked → 收回） |
| 8 | 本体伤害 | 方块碰撞本身不造成伤害；伤害只来自火舌 |

---

## 9. 待确认

1. **属性系统归属**：`ApplyElement / HasElement` 由谁实现？当前 `ApplyFireAttribute` 只打日志。
2. **玩家侧效果**：火焰属性给玩家触发什么？（先留接口与事件）
3. **联动矩阵**：第 7 节的「火 + 雷 = 爆炸 / 火 + 自然 = 灼烧 / 雷 + 自然 = 导电」是否正确？
4. **自动收回**：当前基类撞敌是穿透继续飞；如果基类侧要改成命中收回，需要基类负责人确认。

---

## 10. 风险与依赖

- **旋转数据层缺失**：当前 HEAD 没有 `CS_CubeRotationComponent`，火焰轴先取方块上轴；旋转层就绪后通过 `SetFireAxisOverride` 接入，火焰侧不改。
- **基类命中行为**：当前基类撞敌是穿透继续飞，不主动收回；如果预期是「命中收回」，属于基类侧改动。
- **BP 旧属性**：`BP_FireCube` 里可能还留着 `BurnTotalDamage / BurnDuration / BurnTickInterval`，重编译蓝图会清掉。
- **火舌视觉**：C++ 没有网格 / Niagara；需要在 `BP_FlameTongue` 里挂。
- **伤害归因**：`DamageCauser = 火舌`；击杀统计如果按 Causer 找玩家，需要改用 Instigator 或额外标记。
- **性能**：每次投掷生成两个火舌 Actor；每个火舌一个 timer。火墙数量多时再考虑对象池。
- **构建环境**：编译前必须关 UnrealEditor；UBT 需要写 `%LOCALAPPDATA%\UnrealBuildTool`，在受限沙箱下要放行。

---

## 附录 A：与 `CUBE_ROTATION.md` 的衔接

- 面组枚举、槽位、`GetFaceAxis` 直接复用；
- 火焰只读 Slots，不写，不引入第二套朝向；
- 旋转层就绪前用 `GetActorUpVector()` 主轴 + `SetFireAxisOverride` 兜底。

---

## 附录 B：BP 接线与测试步骤

1. 打开项目，新建 Blueprint Class，父类选 `ACS_FlameTongue`，命名 `BP_FlameTongue`（放在 `Content/Projectile/` 或 `Content/Actor/`）；
2. 在 `BP_FlameTongue` 里加 Niagara / 音效；可把 Niagara 挂在 Root 下；
3. 打开 `BP_FireCube`：
   - 把 `FlameTongueClass` 指向 `BP_FlameTongue`；
   - 调 `FlameSettings`（间隔 / 伤害 / 长度 / 半径 / 持续）；
   - 重编译蓝图，清掉旧的 Burn 参数；
4. PIE 测试：
   - 丢出火焰方块，观察两条火舌；
   - 敌人进入范围，看 `LogCS_Fire` 与血条；
   - 走远 / 撞墙触发收回，确认火舌停止、没有残留；
   - 调 `bSprayWhileReturning`，确认收回期间是否继续喷。

> 如果暂时不建 `BP_FlameTongue`，`FlameTongueClass` 留空会用 C++ 的 `ACS_FlameTongue`，逻辑能跑，但没有视觉。
