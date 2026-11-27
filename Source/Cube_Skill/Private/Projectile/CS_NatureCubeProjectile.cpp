// Fill out your copyright notice in the Description page of Project Settings.


#include "Projectile/CS_NatureCubeProjectile.h"

#include "Characters/CS_EnemyCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace CSANatureCubeLocals
{
	/** 轨迹标记特效（发射神秘气液体），绿色 */
	const TCHAR* const DefaultTrailFXPath = TEXT("/Game/AbsorbtionFX/FX/NS_Absorption_Green.NS_Absorption_Green");

	/** 中毒标记特效，紫绿色系 */
	const TCHAR* const DefaultPoisonMarkFXPath = TEXT("/Game/AbsorbtionFX/FX/NS_Absorption_Teal.NS_Absorption_Teal");

	/** 减速标记特效 */
	const TCHAR* const DefaultSlowMarkFXPath = TEXT("/Game/AbsorbtionFX/FX/NS_Absorption_Blue.NS_Absorption_Blue");

	/** 自然属性标记特效 */
	const TCHAR* const DefaultNatureMarkFXPath = TEXT("/Game/AbsorbtionFX/FX/NS_Absorption_Orange.NS_Absorption_Orange");

	/**
	 * 延时后自动销毁特效
	 *
	 * 用【世界】作定时器 owner：方块回收后会 Destroy()，
	 * 以方块为 owner 的定时器会一起失效，特效就再也删不掉了。
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


ACS_NatureCubeProjectile::ACS_NatureCubeProjectile()
{
	CubeColor = FLinearColor(0.15f, 0.9f, 0.2f, 1.f);   // 绿色，代表自然

	// 需要 Tick 来逐帧留下飞行轨迹
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> TrailFinder(CSANatureCubeLocals::DefaultTrailFXPath);
	if (TrailFinder.Succeeded())
	{
		TrailFX = TrailFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> PoisonFinder(CSANatureCubeLocals::DefaultPoisonMarkFXPath);
	if (PoisonFinder.Succeeded())
	{
		PoisonMarkFX = PoisonFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> SlowFinder(CSANatureCubeLocals::DefaultSlowMarkFXPath);
	if (SlowFinder.Succeeded())
	{
		SlowMarkFX = SlowFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> NatureFinder(CSANatureCubeLocals::DefaultNatureMarkFXPath);
	if (NatureFinder.Succeeded())
	{
		NatureMarkFX = NatureFinder.Object;
	}
}


void ACS_NatureCubeProjectile::BeginPlay()
{
	Super::BeginPlay();

	LastTrailLocation = GetActorLocation();
	bTrailStarted = false;
	bTrailActive = true;
}


void ACS_NatureCubeProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 回收开始后不再生成新轨迹，已生成的轨迹按各自寿命自然消失
	if (bIsReturning || State == ECSProjectileState::Recalled)
	{
		bTrailActive = false;
	}

	if (bTrailActive)
	{
		// 按间距生成轨迹标记，避免每帧都刷组件
		const FVector CurrentLocation = GetActorLocation();
		if (!bTrailStarted || FVector::Dist(CurrentLocation, LastTrailLocation) >= TrailSpacing)
		{
			SpawnTrailMark();
			LastTrailLocation = CurrentLocation;
			bTrailStarted = true;
		}
	}

	// 周期性让轨迹上的敌人减速
	TrailSlowElapsed += DeltaSeconds;
	if (TrailSlowElapsed >= TrailSlowCheckInterval)
	{
		TrailSlowElapsed = 0.f;
		ApplyTrailSlow();
	}
}


void ACS_NatureCubeProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bTrailActive = false;
	Super::EndPlay(EndPlayReason);
}


void ACS_NatureCubeProjectile::SpawnTrailMark()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 蓝图未配置时从默认路径兜底加载
	UNiagaraSystem* FX = TrailFX;
	if (!FX)
	{
		FX = LoadObject<UNiagaraSystem>(nullptr, CSANatureCubeLocals::DefaultTrailFXPath);
	}
	if (!FX)
	{
		return;
	}

	UNiagaraComponent* Comp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World,
		FX,
		GetActorLocation(),
		FRotator::ZeroRotator,
		FVector(TrailFXScale),
		/*bAutoDestroy=*/false,
		/*bAutoActivate=*/true);

	// 轨迹存活 TrailLifetime 秒后自动清理
	CSANatureCubeLocals::ScheduleDestroyNiagara(World, Comp, TrailLifetime);
}


void ACS_NatureCubeProjectile::ApplyTrailSlow()
{
	if (!bTrailActive)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<FOverlapResult> Overlaps;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CS_NatureTrail), false, this);
	if (GetOwner())
	{
		QueryParams.AddIgnoredActor(GetOwner());
	}

	const bool bFound = World->OverlapMultiByObjectType(
		Overlaps,
		GetActorLocation(),
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(TrailSlowRadius),
		QueryParams);

	if (!bFound)
	{
		return;
	}

	// Overlap 对同一敌人会返回多条结果，先去重
	TSet<ACS_EnemyCharacter*> UniqueEnemies;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (ACS_EnemyCharacter* Found = Cast<ACS_EnemyCharacter>(Overlap.GetActor()))
		{
			UniqueEnemies.Add(Found);
		}
	}

	// 轨迹减速沿用 SlowDuration，让敌人离开气体后仍保持一段减速
	for (ACS_EnemyCharacter* Enemy : UniqueEnemies)
	{
		if (Enemy)
		{
			ApplySlow(Enemy, SlowDuration);
		}
	}
}


