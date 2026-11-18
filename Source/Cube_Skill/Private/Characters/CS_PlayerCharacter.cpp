// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/CS_PlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"


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
}


void ACS_PlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

