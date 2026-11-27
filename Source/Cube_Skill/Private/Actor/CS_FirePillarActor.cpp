#include "Actor/CS_FirePillarActor.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "TimerManager.h"

ACS_FirePillarActor::ACS_FirePillarActor()
{
	PrimaryActorTick.bCanEverTick = false;

	PillarRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PillarRoot"));
	SetRootComponent(PillarRoot);

	DamageBox = CreateDefaultSubobject<UBoxComponent>(TEXT("DamageBox"));
	DamageBox->SetupAttachment(PillarRoot);
	DamageBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DamageBox->SetCollisionObjectType(ECC_WorldDynamic);
	DamageBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	DamageBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	DamageBox->SetGenerateOverlapEvents(true);
	DamageBox->SetMobility(EComponentMobility::Movable);

	PillarCascadeFX = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("PillarCascadeFX"));
	PillarCascadeFX->SetupAttachment(PillarRoot);
	PillarCascadeFX->bAutoActivate = false;
	PillarCascadeFX->SetTemplate(nullptr);

	PillarFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("PillarFX"));
	PillarFX->SetupAttachment(PillarRoot);
	PillarFX->SetAutoActivate(false);

	ApplyBoxLayout();
	ApplyFXOrientation();
}

void ACS_FirePillarActor::ApplyBoxLayout()
{
	const float HalfLength = FMath::Max(PillarLength, 1.f) * 0.5f;
	DamageBox->SetBoxExtent(FVector(
		HalfLength,
		FMath::Max(PillarWidth, 1.f) * 0.5f,
		FMath::Max(PillarHeight, 1.f) * 0.5f));
}

void ACS_FirePillarActor::ApplyDamageBoxFacing(float RootYaw, float BoxYaw)
{
	const float HalfLength = FMath::Max(PillarLength, 1.f) * 0.5f;
	// 相对根的偏角：根跟特效，盒可以慢一拍。
	const float RelYaw = FMath::FindDeltaAngleDegrees(RootYaw, BoxYaw);
	const FRotator RelRot(0.f, RelYaw, 0.f);
	DamageBox->SetRelativeRotation(RelRot);
	// RelativeLocation 在父空间，要沿「盒的朝向」前移半长。
	DamageBox->SetRelativeLocation(RelRot.RotateVector(FVector(HalfLength, 0.f, 0.f)));
}

void ACS_FirePillarActor::ApplyFXOrientation()
{
	// 特效留在插槽原点；用偏移旋转把「向上喷」的资产拧成「向前喷」。
	if (PillarCascadeFX)
	{
		PillarCascadeFX->SetRelativeLocation(FVector::ZeroVector);
		PillarCascadeFX->SetRelativeRotation(FXRotationOffset);
	}
	if (PillarFX)
	{
		PillarFX->SetRelativeLocation(FVector::ZeroVector);
		PillarFX->SetRelativeRotation(FXRotationOffset);
	}
}

void ACS_FirePillarActor::ActivatePillarFX()
{
	ApplyFXOrientation();

	// 双槽：Cascade / Niagara 谁配了播谁，都能配就一起播。
	if (PillarCascadeFX && PillarCascadeFX->Template)
	{
		PillarCascadeFX->Activate(true);
	}

	if (PillarFX && PillarFX->GetAsset())
	{
		PillarFX->Activate(true);
	}
}

void ACS_FirePillarActor::RestartPillarFX()
{
	ApplyFXOrientation();

	// 清掉还停在旧朝向的粒子，再按根（特效）当前朝向重喷。
	if (PillarCascadeFX && PillarCascadeFX->Template)
	{
		PillarCascadeFX->Deactivate();
		PillarCascadeFX->KillParticlesForced();
		PillarCascadeFX->Activate(true);
	}

	if (PillarFX && PillarFX->GetAsset())
	{
		PillarFX->DeactivateImmediate();
		PillarFX->Activate(true);
	}
}

void ACS_FirePillarActor::BeginPlay()
{
	Super::BeginPlay();

	ApplyBoxLayout();

	DamageBox->OnComponentBeginOverlap.AddDynamic(this, &ACS_FirePillarActor::OnDamageBoxBeginOverlap);
	DamageBox->OnComponentEndOverlap.AddDynamic(this, &ACS_FirePillarActor::OnDamageBoxEndOverlap);

	ActivatePillarFX();
}

void ACS_FirePillarActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopDamageTimer();
	Super::EndPlay(EndPlayReason);
}

void ACS_FirePillarActor::UpdatePlacement(const FVector& SocketLocation, const FVector& ForwardFlat, float DeltaSeconds)
{
	ApplyBoxLayout();
	ApplyFXOrientation();

	FVector Forward = ForwardFlat;
	Forward.Z = 0.f;
	if (!Forward.Normalize())
	{
		Forward = GetActorForwardVector();
		Forward.Z = 0.f;
		if (!Forward.Normalize())
		{
			Forward = FVector::ForwardVector;
		}
	}

	const float TargetYaw = Forward.Rotation().Yaw;
	if (!bHasYaw)
	{
		CurrentYaw = TargetYaw;
		DesiredYaw = TargetYaw;
		bHasYaw = true;
	}
	else
	{
		// 只看「目标朝向」跳变，避免追转过程中每帧都清粒子。
		const float DesiredDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(DesiredYaw, TargetYaw));
		if (FXRestartYawThreshold > 0.f && DesiredDelta >= FXRestartYawThreshold)
		{
			RestartPillarFX();
		}
		DesiredYaw = TargetYaw;

		if (DamageBoxTurnSpeed <= 0.f || DeltaSeconds <= 0.f)
		{
			CurrentYaw = TargetYaw;
		}
		else
		{
			const float MaxStep = DamageBoxTurnSpeed * DeltaSeconds;
			CurrentYaw = FMath::FixedTurn(CurrentYaw, TargetYaw, MaxStep);
		}
	}

	// 根/特效：立刻跟敌人前方；伤害盒：单独按 DamageBoxTurnSpeed 追。
	const FVector RootLocation = SocketLocation + FVector(0.f, 0.f, HeightOffset);
	SetActorLocationAndRotation(RootLocation, FRotator(0.f, TargetYaw, 0.f));
	ApplyDamageBoxFacing(TargetYaw, CurrentYaw);
}

void ACS_FirePillarActor::OnDamageBoxBeginOverlap(
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
		&ACS_FirePillarActor::ApplyTickDamage,
		TickInterval,
		true);
}

void ACS_FirePillarActor::OnDamageBoxEndOverlap(
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

void ACS_FirePillarActor::ApplyTickDamage()
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

void ACS_FirePillarActor::StopDamageTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DamageTimerHandle);
	}
}

bool ACS_FirePillarActor::IsPlayerVictim(const AActor* OtherActor) const
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn)
	{
		return false;
	}

	return Pawn->IsPlayerControlled();
}
