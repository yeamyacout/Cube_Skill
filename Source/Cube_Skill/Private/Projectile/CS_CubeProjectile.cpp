// Fill out your copyright notice in the Description page of Project Settings.


#include "Projectile/CS_CubeProjectile.h"

#include "Characters/CS_EnemyCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/BodyInstance.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

#define CS_CUBE_MESH_PATH TEXT("/Engine/BasicShapes/Cube.Cube")

// 回收到达判定频率（秒），约 60fps
#define CS_RECALL_CHECK_INTERVAL 0.016f
// 推动敌人频率（秒）
#define CS_ENEMY_PUSH_INTERVAL 0.033f


ACS_CubeProjectile::ACS_CubeProjectile()
{
	// 不使用 Tick：碰撞靠事件、检测靠定时器
	PrimaryActorTick.bCanEverTick = false;

	// ---------- 碰撞球（根组件） ----------
	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	SetRootComponent(CollisionComponent);

	CollisionComponent->InitSphereRadius(20.f);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Block);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);

	// 不开这个 OnComponentHit 不会触发
	CollisionComponent->SetNotifyRigidBodyCollision(true);

	// ---------- 视觉网格 ----------
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(CollisionComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(CS_CUBE_MESH_PATH);
	if (CubeMesh.Succeeded())
	{
		MeshComponent->SetStaticMesh(CubeMesh.Object);
	}

	// ---------- 移动组件 ----------
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionComponent;
	ProjectileMovement->bRotationFollowsVelocity = false;
	ProjectileMovement->bSweepCollision = true;
	ProjectileMovement->bAutoActivate = false;
	ProjectileMovement->Velocity = FVector::ZeroVector;

	// 速度完全由 Velocity 控制。
	// InitialSpeed > 0 会让组件用 InitialSpeed 重算速度，覆盖我们设的值。
	ProjectileMovement->InitialSpeed = 0.f;
	ProjectileMovement->MaxSpeed = InitialSpeed;

	// 开启 Bounce 但弹性 0：bShouldBounce=false 时碰撞后引擎会 StopSimulating()
	// 并把 UpdatedComponent 置 NULL，移动组件会彻底失效。
	ProjectileMovement->bShouldBounce = true;
	ProjectileMovement->Bounciness = 0.f;
	ProjectileMovement->Friction = 0.f;
	ProjectileMovement->ProjectileGravityScale = 0.f;
}


void ACS_CubeProjectile::BeginPlay()
{
	Super::BeginPlay();

	// 米 → 厘米
	MaxTravelDistance = MaxTravelDistanceMeters * 100.f;

	MeshComponent->SetRelativeScale3D(FVector(CubeSize / 100.f));

	// 碰撞球放大到 2 倍：高速飞行时擦边也能检测到
	CollisionComponent->SetSphereRadius(CubeSize * 0.5f * 2.0f);

	ProjectileMovement->MaxSpeed = InitialSpeed + 1.f;
	ProjectileMovement->ProjectileGravityScale = GravityScale;

	// 忽略发射者身上的所有碰撞组件（含胶囊、相机、弹簧臂）
	if (bIgnoreOwner && GetOwner())
	{
		AActor* OwnerActor = GetOwner();
		CollisionComponent->IgnoreActorWhenMoving(OwnerActor, true);

		TArray<UPrimitiveComponent*> OwnerPrims;
		OwnerActor->GetComponents<UPrimitiveComponent>(OwnerPrims);
		for (UPrimitiveComponent* Comp : OwnerPrims)
		{
			if (Comp && Comp != CollisionComponent)
			{
				CollisionComponent->IgnoreComponentWhenMoving(Comp, true);
			}
		}

		if (APawn* OwnerPawn = Cast<APawn>(OwnerActor))
		{
			if (AController* Ctrl = OwnerPawn->GetController())
			{
				TArray<UPrimitiveComponent*> CtrlPrims;
				Ctrl->GetComponents<UPrimitiveComponent>(CtrlPrims);
				for (UPrimitiveComponent* C : CtrlPrims)
				{
					if (C)
					{
						CollisionComponent->IgnoreComponentWhenMoving(C, true);
					}
				}
			}
		}
	}

	CollisionComponent->OnComponentHit.AddDynamic(this, &ACS_CubeProjectile::OnProjectileHit);

	// 出生保护：方块生成时可能与玩家重叠，先忽略碰撞一小段时间
	bCollisionArmed = (SpawnCollisionGraceTime <= 0.f);
	if (!bCollisionArmed)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				SpawnGraceTimerHandle, this,
				&ACS_CubeProjectile::ArmCollision,
				SpawnCollisionGraceTime, false);
		}
		else
		{
			bCollisionArmed = true;
		}
	}

	if (bHasPendingLaunch)
	{
		ApplyLaunchVelocity();
	}
}


