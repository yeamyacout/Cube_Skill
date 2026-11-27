// Fill out your copyright notice in the Description page of Project Settings.


#include "Projectile/CS_LightningCubeProjectile.h"

#include "Characters/CS_EnemyCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** 蓝图未配置时兜底加载的默认特效资源路径 */
	const TCHAR* const DefaultAuraFXPath     = TEXT("/Game/AbsorbtionFX/FX/NS_Absorption_Yellow.NS_Absorption_Yellow");
	const TCHAR* const DefaultPathFXPath     = TEXT("/Game/GrimzaFX_simple_lightning/Particles/NS_Lightning_01b.NS_Lightning_01b");
	const TCHAR* const DefaultEnemyHitFXPath = TEXT("/Game/VFX/NS_EnemyHit_Lightning.NS_EnemyHit_Lightning");

	/** 距离小于该值（cm）视为同一点，避免除零和退化扫掠 */
	constexpr float MinValidDistance = 1.f;

	/**
	 * 延长特效存活时间后自动销毁
	 *
	 * 用【世界】作为定时器 owner，而不是方块：
	 * 方块回收完成后会立即 Destroy()，以方块为 owner 的定时器会一起失效，
	 * 导致特效永远收不到销毁指令（表现为"特效一直留在敌人身上"）。
	 */
	void ScheduleDestroyNiagara(UWorld* World, UNiagaraComponent* Comp, float Delay)
	{
		if (!World || !Comp)
		{
			return;
		}

		TWeakObjectPtr<UNiagaraComponent> WeakComp(Comp);
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(
			Handle,
			FTimerDelegate::CreateWeakLambda(World, [WeakComp]()
			{
				if (UNiagaraComponent* C = WeakComp.Get())
				{
					C->Deactivate();
					C->DestroyComponent();
				}
			}),
			Delay,
			/*bLoop=*/false);
	}
}


