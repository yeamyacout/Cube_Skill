// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/CS_EnemyCharacter.h"

#include "AIController.h"
#include "BrainComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Components/CS_HealthComponent.h"
#include "GAS/Abilities/CS_AbilitySystemComponent.h"
#include "GAS/Abilities/CS_HitReact.h"
#include "GameFramework/CharacterMovementComponent.h"


ACS_EnemyCharacter::ACS_EnemyCharacter()
{
	// 击飞状态需要 Tick 来检测落地与超时
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	
	HealthComponent = CreateDefaultSubobject<UCS_HealthComponent>(TEXT("HealthComponent"));

	CS_AbilitySystemComponent = CreateDefaultSubobject<UCS_AbilitySystemComponent>("CS_AbilitySystemComponent");
	//设置是否可以复制
	CS_AbilitySystemComponent->SetIsReplicated(true);
	//复制的模式
	CS_AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
}

UAbilitySystemComponent* ACS_EnemyCharacter::GetAbilitySystemComponent() const
{
	return CS_AbilitySystemComponent;
}

void ACS_EnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	if(!IsValid(GetAbilitySystemComponent())) return;
	
	//设置ASC的拥有者和化身
	GetAbilitySystemComponent()->InitAbilityActorInfo(this, this);
	
	//判断我这里是不是服务器，只有服务器可以赋予能力 , 下方为服务器端执行的代码
	if(!HasAuthority()) return;

	GiveStartupAbilities();
}


void ACS_EnemyCharacter::GiveStartupAbilities()
{
	if(!IsValid(GetAbilitySystemComponent())) return;

	//循环将技能列表的每个技能 赋予Ability Ability再将技能变成能力规范的形式，最后再将修改后的形式给GAS
	bool bHasHitReact = false;
	for(const auto& Ability : StartupAbilities)
	{
		if (Ability && Ability->IsChildOf(UCS_HitReact::StaticClass()))
		{
			bHasHitReact = true;
		}
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(Ability);
		GetAbilitySystemComponent()->GiveAbility(AbilitySpec);
	}

	// 列表里没有受击技能时补上原生类。开局由 ActivateOnGiven 激活。
	// 若蓝图子类已经放进 StartupAbilities，就不再给第二次。
	if (!bHasHitReact)
	{
		GetAbilitySystemComponent()->GiveAbility(FGameplayAbilitySpec(UCS_HitReact::StaticClass()));
	}
}


void ACS_EnemyCharacter::StartKnockback(float MaxKnockDuration, bool bRequireLandingToEnd)
{
	// 如果已经在击飞状态，说明上一次没正常结束（可能 EndKnockback 没被调用）。
	// 这种情况下强制重置状态，而不是直接 return ——
	// 否则后续所有推动都会因为这条 return 被完全跳过，
	// 表现为"第一次能推，之后永远推不动"。
	if (bIsKnockedBack)
	{
		EndKnockback();
	}

	bIsKnockedBack = true;
	KnockbackStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	KnockbackMaxDuration = MaxKnockDuration;
	bRequireLandingToEndKnockback = bRequireLandingToEnd;

	// ---------- 1. 彻底停掉 AI ----------
	// 为什么要停 AI：
	//   AI 的 MoveTo 每帧都会通过 AddMovementInput 写入期望速度，
	//   CharacterMovement 再据此重算 Velocity。
	//   这会和我们的推力互相覆盖，表现为"推不动"或"推一下就停"。
	//
	// 仅靠 SetIgnoreMoveInput 有时不够（行为树可能绕过输入直接设速度），
	// 所以这里把 AI 控制器整体暂停。
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		bWasAIPausedBeforeKnockback = true;

		AI->StopMovement();                            // 清掉当前寻路
		AI->ClearFocus(EAIFocusPriority::Gameplay);    // 清除注视目标
		AI->ClearFocus(EAIFocusPriority::Move);
		AI->ClearFocus(EAIFocusPriority::LastFocusPriority);

		// 停掉行为树（BrainComponent），让它不再产生移动请求
		if (UBrainComponent* Brain = AI->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("Knockback"));
		}
	}

	// ---------- 2. 停止旧的速度残留 ----------
	// 注意：这里不再调用 StopMovementImmediately()。
	//   它会直接把 Velocity 清零，导致紧接着 BlastEnemy 里
	//   LaunchCharacter 设置的速度也被覆盖，表现为"敌人完全不动"。
	//   改为只清水平速度，保留垂直速度交给后续逻辑处理。
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		// 记录击飞前的重力值，用于后续恢复
		SavedGravityScale = MoveComp->GravityScale;

		// 记录移动参数：推动期间会临时改写它们，结束后要还原
		SavedMaxWalkSpeed = MoveComp->MaxWalkSpeed;
		SavedGroundFriction = MoveComp->GroundFriction;
		SavedBrakingDecelerationWalking = MoveComp->BrakingDecelerationWalking;

		// 记录击飞起始高度，用于限制最大抬升高度
		KnockbackStartHeight = GetActorLocation().Z;
	}

	// ---------- 3. 让 AI 的移动输入彻底失效 ----------
	// AController::SetIgnoreMoveInput 会忽略后续的所有移动输入（包括 AI 的 MoveTo）
	if (AController* Ctrl = GetController())
	{
		Ctrl->SetIgnoreMoveInput(true);
	}

	// ---------- 4. 冻结转向 ----------
	// 光屏蔽移动输入还不够：AI 仍可能通过以下机制转身
	//   · bUseControllerDesiredRotation / bOrientRotationToMovement
	//   · AIController 的 FocusOnActor / SetFocus
	//   · 行为树里的 RotateTo
	//
	// 敌人转身时身体旋转会把正在推它的方块"挤开"，
	// 表现出"推背面推不动，必须等它转身碰到正面才生效"。
	//
	// 处理：击飞期间关闭这两种自动转向，并清除 AI 的注视目标。
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		SavedbUseControllerDesiredRotation = MoveComp->bUseControllerDesiredRotation;
		SavedbOrientRotationToMovement = MoveComp->bOrientRotationToMovement;

		MoveComp->bUseControllerDesiredRotation = false;
		MoveComp->bOrientRotationToMovement = false;
	}

	// 清除 AI 的注视目标（避免它继续对着玩家转身）
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		AI->ClearFocus(EAIFocusPriority::Gameplay);
		AI->ClearFocus(EAIFocusPriority::Move);
		AI->ClearFocus(EAIFocusPriority::LastFocusPriority);
	}
}


