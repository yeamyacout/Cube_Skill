// Fill out your copyright notice in the Description page of Project Settings.


#include "Projectile/CS_CubeProjectile.h"

#include "Characters/CS_EnemyCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

// 引擎内置立方体网格路径（不依赖项目美术资源）
#define CS_CUBE_MESH_PATH TEXT("/Engine/BasicShapes/Cube.Cube")


ACS_CubeProjectile::ACS_CubeProjectile()
{
	// 需要每帧：检测超距、检测敌人爆炸、检测回收到位
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// ---------- 碰撞球（根组件） ----------
	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	SetRootComponent(CollisionComponent);

	CollisionComponent->InitSphereRadius(20.f);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Block);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);

	// 关键：对 Pawn（敌人/玩家）完全忽略碰撞。
	//
	// 为什么不靠 OnHit 里 IgnoreActorWhenMoving：
	//   ProjectileMovementComponent 的扫掠移动在世界查询阶段就会检测碰撞，
	//   等 OnHit 回调触发时，物理已经判定过"被挡住"了，
	//   此时再忽略该 Actor 也来不及，方块这一帧就已经停住。
	//   只有在碰撞配置层面直接不响应 Pawn，才能保证方块穿过敌人继续飞。
	//
	// 敌人的检测与炸开完全由 Tick 中的 CheckForEnemyExplosion 通过
	// Overlap 查询完成，不依赖碰撞响应，所以忽略 Pawn 不影响炸开逻辑。
	CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	// 关键：必须开启，否则 OnComponentHit 回调根本不会触发。
	//   ProjectileMovementComponent 的扫掠会把方块挡在墙外，
	//   但如果这个开关是关的，我们收不到任何回调，
	//   就永远不知道"撞墙了"，方块会一直贴着墙卡住。
	CollisionComponent->SetNotifyRigidBodyCollision(true);

	// ---------- 立方体网格（视觉） ----------
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(CollisionComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(CS_CUBE_MESH_PATH);
	if (CubeMesh.Succeeded())
	{
		MeshComponent->SetStaticMesh(CubeMesh.Object);
	}

	// ---------- 飞行组件 ----------
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionComponent;
	ProjectileMovement->InitialSpeed = InitialSpeed;
	ProjectileMovement->MaxSpeed = InitialSpeed;
	ProjectileMovement->bRotationFollowsVelocity = false;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->ProjectileGravityScale = GravityScale;
	ProjectileMovement->bSweepCollision = true;
	ProjectileMovement->Velocity = FVector(1.f, 0.f, 0.f) * InitialSpeed;
}


void ACS_CubeProjectile::BeginPlay()
{
	Super::BeginPlay();

	// 网格缩放：引擎内置 Cube 是 100x100x100
	MeshComponent->SetRelativeScale3D(FVector(CubeSize / 100.f));

	// 碰撞球半径略大于立方体半边长
	CollisionComponent->SetSphereRadius(CubeSize * 0.5f * 1.2f);

	ProjectileMovement->InitialSpeed = InitialSpeed;
	ProjectileMovement->MaxSpeed = InitialSpeed;
	ProjectileMovement->ProjectileGravityScale = GravityScale;

	if (bIgnoreOwner && GetOwner())
	{
		CollisionComponent->IgnoreActorWhenMoving(GetOwner(), true);
	}

	CollisionComponent->OnComponentHit.AddDynamic(this, &ACS_CubeProjectile::OnProjectileHit);

	LastFrameLocation = GetActorLocation();

	// 如果 LaunchInDirection 在 BeginPlay 之前被调用过（SpawnAndLaunch 的常见时序），
	// 这里再应用一次速度，确保它在组件完全初始化后依然正确。
	if (bHasPendingLaunch)
	{
		ApplyLaunchVelocity();
	}

	// 刻意不调用 SetLifeSpan：方块存在到被回收为止
}


