# AGENTS.md — Cube_Skill 项目约束

本文件是 AI 助手在本项目中工作时**必须遵守**的规则。
每次收到需求前先读完本文件。

---

# 一、需求处理流程（强制，每次必须执行）

收到任何**新需求**后，严格按以下 4 步执行，**不得跳步、不得边做边问**。

```
收到需求
   ↓
1️⃣ 搜索网上成熟方案
   ↓
2️⃣ 向用户询问细节
   ↓
3️⃣ 展示多种实现方案的优劣
   ↓
4️⃣ 用户确认后才动手
```

## 1️⃣ 搜索网上成熟方案

- 优先查 **UE 官方文档**（`dev.epicgames.com/documentation`）
- 再查 **Epic 论坛 / UE 社区 / GitHub**
- 至少找到 **2–3 个**可行方案
- **记录来源 URL**，后续引用时必须附链接

## 2️⃣ 向用户询问细节

- 用 `ask_user_question` 工具确认**需求边界**
- 明确：参数、预期表现、边界情况、与其他系统的交互
- **不允许自行猜测**用户意图
- 一次问清，不要反复打断

## 3️⃣ 展示多种实现方案的优劣

用**表格**对比，附来源：

| 方案 | 实现方式 | 优点 | 缺点 | 适用场景 |
| --- | --- | --- | --- | --- |
| A | ... | ... | ... | ... |
| B | ... | ... | ... | ... |

- 给出**推荐方案**及理由
- 说明每种方案的**风险**

## 4️⃣ 用户确认后才动手

- **严格等待用户明确选择**后才开始编码
- 不做"先写一半再说"
- 用户说"用 A" → 才实现 A

## 例外：以下情况可直接执行

| 情况 | 说明 |
| --- | --- |
| **修复明确 Bug** | 用户描述了具体现象且原因唯一 |
| **调参数** | 用户明确说"改成 X" |
| **纯代码整理** | 删除无用代码、改注释、格式化 |
| **用户明确说"直接做"** | 用户跳过流程的授权 |

**即使例外，也要先说明你的理解再动手。**

---

# 二、代码规范

## 类名前缀

所有 C++ 类名必须使用 **`CS_`** 前缀（项目缩写）。

```
CS_CubeProjectile
CS_PlayerController
CS_EnemyCharacter
CS_FireCubeProjectile
```

## 文件位置

代码要加入到**符合类型的文件夹**中：

```
Source/Cube_Skill/
├── Public/
│   ├── Projectile/     ← 方块/投射物
│   ├── Player/         ← 玩家控制器
│   ├── Characters/     ← 角色
│   ├── Components/     ← 组件
│   └── Notify/         ← 动画通知
└── Private/            ← 与 Public 镜像
```

## 注释

- 用**中文**注释，说明**为什么**这么做（不是重复代码在做什么）
- 复杂逻辑必须先写思路再写代码
- 保持精简，不写废话

---

# 三、资源命名规范

在 UE 编辑器中创建的资产**必须**使用对应前缀：

| 类型 | 前缀 | 示例 |
| --- | --- | --- |
| 蓝图 | `BP_` | `BP_Player` |
| 控件蓝图 | `WBP_` | `WBP_SkillBar` |
| 蓝图接口 | `BPI_` | `BPI_Damageable` |
| 动画蓝图 | `ABP_` | `ABP_Player` |
| 蒙太奇 | `AM_` | `AM_Cast` |
| 动画序列 | `AS_` | `AS_Idle` |
| 混合空间 | `BS_` | `BS_Locomotion` |
| 材质 | `M_` | `M_Cube` |
| 材质实例 | `MI_` | `MI_Cube_Red` |
| 材质函数 | `MF_` | `MF_Pulse` |
| 贴图 | `T_` | `T_Cube_D` |
| 静态网格 | `SM_` | `SM_Cube` |
| 骨骼网格 | `SK_` | `SK_Player` |
| Niagara | `NS_` | `NS_Hit` |
| 数据表 | `DT_` | `DT_Skill` |
| 枚举 | `E_` | `E_SkillState` |
| 关卡 | `L_` | `L_Test` |
| 输入动作 | `IA_` | `IA_Cast` |
| 输入映射 | `IMC_` | `IMC_Default` |
| 行为树 | `BT_` | `BT_Enemy` |
| 黑板 | `BB_` | `BB_Enemy` |

---

# 四、提交规范

## 提交信息必须写清

- **更新了什么**
- **新增了什么**
- **修复了什么**
- **为什么这么改**

## 提交格式

```
feat(模块): 简短标题（中文）

详细说明：
  · 改动点 1
  · 改动点 2

修复：
  · 问题描述 → 解决方案

新增：
  · 文件/类/资产
```

## 提交前必须

1. **关闭 UnrealEditor**（Live Coding 会锁文件）
2. **编译通过**：`Result: Succeeded`
3. **`git status` 检查**没有遗漏文件

## 不提交的内容

