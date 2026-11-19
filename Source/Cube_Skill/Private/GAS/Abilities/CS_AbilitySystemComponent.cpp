// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/Abilities/CS_AbilitySystemComponent.h"
#include "GAS/GameplayTags/CSTag.h"

void UCS_AbilitySystemComponent::OnGiveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	Super::OnGiveAbility(AbilitySpec);
	
	if(!IsValid(AbilitySpec.Ability)) return;
	
	//检查能力的标签是否包含"ActivateOnGiven"标签，如果包含则尝试激活该能力
	HandleAutoActivatedAbility(AbilitySpec);
}

void UCS_AbilitySystemComponent::OnRep_ActivateAbilities()
{
	Super::OnRep_ActivateAbilities();
	
	//加锁，防止遍历时列表被改     一个作用域锁，在它的生命周期内锁定技能列表，防止遍历过程中列表被修改（比如技能被移除），导致迭代器失效崩溃。
	FScopedAbilityListLock ActiveSpecsLock(*this);
	//遍历所有可激活的能力，尝试激活包含"ActivateOnGiven"标签的能力
	for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		HandleAutoActivatedAbility(AbilitySpec);
	}
}

void UCS_AbilitySystemComponent::SetAbilityLevel(TSubclassOf<UGameplayAbility> AbilityClass, int32 Level)
{
	if (IsValid(GetAvatarActor()) && ! GetAvatarActor()->HasAuthority()) return;

	//查找技能列表里面的 某个技能， 然后返回一个句柄给我， 我使用这个句柄设置技能的等级
	if (FGameplayAbilitySpec* AbilitySpec =  FindAbilitySpecFromClass(AbilityClass))
	{
		AbilitySpec->Level = Level;
		//调用 MarkAbilitySpecDirty，就是拿个喇叭对 ASC 喊：“这个技能的等级数据我改了，它变‘脏’了，请你重新打包，把它同步给客户端！
		MarkAbilitySpecDirty(*AbilitySpec);
	}
}

void UCS_AbilitySystemComponent::HandleAutoActivatedAbility(const FGameplayAbilitySpec& AbilitySpec)
{
	for (const FGameplayTag& Tag : AbilitySpec.Ability->AbilityTags)
	{
		//MatchesTagExact：精确匹配，要求完全一样
		if(Tag.MatchesTagExact(CSTag::CSAbilities::ActivateOnGiven))
		{						
			TryActivateAbility(AbilitySpec.Handle);
			return;
		}
	}
}
