// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CS_CubeRotationComponent.generated.h"

/**
 * 立方体的三组「相对面」
 *
 * 每组包含两个相对的面，这两面完全相同。
 * 所以「朝左」和「朝右」是同一组面，只是同一组的两个侧面。
 */
UENUM(BlueprintType)
enum class ECS_CubeFaceGroup : uint8
{
	/** 占据本地 ±Z（顶面 / 底面） */
	UpDown      UMETA(DisplayName = "上下"),

	/** 占据本地 ±X（正面 / 背面） */
	FrontBack   UMETA(DisplayName = "前后"),

	/** 占据本地 ±Y（右面 / 左面） */
	LeftRight   UMETA(DisplayName = "左右")
};

/**
 * 三个槽位：立方体在世界里被三根轴划分出的三个位置
 *
 * 序号与下面的轴一一对应，不要随意调整。
 */
UENUM(BlueprintType)
enum class ECS_CubeSlot : uint8
{
	/** +Z 上 */
	Up      UMETA(DisplayName = "上(+Z)"),

	/** +Y 右 */
	Right   UMETA(DisplayName = "右(+Y)"),

	/** +X 前 */
	Front   UMETA(DisplayName = "前(+X)")
};

/** 逻辑朝向发生变化时广播（按下瞬间就触发，不等动画） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCSOnCubeOrientationChanged);

/**
 * 一个待播放的翻转增量（表现层专用）
 */
struct FCSFlipIncrement
{
	/** 旋转轴（模型空间的单位向量） */
	FVector Axis = FVector::ForwardVector;

	/** 这次转多少度，带符号 */
	float AngleDeg = 0.f;

	/** 起播时刻（世界时间，秒） */
	float StartTime = 0.f;
};

/**
 * UCS_CubeRotationComponent - 方块翻转
 *
 * ============ 职责划分 ============
 *
 * 本组件同时包含两层，但两者互不依赖：
 *
 *   数据逻辑层（唯一权威，按键瞬间就更新，不等待动画）
 *     Slots[3] 记录三根轴各被哪一组面占据，是 A/B/C 的一个排列，共 6 种。
 *     用整数而不是四元数存，所以没有浮点误差，也就不需要"吸附到 90°"。
 *     能力、发射结算全部只读这一层。
 *
 *   显示层（纯表现，慢一点/跳过/直接跳到终点都不影响判定）
 *     每次按键生成一个 90° 增量，各自带独立的起始时刻；
 *     多个增量并行播放、按按下顺序左乘，合成当前显示姿态 DisplayQuat。
 *     外部（UI 方块、世界里的展示物）每帧读取 GetDisplayQuat() 即可。
 *
 * ============ 为什么不做成 SceneComponent ============
 *
 * 它不参与任何空间变换：显示姿态是给外部载体用的，组件自己不需要 Transform。
 * 做成普通 UActorComponent 可以让使用方自由决定把方块画在哪里。
 *
 * ============ 典型用法 ============
 *
 *   C++：
 *     CubeRotation->FlipRight();                                  // E
 *     CubeRotation->FlipForward();                                // Q
 *     CubeRotation->IsFacePointingTo(ECS_CubeFaceGroup::UpDown, FVector::ForwardVector);
 *     Mesh->SetRelativeRotation(CubeRotation->GetDisplayQuat());  // 显示载体每帧同步
 *
 *   蓝图：
 *     Add Component -> CS Cube Rotation，参数在细节面板里调。
 */