void ACS_CubeProjectile::ArmCollision()
{
	bCollisionArmed = true;
}


void ACS_CubeProjectile::LaunchInDirection(const FVector& Direction)
{
	if (Direction.IsNearlyZero())
	{
		return;
	}

	PendingLaunchDirection = Direction.GetSafeNormal();
	bHasPendingLaunch = true;
	ApplyLaunchVelocity();
}


void ACS_CubeProjectile::RestartMovement(const FVector& NewVelocity)
{
	if (!ProjectileMovement)
	{
		return;
	}

	// InitialSpeed 必须为 0，否则组件会用重算速度覆盖我们设的值
	ProjectileMovement->InitialSpeed = 0.f;

	// 重绑 UpdatedComponent：碰撞后它会被置 NULL，不重绑就动不了
	ProjectileMovement->SetUpdatedComponent(CollisionComponent);
	ProjectileMovement->bSimulationEnabled = true;
	ProjectileMovement->SetComponentTickEnabled(true);
	ProjectileMovement->Activate(true);

	ProjectileMovement->Velocity = NewVelocity;
	ProjectileMovement->UpdateComponentVelocity();
	ProjectileMovement->Velocity = NewVelocity;
}


void ACS_CubeProjectile::ApplyLaunchVelocity()
{
	if (!ProjectileMovement || !bHasPendingLaunch)
	{
		return;
	}

	bSwitchedToPhysics = false;
	AlreadyHitEnemies.Reset();

	// 应用「是否可以有碰撞」开关
	if (CollisionComponent)
	{
		if (bEnableCollisionDetection)
		{
			// 正常：物理碰撞 + 查询
			CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			CollisionComponent->SetCollisionResponseToAllChannels(ECR_Block);
			CollisionComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
			CollisionComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		}
		else
		{
			// 穿透模式：
			//   用 QueryOnly + 全部 Overlap —— 扫掠时不再阻挡（能穿过去），
			//   但 Overlap 查询仍然有效（球形检测敌人依然生效）。
			//
			// 注意：不能只改成 QueryOnly 而不改响应，
			// 因为 Block 响应在 QueryOnly 下仍会让移动组件被挡住。
			CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			CollisionComponent->SetCollisionResponseToAllChannels(ECR_Overlap);
			CollisionComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
			CollisionComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		}
	}

	const FVector LaunchVel = PendingLaunchDirection * InitialSpeed;

	ProjectileMovement->MaxSpeed = InitialSpeed + 1.f;
	ProjectileMovement->ProjectileGravityScale = GravityScale;

	RestartMovement(LaunchVel);

	bIsFlying = true;
	bHasCollided = false;
	bIsReturning = false;
	SetState(ECSProjectileState::Flying);

	LaunchOrigin = GetActorLocation();
	bLaunchOriginValid = true;
	LaunchStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 距离检测（低频）
	World->GetTimerManager().SetTimer(
		DistanceCheckTimerHandle, this,
		&ACS_CubeProjectile::CheckTravelDistance,
		DistanceCheckInterval, true);

	// 敌人范围检测（球形 Overlap，不依赖物理碰撞）
	World->GetTimerManager().SetTimer(
		EnemyDetectTimerHandle, this,
		&ACS_CubeProjectile::CheckNearbyEnemies,
		EnemyDetectInterval, true);

	// 立刻检测一次：贴脸发射时方块一出生就在敌人旁边
	CheckNearbyEnemies();
}