| 目录 | 说明 |
| --- | --- |
| `Binaries/` | 每台机器各自编译 |
| `Intermediate/` | 编译中间产物 |
| `Saved/` | 本地缓存与日志 |
| `DerivedDataCache/` | 派生数据缓存 |
| `.idea/`、`.vs/` | 本机 IDE 设置 |
| `Plugins/EngineWire/` | 本机 MCP 插件 |
| `Docs/` | 本机文档（含 MCP 令牌） |
| `.claude/` | 本机 AI Skill |

---

# 五、目录分工（减少 `.uasset` 冲突）

`.uasset` / `.umap` 是二进制文件，**Git 无法合并**。

| 目录 | 负责人 | 内容 |
| --- | --- | --- |
| `Content/Animation/` | **动画** | 动画蓝图、序列、蒙太奇 |
| `Content/Character/` | **程序** | 角色蓝图 |
| `Content/Player/` | **程序** | 玩家控制器、输入配置 |
| `Content/Map/` | — | 关卡 |

## 蓝图冲突处理

- **提前沟通谁改哪张图**
- 拉取前先提交自己的改动
- 冲突时优先保留双方 → **逐张手动重建**
- 建议 `git config merge.conflictStyle diff3`

---

# 六、环境要求

| 依赖 | 版本 |
| --- | --- |
| Unreal Engine | **5.8**（`D:\UE_5.8`） |
| JetBrains Rider | **2026.2.4+**（必须与 RiderLink 版本一致） |
| Git | 近期版本 |

## 编译命令

```powershell
# 必须先关闭 UnrealEditor
"D:\UE_5.8\Engine\Build\BatchFiles\Build.bat" Cube_SkillEditor Win64 Development `
    -Project="D:\Cube_Skill\Cube_Skill.uproject" -WaitMutex
```

**验证标准**：输出中出现 `Result: Succeeded`

## RiderLink

已随仓库提交（`Plugins/Developer/RiderLink/`）。
Rider 升级后需从 `Rider安装目录\plugins\UnrealLink\EditorPlugin\RiderLink.zip` 重新解压，
详见 `README.md`。

---

# 七、项目需求（游戏设计）

## 玩家能力

- 玩家初始拥有一个方块，**贴在手上插槽**
- **左键丢出方块**，方块向前飞行，可以**推出敌人**（设计上无法直接击杀）
- 方块**距离过远**或**碰到墙** → 自动回到玩家手中
- **击杀敌人可以获取敌人的能力**，通过**方块颜色**区分
- 当**选中某个能力**时，释放能力的**那一面会发出粒子特效**
- 可以通过**旋转方块**决定能力释放朝向（简化为三个面：前后、左右、上下）

## 敌人能力

- 可以让玩家和某个物体或敌人**交换位置**

## UI

- **右下角**显示获得的能力方块，通过**颜色**区分
- 当玩家**选中某个能力**时，手中的方块也会跟着**变颜色**

## 关卡设置

- 方块可以给门上色，上色后可以**传送到该颜色的门后**

---

# 八、开发环境技术说明

## MCP 工具（EngineWire）

- HTTP MCP 服务：`http://127.0.0.1:3000/mcp`
- 需要 `X-MCP-Capability-Token` 请求头
- Token 位置：`Saved/MCP/capability-token`
- **编辑器必须运行**才有 MCP 服务

## 常用 MCP 能力

| 操作 | 能力名 |
| --- | --- |
| 查资产 | `asset.inspect_asset` |
| 创建蓝图 | `blueprint.create_blueprint` |
| 改蓝图变量 | `blueprint.edit_variable` |
| 改图表连线 | `blueprint.edit_graph` |
| 创建 IA 资产 | `manage_networking.configure_input` |
| 查 Niagara 参数 | `manage_effect.get_niagara_info` |
| 保存 | `control_editor.save_all` |

## UE 5.8 注意事项

| 项目 | 说明 |
| --- | --- |
| `TakeDamage` | 需要 4 个参数：`(Damage, FDamageEvent(), Instigator, Causer)` |
| `FDamageEvent` | 需要 `#include "Engine/DamageEvents.h"` |
| `TObjectPtr` | `UPROPERTY` 里用 `TObjectPtr<T>` 而非裸指针 |
| `BlueprintReadOnly` | **不能**用于 private 成员，会编译报错 |
| Niagara Sprite 特效 | 组件 `SetWorldScale3D` **不生效**，需改内部参数 `ScaleSpriteSize.Uniform Curve Scale` |
| `CollisionComponent` | 必须 `SetMobility(Movable)`，否则 `ProjectileMovementComponent` 无法移动 |

---

# 九、验证与交付

## 每次改代码后

1. **编译验证** → 必须看到 `Result: Succeeded`
2. **说明改了什么** → 用中文清楚描述
3. **给出测试步骤** → 用户知道怎么验证
4. **提醒风险** → 如果可能影响其他功能，明确说明

## 不确定时

- **不要猜** → 直接问
- **不要假设** → 查代码或查文档
- **不要硬编码** → 优先用 `UPROPERTY` 暴露到蓝图让用户调
