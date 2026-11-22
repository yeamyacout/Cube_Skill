// Fill out your copyright notice in the Description page of Project Settings.


#include "Cube_Skill/Public/Player/CS_PlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Projectile/CS_CubeProjectile.h"
#include "Characters/CS_EnemyCharacter.h"
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

	// ---------- 0. 前置检查 ----------
	// (a) 场上已有方块 -> 不允许再发射
	//     需求：同一时间只能存在一个方块，回收后才能再次发射。
	//     用弱引用判断：方块回收时会 Destroy()，弱引用自动失效。
	if (ActiveProjectile.IsValid() && IsValid(ActiveProjectile.Get()))
	{
		return;
	}

	// (b) 攻击动画还没播完 -> 不允许再次触发
	//     需求：动画播完才能再发。
	if (bIsAttacking)
	{
		return;
	}

	// ---------- 1. 播放攻击动画 ----------
	// 现在左键不再直接生成方块，而是播放攻击动画，
	// 由动画第 30 帧的 UCS_AttackNotify 触发真正的生成。
	//
	// 若没有配置攻击蒙太奇（AttackMontage 为空），
	// 退回旧的即时投掷行为，保证功能仍可用。
	USkeletalMeshComponent* MeshComp = MyCharacter->GetMesh();
	if (!AttackMontage || !MeshComp || !MeshComp->GetAnimInstance())
	{
		// 没有配置攻击动画 -> 直接生成方块
		SpawnProjectile();
		return;
	}

	bIsAttacking = true;
	bHasThrownThisAttack = false;

	// 绑定"播放结束"回调，用于恢复状态
	AttackMontageEndedDelegate.BindUObject(this, &ACS_PlayerController::HandleAttackMontageEnded);

	const float Duration = MeshComp->GetAnimInstance()->Montage_Play(AttackMontage);

	if (Duration <= 0.f)
	{
		// 播放失败（比如蒙太奇与当前骨架不匹配），直接兜底生成
		bIsAttacking = false;
		SpawnProjectile();
		return;
	}

	MeshComp->GetAnimInstance()->Montage_SetEndDelegate(AttackMontageEndedDelegate, AttackMontage);
}


void ACS_PlayerController::HandleAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != AttackMontage)
	{
		return;
	}

	// 动画结束（或被中断）后恢复状态，允许下次攻击
	bIsAttacking = false;

	// 若动画被打断且还没到投掷帧，本次攻击视为无效，不生成方块。
	// bHasThrownThisAttack 会在 Notify 里置 true；
	// 这里不需要额外处理，因为没生成就是没生成。
	bHasThrownThisAttack = false;
}


void ACS_PlayerController::OnAttackThrowNotify()
{
	// 只响应本次攻击的第一次通知
	if (!bIsAttacking || bHasThrownThisAttack)
	{
		return;
	}

	// 场上已经有方块就不再生成（双保险）
	if (ActiveProjectile.IsValid() && IsValid(ActiveProjectile.Get()))
	{
		return;
	}

	bHasThrownThisAttack = true;
	SpawnProjectile();
}


