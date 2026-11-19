// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/CS_EnemyCharacter.h"

#include "GAS/Abilities/CS_AbilitySystemComponent.h"


ACS_EnemyCharacter::ACS_EnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	
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