void ACS_CubeProjectile::CheckNearbyEnemies()
{
	if (bIsReturning || State == ECSProjectileState::Recalled)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(EnemyDetectTimerHandle);
		}
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Center = GetActorLocation();

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CS_EnemyDetect), false, this);
	if (GetOwner())
	{
		QueryParams.AddIgnoredActor(GetOwner());
	}

	TArray<FOverlapResult> Overlaps;
	const FCollisionShape Shape = FCollisionShape::MakeSphere(EnemyDetectRadius);

	const bool bFound = World->OverlapMultiByObjectType(
		Overlaps, Center, FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		Shape, QueryParams);

	if (!bFound)
	{
		return;
	}

	for (const FOverlapResult& Overlap : Overlaps)
	{
		ACS_EnemyCharacter* Enemy = Cast<ACS_EnemyCharacter>(Overlap.GetActor());
		if (!Enemy || AlreadyHitEnemies.Contains(Enemy))
		{
			continue;
		}

		// 推力方向 = 方块飞行方向
		FVector PushDir = ProjectileMovement
			? ProjectileMovement->Velocity.GetSafeNormal()
			: PendingLaunchDirection;
		if (PushDir.IsNearlyZero())
		{
			PushDir = PendingLaunchDirection;
		}

		AlreadyHitEnemies.Add(Enemy);
		ProcessEnemyHit(Enemy, PushDir);
		break;
	}
}


void ACS_CubeProjectile::CheckTravelDistance()
{
	if (bIsReturning || State == ECSProjectileState::Recalled)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(DistanceCheckTimerHandle);
		}
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(DistanceCheckTimerHandle);
		}
		SetState(ECSProjectileState::Recalled);
		Destroy();
		return;
	}

	const float DistToOwner = FVector::Dist(GetActorLocation(), OwnerActor->GetActorLocation());

	// 超距回收（无论是否碰撞过、是否已转物理，都要生效）
	if (DistToOwner >= MaxTravelDistance)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(DistanceCheckTimerHandle);
			World->GetTimerManager().ClearTimer(RecallDelayTimerHandle);
		}
		StartRecall();
		return;
	}

	// 卡死兜底：贴墙/贴地/墙角时方块可能嵌在障碍里，碰撞检测不到，
	// 若离玩家很近且几乎不动，判定卡死直接回收。
	if (!bHasCollided && !bSwitchedToPhysics)
	{
		const float Speed = ProjectileMovement ? ProjectileMovement->Velocity.Size() : 0.f;

		if (DistToOwner <= StuckTeleportRecoverDistance && Speed <= StuckSpeedThreshold)
		{
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().ClearTimer(DistanceCheckTimerHandle);
				World->GetTimerManager().ClearTimer(EnemyDetectTimerHandle);
				World->GetTimerManager().ClearTimer(RecallDelayTimerHandle);
			}

			SetState(ECSProjectileState::Recalled);
			Destroy();
		}
	}
}