void ACS_CubeProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const FVector CurrentLocation = GetActorLocation();
	switch (State)
	{
	case ECSProjectileState::Flying:
	{
		// 1) 累计飞行距离，超过上限自动回收
		TraveledDistance += FVector::Dist(CurrentLocation, LastFrameLocation);

		// 2) 兜底检测：如果方块这一帧几乎没动，说明被挡住了（撞墙/撞地）。
		const float FrameDist = FVector::Dist(CurrentLocation, LastFrameLocation);

		// 起飞宽限期：方块刚生成时可能与地面/角色碰撞体重叠，
		// 前几帧位移很小属正常，不能立刻判定为卡住。
		if (StuckGraceTime > 0.f)
		{
			StuckGraceTime -= DeltaSeconds;
			LastFrameLocation = CurrentLocation;
			TraveledDistance += FrameDist;
			CheckForEnemyExplosion(DeltaSeconds);
			break;
		}

		// 正常飞行每帧至少移动 Speed*DeltaTime（2000cm/s * 0.016 ≈ 32cm），
		// 如果一帧位移小于 2cm，就是被卡住了。
		const bool bStuck = (TraveledDistance > 5.f) && (FrameDist < 2.f);

		LastFrameLocation = CurrentLocation;

		// 撞墙或超距 -> 自动回收
		if (bStuck || TraveledDistance >= MaxTravelDistance)
		{
			// 注意：这里不再调用 StopMovementImmediately。
			// 原因：它会停用 ProjectileMovement 组件，导致后续回收移动失效。
			// 回收阶段本来就由 Tick 自己驱动位移，不依赖该组件。
			StartRecall(GetOwner());
			break;
		}

		// 3) 检测周围敌人并炸开（兜底，与 Hit 回调互为补充）
		CheckForEnemyExplosion(DeltaSeconds);
		break;
	}

	case ECSProjectileState::Returning:
	{
		AActor* Target = RecallTarget.Get();
		if (!Target)
		{
			Destroy();
			break;
		}

		// 回收阶段完全自己驱动移动，不依赖 ProjectileMovement。
		//
		// 为什么不依赖 ProjectileMovement：
		//   撞墙时调用过 StopMovementImmediately()，它会把组件停用，
		//   之后再设置 Velocity 也不会生效（表现为方块原地不动）。
		//   改用 SetActorLocation 直接位移，简单可靠，且能穿墙飞回玩家身边。
		const FVector TargetLoc = Target->GetActorLocation();
		const FVector ToTarget = TargetLoc - CurrentLocation;
		const float Dist3D = ToTarget.Size();
		const float Dist2D = FVector(ToTarget.X, ToTarget.Y, 0.f).Size();

		// 到位判定：2D 或 3D 距离任一满足都算接住
		if (Dist2D <= RecallAcceptRadius || Dist3D <= RecallAcceptRadius)
		{
			State = ECSProjectileState::Recalled;
			Destroy();
			break;
		}

		// 朝目标直线位移
		const FVector Dir = ToTarget.GetSafeNormal();
		if (!Dir.IsNearlyZero())
		{
			const float Step = FMath::Min(RecallSpeed * DeltaSeconds, Dist3D);
			SetActorLocation(CurrentLocation + Dir * Step, /*bSweep=*/false);
		}

		// 超时保护
		RecallElapsed += DeltaSeconds;
		if (RecallElapsed > RecallMaxDuration)
		{
			State = ECSProjectileState::Recalled;
			Destroy();
			break;
		}

		// 回收途中仍然炸开挡路的敌人
		CheckForEnemyExplosion(DeltaSeconds);

		LastFrameLocation = GetActorLocation();
		break;
	}

	default:
		break;
	}
}


