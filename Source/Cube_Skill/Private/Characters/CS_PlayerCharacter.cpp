// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/CS_PlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CS_CubeRotationComponent.h"
#include "Components/CS_HealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"


ACS_PlayerCharacter::ACS_PlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);
	
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;
	
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	GetCharacterMovement()->JumpZVelocity = 500.0f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.0f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.0f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.0f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	
	FollowCamera = CreateDefaultSubobject<UCameraComponent>("FollowCamera");
	FollowCamera->SetupAttachment(GetCapsuleComponent());
	
	FollowCamera->SetRelativeLocation(FVector(0.f, 0.f, 64.f)); 
	
	FollowCamera->bUsePawnControlRotation = true;

	HealthComponent = CreateDefaultSubobject<UCS_HealthComponent>(TEXT("HealthComponent"));
}


void ACS_PlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 翻转组件是在蓝图里 Add Component 加的，C++ 侧按类型取一次并缓存。
	// 取不到不报错：说明这个角色没有方块翻转功能。
	CubeRotation = FindComponentByClass<UCS_CubeRotationComponent>();
	if (IsValid(CubeRotation))
	{
		CubeRotation->OnOrientationChanged.AddDynamic(this, &ACS_PlayerCharacter::HandleFlipStarted);
	}
	else
	{
		// 组件不在角色身上时，控制器按 Pawn 找组件会拿不到，E/Q 会静默失效
		UE_LOG(LogTemp, Warning,
			TEXT("[CS_PlayerCharacter] 角色上没有 UCS_CubeRotationComponent：E/Q 翻转不会生效，"
				 "请确认组件是加在这个角色蓝图里的。"));
	}

	// 显示部件优先用蓝图里指定的；没指定就自动找（见 ResolveDisplayTarget）
	ResolveDisplayTarget();
	ApplyDisplayRotation();
}

void ACS_PlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DisplayTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void ACS_PlayerCharacter::HandleFlipStarted()
{
	// 先立刻写一次：bAnimate 关掉时这里就是最终结果，后面不会再有任何一帧
	ApplyDisplayRotation();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 这里无脑开表：组件是先广播、后把增量放进队列的，此刻 IsAnimating() 可能还是 false。
	// 停表的判断放到回调里做，既不漏帧也不用依赖广播时机。
	World->GetTimerManager().SetTimer(
		DisplayTimerHandle,
		this,
		&ACS_PlayerCharacter::UpdateDisplayFromTimer,
		DisplayUpdateInterval,
		true);
}

void ACS_PlayerCharacter::UpdateDisplayFromTimer()
{
	ApplyDisplayRotation();

	// 动画播完就停表，回到"没有 Tick、也没有定时器"的状态
	if (!IsValid(CubeRotation) || !CubeRotation->IsAnimating())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(DisplayTimerHandle);
		}
	}
}

void ACS_PlayerCharacter::ApplyDisplayRotation()
{
	if (IsValid(CubeRotation) && IsValid(CubeDisplayMesh))
	{
		CubeDisplayMesh->SetRelativeRotation(CubeRotation->GetDisplayQuat());
	}
}

void ACS_PlayerCharacter::ResolveDisplayTarget()
{
	if (IsValid(CubeDisplayMesh))
	{
		return;
	}

	// 退路 1：带 CubeDisplay 标签的场景组件
	TArray<USceneComponent*> SceneComponents;
	GetComponents<USceneComponent>(SceneComponents);
	for (USceneComponent* Component : SceneComponents)
	{
		if (Component && Component->ComponentHasTag(TEXT("CubeDisplay")))
		{
			CubeDisplayMesh = Component;
			UE_LOG(LogTemp, Log, TEXT("[CS_PlayerCharacter] 朝向写入带标签组件：%s"), *Component->GetName());
			return;
		}
	}

	// 退路 2：角色上恰好只有一个 StaticMesh 时直接用它
	TArray<UStaticMeshComponent*> MeshComponents;
	GetComponents<UStaticMeshComponent>(MeshComponents);
	if (MeshComponents.Num() == 1)
	{
		CubeDisplayMesh = MeshComponents[0];
		UE_LOG(LogTemp, Log, TEXT("[CS_PlayerCharacter] 朝向写入唯一网格：%s"), *MeshComponents[0]->GetName());
		return;
	}

	// 到这里说明没配上，且自动找也不唯一——直接说清楚，免得又变成"没反应"
	UE_LOG(LogTemp, Warning,
		TEXT("[CS_PlayerCharacter] 找不到显示朝向的部件：请在蓝图里指定 CubeDisplayMesh，"
			 "或给目标组件加 Tag \"CubeDisplay\"。"));
}