void ACS_CubeProjectile::ProcessEnemyHit(ACS_EnemyCharacter* Enemy, const FVector& PushDir)
{
	if (!Enemy)
	{
		return;
	}

	// 推力方向严格投影到水平面，敌人只沿此方向后退
	LockedPushDirection = FVector(PushDir.X, PushDir.Y, 0.f).GetSafeNormal();

	// 记录撞击时的飞行速度，用于穿透后恢复
	const FVector PreservedVelocity = ProjectileMovement
		? ProjectileMovement->Velocity
		: PendingLaunchDirection * InitialSpeed;

	// 「是否可以弹开敌人」关闭时，跳过推敌部分
	const bool bShouldPush = bEnableEnemyKnockback && !LockedPushDirection.IsNearlyZero();

	if (bShouldPush)
	{
		if (bIsPushingEnemy)
		{
			StopEnemyPush();
		}

		if (UCharacterMovementComponent* EnemyMove = Enemy->GetCharacterMovement())
		{
			SavedEnemyMaxWalkSpeed = EnemyMove->MaxWalkSpeed;
			SavedEnemyBrakingDeceleration = EnemyMove->BrakingDecelerationWalking;
		}

		// 锁 AI，不要求落地解除（水平推时敌人全程贴地）
		Enemy->StartKnockback(EnemyKnockbackLockTime, /*bRequireLandingToEnd=*/false);

		bIsPushingEnemy = true;
		PushedEnemy = Enemy;

		// ---------- 让方块穿透这个敌人 ----------
		// 注意：IgnoreActorWhenMoving 在 OnHit 回调里调用已经太晚，
		// 物理扫掠在回调之前就完成了碰撞判定，方块这一帧已被挡住。
		// 但下一帧开始生效，配合下面的"恢复速度"就能穿过去。
		CollisionComponent->IgnoreActorWhenMoving(Enemy, true);

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				PushTimerHandle, this,
				&ACS_CubeProjectile::ApplyEnemyPush,
				CS_ENEMY_PUSH_INTERVAL, true);

			ApplyEnemyPush();

			World->GetTimerManager().SetTimer(
				PushStopTimerHandle,
				FTimerDelegate::CreateWeakLambda(this, [this]() { StopEnemyPush(); }),
				EnemyPushDuration, false);
		}
	}

	// ---------- 方块继续向前飞行（穿透）----------
	//
	// 撞到敌人不转物理、不停住、不回收，
	// 只把速度恢复成撞击前的值，让方块继续沿原方向飞。
	// 配合上面的 IgnoreActorWhenMoving，下一帧起就不会再被该敌人挡住。
	if (ProjectileMovement)
	{
		// 若方块已经因为碰撞被清速，这里恢复
		if (PreservedVelocity.SizeSquared() > 1.f)
		{
			RestartMovement(PreservedVelocity);
		}
	}

	// 只把敌人记为"已命中"，方块本身保持 Flying 状态
	// 不设置 bHasCollided，不切换状态，不启动回收
}


void ACS_CubeProjectile::HitEnemyImmediately(ACS_EnemyCharacter* Enemy, const FVector& Direction)
{
	ProcessEnemyHit(Enemy, Direction);
}


void ACS_CubeProjectile::ApplyEnemyPush()
{
	ACS_EnemyCharacter* Enemy = PushedEnemy.Get();
	if (!Enemy)
	{
		StopEnemyPush();
		return;
	}

	// 通过敌人的接口，让它在自己的 Tick 里每帧压速度
	Enemy->SetKnockbackVelocityOverride(LockedPushDirection * EnemyPushSpeed, true);
}


void ACS_CubeProjectile::StopEnemyPush()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PushTimerHandle);
		World->GetTimerManager().ClearTimer(PushStopTimerHandle);
	}

	if (ACS_EnemyCharacter* Enemy = PushedEnemy.Get())
	{
		Enemy->SetKnockbackVelocityOverride(FVector::ZeroVector, false);

		if (UCharacterMovementComponent* MoveComp = Enemy->GetCharacterMovement())
		{
			MoveComp->MaxWalkSpeed = SavedEnemyMaxWalkSpeed;
			MoveComp->BrakingDecelerationWalking = SavedEnemyBrakingDeceleration;
		}
	}

	bIsPushingEnemy = false;
	PushedEnemy = nullptr;
}