void ACS_EnemyCharacter::EndKnockback()
{
	if (!bIsKnockedBack)
	{
		return;
	}

	bIsKnockedBack = false;

	// 恢复重力缩放（击飞期间可能被调整过）
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->GravityScale = SavedGravityScale;

		// 恢复转向设置（击飞期间被临时关闭）
		MoveComp->bUseControllerDesiredRotation = SavedbUseControllerDesiredRotation;
		MoveComp->bOrientRotationToMovement = SavedbOrientRotationToMovement;

		// 恢复推动期间被改写的移动参数
		MoveComp->MaxWalkSpeed = SavedMaxWalkSpeed;
		MoveComp->GroundFriction = SavedGroundFriction;
		MoveComp->BrakingDecelerationWalking = SavedBrakingDecelerationWalking;

		// 如果还悬在空中，清掉上升速度，让它自然落下
		FVector Vel = MoveComp->Velocity;
		if (Vel.Z > 0.f)
		{
			Vel.Z = 0.f;
			MoveComp->Velocity = Vel;
		}
	}

	// 清除速度覆盖
	bUseKnockbackVelocityOverride = false;
	KnockbackVelocityOverride = FVector::ZeroVector;

	// 已经死亡时不要把行为树和移动输入开回来，否则击飞超时会把尸体重新拉起来。
	if (HealthComponent && HealthComponent->IsDead())
	{
		return;
	}

	// 恢复 AI 的移动输入
	if (AController* Ctrl = GetController())
	{
		Ctrl->SetIgnoreMoveInput(false);
	}

	// 重启 AI 行为树
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		if (bWasAIPausedBeforeKnockback)
		{
			if (UBrainComponent* Brain = AI->GetBrainComponent())
			{
				Brain->RestartLogic();
			}
			AI->SetActorTickEnabled(true);
		}
	}
}


void ACS_EnemyCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	// 落地即解除击飞锁定，AI 重新接管
	if (bIsKnockedBack)
	{
		EndKnockback();
	}
}


void ACS_EnemyCharacter::SetKnockbackVelocityOverride(const FVector& NewVelocity, bool bEnable)
{
	bUseKnockbackVelocityOverride = bEnable;
	KnockbackVelocityOverride = bEnable ? NewVelocity : FVector::ZeroVector;
}


