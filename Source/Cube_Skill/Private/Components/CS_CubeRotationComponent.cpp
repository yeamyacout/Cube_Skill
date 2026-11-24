// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/CS_CubeRotationComponent.h"

#include "Engine/Engine.h"
#include "Engine/World.h"

// ==================== 构造 / 生命周期 ====================

UCS_CubeRotationComponent::UCS_CubeRotationComponent()
{
	// 平时不 Tick：只有存在待播动画时才打开，播完立刻关掉。
	// 这个组件每帧的开销因此只在"正在翻"的那零点几秒里存在。
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// 初始排列：上 = 上下组、右 = 左右组、前 = 前后组
	Slots.Reset();
	Slots.Add(ECS_CubeFaceGroup::UpDown);
	Slots.Add(ECS_CubeFaceGroup::LeftRight);
	Slots.Add(ECS_CubeFaceGroup::FrontBack);
}

void UCS_CubeRotationComponent::BeginPlay()
{
	Super::BeginPlay();

	// 允许在细节面板里改初始排列，但必须是合法的三排列；否则回退到默认。
	if (Slots.Num() != 3)
	{
		ResetOrientation();
	}
	else
	{
		BaseQuat = FQuat::Identity;
		DisplayQuat = FQuat::Identity;
		Pending.Reset();
	}

	SetComponentTickEnabled(false);
}

// ==================== 输入 ====================

void UCS_CubeRotationComponent::FlipRight()
{
	if (!bFlipEnabled)
	{
		if (bLogFlips)
		{
			LogFlip(TEXT("FlipRight IGNORED (disabled)"));
		}
		return;
	}

	// 数据层：上 ↔ 右
	SwapSlots(SlotToIndex(ECS_CubeSlot::Up), SlotToIndex(ECS_CubeSlot::Right));

	// 显示层：绕模型 +X 转一个 90°
	QueueIncrement(FVector::ForwardVector, RollStepDeg);

	if (bLogFlips)
	{
		LogFlip(TEXT("FlipRight"));
	}
}

void UCS_CubeRotationComponent::FlipForward()
{
	if (!bFlipEnabled)
	{
		if (bLogFlips)
		{
			LogFlip(TEXT("FlipForward IGNORED (disabled)"));
		}
		return;
	}

	// 数据层：上 ↔ 前
	SwapSlots(SlotToIndex(ECS_CubeSlot::Up), SlotToIndex(ECS_CubeSlot::Front));

	// 显示层：绕模型 +Y 转一个 90°
	QueueIncrement(FVector::RightVector, PitchStepDeg);

	if (bLogFlips)
	{
		LogFlip(TEXT("FlipForward"));
	}
}

void UCS_CubeRotationComponent::ResetOrientation()
{
	Slots.Reset();
	Slots.Add(ECS_CubeFaceGroup::UpDown);
	Slots.Add(ECS_CubeFaceGroup::LeftRight);
	Slots.Add(ECS_CubeFaceGroup::FrontBack);

	Pending.Reset();
	BaseQuat = FQuat::Identity;
	DisplayQuat = FQuat::Identity;

	SetComponentTickEnabled(false);
	OnOrientationChanged.Broadcast();
}

void UCS_CubeRotationComponent::FinishPendingImmediately()
{
	if (Pending.Num() == 0)
	{
		return;
	}

	// 待播增量本来就是"一定会生效"的，这里只是让显示姿态一次走完
	for (const FCSFlipIncrement& Increment : Pending)
	{
		CommitIncrement(Increment);
	}

	Pending.Reset();
	DisplayQuat = BaseQuat;

	SetComponentTickEnabled(false);
}

// ==================== 数据层查询 ====================

ECS_CubeFaceGroup UCS_CubeRotationComponent::GetFaceOnSlot(ECS_CubeSlot Slot) const
{
	const int32 Index = SlotToIndex(Slot);
	if (Slots.IsValidIndex(Index))
	{
		return Slots[Index];
	}

	return ECS_CubeFaceGroup::UpDown;
}

FVector UCS_CubeRotationComponent::GetFaceForward(ECS_CubeFaceGroup Face) const
{
	const int32 Index = Slots.IndexOfByKey(Face);
	if (Index == INDEX_NONE)
	{
		return FVector::ZeroVector;
	}

	return SlotAxis(static_cast<ECS_CubeSlot>(Index));
}

ECS_CubeFaceGroup UCS_CubeRotationComponent::GetFacePointingTo(const FVector& Direction) const
{
	// 只比较绝对值：相对面完全相同，所以 +Y 和 -Y 得到同一组面
	const FVector Dir = Direction.GetSafeNormal();
	const float AbsX = FMath::Abs(Dir.X);
	const float AbsY = FMath::Abs(Dir.Y);
	const float AbsZ = FMath::Abs(Dir.Z);

	// 取绝对值最大的那根轴；零向量默认落到"上"
	int32 Index = SlotToIndex(ECS_CubeSlot::Up);
	if (AbsY >= AbsX && AbsY >= AbsZ)
	{
		Index = SlotToIndex(ECS_CubeSlot::Right);
	}
	else if (AbsX >= AbsY && AbsX >= AbsZ)
	{
		Index = SlotToIndex(ECS_CubeSlot::Front);
	}

	return Slots.IsValidIndex(Index) ? Slots[Index] : ECS_CubeFaceGroup::UpDown;
}