ACS_LightningCubeProjectile::ACS_LightningCubeProjectile()
{
	CubeColor = FLinearColor(0.55f, 0.15f, 1.f, 1.f);   // 紫色，代表雷电

	// 雷电方块穿过一切物体，也不做物理弹跳
	bEnableCollisionDetection = false;
	bEnablePhysicsOnImpact = false;
	bEnableEnemyKnockback = false;

	// 攻击判定：每 0.5 秒扫描一次 EnemyDetectRadius 范围内的敌人，
	// 扫描到的每个敌人都触发 OnEnemyHitEffect（伤害 + 特效）。
	// 回收途中继续保持扫描，让方块"电着回来"。
	EnemyDetectInterval = 0.5f;
	EnemyDetectRadius = 300.f;
	bDetectEnemiesWhileReturning = true;
	bAllowRepeatEnemyHit = true;

	// 默认特效资源，蓝图里可随时替换
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> AuraFinder(DefaultAuraFXPath);
	if (AuraFinder.Succeeded())
	{
		FlightAuraFX = AuraFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> PathFinder(DefaultPathFXPath);
	if (PathFinder.Succeeded())
	{
		TeleportPathFX = PathFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> HitFinder(DefaultEnemyHitFXPath);
	if (HitFinder.Succeeded())
	{
		EnemyHitFX = HitFinder.Object;
	}
}


void ACS_LightningCubeProjectile::BeginPlay()
{
	Super::BeginPlay();

	// 飞行光环：挂在方块根组件上跟随移动，方块销毁时自动清理（见 EndPlay）
	UNiagaraSystem* Aura = FlightAuraFX;
	if (!Aura)
	{
		Aura = LoadObject<UNiagaraSystem>(nullptr, DefaultAuraFXPath);
	}

	if (Aura && !FlightAuraComponent && GetRootComponent())
	{
		FlightAuraComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
			Aura,
			GetRootComponent(),
			NAME_None,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset,
			/*bAutoDestroy=*/false,
			/*bAutoActivate=*/true);
	}
}


void ACS_LightningCubeProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (FlightAuraComponent)
	{
		FlightAuraComponent->Deactivate();
		FlightAuraComponent->DestroyComponent();
		FlightAuraComponent = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}


void ACS_LightningCubeProjectile::OnEnemyHitEffect(ACS_EnemyCharacter* Enemy)
{
	if (!Enemy)
	{
		return;
	}

	// 基类已完成范围扫描，这里只需处理「被扫到的这一个敌人」
	Enemy->TakeDamage(LightningDamage, FDamageEvent(), nullptr, this);
	SpawnEnemyHitEffects(Enemy);
}


void ACS_LightningCubeProjectile::SpawnEnemyHitEffects(ACS_EnemyCharacter* Enemy)
{
	UWorld* World = GetWorld();
	if (!Enemy || !World)
	{
		return;
	}

	// 蓝图未配置时从默认路径兜底加载，保证一定有特效
	UNiagaraSystem* FX = EnemyHitFX;
	if (!FX)
	{
		FX = LoadObject<UNiagaraSystem>(nullptr, DefaultEnemyHitFXPath);
	}
	if (!FX)
	{
		return;
	}

	// 优先挂到骨骼网格，敌人移动/转身时特效跟着走
	ACharacter* EnemyChar = Cast<ACharacter>(Enemy);
	USceneComponent* AttachTarget = EnemyChar && EnemyChar->GetMesh()
		? EnemyChar->GetMesh()
		: Enemy->GetRootComponent();

	if (!AttachTarget)
	{
		return;
	}

	// 挂载点：默认在敌人身体中心，可再按胶囊高度上抬一半，避免埋进模型
	FVector AttachOffset = EnemyHitFXOffset;
	if (bEnemyHitFXAutoHeight && EnemyChar)
	{
		if (const UCapsuleComponent* Capsule = EnemyChar->GetCapsuleComponent())
		{
			AttachOffset.Z += Capsule->GetScaledCapsuleHalfHeight() * 0.5f;
		}
	}

	UNiagaraComponent* Comp = UNiagaraFunctionLibrary::SpawnSystemAttached(
		FX,
		AttachTarget,
		NAME_None,
		AttachOffset,
		FRotator(EnemyHitFXPitch, EnemyHitFXYaw, EnemyHitFXRoll),
		EAttachLocation::KeepRelativeOffset,
		/*bAutoDestroy=*/false,
		/*bAutoActivate=*/true);

	if (!Comp)
	{
		return;
	}

	// 先重启模拟再设缩放：ReinitializeSystem 会重置组件状态，
	// 顺序反了会导致刚设好的缩放被清掉。
	Comp->ReinitializeSystem();
	Comp->Activate(true);

	// 统一缩放或 XYZ 分轴缩放
	const FVector EffectScale = bEnemyHitFXUniformScale
		? FVector(EnemyHitFXScale)
		: FVector(EnemyHitFXScaleX, EnemyHitFXScaleY, EnemyHitFXScaleZ);

	// 两条路径同时设置：组件缩放（Mesh 渲染器）+ Niagara 内部 User 参数。
	// 资产里不存在的 User 参数会被安全忽略。
	Comp->SetWorldScale3D(EffectScale);
	Comp->SetNiagaraVariableFloat(TEXT("ScaleX"), EffectScale.X);
	Comp->SetNiagaraVariableFloat(TEXT("ScaleY"), EffectScale.Y);
	Comp->SetNiagaraVariableFloat(TEXT("ScaleZ"), EffectScale.Z);

	ScheduleDestroyNiagara(World, Comp, EnemyHitFXDuration);
}


bool ACS_LightningCubeProjectile::ExecuteTeleport()
{
	if (bTeleportUsed)
	{
		return false;
	}

	UWorld* World = GetWorld();
	ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
	if (!World || !OwnerChar)
	{
		return false;
	}

	bTeleportUsed = true;

	// ---------- 1. 锁定方块当前位置作为冲刺目标 ----------
	// 冲刺开始后即使方块继续飞，目标也不再变化
	const FVector FromLocation = OwnerChar->GetActorLocation();
	const FVector TargetLocation = GetActorLocation();

	FVector DashDirection = TargetLocation - FromLocation;
	const float FullDistance = DashDirection.Size();
	DashDirection = DashDirection.GetSafeNormal();

	// ---------- 2. 胶囊扫掠求最远可达点（撞墙截断）----------
	// 即使目标在墙后，玩家也只会停在墙前，不会穿墙或飞出地图
	FVector FinalLocation = TargetLocation;

	if (FullDistance > MinValidDistance)
	{
		FHitResult SweepHit;
		FCollisionQueryParams SweepParams(SCENE_QUERY_STAT(CS_DashSweep), false, OwnerChar);
		SweepParams.AddIgnoredActor(OwnerChar);

		const UCapsuleComponent* Capsule = OwnerChar->GetCapsuleComponent();
		const float CapsuleRadius = Capsule ? Capsule->GetScaledCapsuleRadius() : DashProbeRadius;
		const float CapsuleHalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : CapsuleRadius;

		const bool bBlocked = World->SweepSingleByChannel(
			SweepHit,
			FromLocation,
			TargetLocation,
			FQuat::Identity,
			ECC_Pawn,
			FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight),
			SweepParams);

		if (bBlocked)
		{
			// 停在障碍之前，并留出胶囊半径的余量
			const float SafeDistance = FMath::Max(SweepHit.Distance - CapsuleRadius, 0.f);
			FinalLocation = FromLocation + DashDirection * SafeDistance;
		}
	}

	const float ActualDistance = FVector::Dist(FromLocation, FinalLocation);

	// ---------- 3. 路径闪电特效（长度 = 实际冲刺距离）----------
	UNiagaraSystem* PathFX = TeleportPathFX;
	if (!PathFX)
	{
		PathFX = LoadObject<UNiagaraSystem>(nullptr, DefaultPathFXPath);
	}

	if (PathFX && ActualDistance > MinValidDistance)
	{
		UNiagaraComponent* PathComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World,
			PathFX,
			FromLocation,
			DashDirection.Rotation(),
			FVector(1.f),
			/*bAutoDestroy=*/false,
			/*bAutoActivate=*/true);

		if (PathComp)
		{
			// 资产里有对应 User 参数才会生效，否则安全忽略
			PathComp->SetNiagaraVariableFloat(TEXT("User.PathLength"), ActualDistance);
			PathComp->SetNiagaraVariableVec3(TEXT("User.EndPoint"), FinalLocation);

			PathComp->ReinitializeSystem();
			PathComp->Activate(true);

			ScheduleDestroyNiagara(World, PathComp, TeleportPathFXDuration);
		}
	}

	// ---------- 4. 路径上的敌人受伤 ----------
	{
		TArray<FOverlapResult> Overlaps;

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CS_TeleportPath), false, OwnerChar);
		QueryParams.AddIgnoredActor(OwnerChar);

		const float HalfHeight = FMath::Max(ActualDistance * 0.5f, TeleportPathRadius);

		World->OverlapMultiByObjectType(
			Overlaps,
			(FromLocation + FinalLocation) * 0.5f,
			DashDirection.Rotation().Quaternion(),
			FCollisionObjectQueryParams(ECC_Pawn),
			FCollisionShape::MakeCapsule(TeleportPathRadius, HalfHeight),
			QueryParams);

		// Overlap 对同一敌人会返回多条结果，必须去重，否则重复伤害 + 特效刷屏
		TSet<ACS_EnemyCharacter*> PathEnemies;
		for (const FOverlapResult& Overlap : Overlaps)
		{
			if (ACS_EnemyCharacter* Found = Cast<ACS_EnemyCharacter>(Overlap.GetActor()))
			{
				PathEnemies.Add(Found);
			}
		}

		for (ACS_EnemyCharacter* Target : PathEnemies)
		{
			if (!Target)
			{
				continue;
			}

			Target->TakeDamage(TeleportPathDamage, FDamageEvent(), nullptr, this);
			SpawnEnemyHitEffects(Target);
		}
	}

	// ---------- 5. 玩家冲到方块位置 ----------
	// TeleportTo 位移速度等同于瞬移，但目标点已被上面的扫掠截断，
	// 所以撞墙时只会停在墙前。
	if (ActualDistance > MinValidDistance)
	{
		OwnerChar->TeleportTo(
			FinalLocation,
			OwnerChar->GetActorRotation(),
			/*bIsATest=*/false,
			/*bNoCheck=*/false);

		// 清掉残留速度，避免落地后继续滑行
		if (UCharacterMovementComponent* MoveComp = OwnerChar->GetCharacterMovement())
		{
			MoveComp->StopMovementImmediately();
		}
	}

	return true;
}
