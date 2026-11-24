// Fill out your copyright notice in the Description page of Project Settings.


#include "Projectile/CS_LightningCubeProjectile.h"

#include "Characters/CS_EnemyCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"


// 默认特效资源路径（蓝图未配置时运行时兜底加载）
#define CS_NS_FLIGHT_AURA   TEXT("/Game/AbsorbtionFX/FX/NS_Absorption_Yellow.NS_Absorption_Yellow")
#define CS_NS_TELEPORT_PATH TEXT("/Game/GrimzaFX_simple_lightning/Particles/NS_Lightning_01b.NS_Lightning_01b")
// 命中特效：NS_Lightning_01a
// 它是 Mesh 型渲染器，组件 SetWorldScale3D 能真正生效（Sprite 型不行）。
// 注意它默认每秒只生成 1 个粒子、尺寸 3cm，需要用 EnemyHitFXScale
// 放大才能看清，需要更多粒子可调 EnemyHitFXSpawnRate。
#define CS_NS_ENEMY_HIT     TEXT("/Game/VFX/NS_EnemyHit_Lightning.NS_EnemyHit_Lightning")


ACS_LightningCubeProjectile::ACS_LightningCubeProjectile()
{
	// 紫色：代表雷电
	CubeColor = FLinearColor(0.55f, 0.15f, 1.f, 1.f);

	// 雷电方块特征：无碰撞（飞行中穿过一切物体）
	bEnableCollisionDetection = false;

	// 不进入物理模拟
	bEnablePhysicsOnImpact = false;

	// 撞到敌人不推开，用范围伤害代替
	bEnableEnemyKnockback = false;

	// 默认特效资源
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> AuraFX(
		TEXT("/Game/AbsorbtionFX/FX/NS_Absorption_Yellow.NS_Absorption_Yellow"));
	if (AuraFX.Succeeded())
	{
		FlightAuraFX = AuraFX.Object;
	}

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> PathFX(
		TEXT("/Game/GrimzaFX_simple_lightning/Particles/NS_Lightning_01b.NS_Lightning_01b"));
	if (PathFX.Succeeded())
	{
		TeleportPathFX = PathFX.Object;
	}

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> HitFX(
		TEXT("/Game/VFX/NS_EnemyHit_Lightning.NS_EnemyHit_Lightning"));
	if (HitFX.Succeeded())
	{
		EnemyHitFX = HitFX.Object;
	}
}


void ACS_LightningCubeProjectile::BeginPlay()
{
	Super::BeginPlay();

	// ---------- 飞行光环：以方块为中心，跟随方块移动 ----------
	// Attached 模式，方块销毁时特效一起消失
	UNiagaraSystem* AuraFX = FlightAuraFX;
	if (!AuraFX)
	{
		AuraFX = LoadObject<UNiagaraSystem>(nullptr, CS_NS_FLIGHT_AURA);
	}

	if (AuraFX && !FlightAuraComponent && GetRootComponent())
	{
		FlightAuraComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
			AuraFX,
			GetRootComponent(),
			NAME_None,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset,
			/*bAutoDestroy=*/false);

		if (FlightAuraComponent)
		{
			FlightAuraComponent->Activate(true);
		}
	}
}


void ACS_LightningCubeProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 回收/销毁时清掉飞行光环
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
	UE_LOG(LogTemp, Warning, TEXT("[雷DFX] OnEnemyHitEffect 被调用 Enemy=%s bDischarged=%d"),
		Enemy ? *Enemy->GetName() : TEXT("None"), bLightningDischarged ? 1 : 0);

	// 只在首次命中时释放范围雷电
	if (bLightningDischarged)
	{
		UE_LOG(LogTemp, Warning, TEXT("[雷DFX] 已释放过，跳过"));
		return;
	}

	bLightningDischarged = true;

	DamageEnemiesInRadius(GetActorLocation());
}