void ACS_NatureCubeProjectile::OnEnemyHitEffect(ACS_EnemyCharacter* Enemy)
{
	if (!Enemy)
	{
		return;
	}

	// 命中敌人：中毒 + 减速 + 留下自然标记
	ApplyPoison(Enemy);
	ApplySlow(Enemy, SlowDuration);
	AttachMarkEffect(Enemy, NatureMarkFX, NatureMarkDuration);
}


void ACS_NatureCubeProjectile::ApplyPoison(ACS_EnemyCharacter* Enemy)
{
	UWorld* World = GetWorld();
	if (!Enemy || !World)
	{
		return;
	}

	// 中毒在敌人身上挂一个持续 PoisonDuration 的标记特效
	AttachMarkEffect(Enemy, PoisonMarkFX, PoisonDuration);

	// 拆成多段跳伤，总伤害均分
	const int32 TickCount = FMath::Max(1, FMath::RoundToInt(PoisonDuration / PoisonTickInterval));
	const float DamagePerTick = PoisonTotalDamage / TickCount;

	TWeakObjectPtr<ACS_EnemyCharacter> WeakEnemy(Enemy);
	TWeakObjectPtr<ACS_NatureCubeProjectile> WeakSelf(this);

	for (int32 i = 1; i <= TickCount; ++i)
	{
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(
			Handle,
			FTimerDelegate::CreateWeakLambda(World, [WeakEnemy, WeakSelf, DamagePerTick]()
			{
				ACS_EnemyCharacter* Target = WeakEnemy.Get();
				if (!Target)
				{
					return;
				}

				// 方块可能已销毁，此时把敌人自己当作伤害来源
				AActor* Causer = WeakSelf.IsValid() ? static_cast<AActor*>(WeakSelf.Get()) : static_cast<AActor*>(Target);
				Target->TakeDamage(DamagePerTick, FDamageEvent(), nullptr, Causer);
			}),
			PoisonTickInterval * i,
			false);
	}
}


void ACS_NatureCubeProjectile::ApplySlow(ACS_EnemyCharacter* Enemy, float Duration)
{
	UWorld* World = GetWorld();
	if (!Enemy || !World || Duration <= 0.f)
	{
		return;
	}

	UCharacterMovementComponent* MoveComp = Enemy->GetCharacterMovement();
	if (!MoveComp)
	{
		return;
	}

	// 记录减速前的原始速度，到期后恢复
	const float OriginalSpeed = MoveComp->MaxWalkSpeed;
	const float SlowedSpeed = OriginalSpeed * (1.f - SlowRatio);
	MoveComp->MaxWalkSpeed = SlowedSpeed;

	AttachMarkEffect(Enemy, SlowMarkFX, Duration);

	TWeakObjectPtr<ACS_EnemyCharacter> WeakEnemy(Enemy);
	TWeakObjectPtr<UCharacterMovementComponent> WeakMove(MoveComp);

	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(
		Handle,
		FTimerDelegate::CreateWeakLambda(World, [WeakMove, OriginalSpeed, SlowedSpeed]()
		{
			UCharacterMovementComponent* MC = WeakMove.Get();
			if (!MC)
			{
				return;
			}

			// 只有当前值仍是本次设置的减速值时才恢复，
			// 避免覆盖掉后来施加的其他减速/加速效果
			if (FMath::IsNearlyEqual(MC->MaxWalkSpeed, SlowedSpeed, 1.f))
			{
				MC->MaxWalkSpeed = OriginalSpeed;
			}
		}),
		Duration,
		false);
}


void ACS_NatureCubeProjectile::AttachMarkEffect(ACS_EnemyCharacter* Enemy, UNiagaraSystem* Effect, float Duration)
{
	UWorld* World = GetWorld();
	if (!Enemy || !World || !Effect || Duration <= 0.f)
	{
		return;
	}

	// 优先挂骨骼网格，敌人移动时标记跟着走
	ACharacter* EnemyChar = Cast<ACharacter>(Enemy);
	USceneComponent* AttachTarget = EnemyChar && EnemyChar->GetMesh()
		? EnemyChar->GetMesh()
		: Enemy->GetRootComponent();

	if (!AttachTarget)
	{
		return;
	}

	UNiagaraComponent* Comp = UNiagaraFunctionLibrary::SpawnSystemAttached(
		Effect,
		AttachTarget,
		NAME_None,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		EAttachLocation::KeepRelativeOffset,
		/*bAutoDestroy=*/false,
		/*bAutoActivate=*/true);

	// 应用统一缩放（蓝图可调）
	if (Comp)
	{
		Comp->SetWorldScale3D(FVector(MarkFXScale));
	}

	CSANatureCubeLocals::ScheduleDestroyNiagara(World, Comp, Duration);
}
