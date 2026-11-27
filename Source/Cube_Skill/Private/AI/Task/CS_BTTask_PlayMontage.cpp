#include "AI/Task/CS_BTTask_PlayMontage.h"

#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"

UCS_BTTask_PlayMontage::UCS_BTTask_PlayMontage()
{
	NodeName = TEXT("播放蒙太奇");
	INIT_TASK_NODE_NOTIFY_FLAGS();
}

EBTNodeResult::Type UCS_BTTask_PlayMontage::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* Controller = OwnerComp.GetAIOwner();
	ACharacter* Character = Controller ? Cast<ACharacter>(Controller->GetPawn()) : nullptr;
	UAnimInstance* AnimInstance = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance || !Montage)
	{
		return EBTNodeResult::Failed;
	}

	// 寻路和根运动会抢位移。施法和大躲期间先停掉移动，蒙太奇自己的根运动才能生效。
	Controller->StopMovement();

	if (Character->PlayAnimMontage(Montage) <= 0.f)
	{
		return EBTNodeResult::Failed;
	}

	// 资产关掉自动混出时，蒙太奇会停在最后一帧，这个任务就永远等不到结束。
	if (FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(Montage))
	{
		MontageInstance->bEnableAutoBlendOut = true;
	}

	FCS_PlayMontageMemory* Memory = CastInstanceNodeMemory<FCS_PlayMontageMemory>(NodeMemory);
	Memory->ElapsedTime = 0.f;
	return EBTNodeResult::InProgress;
}

void UCS_BTTask_PlayMontage::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FCS_PlayMontageMemory* Memory = CastInstanceNodeMemory<FCS_PlayMontageMemory>(NodeMemory);
	Memory->ElapsedTime += DeltaSeconds;

	AAIController* Controller = OwnerComp.GetAIOwner();
	ACharacter* Character = Controller ? Cast<ACharacter>(Controller->GetPawn()) : nullptr;
	UAnimInstance* AnimInstance = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance || !Montage)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	if (PlayDuration > 0.f && Memory->ElapsedTime >= PlayDuration)
	{
		AnimInstance->Montage_Stop(0.1f, Montage);
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	if (!AnimInstance->Montage_IsPlaying(Montage))
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}

EBTNodeResult::Type UCS_BTTask_PlayMontage::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* Controller = OwnerComp.GetAIOwner();
	ACharacter* Character = Controller ? Cast<ACharacter>(Controller->GetPawn()) : nullptr;
	if (UAnimInstance* AnimInstance = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr)
	{
		AnimInstance->Montage_Stop(0.1f, Montage);
	}
	return EBTNodeResult::Aborted;
}