void ACS_LightningCubeProjectile::DamageEnemiesInRadius(const FVector& Center)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<FOverlapResult> Overlaps;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CS_LightningRadius), false, this);
	if (GetOwner())
	{
		QueryParams.AddIgnoredActor(GetOwner());
	}

	const bool bFound = World->OverlapMultiByObjectType(
		Overlaps,
		Center,
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(LightningRadius),
		QueryParams);

	if (!bFound)
	{
		UE_LOG(LogTemp, Warning, TEXT("[雷DFX] 范围内没有找到任何 Pawn"));
		return;
	}

	// Overlap 可能对同一个敌人返回多条结果（多个碰撞体/重叠体），
	// 这里用 SET 去重，保证每个敌人只受一次伤害、只生成一个特效。
	TSet<ACS_EnemyCharacter*> UniqueEnemies;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (ACS_EnemyCharacter* Enemy = Cast<ACS_EnemyCharacter>(Overlap.GetActor()))
		{
			UniqueEnemies.Add(Enemy);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[雷DFX] Overlap 返回 %d 条，去重后 %d 个敌人"),
		Overlaps.Num(), UniqueEnemies.Num());

	for (ACS_EnemyCharacter* Target : UniqueEnemies)
	{
		if (!Target)
		{
			continue;
		}

		Target->TakeDamage(LightningDamage, FDamageEvent(), nullptr, this);
		SpawnEnemyHitEffects(Target);
	}
}


void ACS_LightningCubeProjectile::SpawnBurstEffect(UNiagaraSystem* System, const FVector& Location) const
{
	if (!System || !GetWorld())
	{
		return;
	}

	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetWorld(),
		System,
		Location,
		FRotator::ZeroRotator,
		FVector(1.f),
		/*bAutoDestroy=*/true);
}