UCLASS(ClassGroup = (CS), meta = (BlueprintSpawnableComponent), DisplayName = "CS 方块翻转")
class CUBE_SKILL_API UCS_CubeRotationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCS_CubeRotationComponent();

	// ---------- 输入 ----------

	/**
	 * 向右翻 90°（E）
	 *
	 * 数据层立刻把「上」和「右」两个槽位对换；显示层排入一个绕 +X 的旋转增量。
	 * 同一个键连按 N 次，显示层会连续转 N 个 90°，不会在中途停下。
	 */
	UFUNCTION(BlueprintCallable, Category = "CS|Cube")
	void FlipRight();

	/**
	 * 向前翻 90°（Q）
	 *
	 * 数据层立刻把「上」和「前」两个槽位对换；显示层排入一个绕 +Y 的旋转增量。
	 */
	UFUNCTION(BlueprintCallable, Category = "CS|Cube")
	void FlipForward();

	/**
	 * 开关翻转输入
	 *
	 * 关闭期间按键直接丢弃，不排队、不影响已经在播的动画。
	 * 例如方块飞行、回收期间由外部关掉。
	 */
	UFUNCTION(BlueprintCallable, Category = "CS|Cube")
	void SetFlipEnabled(bool bEnabled) { bFlipEnabled = bEnabled; }

	UFUNCTION(BlueprintPure, Category = "CS|Cube")
	bool IsFlipEnabled() const { return bFlipEnabled; }

	/** 回到初始排列（上=上下、右=左右、前=前后），并清空所有待播动画 */
	UFUNCTION(BlueprintCallable, Category = "CS|Cube")
	void ResetOrientation();

	/**
	 * 立刻结束所有待播动画，直接落到最终姿态
	 *
	 * 用于"抛出瞬间按最终结果算"这类场合：数据层本来就已经是最终结果，
	 * 这里只是让显示姿态追上它，避免 UI 与实际状态不一致。
	 */
	UFUNCTION(BlueprintCallable, Category = "CS|Cube")
	void FinishPendingImmediately();

	// ---------- 数据层查询 ----------

	/** 某个槽位被哪一组面占据 */
	UFUNCTION(BlueprintPure, Category = "CS|Cube|Query")
	ECS_CubeFaceGroup GetFaceOnSlot(ECS_CubeSlot Slot) const;

	/** 这组面现在朝向哪个方向（返回所在轴的正方向，如 +Z/+Y/+X） */
	UFUNCTION(BlueprintPure, Category = "CS|Cube|Query")
	FVector GetFaceForward(ECS_CubeFaceGroup Face) const;

	/** 朝向指定方向的是哪一组面（方向正负不敏感，因为相对面相同） */
	UFUNCTION(BlueprintPure, Category = "CS|Cube|Query")
	ECS_CubeFaceGroup GetFacePointingTo(const FVector& Direction) const;

	/** 这组面是否正朝向指定方向（能力结算用这个） */
	UFUNCTION(BlueprintPure, Category = "CS|Cube|Query")
	bool IsFacePointingTo(ECS_CubeFaceGroup Face, const FVector& Direction) const;

	/**
	 * 当前排列的可读描述，例如 "上=上下 右=左右 前=前后"
	 *
	 * 调试时直接 Print String 出来看，比读 Slots 数组直观。
	 */
	UFUNCTION(BlueprintPure, Category = "CS|Cube|Query")
	FString DescribeSlots() const;

	// ---------- 显示层 ----------

	/** 当前显示姿态（供 UI / 展示物直接使用） */
	UFUNCTION(BlueprintPure, Category = "CS|Cube|Display")
	FQuat GetDisplayQuat() const { return DisplayQuat; }

	/** 当前显示姿态的 Rotator 形式（给只能吃 Rotator 的接口用） */
	UFUNCTION(BlueprintPure, Category = "CS|Cube|Display")
	FRotator GetDisplayRotator() const { return DisplayQuat.Rotator(); }

	/** 是否还有待播动画 */
	UFUNCTION(BlueprintPure, Category = "CS|Cube|Display")
	bool IsAnimating() const { return Pending.Num() > 0; }

	/** 逻辑朝向变化时广播（按下瞬间触发，不等动画播完） */
	UPROPERTY(BlueprintAssignable, Category = "CS|Cube")
	FCSOnCubeOrientationChanged OnOrientationChanged;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ---------- 内部 ----------

	/** 排入一个增量，并在需要时启动 Tick */
	void QueueIncrement(const FVector& Axis, float AngleDeg);

	/** 把一个已播完的增量并入基准姿态 */
	void CommitIncrement(const FCSFlipIncrement& Increment);

	/** 数据层：对换两个槽位 */
	void SwapSlots(int32 SlotA, int32 SlotB);

	/** 表现层：重算 DisplayQuat */
	void UpdateDisplay(float Now);

	/** 调试打印一行（屏幕 + Output Log） */
	void LogFlip(const TCHAR* FuncName) const;

	/**
	 * 进度曲线：quintic（C2 连续）
	 *
	 * 位置 / 速度 / 加速度都连续，起步与收尾没有加速度跳变；
	 * 无分支、无除法、无内存访问，5 次乘法。
	 */
	static float QuinticCurve(float U);

	/** 槽位对应的世界轴正方向 */
	static FVector SlotAxis(ECS_CubeSlot Slot);

	/** 槽位对应的数组下标 */
	static int32 SlotToIndex(ECS_CubeSlot Slot);

	// ---------- 数据层状态 ----------

	/**
	 * 三个槽位各被哪一组面占据
	 *
	 * 下标 0 = 上(+Z)、1 = 右(+Y)、2 = 前(+X)。
	 * 这是唯一权威，按下按键的同一帧就更新完毕。
	 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "CS|Cube|State")
	TArray<ECS_CubeFaceGroup> Slots;

	// ---------- 显示层参数 ----------

	/** 单个增量从起播到到位的时长（秒），也就是最后一次按键后的收敛时限 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Cube|Display", meta = (ClampMin = "0.01", DisplayName = "单次翻转时长"))
	float SettleTime = 0.2f;

	/**
	 * 右翻的角度
	 *
	 * 负值表示"顶面转到右侧"。如果做成之后发现方向反了，
	 * 把它改成 +90° 即可，不需要改代码。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Cube|Display", DisplayName = "右翻角度(负=顶面转右)")
	float RollStepDeg = -90.f;

	/** 前翻的角度（同理，方向反了改成 -90°） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Cube|Display", DisplayName = "前翻角度")
	float PitchStepDeg = 90.f;

	/** 是否播放动画。关掉则每次翻转瞬间到位（调试用） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Cube|Display")
	bool bAnimate = true;

	/**
	 * 调试开关：每次翻转在屏幕左上角打印一行，同时写 Output Log
	 *
	 * 用来快速区分"按键没进来"和"进来了但画面没同步"：
	 *   按 E 有打印 → 函数被调到了，问题在显示侧
	 *   按 E 没打印 → 按键根本到不了这里，问题在输入侧
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Cube|Debug")
	bool bLogFlips = false;

	// ---------- 显示层运行时状态 ----------

	/** 是否接受翻转输入 */
	bool bFlipEnabled = true;

	/** 待播增量，按入队顺序排列（最早的在最前） */
	TArray<FCSFlipIncrement> Pending;

	/** 已经播完的增量累积出的姿态（表现层的"地基"） */
	FQuat BaseQuat = FQuat::Identity;

	/** 当前显示姿态 = 待播增量的部分旋转 × BaseQuat */
	FQuat DisplayQuat = FQuat::Identity;
};
