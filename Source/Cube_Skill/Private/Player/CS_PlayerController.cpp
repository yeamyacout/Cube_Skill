// Fill out your copyright notice in the Description page of Project Settings.


#include "Cube_Skill/Public/Player/CS_PlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "GameFramework/Character.h"
#include "Projectile/CS_CubeProjectile.h"
#include "Engine/World.h"

void ACS_PlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (IsValid(InputSubsystem))
	{
		for (UInputMappingContext* InputMappingContext : InputMappingContexts)
		{
			InputSubsystem->AddMappingContext(InputMappingContext , 0);
		}
	}
	
	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	if (!IsValid(EnhancedInputComponent)) return;
	
	EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Triggered, this, &ACS_PlayerController::Jump);
	EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACS_PlayerController::Stopjump);
	EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ACS_PlayerController::Move);
	EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this,&ACS_PlayerController::Look);

	// 左键发射：必须用 Started 而不是 Triggered。
	//   Triggered：按住期间每帧都触发 -> 一次点击可能连发多颗
	//   Started  ：只在按下的瞬间触发一次 -> 正好符合"点一下扔一个"
	EnhancedInputComponent->BindAction(PrimaryAction, ETriggerEvent::Started, this, &ACS_PlayerController::Primary);
}

void ACS_PlayerController::Jump()
{
	if (!IsValid(GetCharacter())) return;
	
	GetCharacter()->Jump();
}

void ACS_PlayerController::Stopjump()
{
	if (!IsValid(GetCharacter())) return;
	
	GetCharacter()->StopJumping();
}

void ACS_PlayerController::Move(const FInputActionValue& Value)
{
	if (!IsValid(GetPawn())) return;
	
	const FVector2D MovementVector = Value.Get<FVector2D>();
	
	const FRotator YawRotation(0.f, GetControlRotation().Yaw, 0.f);
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	GetPawn()->AddMovementInput(ForwardDirection, MovementVector.Y);
	GetPawn()->AddMovementInput(RightDirection, MovementVector.X);
}

void ACS_PlayerController::Look(const FInputActionValue& Value)
{
	const FVector2D LookAxisVector = Value.Get<FVector2D>();

	AddYawInput(LookAxisVector.X);
	AddPitchInput(LookAxisVector.Y);
	
	
}

void ACS_PlayerController::Primary()
{
	ACharacter* MyCharacter = GetCharacter();
	if (!IsValid(MyCharacter)) return;

	// ---------- 1. 从屏幕中心（准星）发射一条射线 ----------
	// 为什么要用 "屏幕中心" 而不是角色朝向？
	//   因为相机是第一人称跟随相机，玩家看到的是准星位置。
	//   用准星射线能实现"瞄准哪打哪"。
	FVector CameraLocation;
	FRotator CameraRotation;
	GetPlayerViewPoint(CameraLocation, CameraRotation);

	const FVector TraceStart = CameraLocation;
	// 射线终点：准星方向延伸 10000 单位（100 米），足够覆盖常见战斗距离
	const FVector TraceEnd = TraceStart + CameraRotation.Vector() * 10000.f;

	// ---------- 2. 射线检测，找出准星指向的点 ----------
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CS_PrimaryTrace), false, MyCharacter);
	// 忽略自身，避免射线打到自己身上

	FHitResult HitResult;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		TraceStart,
		TraceEnd,
		ECC_Visibility,     // 使用可见性通道，与常规瞄准一致
		QueryParams);

	// 命中则朝命中点，未命中则朝射线终点（相当于朝天空/远处打）
	const FVector AimPoint = bHit ? HitResult.ImpactPoint : TraceEnd;

	// ---------- 3. 计算生成位置 ----------
	// 从角色眼睛附近生成，往前推一点，避免和自身碰撞体重叠
	// 注意：变量名不能叫 SpawnLocation，会与 APlayerController::SpawnLocation 成员重名
	const FVector EyeHeight = MyCharacter->GetActorLocation() + FVector(0.f, 0.f, 60.f);
	const FVector ProjectileSpawnLoc = EyeHeight + CameraRotation.Vector() * 60.f;

	// ---------- 4. 计算飞行方向 ----------
	// 从生成点指向准星目标点，这样方块才会真正飞向准星位置
	FVector LaunchDirection = (AimPoint - ProjectileSpawnLoc).GetSafeNormal();
	if (LaunchDirection.IsNearlyZero())
	{
		// 极端情况（目标点和生成点重合）：退回用相机朝向
		LaunchDirection = CameraRotation.Vector();
	}

	// 生成朝向跟随飞行方向
	const FTransform SpawnTransform(CameraRotation, ProjectileSpawnLoc);

	// ---------- 5. 生成并发射立方体 ----------
	// ProjectileClass 若在蓝图里配了（指向方块蓝图子类），就用子类，
	// 这样子类细节面板里改的大小的参数会生效；没配则用 C++ 默认类。
	ACS_CubeProjectile::SpawnAndLaunch(
		this,
		SpawnTransform,
		LaunchDirection,
		MyCharacter,
		MyCharacter,
		ProjectileClass);
}