void ACS_LightningCubeProjectile::SpawnEnemyHitEffects(ACS_EnemyCharacter* Enemy)
{
	if (!Enemy)
	{
		UE_LOG(LogTemp, Warning, TEXT("[雷DFX] SpawnEnemyHitEffects: Enemy 为空"));
		return;
	}
	if (!GetWorld())
	{
		UE_LOG(LogTemp, Warning, TEXT("[雷DFX] SpawnEnemyHitEffects: World 为空"));
		return;
	}

	// 兜底：蓝图里若没配，运行时从默认路径加载，保证一定能生成
	UNiagaraSystem* FX = EnemyHitFX;
	if (!FX)
	{
		FX = LoadObject<UNiagaraSystem>(nullptr, CS_NS_ENEMY_HIT);
		UE_LOG(LogTemp, Warning, TEXT("[雷DFX] 蓝图未配置 EnemyHitFX，代码兜底加载结果=%s"),
			FX ? TEXT("成功") : TEXT("失败"));
	}
	if (!FX)
	{
		UE_LOG(LogTemp, Warning, TEXT("[雷DFX] 特效资源加载失败，无法生成"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[雷DFX] 即将生成特效 %s 于 %s 缩放=%.2f"),
		*FX->GetName(), *Enemy->GetActorLocation().ToCompactString(), EnemyHitFXScale);

	// ---------- 挂到敌人身上，跟随敌人移动 ----------
	// 用 SpawnSystemAttached 绑到敌人骨骼网格（优先）或根组件，
	// 特效会随之移动，敌人移动/转身时特效跟着走。
	USceneComponent* AttachTarget = Enemy->GetRootComponent();
	if (const ACharacter* EnemyChar = Cast<ACharacter>(Enemy))
	{
		if (USkeletalMeshComponent* EnemyMesh = EnemyChar->GetMesh())
		{
			AttachTarget = EnemyMesh;
		}
	}

	if (!AttachTarget)
	{
		UE_LOG(LogTemp, Warning, TEXT("[雷DFX] 敌人没有可挂载的组件"));
		return;
	}

	// 挂载点偏移：敌人身体中心 + 可调偏移
	FVector AttachOffset = EnemyHitFXOffset;

	// 自动按胶囊高度抬高，避免特效埋在地面或身体模型里
	if (bEnemyHitFXAutoHeight)
	{
		if (const ACharacter* EnemyChar = Cast<ACharacter>(Enemy))
		{
			if (const UCapsuleComponent* Capsule = EnemyChar->GetCapsuleComponent())
			{
				AttachOffset.Z += Capsule->GetScaledCapsuleHalfHeight() * 0.5f;
			}
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
		UE_LOG(LogTemp, Warning, TEXT("[雷DFX] SpawnSystemAttached 返回空，特效生成失败"));
		return;
	}

	// ---------- 先重启模拟，再设置缩放 ----------
	// 顺序很重要：ReinitializeSystem() 会重置组件状态，
	// 如果先设缩放再 Reinitialize，缩放会被清掉（这就是之前"改了大小没变化"的原因）。
	// 所以必须 Reinitialize 在前，所有参数设置在后。
	Comp->ReinitializeSystem();

	// 确保激活
	Comp->Activate(true);

	// ---------- 缩放 ----------
	// 支持统一缩放或 XYZ 分轴独立缩放。
	const FVector EffectScale = bEnemyHitFXUniformScale
		? FVector(EnemyHitFXScale)
		: FVector(EnemyHitFXScaleX, EnemyHitFXScaleY, EnemyHitFXScaleZ);

	// ① 组件世界缩放（Mesh 渲染器路径）
	Comp->SetWorldScale3D(EffectScale);

	// ② Niagara 内部 User 参数（资产里已加 User.ScaleX/Y/Z）
	// 对不存在的参数会被安全忽略。
	Comp->SetNiagaraVariableFloat(TEXT("ScaleX"), EffectScale.X);
	Comp->SetNiagaraVariableFloat(TEXT("ScaleY"), EffectScale.Y);
	Comp->SetNiagaraVariableFloat(TEXT("ScaleZ"), EffectScale.Z);

	UE_LOG(LogTemp, Warning, TEXT("[雷DFX] 特效已挂到 %s，组件=%s 激活=%d 组件缩放=%s 目标缩放=%s"),
		*AttachTarget->GetName(), *Comp->GetName(), Comp->IsActive() ? 1 : 0,
		*Comp->GetComponentScale().ToCompactString(),
		*EffectScale.ToCompactString());

	// 固定时长后强制销毁
	TWeakObjectPtr<UNiagaraComponent> WeakComp(Comp);
	FTimerHandle Handle;
	GetWorld()->GetTimerManager().SetTimer(
		Handle,
		FTimerDelegate::CreateWeakLambda(this, [WeakComp]()
		{
			if (UNiagaraComponent* C = WeakComp.Get())
			{
				C->Deactivate();
				C->DestroyComponent();
			}
		}),
		EnemyHitFXDuration,
		false);
}


bool ACS_LightningCubeProjectile::ExecuteTeleport()
{
	// 每次发射只能冲刺一次
	if (bTeleportUsed)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
	if (!OwnerChar)
	{
		return false;
	}

	bTeleportUsed = true;

	// ---------- 1. 锁定方块位置作为冲刺目标 ----------
	// 冲刺开始后即使方块继续飞，目标也不再变化
	const FVector FromLocation = OwnerChar->GetActorLocation();
	const FVector TargetLocation = GetActorLocation();

	FVector DashDirection = TargetLocation - FromLocation;
	const float FullDistance = DashDirection.Size();
	DashDirection = DashDirection.GetSafeNormal();

	// ---------- 2. 用胶囊扫掠确定最远可到达位置（撞墙截断）----------
	// 这样即使目标在墙后，玩家也只会冲到墙前，不会穿墙或冲出地图
	FVector FinalLocation = TargetLocation;

	if (FullDistance > 1.f)
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
			// 停在障碍之前，留出胶囊半径的余量
			const float SafeDistance = FMath::Max(SweepHit.Distance - CapsuleRadius, 0.f);
			FinalLocation = FromLocation + DashDirection * SafeDistance;
		}
	}

	const float ActualDistance = FVector::Dist(FromLocation, FinalLocation);

	// ---------- 3. 路径闪电特效（长度 = 实际冲刺距离）----------
	UNiagaraSystem* PathFX = TeleportPathFX;
	if (!PathFX)
	{
		PathFX = LoadObject<UNiagaraSystem>(nullptr, CS_NS_TELEPORT_PATH);
	}

	if (PathFX && ActualDistance > 1.f)
	{
		UNiagaraComponent* PathComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World,
			PathFX,
			FromLocation,
			DashDirection.Rotation(),
			FVector(1.f),
			/*bAutoDestroy=*/false,      // 不自动销毁，由下面的定时器控制时长
			/*bAutoActivate=*/true);

		if (PathComp)
		{
			// 若特效里没有这些 User 参数，设置会被安全忽略
			PathComp->SetNiagaraVariableFloat(TEXT("User.PathLength"), ActualDistance);
			PathComp->SetNiagaraVariableVec3(TEXT("User.EndPoint"), FinalLocation);

			// 重启模拟，保证立即出粒子
			PathComp->ReinitializeSystem();
			PathComp->Activate(true);

			// 持续 TeleportPathFXDuration（默认 2 秒）后销毁
			TWeakObjectPtr<UNiagaraComponent> WeakPath(PathComp);
			FTimerHandle PathHandle;
			World->GetTimerManager().SetTimer(
				PathHandle,
				FTimerDelegate::CreateWeakLambda(this, [WeakPath]()
				{
					if (UNiagaraComponent* C = WeakPath.Get())
					{
						C->Deactivate();
						C->DestroyComponent();
					}
				}),
				TeleportPathFXDuration,
				false);

			UE_LOG(LogTemp, Warning, TEXT("[雷DFX] 路径闪电已生成，长度=%.1f 持续=%.1f秒"),
				ActualDistance, TeleportPathFXDuration);
		}
	}

	// ---------- 4. 路径上的敌人受伤 ----------
	{
		TArray<FOverlapResult> Overlaps;

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CS_TeleportPath), false, OwnerChar);
		QueryParams.AddIgnoredActor(OwnerChar);

		const float HalfHeight = FMath::Max(ActualDistance * 0.5f, TeleportPathRadius);
		const FVector Center = (FromLocation + FinalLocation) * 0.5f;

		World->OverlapMultiByObjectType(
			Overlaps,
			Center,
			DashDirection.Rotation().Quaternion(),
			FCollisionObjectQueryParams(ECC_Pawn),
			FCollisionShape::MakeCapsule(TeleportPathRadius, HalfHeight),
			QueryParams);

		// 同样用 SET 去重，避免同一个敌人被重复命中、重复生成特效
		TSet<ACS_EnemyCharacter*> PathEnemies;
		for (const FOverlapResult& Overlap : Overlaps)
		{
			if (ACS_EnemyCharacter* Enemy = Cast<ACS_EnemyCharacter>(Overlap.GetActor()))
			{
				PathEnemies.Add(Enemy);
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
	// 用 TeleportTo 直接位移，速度上等同于瞬移；
	// 但目标点已经过上面的胶囊扫掠截断，
	// 所以撞墙时只会停在墙前，不会穿墙或冲出地图。
	if (ActualDistance > 1.f)
	{
		OwnerChar->TeleportTo(
			FinalLocation,
			OwnerChar->GetActorRotation(),
			/*bIsATest=*/false,
			/*bNoCheck=*/false);

		if (UCharacterMovementComponent* MoveComp = OwnerChar->GetCharacterMovement())
		{
			// 清掉残留速度，避免落地后继续滑行
			MoveComp->StopMovementImmediately();
		}
	}

	return true;
}
