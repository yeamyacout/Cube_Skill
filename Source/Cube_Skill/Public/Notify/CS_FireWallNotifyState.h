#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "CS_FireWallNotifyState.generated.h"

class ACS_FireWallActor;

/** 一次合墙过程。通知资产会被所有敌人共用，所以进度和 Actor 都按网格分开存。 */
struct FCS_FireWallRuntime
{
	TWeakObjectPtr<ACS_FireWallActor> LeftWall;
	TWeakObjectPtr<ACS_FireWallActor> RightWall;

	FVector GroundCenter = FVector::ZeroVector;
	FVector SideOffset = FVector::ZeroVector;
	FVector ForwardOffset = FVector::ZeroVector;
	FVector MergeLocation = FVector::ZeroVector;

	float Duration = 0.f;
	float Elapsed = 0.f;
};

/**
 * 重击一：左右生成火墙并合拢。
 * 只选火墙类、驱动位移；尺寸/伤害/特效全在 ACS_FireWallActor（或蓝图）上改。
 */
UCLASS(meta = (DisplayName = "火墙合拢"))
class CUBE_SKILL_API UCS_FireWallNotifyState : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UCS_FireWallNotifyState();

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

	/**
	 * 生成哪一种火墙。
	 * 选 BP（父类 ACS_FireWallActor）；留空用 C++ 基类。
	 */
	UPROPERTY(EditAnywhere, Category = "CS|FireWall")
	TSubclassOf<ACS_FireWallActor> FireWallActorClass;

private:
	void DestroyWalls(FCS_FireWallRuntime& State) const;

	TMap<TWeakObjectPtr<USkeletalMeshComponent>, FCS_FireWallRuntime> ActiveWalls;
};