void ACS_EnemyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// ============================================================
	//  被推动：每帧强制速度（在 Super::Tick 之后压，覆盖 CalcVelocity 的结果）
	// ============================================================
	//
	// ============ 为什么必须在 Super::Tick 之后 ============
	// CharacterMovementComponent 的 CalcVelocity 在它自己的 Tick 里执行，
	// 会把速度钳制到 MaxWalkSpeed 以内，并按 AI 的输入重算速度。
	// 如果我们在它之前写 Velocity，会被它整个覆盖掉。
	//
	// 本函数第一行就是 Super::Tick(DeltaSeconds)（已经执行完），
	// 所以在这里写 Velocity 是**最后一道写入**，不会被覆盖。
	//
	// ============ 三个必须同时满足的条件 ============
	//   ① MaxWalkSpeed 要足够大（否则下一帧 CalcVelocity 又钳回去）
	//   ② 刹车减速度要清零（否则会立刻刹停）
	//   ③ 每帧都要写（AI 每帧也在写）
	if (bUseKnockbackVelocityOverride && bIsKnockedBack)
	{
		if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
		{
			// ① 保证速度上限够高
			const float OverrideSpeed = KnockbackVelocityOverride.Size2D();
			if (MoveComp->MaxWalkSpeed < OverrideSpeed)
			{
				MoveComp->MaxWalkSpeed = OverrideSpeed;
			}

			// ② 去掉刹车
			MoveComp->BrakingDecelerationWalking = 0.f;
			MoveComp->GroundFriction = 0.f;

			// ③ 强制写入速度（保留垂直分量，重力正常）
			const float CurrentZ = MoveComp->Velocity.Z;
			MoveComp->Velocity = FVector(
				KnockbackVelocityOverride.X,
				KnockbackVelocityOverride.Y,
				CurrentZ);

		}
	}

	// 只在击飞状态下才需要检查
	if (!bIsKnockedBack)
	{
		return;
	}

	// ---------- 高度上限限制 ----------
	// 需求：允许向上击飞，但抬升高度不超过 2 米。
	//
	// 做法：每帧检查当前高度相对击飞起点的抬升量，
	//   一旦超过 MaxKnockbackHeight，就把向上速度清零，
	//   角色靠重力自然回落，不会继续升高。
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		const float CurrentLift = GetActorLocation().Z - KnockbackStartHeight;

		if (CurrentLift > MaxKnockbackHeight)
		{
			FVector Vel = MoveComp->Velocity;

			// 只清掉向上的分量，保留下落速度（避免角色卡在空中）
			if (Vel.Z > 0.f)
			{
				Vel.Z = 0.f;
				MoveComp->Velocity = Vel;
			}
		}
	}

	// ---------- 超时保护 ----------
	// 正常情况下靠 Landed() 解除；但如果角色被卡在半空、
	// 或落到无法触发 Landed 的地方，这里兜底强制恢复。
	if (GetWorld() && (GetWorld()->GetTimeSeconds() - KnockbackStartTime) > KnockbackMaxDuration)
	{
		EndKnockback();
		return;
	}

	// ---------- 兜底落地检测 ----------
	// 只在"需要落地才解除"时才做这个检测。
	//
	// 为什么需要这个开关：
	//   方块把敌人**水平**推走时，敌人全程贴地，
	//   IsMovingOnGround() 一直为 true，
	//   会在第一帧就解除锁定，AI 立刻把速度拉回去 —— 表现为推不动。
	//   水平推的场景传 bRequireLandingToEnd = false，只靠超时解除。
	if (bRequireLandingToEndKnockback)
	{
		if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
		{
			if (MoveComp->IsMovingOnGround())
			{
				EndKnockback();
			}
			else if (GetWorld())
			{
				// 从胶囊体底部向下打一条短射线，判断是否已贴地
				const FVector CapsuleHalf = GetCapsuleComponent()
					? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * FVector::UpVector
					: FVector::ZeroVector;
				const FVector Start = GetActorLocation() - CapsuleHalf + FVector(0.f, 0.f, 5.f);
				const FVector End = Start - FVector(0.f, 0.f, GroundCheckDistance);

				FCollisionQueryParams Params(SCENE_QUERY_STAT(CS_EnemyGroundCheck), false, this);
				FHitResult Hit;
				const bool bGroundBelow = GetWorld()->LineTraceSingleByChannel(
					Hit, Start, End, ECC_Visibility, Params);

				// 贴地且垂直速度不再向上 -> 视为已落地，恢复 AI
				if (bGroundBelow && MoveComp->Velocity.Z <= 1.f)
				{
					EndKnockback();
				}
			}
		}
	}
}