void ACS_CubeProjectile::CheckForEnemyExplosion(float DeltaSeconds)
{
	if (!GetWorld())
	{
		return;
	}

	// 用碰撞球自身的半径 + 一个较小的容差来检测接触。
	//
	// 容差为什么不能太小：
	//   方块飞行速度 2000 cm/s，60fps 下每帧移动约 33cm，
	//   如果检测半径刚好等于方块半径，高速飞行时可能"跨过"敌人检测不到。
	//   这里多加 60cm 容差，配合"一帧最多跨 33cm"能保证不漏检。
	//
	// 注意：检测半径不影响爆炸半径（ExplosionRadius 单独控制），
	//       检测只是"什么时候引爆"，爆炸范围由 ExplosionRadius 决定。
	const FVector Center = GetActorLocation();
	const float QueryRadius = CubeSize * 0.5f + 60.f;

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CS_BlastProbe), false, this);
	Params.AddIgnoredActor(this);
	if (bIgnoreOwner && GetOwner())
	{
		Params.AddIgnoredActor(GetOwner());
	}

	const bool bFound = GetWorld()->OverlapMultiByObjectType(
		Overlaps,
		Center,
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(QueryRadius),
		Params);

	if (!bFound)
	{
		return;
	}

	// 只要检测到有敌人接触，就触发一次以方块为中心的爆炸
	bool bAnyEnemy = false;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (IsEnemyActor(Overlap.GetActor()))
		{
			bAnyEnemy = true;
			break;
		}
	}

	if (bAnyEnemy)
	{
		ExplodeAt(Center, ExplosionRadius);
	}
}


void ACS_CubeProjectile::ExplodeAt(const FVector& Epicenter, float Radius)
{
	if (!GetWorld())
	{
		return;
	}

	const float UseRadius = (Radius > 0.f) ? Radius : ExplosionRadius;
	const float Now = GetWorld()->GetTimeSeconds();

	// 查询半径内的所有 Pawn
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CS_Explosion), false, this);
	Params.AddIgnoredActor(this);
	if (bIgnoreOwner && GetOwner())
	{
		Params.AddIgnoredActor(GetOwner());
	}

	const bool bFound = GetWorld()->OverlapMultiByObjectType(
		Overlaps,
		Epicenter,
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(UseRadius),
		Params);

	if (!bFound)
	{
		return;
	}

	// 去重：同一个敌人可能有多个体积
	TSet<ACS_EnemyCharacter*> Processed;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Other = Overlap.GetActor();
		if (!IsEnemyActor(Other))
		{
			continue;
		}

		ACS_EnemyCharacter* Enemy = Cast<ACS_EnemyCharacter>(Other);
		if (!Enemy || Processed.Contains(Enemy))
		{
			continue;
		}
		Processed.Add(Enemy);

		// 冷却检查：同一个敌人短时间内不重复炸，避免速度失控
		TWeakObjectPtr<ACS_EnemyCharacter> Key(Enemy);
		if (const float* LastTime = LastBlastTimeMap.Find(Key))
		{
			if (Now - *LastTime < BlastCooldown)
			{
				continue;
			}
		}
		LastBlastTimeMap.Add(Key, Now);

		// 距离越近，冲量越强（线性衰减）
		const float Dist = FVector::Dist(Epicenter, Enemy->GetActorLocation());
		const float Strength = FMath::Clamp(1.f - (Dist / UseRadius), 0.35f, 1.f);

		BlastEnemy(Enemy, Epicenter, Strength);
	}

	// 定期清理已销毁敌人的记录，避免 Map 无限增长
	if (LastBlastTimeMap.Num() > 32)
	{
		for (auto It = LastBlastTimeMap.CreateIterator(); It; ++It)
		{
			if (!It.Key().IsValid())
			{
				It.RemoveCurrent();
			}
		}
	}
}


