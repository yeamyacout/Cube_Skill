// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"

class AActor;

#include "CS_MeleeNotify.generated.h"

/**
 * 
 */
UCLASS()
class CUBE_SKILL_API UCS_MeleeNotify : public UAnimNotifyState
{
	GENERATED_BODY()
	
public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

private:
	UPROPERTY(EditAnywhere , Category = "CS|Debug")
	bool bDebug = false;

	UPROPERTY(EditAnywhere , Category = "CS|Socket")
	FName SocketName{"hand_r"};

	UPROPERTY(EditAnywhere , Category = "CS|Socket")
	float SocketExtensionOffset{40.f};

	UPROPERTY(EditAnywhere , Category = "CS|Socket")
	float SphereTraceRadius{60.f};

	UPROPERTY(EditAnywhere, Category = "CS|Damage")
	float Damage{20.f};

	TArray<FHitResult> PerformSphereTrace(USkeletalMeshComponent* MeshCom);

	void SendEventToActor(TArray<FHitResult> HitResults , USkeletalMeshComponent* MeshCom);

	/**
	 * "这一刀已经打过谁"的记录
	 *
	 * 注意：AnimNotifyState 对象是**动画资产共享**的，不是每个角色一份。
	 * （引擎 AnimInstance.cpp 里直接调用 AnimNotifyEvent->NotifyStateClass->NotifyBegin，
	 *   NotifyStateClass 是蒙太奇资产上的那个对象。）
	 *
	 * 所以这里不能只存一个 TSet：两只敌人同时挥同一张蒙太奇时会共用同一个集合，
	 * A 打中玩家后 B 的刀就会被误判成"已经打过"，表现为"敌人挥刀但没伤害"。
	 * 用 SkeletalMeshComponent 做 key，让每个角色各有各的记录。
	 */
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, TSet<TWeakObjectPtr<AActor>>> AlreadyHit;
};
