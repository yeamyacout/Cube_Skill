#include "Notify/CS_ProjectileNotify.h"

#include "Actor/CS_Projectile.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

void UCS_ProjectileNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AActor* OwnerActor = MeshComp ? MeshComp->GetOwner() : nullptr;
	UWorld* World = OwnerActor ? OwnerActor->GetWorld() : nullptr;
	if (!World || !MeshComp->DoesSocketExist(SocketName))
	{
		return;
	}

	// 插槽只提供出生点。它这一帧的旋转跟着手指走，不能当弹道。
	const FVector SpawnLocation = MeshComp->GetSocketLocation(SocketName);

	FVector Direction = FVector::ZeroVector;
	if (const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0))
	{
		FVector TargetLocation = PlayerPawn->GetActorLocation();
		if (const ACharacter* PlayerCharacter = Cast<ACharacter>(PlayerPawn))
		{
			if (const UCapsuleComponent* Capsule = PlayerCharacter->GetCapsuleComponent())
			{
				TargetLocation.Z += Capsule->GetScaledCapsuleHalfHeight() * ChestHeightScale;
			}
		}
		Direction = TargetLocation - SpawnLocation;
	}

	// 动画预览里没有玩家时，沿施法者正前方飞，避免这一发直接消失。
	if (Direction.IsNearlyZero())
	{
		Direction = OwnerActor->GetActorForwardVector();
	}
	Direction = Direction.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return;
	}

	const FTransform SpawnTransform(Direction.Rotation(), SpawnLocation);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwnerActor;
	SpawnParams.Instigator = Cast<APawn>(OwnerActor);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	UClass* UseClass = ProjectileClass ? ProjectileClass.Get() : ACS_Projectile::StaticClass();
	ACS_Projectile* Projectile = World->SpawnActor<ACS_Projectile>(UseClass, SpawnTransform, SpawnParams);
	if (Projectile)
	{
		Projectile->LaunchInDirection(Direction);
	}
}

FString UCS_ProjectileNotify::GetNotifyName_Implementation() const
{
	return TEXT("发射投射物");
}
