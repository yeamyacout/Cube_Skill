#include "Camera/CS_FireWallExplosionCameraShake.h"

#include "Shakes/PerlinNoiseCameraShakePattern.h"

UCS_FireWallExplosionCameraShake::UCS_FireWallExplosionCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSingleInstance = true;

	UPerlinNoiseCameraShakePattern* Pattern = CreateDefaultSubobject<UPerlinNoiseCameraShakePattern>(TEXT("ShakePattern"));
	// 短、猛：先一下砸上去再很快收掉，才有冲击感。
	Pattern->Duration = 0.32f;
	Pattern->BlendInTime = 0.02f;
	Pattern->BlendOutTime = 0.18f;

	Pattern->LocationAmplitudeMultiplier = 1.f;
	Pattern->X.Amplitude = 10.f;
	Pattern->X.Frequency = 45.f;
	Pattern->Y.Amplitude = 10.f;
	Pattern->Y.Frequency = 40.f;
	Pattern->Z.Amplitude = 6.f;
	Pattern->Z.Frequency = 35.f;

	Pattern->RotationAmplitudeMultiplier = 1.f;
	Pattern->Pitch.Amplitude = 4.f;
	Pattern->Pitch.Frequency = 35.f;
	Pattern->Yaw.Amplitude = 3.f;
	Pattern->Yaw.Frequency = 30.f;
	Pattern->Roll.Amplitude = 2.f;
	Pattern->Roll.Frequency = 25.f;

	Pattern->FOV.Amplitude = 2.5f;
	Pattern->FOV.Frequency = 20.f;

	SetRootShakePattern(Pattern);
}