void ACS_CubeProjectile::OnProjectileHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	// 出生保护期内忽略
	if (!bCollisionArmed)
	{
		return;
	}

	if (!OtherActor || OtherActor == this)
	{
		return;
	}

	// 撞到拥有者：正常忽略；但若已转物理并弹回来撞到玩家，说明碰撞流程走完，需开始回收。
	if (bIgnoreOwner && OtherActor == GetOwner())
	{
		if (bSwitchedToPhysics && !bHasCollided)
		{
			bHasCollided = true;
			bIsFlying = false;
			SetState(ECSProjectileState::Blocked);

			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimer(
					RecallDelayTimerHandle, this,
					&ACS_CubeProjectile::HandleRecallDelayFinished,
					RecallDelayAfterHit, false);
			}
			else
			{
				HandleRecallDelayFinished();
			}
		}
		return;
	}

	// 撞到敌人：交给 ProcessEnemyHit（Overlap 检测已覆盖，这里兜底）
	if (IsEnemyActor(OtherActor))
	{
		if (ACS_EnemyCharacter* Enemy = Cast<ACS_EnemyCharacter>(OtherActor))
		{
			if (!AlreadyHitEnemies.Contains(Enemy))
			{
				FVector PushDir = ProjectileMovement
					? ProjectileMovement->Velocity.GetSafeNormal()
					: PendingLaunchDirection;
				if (PushDir.IsNearlyZero())
				{
					PushDir = PendingLaunchDirection;
				}

				AlreadyHitEnemies.Add(Enemy);
				ProcessEnemyHit(Enemy, PushDir);
			}
		}
		return;
	}

	// 撞到墙/地面/道具
	if (bHasCollided || bIsReturning)
	{
		return;
	}

	bHasCollided = true;
	bIsFlying = false;

	// 转刚体（必须先停移动组件，再开物理）
	if (bEnablePhysicsOnImpact)
	{
		SwitchToPhysicsOnImpact(Hit);
	}
	else if (ProjectileMovement)
	{
		ProjectileMovement->Velocity = FVector::ZeroVector;
		ProjectileMovement->UpdateComponentVelocity();
	}

	SetState(ECSProjectileState::Blocked);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RecallDelayTimerHandle, this,
			&ACS_CubeProjectile::HandleRecallDelayFinished,
			RecallDelayAfterHit, false);
	}
	else
	{
		HandleRecallDelayFinished();
	}
}


void ACS_CubeProjectile::SwitchToPhysicsOnImpact(const FHitResult& Hit, bool bIsEnemyImpact)
{
	if (!CollisionComponent)
	{
		return;
	}

	const FVector ImpactVelocity = ProjectileMovement ? ProjectileMovement->Velocity : FVector::ZeroVector;
	const float ImpactSpeed = ImpactVelocity.Size();

	// 第 1 步：停掉并解绑移动组件（必须先做，否则和物理打架）
	if (ProjectileMovement)
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
		ProjectileMovement->SetComponentTickEnabled(false);
		ProjectileMovement->bSimulationEnabled = false;
		ProjectileMovement->SetUpdatedComponent(nullptr);
	}

	// 第 2 步：开启刚体
	CollisionComponent->SetSimulatePhysics(true);
	CollisionComponent->SetMassOverrideInKg(NAME_None, ImpactPhysicsMass, true);
	CollisionComponent->SetLinearDamping(ImpactLinearDamping);
	CollisionComponent->SetAngularDamping(ImpactAngularDamping);
	CollisionComponent->SetEnableGravity(true);

	// 第 3 步：反弹系数（若没有物理材质则跳过，默认行为仍然可用）
	if (UPhysicalMaterial* PhysMat = CollisionComponent->BodyInstance.GetSimplePhysicalMaterial())
	{
		PhysMat->Restitution = ImpactRestitution;
	}

	// 第 4 步：按法线反弹 V' = -V - 2(-V·N)N
	if (ImpactSpeed > 1.f && !Hit.ImpactNormal.IsNearlyZero())
	{
		const FVector N = Hit.ImpactNormal.GetSafeNormal();
		const FVector Reflected = -ImpactVelocity - 2.f * FVector::DotProduct(-ImpactVelocity, N) * N;
		CollisionComponent->SetPhysicsLinearVelocity(Reflected * ImpactRestitution);
	}

	// 第 5 步：随机旋转，让方块翻滚
	// 撞墙与撞敌人分别由蓝图开关控制是否旋转，以及翻转力度
	const bool bAllowRotation = bIsEnemyImpact ? bEnableRotationOnEnemyImpact : bEnableRotationOnSurfaceImpact;
	const float AngularAmount = bIsEnemyImpact ? EnemyImpactAngularImpulse : SurfaceImpactAngularImpulse;

	if (bAllowRotation && AngularAmount > 0.f)
	{
		CollisionComponent->AddAngularImpulseInDegrees(
			FMath::VRand() * AngularAmount, NAME_None, /*bVelChange=*/true);
	}

	bSwitchedToPhysics = true;
}


