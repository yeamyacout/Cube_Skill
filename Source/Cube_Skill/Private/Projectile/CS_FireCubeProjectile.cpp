// Fill out your copyright notice in the Description page of Project Settings.


#include "Projectile/CS_FireCubeProjectile.h"

#include "Characters/CS_EnemyCharacter.h"
#include "Characters/CS_PlayerCharacter.h"
#include "Components/ChildActorComponent.h"
#include "Components/CS_CubeRotationComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"

DEFINE_LOG_CATEGORY_STATIC(LogCS_Fire, Log, All);


ACS_FireCubeProjectile::ACS_FireCubeProjectile()
{
	// 红色：代表火焰
	CubeColor = FLinearColor(1.f, 0.15f, 0.05f, 1.f);

	// 火焰方块不推敌、不进入模拟物理（FIRE_CUBE.md 的两个固定开关）
	bEnablePhysicsOnImpact = false;
	bEnableEnemyKnockback = false;
}


void ACS_FireCubeProjectile::BeginPlay()
{
	// 蓝图里可能残留旧默认值，运行时再强制一次，保证行为与文档一致
	bEnablePhysicsOnImpact = false;
	bEnableEnemyKnockback = false;

	// 需求：飞回来的时候也要喷火；火舌会一直喷到收回时 StopFlame。
	bSprayWhileReturning = true;

	// 基类默认在 250cm 处就销毁；这里压到 1cm，
	// 让火焰方块自己接管“到达玩家附近”的检测（见 CheckRecallHide）。
	RecallAcceptRadius = 1.f;

	Super::BeginPlay();

	// 整个方块转到当前翻转姿态；挂载的火舌跟着一起转
	ApplyFlipRotation();

	OnStateChanged.AddDynamic(this, &ACS_FireCubeProjectile::HandleStateChanged);

	AcquireFlameTongues();
}


void ACS_FireCubeProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RecallHideCheckTimerHandle);
		World->GetTimerManager().ClearTimer(RecallHideTimerHandle);
	}

	DestroyFlameTongues();

	Super::EndPlay(EndPlayReason);
}


FVector ACS_FireCubeProjectile::GetFireAxis() const
{
	// 旋转数据层 / 控制器给了覆盖值就用覆盖值
	if (!FireAxisOverride.IsNearlyZero())
	{
		return FireAxisOverride.GetSafeNormal();
	}

	// 当前翻转方向：从玩家角色身上的旋转组件读「上下」面组所在轴
	if (ACS_PlayerCharacter* Player = Cast<ACS_PlayerCharacter>(GetOwner()))
	{
		UCS_CubeRotationComponent* Rotation = Player->CubeRotation;
		if (!Rotation)
		{
			Rotation = Player->FindComponentByClass<UCS_CubeRotationComponent>();
		}

		if (Rotation)
		{
			return Rotation->GetFaceForward(ECS_CubeFaceGroup::UpDown);
		}
	}

	// 兜底：取方块世界 +Z 的主轴，保证结果一定是 ±X / ±Y / ±Z
	const FVector Up = GetActorUpVector();
	if (Up.IsNearlyZero())
	{
		return FVector::UpVector;
	}

	const FVector Abs = Up.GetAbs();
	if (Abs.Z >= Abs.X && Abs.Z >= Abs.Y)
	{
		return FVector(0.f, 0.f, FMath::Sign(Up.Z));
	}
	if (Abs.X >= Abs.Y)
	{
		return FVector(FMath::Sign(Up.X), 0.f, 0.f);
	}
	return FVector(0.f, FMath::Sign(Up.Y), 0.f);
}


void ACS_FireCubeProjectile::SetFireAxisOverride(const FVector& InAxis)
{
	FireAxisOverride = InAxis.GetSafeNormal();
	if (FireAxisOverride.IsNearlyZero())
	{
		return;
	}

	// 不单独改火舌方向：把整个方块旋转到该轴，挂载的火舌自然跟着转
	const FVector CurrentUp = GetActorUpVector().GetSafeNormal();
	if (CurrentUp.IsNearlyZero())
	{
		return;
	}

	const FQuat Delta = FQuat::FindBetweenNormals(CurrentUp, FireAxisOverride);
	SetActorRotation((Delta * GetActorQuat()).GetNormalized());
}


void ACS_FireCubeProjectile::ApplyFlipRotation()
{
	// 发射方块的朝向必须以玩家为基准，并且和手上显示的方块对应：
	// 显示方块的世界朝向 = 它的父级（角色 / 相机 / Pivot）× DisplayQuat。
	// 直接取显示组件的世界朝向，既包含玩家基准，也包含当前翻转结果；
	// GetDisplayQuat() 只是相对父级的偏移，单独用会丢掉玩家/相机那一层。
	if (ACS_PlayerCharacter* Player = Cast<ACS_PlayerCharacter>(GetOwner()))
	{
		if (IsValid(Player->CubeDisplayMesh))
		{
			SetActorRotation(Player->CubeDisplayMesh->GetComponentQuat());
			return;
		}

		UCS_CubeRotationComponent* Rotation = Player->CubeRotation;
		if (!Rotation)
		{
			Rotation = Player->FindComponentByClass<UCS_CubeRotationComponent>();
		}

		if (Rotation)
		{
			SetActorRotation(Rotation->GetDisplayQuat().Rotator());
		}
	}
}


