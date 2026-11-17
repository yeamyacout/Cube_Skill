# Cube_Skill

Unreal Engine 5.8 项目（C++ + Blueprint）。

## 环境要求

| 依赖 | 版本 |
| --- | --- |
| Unreal Engine | 5.8（安装路径例：`D:\UE_5.8`） |
| JetBrains Rider | 2026.2.4 或更新 |
| Git | 任意近期版本 |

> **重要**：Rider 版本必须与 `Plugins/Developer/RiderLink/RiderLink.uplugin` 里的
> `VersionName` 一致（当前为 `2026.2.4`）。Rider 大版本升级后，请按下面
> 「升级 Rider 后」一节更新 RiderLink。

## 首次克隆

```powershell
git clone <repo-url> Cube_Skill
cd Cube_Skill
```

**不需要**额外安装 RiderLink——它已经随仓库提交（`Plugins/Developer/RiderLink/`）。
克隆后直接生成工程文件并编译：

```powershell
# 生成 Rider / VS 工程文件
"D:\UE_5.8\Engine\Build\BatchFiles\Build.bat" -projectfiles -project="<绝对路径>\Cube_Skill.uproject" -game -rocket -progress

# 编译 Editor 目标
"D:\UE_5.8\Engine\Build\BatchFiles\Build.bat" Cube_SkillEditor Win64 Development -Project="<绝对路径>\Cube_Skill.uproject" -WaitMutex
```

然后双击 `Cube_Skill.uproject` 打开编辑器。

## 在 Rider 中打开

1. 先完成上面的「生成工程文件 + 编译」。
2. Rider → `Open` → 选择 `Cube_Skill.uproject`（或 `Cube_Skill.sln`）。
3. Rider 右下角应显示 **已连接到 Unreal Editor**；蓝图索引、Live Coding、
   调试器、断点等功能依赖这个连接。

## 升级 Rider 后如何更新 RiderLink

RiderLink 是 JetBrains 随 IDE 分发的插件，**版本必须匹配本地 Rider**。
从本机 Rider 安装目录重新导出即可，无需联网下载：

```powershell
# 1. 找到你的 Rider 安装目录（含 build.txt 的那一层）
$rider = "D:\JetBrains Rider 2026.2.4"

# 2. 删除旧的 RiderLink（只删源码，编译产物会被下面步骤重新生成）
Remove-Item -Recurse -Force "Plugins\Developer\RiderLink"

# 3. 从 Rider 自带压缩包解压出匹配版本
Add-Type -AssemblyName System.IO.Compression.FileSystem
New-Item -ItemType Directory -Force -Path "Plugins\Developer\RiderLink" | Out-Null
[System.IO.Compression.ZipFile]::ExtractToDirectory(
    "$rider\plugins\UnrealLink\EditorPlugin\RiderLink.zip",
    "Plugins\Developer\RiderLink")
```

4. 重新编译 Editor 目标（见上）。
5. 提交 `Plugins/Developer/RiderLink/Source/**` 与 `RiderLink.uplugin`，
   **不要**提交 `Binaries/`、`Intermediate/`（`.gitignore` 已处理）。

## 为什么 RiderLink 要提交进仓库

以前项目用的是「每台机器自己装一份 RiderLink」的方案（`Plugins/Developer/RiderLink/`
被 `.gitignore` 忽略）。问题是：

- 任何一次 `git clean -xdf`、换新电脑克隆、或 Rider 重新生成工程时，插件都会**静默消失**；
- 表现就是「Rider 连不上编辑器 / 蓝图索引失效 / 断点不生效」，但编辑器本身照常能跑，
  所以很难排查；
- 一台机器 Rider 升级后版本与其他人不一致，也会导致同样的症状。

因此现在改为**提交 RiderLink 源码**。构建产物仍按机器各自生成，互不影响。

## 蓝图资源冲突

`.uasset` / `.umap` 是二进制文件，Git **无法合并**。多人同时改同一张蓝图时：

- 约定「谁改哪张图」提前沟通；
- 拉取前先提交自己的改动，冲突时优先保留双方→ 逐张手动重建；
- 建议开启 `git config merge.conflictStyle diff3` 并尽量把工作拆到不同资源文件。

## 目录说明

| 目录 | 是否提交 | 说明 |
| --- | --- | --- |
| `Source/` | ✅ | C++ 源码 |
| `Content/` | ✅ | 蓝图、地图、资源 |
| `Config/` | ✅ | 项目配置 |
| `Plugins/Developer/RiderLink/` | ✅ 源码 | Rider 集成插件 |
| `Binaries/`, `Intermediate/` | ❌ | 每台机器各自编译 |
| `Saved/`, `DerivedDataCache/` | ❌ | 本地缓存与日志 |
| `.idea/`, `.vs/` | ❌ | 本机 IDE 设置 |
