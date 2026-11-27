#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "CS_FirePillarNotifyState.generated.h"

class ACS_FirePillarActor;

/** 一次火柱喷射。通知资产共用，所以按网格分开存运行时。 */
struct FCS_FirePillarRuntime
{
	TWeakObjectPtr<ACS_FirePillarActor> Pillar;
};

/**
 * 重击二：手部往前喷火柱。
 * 只选火柱类 + Socket；尺寸/伤害/特效全在 ACS_FirePillarActor（或蓝图）上改。
 * 时长跟通知条区间走，End 时销毁。
 */
UCLASS(meta = (DisplayName = "火柱喷射"))
class CUBE_SKILL_API UCS_FirePillarNotifyState : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UCS_FirePillarNotifyState();

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

	/** 生成哪一种火柱。选 BP（父类 ACS_FirePillarActor）；留空用 C++ 基类。 */
	UPROPERTY(EditAnywhere, Category = "CS|FirePillar")
	TSubclassOf<ACS_FirePillarActor> FirePillarActorClass;

	/** 出手骨骼插槽，默认右手。 */
	UPROPERTY(EditAnywhere, Category = "CS|FirePillar")
	FName SocketName = TEXT("hand_r");

private:
	void UpdatePillarTransform(USkeletalMeshComponent* MeshComp, ACS_FirePillarActor* Pillar, float DeltaSeconds) const;
	void DestroyPillar(FCS_FirePillarRuntime& State) const;

	TMap<TWeakObjectPtr<USkeletalMeshComponent>, FCS_FirePillarRuntime> ActivePillars;
};
