// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/CS_EnemyCharacter.h"

#include "AIController.h"
#include "GAS/Abilities/CS_AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"


ACS_EnemyCharacter::ACS_EnemyCharacter()
{
	// 击飞状态需要 Tick 来检测落地与超时
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	
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
	for(const auto& Ability : StartupAbilities)
	{
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(Ability);
		GetAbilitySystemComponent()->GiveAbility(AbilitySpec);
	}
}


void ACS_EnemyCharacter::StartKnockback(float MaxKnockDuration)
{
	// 已经在击飞状态就不重复处理
	if (bIsKnockedBack)
	{
		return;
	}

	bIsKnockedBack = true;
	KnockbackStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	KnockbackMaxDuration = MaxKnockDuration;

	// ---------- 1. 停掉 AI 的寻路与移动 ----------
	// 为什么要停 AI：
	//   敌人被击飞在空中时，AI 的 MoveTo 会继续每帧写入移动输入，
	//   导致角色一边被弹飞一边"向前行走"，轨迹被拉回目标方向。
	//   这里让 AI 控制器暂停逻辑，彻底停止其移动指令。
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		bWasAIPausedBeforeKnockback = AI->IsMoveInputIgnored() == false;
		AI->StopMovement();      // 清掉当前寻路
	}

	// ---------- 2. 让角色不再响应"移动输入" ----------
	// 这样即使 AI 或其他逻辑试图给移动输入，也不会生效。
	// 注意：不禁用移动组件本身，否则重力/物理就不起作用，角色会浮空。
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		// 清掉水平速度残留，避免一边被推一边保留旧的水平速度
		FVector Vel = MoveComp->Velocity;
		MoveComp->Velocity = FVector(Vel.X, Vel.Y, Vel.Z);

		// 保留重力与空气控制，但屏蔽"自主移动"的意图
		MoveComp->StopMovementImmediately();
	}

	// ---------- 3. 让 AI 的移动输入彻底失效 ----------
	// AController::SetIgnoreMoveInput 会忽略后续的所有移动输入（包括 AI 的 MoveTo）
	if (AController* Ctrl = GetController())
	{
		Ctrl->SetIgnoreMoveInput(true);
	}
}


void ACS_EnemyCharacter::EndKnockback()
{
	if (!bIsKnockedBack)
	{
		return;
	}

	bIsKnockedBack = false;

	// 恢复 AI 的移动输入
	if (AController* Ctrl = GetController())
	{
		Ctrl->SetIgnoreMoveInput(false);
	}

	// 恢复 AI 控制器（如果之前是正常的）
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		if (bWasAIPausedBeforeKnockback)
		{
			// 让 AI 在落地后重新接管行为树
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


void ACS_EnemyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 只在击飞状态下才需要检查
	if (!bIsKnockedBack)
	{
		return;
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
	// 某些情况下 Landed 不一定会触发（比如落点被判定为台阶），
	// 这里补充判断：如果移动组件报告已经在地面上，也解除锁定。
	if (const UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		if (MoveComp->IsMovingOnGround())
		{
			EndKnockback();
		}
	}
}
