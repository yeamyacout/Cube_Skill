// Fill out your copyright notice in the Description page of Project Settings.


#include "Cube_Skill/Public/Characters/CS_BaseCharacter.h"


ACS_BaseCharacter::ACS_BaseCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
}

void ACS_BaseCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}