void ACS_CubeProjectile::RestoreFromPhysics()
{
	if (!bSwitchedToPhysics)
	{
		return;
	}

	if (CollisionComponent)
	{
		CollisionComponent->SetSimulatePhysics(false);
		CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	if (ProjectileMovement)
	{
		ProjectileMovement->bSimulationEnabled = true;
		ProjectileMovement->SetComponentTickEnabled(true);
		ProjectileMovement->SetUpdatedComponent(CollisionComponent);
		ProjectileMovement->Activate(true);
	}

	bSwitchedToPhysics = false;
}


void ACS_CubeProjectile::HandleRecallDelayFinished()
{
	StartRecall();
}


void ACS_CubeProjectile::StartRecall(float ReturnSpeed)
{
	if (bIsReturning || State == ECSProjectileState::Recalled)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RecallDelayTimerHandle);
		World->GetTimerManager().ClearTimer(DistanceCheckTimerHandle);
		World->GetTimerManager().ClearTimer(EnemyDetectTimerHandle);
	}

	StopEnemyPush();

	// 刚体模式切回移动组件（回收要用 Homing）
	RestoreFromPhysics();

	AActor* Target = GetOwner();
	RecallTarget = Target;

	if (!Target)
	{
		SetState(ECSProjectileState::Recalled);
		Destroy();
		return;
	}

	if (ReturnSpeed > 0.f)
	{
		RecallSpeed = ReturnSpeed;
	}

	bIsReturning = true;
	bIsFlying = false;

	if (!ProjectileMovement)
	{
		SetState(ECSProjectileState::Recalled);
		Destroy();
		return;
	}

	ProjectileMovement->ProjectileGravityScale = 0.f;
	ProjectileMovement->MaxSpeed = RecallSpeed + 1.f;

	// 开启引擎内置 Homing 追踪玩家（会自动子步进，不穿模）
	ProjectileMovement->bIsHomingProjectile = true;
	ProjectileMovement->HomingAccelerationMagnitude = RecallHomingAcceleration;
	if (USceneComponent* TargetRoot = Target->GetRootComponent())
	{
		ProjectileMovement->HomingTargetComponent = TargetRoot;
	}

	// ---------- 回收阶段碰撞设置 ----------
	if (bEnableCollisionDetection)
	{
		// 正常模式：保持阻挡，方块会被墙挡住，
		// 但 Homing 每帧朝玩家加速，所以会贴着墙滑动绕回来。
		CollisionComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		CollisionComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
		CollisionComponent->SetCollisionResponseToChannel(ECC_Destructible, ECR_Block);
	}
	else
	{
		// 穿透模式：回收也保持穿透，直接穿墙飞回玩家，避免卡在墙里。
		CollisionComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Overlap);
		CollisionComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
		CollisionComponent->SetCollisionResponseToChannel(ECC_Destructible, ECR_Overlap);
	}

	SetState(ECSProjectileState::Returning);

	// 回收超时：防止被彻底卡死（例如被夹在两堵墙之间）永远飞不回来
	RecallElapsed = 0.f;
	RecallStartTime = 0.f;

	const FVector Dir = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal();
	RestartMovement(Dir.IsNearlyZero() ? FVector::ZeroVector : Dir * RecallSpeed);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RecallCheckTimerHandle, this,
			&ACS_CubeProjectile::CheckRecallArrival,
			CS_RECALL_CHECK_INTERVAL, true);
	}

	CheckRecallArrival();
}


