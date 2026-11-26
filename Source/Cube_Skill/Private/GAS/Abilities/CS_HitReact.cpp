#include "GAS/Abilities/CS_HitReact.h"

#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "BrainComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/CS_HealthComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GAS/GameplayTags/CSTag.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"


/** 设为每个角色一个实例，并打上开局自动激活的标签。 */
UCS_HitReact::UCS_HitReact()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	FGameplayTagContainer Tags;
	Tags.AddTag(CSTag::CSAbilities::ActivateOnGiven);
	SetAssetTags(Tags);
}

/** 技能激活后开始监听化身身上的生命值变化。 */
void UCS_HitReact::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UCS_HealthComponent* Health = Avatar ? Avatar->FindComponentByClass<UCS_HealthComponent>() : nullptr;
	if (!IsValid(Health))
	{
		return;
	}

	if (!Health->OnHealthChanged.IsAlreadyBound(this, &UCS_HitReact::HandleHealthChanged))
	{
		Health->OnHealthChanged.AddDynamic(this, &UCS_HitReact::HandleHealthChanged);
	}
	BoundHealth = Health;
}

/** 技能结束时解除血量监听。 */
void UCS_HitReact::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	UnbindHealth();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

/** 血量下降且未致死、受击蒙太奇未在播放时，播放受击蒙太奇。血量到 0 时改为死亡。 */
void UCS_HitReact::HandleHealthChanged(float CurrentHealth, float MaxHealth, float Delta)
{
	(void)MaxHealth;

	// 治疗、自动回血和开局同步的 Delta 都不是掉血。
	if (Delta >= 0.f)
	{
		return;
	}

	if (CurrentHealth <= 0.f)
	{
		HandleDeath();
		return;
	}

	if (!IsValid(HitReactMontage))
	{
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!AnimInstance || AnimInstance->Montage_IsPlaying(HitReactMontage))
	{
		return;
	}

	Character->PlayAnimMontage(HitReactMontage);
}

/** 停掉行为树和移动，关掉和角色的碰撞，播放死亡蒙太奇，并在原地留下死亡粒子。 */
void UCS_HitReact::HandleDeath()
{
	if (bDeathHandled)
	{
		return;
	}
	bDeathHandled = true;

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Character)
	{
		return;
	}

	if (AAIController* AI = Cast<AAIController>(Character->GetController()))
	{
		AI->StopMovement();
		AI->ClearFocus(EAIFocusPriority::Gameplay);
		AI->ClearFocus(EAIFocusPriority::Move);
		AI->ClearFocus(EAIFocusPriority::LastFocusPriority);
		if (UBrainComponent* Brain = AI->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("Death"));
		}
	}

	if (AController* Controller = Character->GetController())
	{
		Controller->SetIgnoreMoveInput(true);
	}

	if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();
	}

	// 尸体留在地上，但不再挡住玩家。
	if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}

	if (UAnimInstance* AnimInstance = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr)
	{
		AnimInstance->Montage_Stop(0.1f);
	}
	if (IsValid(DeathMontage))
	{
		DeathAvatar = Character;
		Character->PlayAnimMontage(DeathMontage);
		if (UAnimInstance* DeathAnim = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr)
		{
			// 资产上关掉自动混出时，蒙太奇会停在最后一帧且永远不结束。这里强制让它播完就结束。
			if (FAnimMontageInstance* MontageInstance = DeathAnim->GetActiveInstanceForMontage(DeathMontage))
			{
				MontageInstance->bEnableAutoBlendOut = true;
			}
			FOnMontageBlendingOutStarted BlendOutDelegate;
			BlendOutDelegate.BindUObject(this, &UCS_HitReact::HandleDeathMontageEnded);
			DeathAnim->Montage_SetBlendingOutDelegate(BlendOutDelegate, DeathMontage);
		}
	}

	if (!IsValid(DeathParticle))
	{
		return;
	}

	UGameplayStatics::SpawnEmitterAtLocation(
		Character,
		DeathParticle,
		Character->GetActorLocation(),
		Character->GetActorRotation(),
		DeathParticleScale,
		true,
		EPSCPoolMethod::None,
		true);
}

/** 死亡蒙太奇播完后销毁敌人。粒子已经单独生成，不会跟着消失。 */
void UCS_HitReact::HandleDeathMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	(void)Montage;
	(void)bInterrupted;

	AActor* Avatar = DeathAvatar.Get();
	DeathAvatar = nullptr;
	if (IsValid(Avatar))
	{
		Avatar->Destroy();
	}
}

/** 从生命组件上卸掉血量变化监听。 */
void UCS_HitReact::UnbindHealth()
{
	if (UCS_HealthComponent* Health = BoundHealth.Get())
	{
		Health->OnHealthChanged.RemoveDynamic(this, &UCS_HitReact::HandleHealthChanged);
	}
	BoundHealth = nullptr;
}
