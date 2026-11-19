// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "CS_MeleeNotify.generated.h"

/**
 * 
 */
UCLASS()
class CUBE_SKILL_API UCS_MeleeNotify : public UAnimNotifyState
{
	GENERATED_BODY()
	
public:
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;

private:
	UPROPERTY(EditAnywhere , Category = "CS|Debug")
	bool bDebug = false;

	UPROPERTY(EditAnywhere , Category = "CS|Socket")
	FName SocketName{"hand_r"};

	UPROPERTY(EditAnywhere , Category = "CS|Socket")
	float SocketExtensionOffset{40.f};

	UPROPERTY(EditAnywhere , Category = "CS|Socket")
	float SphereTraceRadius{60.f};

	TArray<FHitResult> PerformSphereTrace(USkeletalMeshComponent* MeshCom);

	void SendEventToActor(TArray<FHitResult> HitResults , USkeletalMeshComponent* MeshCom);
};