void ACS_CubeProjectile::CheckRecallArrival()
{
	if (!bIsReturning)
	{
		return;
	}

	AActor* Target = RecallTarget.Get();
	if (!Target)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(RecallCheckTimerHandle);
		}
		bIsReturning = false;
		SetState(ECSProjectileState::Recalled);
		Destroy();
		return;
	}

	const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();

	// 用水平距离判定：贴墙时有高度差，3D 距离可能永远判定不到
	const float Dist2D = FVector(ToTarget.X, ToTarget.Y, 0.f).Size();

	if (Dist2D <= RecallAcceptRadius)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(RecallCheckTimerHandle);
		}
		bIsReturning = false;
		SetState(ECSProjectileState::Recalled);
		Destroy();
		return;
	}

	// 回收超时兜底：方块被夹死时强制销毁
	if (UWorld* World = GetWorld())
	{
		const float Now = World->GetTimeSeconds();
		if (RecallStartTime <= 0.f)
		{
			RecallStartTime = Now;
		}
		else if (Now - RecallStartTime > RecallTimeout)
		{
			World->GetTimerManager().ClearTimer(RecallCheckTimerHandle);
			bIsReturning = false;
			SetState(ECSProjectileState::Recalled);
			Destroy();
			return;
		}
	}

	// 每帧刷新朝玩家的速度，让方块能沿墙滑行
	if (ProjectileMovement)
	{
		const FVector Dir = ToTarget.GetSafeNormal();
		if (!Dir.IsNearlyZero())
		{
			ProjectileMovement->Velocity = Dir * RecallSpeed;
		}
	}
}


bool ACS_CubeProjectile::IsEnemyActor(const AActor* Actor)
{
	return Actor && Actor->IsA<ACS_EnemyCharacter>();
}


FVector ACS_CubeProjectile::GetFlightVelocity() const
{
	return ProjectileMovement ? ProjectileMovement->Velocity : FVector::ZeroVector;
}


void ACS_CubeProjectile::SetState(ECSProjectileState NewState)
{
	if (State == NewState)
	{
		return;
	}

	State = NewState;
	OnStateChanged.Broadcast();
}


ACS_CubeProjectile* ACS_CubeProjectile::SpawnAndLaunch(
	UObject* WorldContextObject,
	const FTransform& SpawnTransform,
	const FVector& Direction,
	AActor* InOwner,
	APawn* InInstigator,
	TSubclassOf<ACS_CubeProjectile> ProjectileClass)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	if (!World)
	{
		return nullptr;
	}

	FVector FinalDirection = Direction;
	if (FinalDirection.IsNearlyZero())
	{
		FinalDirection = SpawnTransform.GetRotation().GetForwardVector();
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = InOwner;
	SpawnParams.Instigator = InInstigator;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	UClass* UseClass = ProjectileClass ? ProjectileClass.Get() : ACS_CubeProjectile::StaticClass();
	ACS_CubeProjectile* Projectile = World->SpawnActor<ACS_CubeProjectile>(
		UseClass, SpawnTransform, SpawnParams);

	if (Projectile)
	{
		Projectile->LaunchInDirection(FinalDirection);
	}

	return Projectile;
}
