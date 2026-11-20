// Fill out your copyright notice in the Description page of Project Settings.


#include "Projectile/CS_CubeProjectile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

// 引擎内置立方体网格路径（不依赖项目美术资源）
#define CS_CUBE_MESH_PATH TEXT("/Engine/BasicShapes/Cube.Cube")

ACS_CubeProjectile::ACS_CubeProjectile()
{
	// 回收过程需要在 Tick 中检测距离，所以开启 Tick
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

	// 根据 CubeSize 设置网格缩放（引擎内置 Cube 是 100x100x100）
	const float ScaleFactor = CubeSize / 100.f;
	MeshComponent->SetRelativeScale3D(FVector(ScaleFactor));

	// 碰撞球半径略大于立方体半边长，保证视觉和碰撞大致吻合
	CollisionComponent->SetSphereRadius(CubeSize * 0.5f * 1.2f);

	// 应用配置（若蓝图子类改过参数，这里生效）
	ProjectileMovement->InitialSpeed = InitialSpeed;
	ProjectileMovement->MaxSpeed = InitialSpeed;
	ProjectileMovement->ProjectileGravityScale = GravityScale;

	if (bIgnoreOwner && GetOwner())
	{
		CollisionComponent->IgnoreActorWhenMoving(GetOwner(), true);
	}

	CollisionComponent->OnComponentHit.AddDynamic(this, &ACS_CubeProjectile::OnProjectileHit);

	// 注意：这里刻意不调用 SetLifeSpan()。
	// 方块必须一直存在，直到被手动或自动收回。
}


void ACS_CubeProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 只在回收状态下才需要检测距离
	if (State != ECSProjectileState::Returning)
	{
		return;
	}

	// 目标可能已被销毁（玩家死亡、关卡切换等）
	AActor* Target = RecallTarget.Get();
	if (!Target)
	{
		// 目标没了，直接结束回收
		FinishRecall();
		return;
	}

	// 到位判定：距离目标足够近就算"接住"
	const float DistSq = FVector::DistSquared(GetActorLocation(), Target->GetActorLocation());
	if (DistSq <= FMath::Square(RecallAcceptRadius))
	{
		FinishRecall();
		return;
	}

	// 每帧更新回飞方向，这样回收途中玩家移动也能追上
	const FVector Direction = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal();
	if (!Direction.IsNearlyZero())
	{
		ProjectileMovement->Velocity = Direction * RecallSpeed;
	}
}


void ACS_CubeProjectile::LaunchInDirection(const FVector& Direction)
{
	if (!ProjectileMovement || Direction.IsNearlyZero())
	{
		return;
	}

	const FVector NormalizedDir = Direction.GetSafeNormal();
	ProjectileMovement->Velocity = NormalizedDir * InitialSpeed;

	// 若之前是停下状态，重新激活移动组件继续飞
	ProjectileMovement->Activate(true);
	ProjectileMovement->SetUpdatedComponent(CollisionComponent);

	SetState(ECSProjectileState::Flying);
}


void ACS_CubeProjectile::StartRecall(AActor* TargetActor, float ReturnSpeed)
{
	if (!ProjectileMovement)
	{
		Destroy();
		return;
	}

	// 只能在飞行中或已停下时回收，已经回收中/已完成就不重复处理
	if (State == ECSProjectileState::Returning || State == ECSProjectileState::Recalled)
	{
		return;
	}

	RecallTarget = TargetActor;

	// 没有目标就直接销毁（无人接手）
	if (!TargetActor)
	{
		FinishRecall();
		return;
	}

	// 应用速度参数
	if (ReturnSpeed > 0.f)
	{
		RecallSpeed = ReturnSpeed;
	}

	// 回收时忽略重力，保证能准确飞回目标
	ProjectileMovement->ProjectileGravityScale = 0.f;
	ProjectileMovement->Activate(true);
	ProjectileMovement->SetUpdatedComponent(CollisionComponent);

	// 立即给一个朝目标的初始速度，避免第一帧不动
	const FVector Direction = (TargetActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
	if (!Direction.IsNearlyZero())
	{
		ProjectileMovement->Velocity = Direction * RecallSpeed;
	}

	SetState(ECSProjectileState::Returning);
}


void ACS_CubeProjectile::FinishRecall()
{
	SetState(ECSProjectileState::Recalled);

	// 回收标记（供外部查询"是否已被收走"）
	// 这里直接销毁，让持有者（玩家）重新获得方块。
	// 若将来需要播放"接住"动画，可改为延迟销毁。
	Destroy();
}


bool ACS_CubeProjectile::CanBeRecalled() const
{
	return State == ECSProjectileState::Flying || State == ECSProjectileState::Stopped;
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
	APawn* InInstigator)
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

	// 方向为零就用生成朝向的前向
	FVector FinalDirection = Direction;
	if (FinalDirection.IsNearlyZero())
	{
		FinalDirection = SpawnTransform.GetRotation().GetForwardVector();
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = InOwner;
	SpawnParams.Instigator = InInstigator;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ACS_CubeProjectile* Projectile = World->SpawnActor<ACS_CubeProjectile>(
		ACS_CubeProjectile::StaticClass(), SpawnTransform, SpawnParams);

	if (Projectile)
	{
		Projectile->LaunchInDirection(FinalDirection);
	}

	return Projectile;
}


void ACS_CubeProjectile::OnProjectileHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	// 忽略自身与拥有者（保险，虽然已在 BeginPlay 中设置忽略）
	if (OtherActor && OtherActor == this)
	{
		return;
	}
	if (bIgnoreOwner && OtherActor && OtherActor == GetOwner())
	{
		return;
	}

	// 回收过程中撞到东西不需要停止，让它继续飞回目标
	if (State == ECSProjectileState::Returning)
	{
		return;
	}

	// 关键行为：命中后减速到停住，而不是销毁。
	// 这样方块会留在命中位置等待玩家收回。
	if (ProjectileMovement)
	{
		ProjectileMovement->StopMovementImmediately();
		// 关闭重力，避免停下后渐渐往下沉
		ProjectileMovement->ProjectileGravityScale = 0.f;
	}

	SetState(ECSProjectileState::Stopped);
}
