#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "CS_HitReact.generated.h"

class UAnimMontage;
class UParticleSystem;
class UCS_HealthComponent;

UCLASS(Blueprintable)
class CUBE_SKILL_API UCS_HitReact : public UGameplayAbility
{
	GENERATED_BODY()

public:
	/** 设为每个角色一个实例，并打上开局自动激活的标签。 */
	UCS_HitReact();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CS|HitReact")
	TObjectPtr<UAnimMontage> HitReactMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CS|HitReact")
	TObjectPtr<UAnimMontage> DeathMontage;

	/** 死亡时留在原地的旧粒子。不跟着敌人销毁。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CS|HitReact")
	TObjectPtr<UParticleSystem> DeathParticle;

	/** 死亡粒子的缩放。1 是原大小。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CS|HitReact")
	FVector DeathParticleScale = FVector(1.f);

protected:
	/** 技能激活后开始监听化身身上的生命值变化。 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** 技能结束时解除血量监听。 */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** 血量下降且未致死、受击蒙太奇未在播放时，播放受击蒙太奇。血量到 0 时改为死亡。 */
	UFUNCTION()
	void HandleHealthChanged(float CurrentHealth, float MaxHealth, float Delta);

	/** 停掉行为树和移动，关掉和角色的碰撞，播放死亡蒙太奇，并在原地留下死亡粒子。 */
	void HandleDeath();

	/** 死亡蒙太奇播完后销毁敌人。粒子已经单独生成，不会跟着消失。 */
	void HandleDeathMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/** 从生命组件上卸掉血量变化监听。 */
	void UnbindHealth();

	TWeakObjectPtr<UCS_HealthComponent> BoundHealth;

	TWeakObjectPtr<AActor> DeathAvatar;

	bool bDeathHandled = false;
};