void ACS_CubeProjectile::BlastEnemy(ACS_EnemyCharacter* Enemy, const FVector& Epicenter, float Strength)
{
	if (!Enemy)
	{
		return;
	}

	// ---------- 第一步：先锁住敌人的移动 ----------
	// 必须在施加弹飞速度之前做，否则 AI 的移动输入会和弹飞速度打架，
	// 表现为"一边被弹上天，一边还在往玩家方向走"。
	// StartKnockback 会：
	//   - 停止 AI 寻路
	//   - 忽略后续所有移动输入
	//   - 在落地时自动恢复
	Enemy->StartKnockback(KnockbackLockDuration);

	// ---------- 第二步：计算弹开方向 ----------
	// 方向 = 从爆炸中心指向敌人（即"从方块向外炸开"）
	FVector Dir = Enemy->GetActorLocation() - Epicenter;

	// 只取水平面，垂直方向单独处理，避免敌人被炸到地底或飞得歪歪扭扭
	FVector HorizontalDir = FVector(Dir.X, Dir.Y, 0.f).GetSafeNormal();

	// 极端情况：敌人正好在爆炸中心，水平方向为零 -> 给一个随机方向
	if (HorizontalDir.IsNearlyZero())
	{
		const float RandomAngle = FMath::FRandRange(0.f, 2.f * PI);
		HorizontalDir = FVector(FMath::Cos(RandomAngle), FMath::Sin(RandomAngle), 0.f);
	}

	// ---------- 第三步：组合出抛射速度 ----------
	// 水平分量 + 向上分量，形成抛物线弹飞
	const float HorizontalSpeed = BlastImpulse * Strength;
	const float VerticalSpeed = BlastImpulse * Strength * BlastUpwardRatio;

	FVector LaunchVelocity = HorizontalDir * HorizontalSpeed;
	LaunchVelocity.Z = VerticalSpeed;

	// ---------- 第四步：施加弹开 ----------
	// LaunchCharacter 会把角色设为"空中"状态并正确应用速度，
	// 手感最接近"被炸飞"。
	Enemy->LaunchCharacter(LaunchVelocity, /*bXYOverride=*/true, /*bZOverride=*/true);
}


bool ACS_CubeProjectile::IsEnemyActor(const AActor* Actor)
{
	// 项目里所有敌人都是 ACS_EnemyCharacter 的子类。
	// 用基类判断，以后新增的敌人类都自动生效。
	return Actor && Actor->IsA<ACS_EnemyCharacter>();
}


void ACS_CubeProjectile::LaunchInDirection(const FVector& Direction)
{
	if (!ProjectileMovement || Direction.IsNearlyZero())
	{
		return;
	}

	const FVector NormalizedDir = Direction.GetSafeNormal();

	// 记录期望的速度，BeginPlay 里还会再应用一次。
	// 原因：SpawnAndLaunch 会在 SpawnActor 后立刻调用本函数，
	//       此时 BeginPlay 可能还没执行完，部分组件参数尚未初始化。
	//       通过在 BeginPlay 里复用 PendingLaunchDirection，可以保证
	//       无论调用时序如何，速度一定被正确设置。
	PendingLaunchDirection = NormalizedDir;
	bHasPendingLaunch = true;

	// 立刻应用一次（正常时序下 BeginPlay 已执行，这里直接生效）
	ApplyLaunchVelocity();

	SetState(ECSProjectileState::Flying);
}


void ACS_CubeProjectile::ApplyLaunchVelocity()
{
	if (!ProjectileMovement || !bHasPendingLaunch)
	{
		return;
	}

	// 速度 = 方向 * 初速。使用 FVector 计算，避免被当成 2D 向量。
	const FVector LaunchVel = PendingLaunchDirection * InitialSpeed;

	// 先设置 MaxSpeed 再设置 Velocity，顺序很重要：
	// ProjectileMovementComponent 会在 Tick 里把 Velocity 钳制到 MaxSpeed，
	// 如果 MaxSpeed 还是旧值（比如 0），速度会被清掉。
	ProjectileMovement->InitialSpeed = InitialSpeed;
	ProjectileMovement->MaxSpeed = InitialSpeed + 1.f;
	ProjectileMovement->ProjectileGravityScale = GravityScale;

	ProjectileMovement->Activate(true);
	ProjectileMovement->SetUpdatedComponent(CollisionComponent);
	ProjectileMovement->Velocity = LaunchVel;

	// 重置飞行距离统计
	TraveledDistance = 0.f;
	LastFrameLocation = GetActorLocation();

	// 重要：刚生成时可能因为与地面/自身重叠而被判定为"卡住"，
	// 这里给一个短暂的宽限期，避免起飞瞬间就被误判回收。
	StuckGraceTime = 0.25f;
}


