#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "CS_FireWallExplosionCameraShake.generated.h"

/** 火墙合拢爆炸用的短促冲击震动。 */
UCLASS()
class CUBE_SKILL_API UCS_FireWallExplosionCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	UCS_FireWallExplosionCameraShake(const FObjectInitializer& ObjectInitializer);
};
