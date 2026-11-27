#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "CS_ProjectileNotify.generated.h"

class ACS_Projectile;

/** 蒙太奇播到这一帧时，从指定插槽生成投射物。 */
UCLASS()
class CUBE_SKILL_API UCS_ProjectileNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

	/** 要生成的投射物。留空则用 ACS_Projectile。 */
	UPROPERTY(EditAnywhere, Category = "CS|Projectile")
	TSubclassOf<ACS_Projectile> ProjectileClass;

	/** 生成用的骨骼插槽。只取位置，不取朝向。 */
	UPROPERTY(EditAnywhere, Category = "CS|Projectile")
	FName SocketName = TEXT("hand_r");

	/**
	 * 瞄准点在玩家胶囊中心之上的高度比例。
	 * 0.5 大约落在胸口，火球不会往脚底扎。
	 */
	UPROPERTY(EditAnywhere, Category = "CS|Projectile", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ChestHeightScale = 0.5f;
};