bool UCS_CubeRotationComponent::IsFacePointingTo(ECS_CubeFaceGroup Face, const FVector& Direction) const
{
	return GetFacePointingTo(Direction) == Face;
}

FString UCS_CubeRotationComponent::DescribeSlots() const
{
	auto FaceName = [](ECS_CubeFaceGroup Face) -> const TCHAR*
	{
		switch (Face)
		{
		case ECS_CubeFaceGroup::UpDown:    return TEXT("上下");
		case ECS_CubeFaceGroup::FrontBack: return TEXT("前后");
		case ECS_CubeFaceGroup::LeftRight: return TEXT("左右");
		default:                           return TEXT("?");
		}
	};

	if (Slots.Num() != 3)
	{
		return TEXT("<未初始化>");
	}

	return FString::Printf(TEXT("上=%s 右=%s 前=%s"),
		FaceName(Slots[0]), FaceName(Slots[1]), FaceName(Slots[2]));
}

void UCS_CubeRotationComponent::LogFlip(const TCHAR* FuncName) const
{
	const FString Message = FString::Printf(TEXT("[CubeRotation] %s | %s"), FuncName, *DescribeSlots());

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, Message);
	}

	UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);
}

// ==================== 表现层 ====================

void UCS_CubeRotationComponent::QueueIncrement(const FVector& Axis, float AngleDeg)
{
	OnOrientationChanged.Broadcast();

	if (!bAnimate || SettleTime <= KINDA_SMALL_NUMBER)
	{
		// 不做动画：增量立刻并入基准姿态
		FCSFlipIncrement Immediate;
		Immediate.Axis = Axis;
		Immediate.AngleDeg = AngleDeg;
		CommitIncrement(Immediate);

		DisplayQuat = BaseQuat;
		SetComponentTickEnabled(false);
		return;
	}

	FCSFlipIncrement Increment;
	Increment.Axis = Axis;
	Increment.AngleDeg = AngleDeg;
	Increment.StartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	Pending.Add(Increment);

	// 同帧内连续按键时，多个增量的 StartTime 相同，进度也相同——
	// 这正是"连续按 N 次就连续转 N 个 90°"的来源。
	UpdateDisplay(Increment.StartTime);
	SetComponentTickEnabled(true);
}

void UCS_CubeRotationComponent::CommitIncrement(const FCSFlipIncrement& Increment)
{
	const FQuat Delta(Increment.Axis, FMath::DegreesToRadians(Increment.AngleDeg));

	// 左乘：后按的增量在更外侧，与数据层的对换顺序一致
	BaseQuat = (Delta * BaseQuat).GetNormalized();
}

void UCS_CubeRotationComponent::UpdateDisplay(float Now)
{
	FQuat Result = BaseQuat;

	// 按入队顺序依次左乘：最早的在最内层
	for (const FCSFlipIncrement& Increment : Pending)
	{
		const float U = FMath::Clamp((Now - Increment.StartTime) / SettleTime, 0.f, 1.f);
		const float S = QuinticCurve(U);

		if (S <= 0.f)
		{
			continue;
		}

		const FQuat Partial(Increment.Axis, FMath::DegreesToRadians(Increment.AngleDeg * S));
		Result = Partial * Result;
	}

	DisplayQuat = Result.GetNormalized();
}

float UCS_CubeRotationComponent::QuinticCurve(float U)
{
	// s(u) = u³ · (u · (6u - 15) + 10)
	// u=0 → 0，u=1 → 1，两端一阶、二阶导都为 0（C2 连续）
	const float U2 = U * U;
	return U2 * U * (U * (6.f * U - 15.f) + 10.f);
}

void UCS_CubeRotationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Pending.Num() == 0)
	{
		SetComponentTickEnabled(false);
		return;
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	// 队列按起始时间有序，而每段时长相同，所以"播完"也是按顺序发生的
	while (Pending.Num() > 0 && (Now - Pending[0].StartTime) >= SettleTime)
	{
		CommitIncrement(Pending[0]);
		Pending.RemoveAt(0);
	}

	UpdateDisplay(Now);

	if (Pending.Num() == 0)
	{
		SetComponentTickEnabled(false);
	}
}

// ==================== 工具 ====================

void UCS_CubeRotationComponent::SwapSlots(int32 SlotA, int32 SlotB)
{
	if (!Slots.IsValidIndex(SlotA) || !Slots.IsValidIndex(SlotB))
	{
		return;
	}

	Swap(Slots[SlotA], Slots[SlotB]);
}

FVector UCS_CubeRotationComponent::SlotAxis(ECS_CubeSlot Slot)
{
	switch (Slot)
	{
	case ECS_CubeSlot::Up:
		return FVector::UpVector;      // +Z

	case ECS_CubeSlot::Right:
		return FVector::RightVector;   // +Y

	case ECS_CubeSlot::Front:
		return FVector::ForwardVector; // +X

	default:
		return FVector::UpVector;
	}
}

int32 UCS_CubeRotationComponent::SlotToIndex(ECS_CubeSlot Slot)
{
	return static_cast<int32>(Slot);
}
