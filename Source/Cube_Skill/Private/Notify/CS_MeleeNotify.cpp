// Fill out your copyright notice in the Description page of Project Settings.


#include "Notify/CS_MeleeNotify.h"

#include "Characters/CS_PlayerCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

// 攻击判定的专用日志分类，Output Log 里过滤 "CS_Melee" 即可。
DEFINE_LOG_CATEGORY_STATIC(LogCSMelee, Log, All);

void UCS_MeleeNotify::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
                                  const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	// 只清空"这个角色自己"的记录（不是全清，别的敌人可能还在挥刀）
	AlreadyHit.FindOrAdd(MeshComp).Reset();

	// 这条日志能证明"蒙太奇确实播到了这个 Notify 窗口"。
	//   有它、但后面没有 Tick 的日志 -> Notify 窗口长度是 0，永远不会 Tick
	//   连它都没有                  -> 蒙太奇根本没播 / GA 没激活
	if (bDebug)
	{
		UE_LOG(LogCSMelee, Log, TEXT("【近战】NotifyBegin  Owner=%s  Socket=%s  动画总长=%.2fs"),
			*GetNameSafe(MeshComp ? MeshComp->GetOwner() : nullptr),
			*SocketName.ToString(), TotalDuration);
	}
}

void UCS_MeleeNotify::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime,
                                 const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
	
	if (!IsValid(MeshComp)) return;

	if (!MeshComp->GetOwner()) return;

	TArray<FHitResult> HitResults;
	
	HitResults = PerformSphereTrace(MeshComp);

	// 只在真的扫到东西时打印，避免每帧刷屏。
	// 如果开了 bDebug 却一直没有这条日志 -> 球扫掠没碰到任何东西（距离太远/通道不对）。
	if (bDebug && HitResults.Num() > 0)
	{
		UE_LOG(LogCSMelee, Log, TEXT("【近战】NotifyTick  扫到 %d 个物件"), HitResults.Num());
	}

	SendEventToActor(HitResults , MeshComp);
}

void UCS_MeleeNotify::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	// 这次挥刀结束，把这个角色的记录整个删掉，避免 Map 无限增长
	AlreadyHit.Remove(MeshComp);
}

TArray<FHitResult> UCS_MeleeNotify::PerformSphereTrace(USkeletalMeshComponent* MeshCom)
{
	TArray<FHitResult> HitResults;

	
	FTransform SocketTransform = MeshCom->GetSocketTransform(SocketName);
	FVector Start = SocketTransform.GetLocation();
	FVector ForWard =UKismetMathLibrary::GetForwardVector(SocketTransform.GetRotation().Rotator());
	FVector End = Start - ForWard * SocketExtensionOffset;

	// ============================================================
	//  为什么用"按对象类型"查询，而不是 Visibility 通道？
	// ============================================================
	// 引擎默认碰撞预设（Engine/Config/BaseEngine.ini）里：
	//   Pawn 预设          -> Visibility 通道 = Ignore   （角色胶囊体用的就是它）
	//   CharacterMesh 预设 -> Visibility 通道 = Ignore   （角色网格用的就是它）
	//
	// 也就是说：角色（包括玩家）对 Visibility 通道是"忽略"的。
	// 之前这里用 ETraceTypeQuery::TraceTypeQuery1（就是 Visibility），
	// 结果球扫掠永远穿角色而过、只打到墙 —— 表现就是"敌人挥刀但没有伤害"。
	//
	// 改成按 ObjectType = Pawn 查询后，胶囊体才会被扫到。
	// 以后若还要让近战打可破坏物，往 ObjectTypes 里再加 WorldDynamic 即可。
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));

	const TArray<AActor*> ActorsToIgnore = { MeshCom->GetOwner() };

	UKismetSystemLibrary::SphereTraceMultiForObjects(
		MeshCom,
		Start,
		End,
		SphereTraceRadius,
		ObjectTypes,
		/*bTraceComplex=*/false,
		ActorsToIgnore,
		bDebug ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None,
		HitResults,
		/*bIgnoreSelf=*/true,
		FLinearColor::Red,
		FLinearColor::Green,
		1.f);

	return HitResults;
}

void UCS_MeleeNotify::SendEventToActor(TArray<FHitResult> HitResults, USkeletalMeshComponent* MeshCom)
{
	// 取出"本次挥刀这个角色"自己的已命中记录
	TSet<TWeakObjectPtr<AActor>>& HitOnce = AlreadyHit.FindOrAdd(MeshCom);

	// 用 const 引用，避免每个命中结果都拷贝一次
	for (const FHitResult& HitResult : HitResults)
	{
		AActor* HitActor = HitResult.GetActor();
		ACS_PlayerCharacter* PlayerCharacter = Cast<ACS_PlayerCharacter>(HitActor);

		// 扫到了、但不是玩家 -> 打印出来。
		// 这一条是"看起来打着人了却没掉血"最常见的解释：命中了敌人自己/场景物件。
		if (!IsValid(PlayerCharacter))
		{
			if (bDebug)
			{
				UE_LOG(LogCSMelee, Warning, TEXT("【近战】命中 %s，但它不是 ACS_PlayerCharacter，已跳过"),
					*GetNameSafe(HitActor));
			}
			continue;
		}

		// 同一次挥刀对同一个人只结算一次
		const TWeakObjectPtr<AActor> Key(PlayerCharacter);
		if (HitOnce.Contains(Key)) continue;

		HitOnce.Add(Key);
		AActor* Source = MeshCom->GetOwner();

		if (bDebug)
		{
			UE_LOG(LogCSMelee, Log, TEXT("【近战】对 %s 调用 ApplyDamage(%.1f, 来源=%s)"),
				*GetNameSafe(PlayerCharacter), Damage, *GetNameSafe(Source));
		}

		UGameplayStatics::ApplyDamage(PlayerCharacter, Damage, Source ? Source->GetInstigatorController() : nullptr, Source, nullptr);
	}
}