void ACS_PlayerController::SpawnProjectile()
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
	// 生成点必须脱离玩家身体，否则一出生就与胶囊/相机重叠并立刻判定碰撞。
	const FVector CameraForward = CameraRotation.Vector();
	FVector ProjectileSpawnLoc = CameraLocation + CameraForward * SpawnForwardOffset;

	{
		// 探测距离需覆盖近距离敌人判定范围（3 米）
		const float ProbeDistance = FMath::Max(
			SpawnForwardOffset + SpawnWallClearance,
			CloseRangeEnemyDistance + SpawnWallClearance);
		const FVector ProbeEnd = CameraLocation + CameraForward * ProbeDistance;

		// 只忽略玩家自己，敌人必须能被探测到
		FCollisionQueryParams ProbeParams(SCENE_QUERY_STAT(CS_SpawnProbe), false, MyCharacter);

		// 先用 Pawn 通道探测敌人（胶囊体一定响应），再用 Visibility 兜底场景
		FHitResult ProbeHit;
		const bool bEnemyHit = GetWorld()->LineTraceSingleByChannel(
			ProbeHit, CameraLocation, ProbeEnd, ECC_Pawn, ProbeParams);

		FHitResult SceneHit;
		const bool bSceneHit = !bEnemyHit && GetWorld()->LineTraceSingleByChannel(
			SceneHit, CameraLocation, ProbeEnd, ECC_Visibility, ProbeParams);

		if (bSceneHit)
		{
			ProbeHit = SceneHit;
		}

		if (bEnemyHit || bSceneHit)
		{
			const float ObstacleDist = ProbeHit.Distance;
			const bool bIsEnemy = ProbeHit.GetActor() && ProbeHit.GetActor()->IsA<ACS_EnemyCharacter>();

			// 近距离敌人：方块无法避免重叠，生成在身前并直接命中
			if (bIsEnemy && ObstacleDist <= CloseRangeEnemyDistance)
			{
				ProjectileSpawnLoc = CameraLocation + CameraForward * SpawnMinDistance;
				PendingNearbyEnemy = Cast<ACS_EnemyCharacter>(ProbeHit.GetActor());
			}
			// 场景物体：生成点必须落在障碍之前的空隙里，否则会嵌进墙或墙角
			else
			{
				const float AvailableDist = ObstacleDist - SpawnObstacleGap;
				float ChosenDist = FMath::Min(SpawnForwardOffset, AvailableDist);

				// 放不下最小距离（紧贴墙角）时，生成在极近处让它立刻撞上
				if (ChosenDist < SpawnMinDistance)
				{
					ChosenDist = FMath::Max(SpawnMinDistance * 0.5f, 10.f);
				}

				ProjectileSpawnLoc = CameraLocation + CameraForward * ChosenDist;
				PendingNearbyEnemy = nullptr;
			}
		}
		else
		{
			PendingNearbyEnemy = nullptr;
		}
	}

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
	// 这样子类细节面板里改的参数会生效；没配则用 C++ 默认类。
	ACS_CubeProjectile* NewProjectile = ACS_CubeProjectile::SpawnAndLaunch(
		this,
		SpawnTransform,
		LaunchDirection,
		MyCharacter,
		MyCharacter,
		ProjectileClass);

	// 记录当前方块，用于"只能存在一个"的判断
	if (IsValid(NewProjectile))
	{
		ActiveProjectile = NewProjectile;

		// ---------- 贴脸敌人：直接命中 ----------
		// 发射前的探测如果发现前方很近处有敌人，
		// 这里立刻对方块下"命中"指令，绕过物理碰撞。
		if (ACS_EnemyCharacter* NearEnemy = PendingNearbyEnemy.Get())
		{
			NewProjectile->HitEnemyImmediately(NearEnemy, CameraForward);
		}
	}

	// 用完即清，避免影响下一次发射
	PendingNearbyEnemy = nullptr;
}


bool ACS_PlayerController::HasActiveProjectile() const
{
	return ActiveProjectile.IsValid() && IsValid(ActiveProjectile.Get());
}


ACS_CubeProjectile* ACS_PlayerController::GetActiveProjectile() const
{
	return ActiveProjectile.Get();
}

	// 记录当前方块，用于"只能存在一个"的判断
	if (IsValid(NewProjectile))
	{
		ActiveProjectile = NewProjectile;

		// ---------- 贴脸敌人：直接命中 ----------
		// 发射前的探测如果发现前方很近处有敌人，
		// 这里立刻对方块下"命中"指令，绕过物理碰撞。
		if (ACS_EnemyCharacter* NearEnemy = PendingNearbyEnemy.Get())
		{
			NewProjectile->HitEnemyImmediately(NearEnemy, CameraForward);
		}
	}

	// 用完即清，避免影响下一次发射
	PendingNearbyEnemy = nullptr;
}


bool ACS_PlayerController::HasActiveProjectile() const
{
	return ActiveProjectile.IsValid() && IsValid(ActiveProjectile.Get());
}


ACS_CubeProjectile* ACS_PlayerController::GetActiveProjectile() const
{
	return ActiveProjectile.Get();
}

