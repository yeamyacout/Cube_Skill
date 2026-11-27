#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CS_FireWallActor.generated.h"

class UBoxComponent;
class UCameraShakeBase;
class UNiagaraComponent;
class UNiagaraSystem;
class UParticleSystemComponent;

/**
 * 单堵可碰撞火墙。
 * 表现、尺寸、跳伤、合拢爆炸都在本 Actor/蓝图上配。
 * 通知只负责生成、合拢移动、销毁。
 */
UCLASS()
class CUBE_SKILL_API ACS_FireWallActor : public AActor
{
	GENERATED_BODY()

public:
	ACS_FireWallActor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	float GetSideDistance() const { return SideDistance; }
	float GetForwardDistance() const { return ForwardDistance; }
	float GetWallLength() const { return WallLength; }

	/**
	 * 同一次技能左右墙合拢时调用。
	 * 播爆炸特效 + 镜头震动；玩家还在爆炸半径内再打一次伤害。
	 */
	void PlayMergeExplosion(const FVector& MergeLocation, AActor* DamageCauser) const;

protected:
	UFUNCTION()
	void OnDamageBoxBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnDamageBoxEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	void ApplyTickDamage();
	void StopDamageTimer();
	void ApplyAuthoredScale();
	void ActivateWallFX();
	bool IsPlayerVictim(const AActor* OtherActor) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CS|FireWall")
	TObjectPtr<UBoxComponent> DamageBox;

	/** 旧 Cascade 特效槽；在组件上填 Template。与 Niagara 可同时播。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CS|FireWall|FX")
	TObjectPtr<UParticleSystemComponent> WallCascadeFX;

	/** Niagara 特效槽；在组件上填 Niagara System Asset。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CS|FireWall|FX")
	TObjectPtr<UNiagaraComponent> WallFX;

	/** 墙离玩家身体中心的水平距离。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|FireWall", meta = (ClampMin = "0.0", Units = "cm"))
	float SideDistance = 180.f;

	/** 沿玩家前方的墙长（特效按 520cm 做好，只拉本地 X）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|FireWall", meta = (ClampMin = "10.0", Units = "cm", DisplayName = "长度"))
	float WallLength = 520.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|FireWall", meta = (ClampMin = "0.0", Units = "cm"))
	float ForwardDistance = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|FireWall", meta = (Units = "cm"))
	FVector BoxExtent = FVector(260.f, 60.f, 200.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|FireWall|Damage", meta = (ClampMin = "0.0"))
	float DamagePerTick = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|FireWall|Damage", meta = (ClampMin = "0.05"))
	float TickInterval = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|FireWall|Damage", meta = (ClampMin = "0.0"))
	float ExplosionDamage = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|FireWall|Damage", meta = (ClampMin = "0.0", Units = "cm"))
	float ExplosionRadius = 300.f;

	/** 合拢爆炸 Niagara（闪光 + 冲击环 + 火星）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|FireWall|Explosion")
	TObjectPtr<UNiagaraSystem> ExplosionFX;

	/** 爆炸特效整体缩放（传给 SpawnSystemAtLocation；发射器需 Local Space）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|FireWall|Explosion", meta = (ClampMin = "0.01"))
	float ExplosionFXScale = 1.f;

	/** 合拢时的镜头震动。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|FireWall|Explosion")
	TSubclassOf<UCameraShakeBase> ExplosionCameraShake;

	/** 震动内圈：满强度。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|FireWall|Explosion", meta = (ClampMin = "0.0", Units = "cm"))
	float CameraShakeInnerRadius = 200.f;

	/** 震动外圈：衰减到 0。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|FireWall|Explosion", meta = (ClampMin = "0.0", Units = "cm"))
	float CameraShakeOuterRadius = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|FireWall|Explosion", meta = (ClampMin = "0.0"))
	float CameraShakeScale = 1.f;

private:
	static constexpr float AuthoredWallLength = 520.f;

	FTimerHandle DamageTimerHandle;
	TWeakObjectPtr<AActor> OverlappingPlayer;
};