bool ACS_FireCubeProjectile::IsSpraying() const
{
	return (FlameTongueA && FlameTongueA->IsFlameActive())
		|| (FlameTongueB && FlameTongueB->IsFlameActive());
}


void ACS_FireCubeProjectile::AcquireFlameTongues()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 整个方块已经转到当前翻转姿态；这里用方块本地上轴作为兜底方向
	const FVector Axis = GetActorUpVector().GetSafeNormal();
	const float MuzzleOffset = FlameMuzzleOffset > 0.f
		? FlameMuzzleOffset
		: FMath::Max(1.f, CubeSize * 0.5f);

	// 先在 BP 里找已经挂好的火舌，找不到才 Spawn
	TArray<ACS_FlameTongue*> Attached;
	FindAttachedFlameTongues(Attached);

	FlameTongueA = Attached.IsValidIndex(0) ? Attached[0] : nullptr;
	FlameTongueB = Attached.IsValidIndex(1) ? Attached[1] : nullptr;
	bFlameTongueSpawnedA = false;
	bFlameTongueSpawnedB = false;

	// 挂载的火舌：位置、朝向都保留 BP 里的，不再单独改方向。
	// 整个方块旋转时，它们自然跟着转。
	if (FlameTongueA)
	{
		SetupFlameTongue(FlameTongueA, FlameTongueA->GetActorForwardVector(), false);
	}
	if (FlameTongueB)
	{
		SetupFlameTongue(FlameTongueB, FlameTongueB->GetActorForwardVector(), false);
	}

	// 挂载数量不足时，才 Spawn 兜底
	if (!FlameTongueA)
	{
		FlameTongueA = SpawnFlameTongue(Axis, MuzzleOffset);
		bFlameTongueSpawnedA = (FlameTongueA != nullptr);
		SetupFlameTongue(FlameTongueA, Axis, true);
	}

	if (!FlameTongueB)
	{
		FlameTongueB = SpawnFlameTongue(-Axis, MuzzleOffset);
		bFlameTongueSpawnedB = (FlameTongueB != nullptr);
		SetupFlameTongue(FlameTongueB, -Axis, true);
	}

	if (FlameTongueA || FlameTongueB)
	{
		OnFireSprayStarted.Broadcast();
	}
}


void ACS_FireCubeProjectile::FindAttachedFlameTongues(TArray<ACS_FlameTongue*>& OutTongues) const
{
	OutTongues.Reset();

	// 路径 1：BP 里用 ChildActorComponent 挂的
	TArray<UChildActorComponent*> ChildComponents;
	GetComponents<UChildActorComponent>(ChildComponents);
	for (UChildActorComponent* Component : ChildComponents)
	{
		if (!Component)
		{
			continue;
		}

		if (ACS_FlameTongue* Tongue = Cast<ACS_FlameTongue>(Component->GetChildActor()))
		{
			OutTongues.AddUnique(Tongue);
		}
	}

	// 路径 2：直接 Attach 到方块上的 Actor
	TArray<AActor*> AttachedActors;
	GetAttachedActors(AttachedActors);
	for (AActor* Actor : AttachedActors)
	{
		if (ACS_FlameTongue* Tongue = Cast<ACS_FlameTongue>(Actor))
		{
			OutTongues.AddUnique(Tongue);
		}
	}
}


ACS_FlameTongue* ACS_FireCubeProjectile::SpawnFlameTongue(const FVector& Direction, float MuzzleOffset)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UClass* ClassToSpawn = FlameTongueClass.Get()
		? FlameTongueClass.Get()
		: ACS_FlameTongue::StaticClass();

	const FVector SpawnLocation = GetActorLocation() + Direction * MuzzleOffset;
	const FRotator SpawnRotation = FRotationMatrix::MakeFromX(Direction).Rotator();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = GetInstigator();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ACS_FlameTongue* Tongue = World->SpawnActor<ACS_FlameTongue>(
		ClassToSpawn, SpawnLocation, SpawnRotation, SpawnParams);
	if (!Tongue)
	{
		return nullptr;
	}

	Tongue->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	return Tongue;
}