void ACS_CubeProjectile::StartRecall(AActor* TargetActor, float ReturnSpeed)
{
	if (!ProjectileMovement)
	{
		Destroy();
		return;
	}

	// 已在回收或已完成则不重复处理
	if (State == ECSProjectileState::Returning || State == ECSProjectileState::Recalled)
	{
		return;
	}

	// 没指定目标时回退到 Owner（即发射方块的人）
	if (!TargetActor)
	{
		TargetActor = GetOwner();
	}

	RecallTarget = TargetActor;

	// 没有可回收的目标（比如发射者已销毁），直接删除
	if (!TargetActor)
	{
		State = ECSProjectileState::Recalled;
		Destroy();
		return;
	}

	if (ReturnSpeed > 0.f)
	{
		RecallSpeed = ReturnSpeed;
	}

	// 回收时关闭重力，保证能准确飞回来
	ProjectileMovement->ProjectileGravityScale = 0.f;
	ProjectileMovement->Activate(true);
	ProjectileMovement->SetUpdatedComponent(CollisionComponent);

	// 给一个朝目标的初始速度
	const FVector Dir = (TargetActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
	if (!Dir.IsNearlyZero())
	{
		ProjectileMovement->Velocity = Dir * RecallSpeed;
	}

	// 回收阶段不应该被场景物体挡住回不来：
	// 对静态/动态物体改为 Overlap（穿过去但不停止），
	// 仍保持与 Pawn（敌人/玩家）的阻挡，以便继续炸开挡路的敌人。
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Overlap);
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);

	LastFrameLocation = GetActorLocation();
	RecallElapsed = 0.f;

	SetState(ECSProjectileState::Returning);
}


bool ACS_CubeProjectile::CanBeRecalled() const
{
	return State == ECSProjectileState::Flying;
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

	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
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

	// 没指定类就用 C++ 类本身；指定了蓝图子类则生成子类，
	// 这样子类里配置的 CubeSize / ExplosionRadius 等参数才会生效
	UClass* UseClass = ACS_CubeProjectile::StaticClass();
	if (ProjectileClass)
	{
		UseClass = ProjectileClass.Get();
	}

	ACS_CubeProjectile* Projectile = World->SpawnActor<ACS_CubeProjectile>(
		UseClass, SpawnTransform, SpawnParams);

	if (Projectile)
	{
		Projectile->LaunchInDirection(FinalDirection);
	}

	return Projectile;
}


void ACS_CubeProjectile::OnProjectileHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	// 忽略自己与拥有者
	if (!OtherActor || OtherActor == this)
	{
		return;
	}
	if (bIgnoreOwner && OtherActor == GetOwner())
	{
		return;
	}

	// ---------- 情况一：撞到敌人 ----------
	// 立即炸开周围敌人，方块本身继续飞行不受影响。
	if (IsEnemyActor(OtherActor))
	{
		// 让方块移动时忽略该敌人，避免被物理挡住
		CollisionComponent->IgnoreActorWhenMoving(OtherActor, true);

		// 以方块当前位置为中心炸开
		ExplodeAt(GetActorLocation(), ExplosionRadius);
		return;
	}

	// ---------- 情况二：撞到其他物体（墙、地面、道具）----------
	// 立即开始回收（飞回持有者）。
	if (State == ECSProjectileState::Flying)
	{
		// 不调用 StopMovementImmediately：它会停用移动组件，
		// 而回收阶段需要继续移动（由 Tick 用 SetActorLocation 驱动）。
		StartRecall(GetOwner());
	}
}