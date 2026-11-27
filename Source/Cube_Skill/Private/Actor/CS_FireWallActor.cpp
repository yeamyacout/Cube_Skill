#include "Actor/CS_FireWallActor.h"

#include "Camera/CS_FireWallExplosionCameraShake.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ACS_FireWallActor::ACS_FireWallActor()
{
	PrimaryActorTick.bCanEverTick = false;

	DamageBox = CreateDefaultSubobject<UBoxComponent>(TEXT("DamageBox"));
	SetRootComponent(DamageBox);
	DamageBox->SetBoxExtent(BoxExtent);
	DamageBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DamageBox->SetCollisionObjectType(ECC_WorldDynamic);
	DamageBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	DamageBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	DamageBox->SetGenerateOverlapEvents(true);
	DamageBox->SetMobility(EComponentMobility::Movable);

	WallCascadeFX = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("WallCascadeFX"));
	WallCascadeFX->SetupAttachment(DamageBox);
	WallCascadeFX->bAutoActivate = false;
	WallCascadeFX->SetTemplate(nullptr);

	WallFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("WallFX"));
	WallFX->SetupAttachment(DamageBox);
	WallFX->SetAutoActivate(false);

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> WallFinder(TEXT("/Game/FireEnemy/NS_FireWall.NS_FireWall"));
	if (WallFinder.Succeeded())
	{
		WallFX->SetAsset(WallFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> ExplosionFinder(TEXT("/Game/FireEnemy/NS_FireWall_Explosion.NS_FireWall_Explosion"));
	if (ExplosionFinder.Succeeded())
	{
		ExplosionFX = ExplosionFinder.Object;
	}

	ExplosionCameraShake = UCS_FireWallExplosionCameraShake::StaticClass();
}

void ACS_FireWallActor::ActivateWallFX()
{
	// 双槽：Cascade / Niagara 谁配了播谁，都能配就一起播。
	if (WallCascadeFX && WallCascadeFX->Template)
	{
		WallCascadeFX->Activate(true);
	}

	if (WallFX && WallFX->GetAsset())
	{
		WallFX->Activate(true);
	}
}

void ACS_FireWallActor::BeginPlay()
{
	Super::BeginPlay();

	DamageBox->SetBoxExtent(BoxExtent);
	ApplyAuthoredScale();

	DamageBox->OnComponentBeginOverlap.AddDynamic(this, &ACS_FireWallActor::OnDamageBoxBeginOverlap);
	DamageBox->OnComponentEndOverlap.AddDynamic(this, &ACS_FireWallActor::OnDamageBoxEndOverlap);

	ActivateWallFX();
}

void ACS_FireWallActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopDamageTimer();
	Super::EndPlay(EndPlayReason);
}

void ACS_FireWallActor::ApplyAuthoredScale()
{
	const float LengthScale = FMath::Max(WallLength, 1.f) / AuthoredWallLength;
	SetActorScale3D(FVector(LengthScale, 1.f, 1.f));
}

void ACS_FireWallActor::PlayMergeExplosion(const FVector& MergeLocation, AActor* DamageCauser) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (ExplosionFX)
	{
		const float FXScale = FMath::Max(ExplosionFXScale, 0.01f);
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World,
			ExplosionFX,
			MergeLocation,
			FRotator::ZeroRotator,
			FVector(FXScale),
			true,
			true,
			ENCPoolMethod::AutoRelease,
			true);
	}

	if (ExplosionCameraShake)
	{
		// 按距离衰减的世界震动，近处冲击更重。
		UGameplayStatics::PlayWorldCameraShake(
			World,
			ExplosionCameraShake,
			MergeLocation,
			CameraShakeInnerRadius,
			FMath::Max(CameraShakeOuterRadius, CameraShakeInnerRadius),
			FMath::Max(CameraShakeScale, 0.f),
			false);
	}

	if (ExplosionDamage <= 0.f || ExplosionRadius <= 0.f)
	{
		return;
	}

	APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0);
	if (!Player || !Player->IsPlayerControlled())
	{
		return;
	}

	if (FVector::Dist(Player->GetActorLocation(), MergeLocation) > ExplosionRadius)
	{
		return;
	}

	AController* InstigatorController = nullptr;
	if (APawn* CauserPawn = Cast<APawn>(DamageCauser))
	{
		InstigatorController = CauserPawn->GetController();
	}
	else
	{
		InstigatorController = GetInstigatorController();
	}

	UGameplayStatics::ApplyDamage(
		Player,
		ExplosionDamage,
		InstigatorController,
		DamageCauser ? DamageCauser : const_cast<ACS_FireWallActor*>(this),
		nullptr);
}

void ACS_FireWallActor::OnDamageBoxBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!IsPlayerVictim(OtherActor) || DamagePerTick <= 0.f)
	{
		return;
	}

	OverlappingPlayer = OtherActor;

	UWorld* World = GetWorld();
	if (!World || World->GetTimerManager().IsTimerActive(DamageTimerHandle))
	{
		return;
	}

	ApplyTickDamage();
	World->GetTimerManager().SetTimer(
		DamageTimerHandle,
		this,
		&ACS_FireWallActor::ApplyTickDamage,
		TickInterval,
		true);
}

void ACS_FireWallActor::OnDamageBoxEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	if (OtherActor && OverlappingPlayer.Get() == OtherActor)
	{
		OverlappingPlayer.Reset();
		StopDamageTimer();
	}
}

void ACS_FireWallActor::ApplyTickDamage()
{
	AActor* Victim = OverlappingPlayer.Get();
	if (!Victim || !IsValid(Victim))
	{
		StopDamageTimer();
		OverlappingPlayer.Reset();
		return;
	}

	UGameplayStatics::ApplyDamage(
		Victim,
		DamagePerTick,
		GetInstigatorController(),
		this,
		nullptr);
}

void ACS_FireWallActor::StopDamageTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DamageTimerHandle);
	}
}

bool ACS_FireWallActor::IsPlayerVictim(const AActor* OtherActor) const
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn)
	{
		return false;
	}

	return Pawn->IsPlayerControlled();
}