void ACS_FireCubeProjectile::SetupFlameTongue(ACS_FlameTongue* Tongue, const FVector& Direction, bool bApplyDirection)
{
	if (!Tongue)
	{
		return;
	}

	// 挂载的火舌 bApplyDirection=false：只初始化参数，不覆盖 BP 朝向；
	// 位置也不动，整个方块旋转时它们跟着转。
	Tongue->InitFlameTongue(this, GetInstigatorController(), Direction, FlameSettings, bApplyDirection);
	Tongue->OnFlameHit.AddDynamic(this, &ACS_FireCubeProjectile::HandleFlameHit);
	Tongue->StartFlame();
}


void ACS_FireCubeProjectile::HandleStateChanged()
{
	if (State == ECSProjectileState::Recalled)
	{
		StopRecallHideCheck();
		StopSpray();
		return;
	}

	if (State == ECSProjectileState::Returning)
	{
		StartRecallHideCheck();

		// bSprayWhileReturning 关闭时，才在进入 Returning 的瞬间停火
		if (!bSprayWhileReturning)
		{
			StopSpray();
		}
	}
}


void ACS_FireCubeProjectile::StartRecallHideCheck()
{
	bRecallHideStarted = false;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		RecallHideCheckTimerHandle, this, &ACS_FireCubeProjectile::CheckRecallHide,
		FMath::Max(0.005f, RecallHideCheckInterval), true);

	// 立刻检测一次
	CheckRecallHide();
}


void ACS_FireCubeProjectile::StopRecallHideCheck()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RecallHideCheckTimerHandle);
	}
}


void ACS_FireCubeProjectile::CheckRecallHide()
{
	if (bRecallHideStarted)
	{
		return;
	}

	AActor* Target = GetOwner();
	UWorld* World = GetWorld();
	if (!Target || !World)
	{
		return;
	}

	// 和基类一样用水平距离：贴墙/高低差时 3D 距离可能永远到不了
	const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
	const float Dist2D = FVector(ToTarget.X, ToTarget.Y, 0.f).Size();
	if (Dist2D > RecallHideTriggerRadius)
	{
		return;
	}

	bRecallHideStarted = true;
	StopRecallHideCheck();

	// 整个方块（连着火舌 / 粒子）一起沉到地下，
	// 玩家就看不到粒子因为父物体被销毁而瞬间消失的那一下
	const FVector HiddenLocation = GetActorLocation() + FVector(0.f, 0.f, -FMath::Abs(RecallHideDepth));
	SetActorLocation(HiddenLocation, false, nullptr, ETeleportType::TeleportPhysics);

	// 地下没有敌人，停掉判定；粒子已经在屏幕外
	StopSpray();

	World->GetTimerManager().SetTimer(
		RecallHideTimerHandle, this, &ACS_FireCubeProjectile::FinishRecallAfterHide,
		FMath::Max(0.f, RecallHideDelay), false);
}


void ACS_FireCubeProjectile::FinishRecallAfterHide()
{
	// 与基类到达玩家后的默认行为一致：标记已回收并销毁
	bIsReturning = false;
	SetState(ECSProjectileState::Recalled);
	Destroy();
}


void ACS_FireCubeProjectile::HandleFlameHit(ACS_EnemyCharacter* Enemy, bool bFirstHit, float Damage)
{
	if (!Enemy)
	{
		return;
	}

	OnFireHit.Broadcast(Enemy, bFirstHit, Damage);
	OnEmberAttached.Broadcast(Enemy, bFirstHit);
	ApplyFireAttribute(Enemy, bFirstHit);
}


void ACS_FireCubeProjectile::ApplyFireAttribute(ACS_EnemyCharacter* Enemy, bool bFirstHit)
{
	// TODO(属性系统): 属性系统就绪后，这里替换为真正的 ApplyElement / 刷新调用。
	UE_LOG(LogCS_Fire, Log,
		TEXT("[火焰] 命中 %s，首次=%s（属性系统未接入，仅日志）"),
		*GetNameSafe(Enemy),
		bFirstHit ? TEXT("是") : TEXT("否"));
}


void ACS_FireCubeProjectile::StopSpray()
{
	if (FlameTongueA)
	{
		FlameTongueA->StopFlame();
	}
	if (FlameTongueB)
	{
		FlameTongueB->StopFlame();
	}
}


void ACS_FireCubeProjectile::DestroyFlameTongues()
{
	// 挂载的火舌属于 ChildActorComponent，不能手动 Destroy，否则组件会留下空引用；
	// 只停火，等方块销毁时由组件一起销毁。
	if (FlameTongueA)
	{
		FlameTongueA->StopFlame();
		if (bFlameTongueSpawnedA)
		{
			FlameTongueA->Destroy();
		}
		FlameTongueA = nullptr;
		bFlameTongueSpawnedA = false;
	}

	if (FlameTongueB)
	{
		FlameTongueB->StopFlame();
		if (bFlameTongueSpawnedB)
		{
			FlameTongueB->Destroy();
		}
		FlameTongueB = nullptr;
		bFlameTongueSpawnedB = false;
	}
}
