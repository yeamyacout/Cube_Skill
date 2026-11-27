#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CS_FirePillarActor.generated.h"

class UBoxComponent;
class UNiagaraComponent;
class UParticleSystemComponent;
class USceneComponent;

/**
 * 重击二：从手部插槽往敌人前方喷的火柱。
 * 尺寸、跳伤、特效都在本 Actor/蓝图上配。
 * 通知只负责按 Socket 摆位、对齐敌人朝向、到点销毁。
 */
UCLASS()
class CUBE_SKILL_API ACS_FirePillarActor : public AActor
{
	GENERATED_BODY()

public:
	ACS_FirePillarActor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 通知每帧调用：根钉在插槽，朝向朝敌人水平前方平滑跟上。 */
	void UpdatePlacement(const FVector& SocketLocation, const FVector& ForwardFlat, float DeltaSeconds);

	float GetPillarLength() const { return PillarLength; }
	float GetHeightOffset() const { return HeightOffset; }

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
	void ApplyBoxLayout();
	/** 伤害盒相对根的朝向/偏移；RootYaw 为特效朝向，BoxYaw 为伤害朝向。 */
	void ApplyDamageBoxFacing(float RootYaw, float BoxYaw);
	void ApplyFXOrientation();
	void ActivatePillarFX();
	void RestartPillarFX();
	bool IsPlayerVictim(const AActor* OtherActor) const;

	/** 钉在手部插槽；特效也挂这里，保证从插槽冒出。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CS|FirePillar")
	TObjectPtr<USceneComponent> PillarRoot;

	/** 相对根前移半长，覆盖「从手往前」的伤害区。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CS|FirePillar")
	TObjectPtr<UBoxComponent> DamageBox;

	/** 旧 Cascade 特效槽；在组件上填 Template。与 Niagara 可同时播。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CS|FirePillar|FX")
	TObjectPtr<UParticleSystemComponent> PillarCascadeFX;

	/** Niagara 特效槽；默认空，蓝图里再配。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CS|FirePillar|FX")
	TObjectPtr<UNiagaraComponent> PillarFX;

	/** 沿敌人前方喷出的长度。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|FirePillar", meta = (ClampMin = "10.0", Units = "cm", DisplayName = "长度"))
	float PillarLength = 600.f;

	/** 水平宽度。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|FirePillar", meta = (ClampMin = "1.0", Units = "cm", DisplayName = "宽度"))
	float PillarWidth = 80.f;

	/** 竖直厚度。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|FirePillar", meta = (ClampMin = "1.0", Units = "cm", DisplayName = "高度"))
	float PillarHeight = 120.f;

	/** 相对手部插槽的竖直偏移。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|FirePillar", meta = (Units = "cm"))
	float HeightOffset = 0.f;

	/**
	 * 特效相对根的旋转。
	 * 多数旧 Cascade 沿本地 +Z 喷，默认 Pitch=-90 把它拧到 Actor 前方（+X）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|FirePillar|FX")
	FRotator FXRotationOffset = FRotator(-90.f, 0.f, 0.f);

	/**
	 * 目标朝向突变超过该角度时清粒子重喷。
	 * 0 = 从不因转向清粒子。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|FirePillar|FX", meta = (ClampMin = "0.0", Units = "deg"))
	float FXRestartYawThreshold = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|FirePillar|Damage", meta = (ClampMin = "0.0"))
	float DamagePerTick = 5.f;

	/**
	 * 仅伤害盒的转向速度（度/秒），不影响特效。
	 * 特效跟敌人目标朝向；伤害盒按此速度追。0 = 伤害盒也立即对齐。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|FirePillar|Damage", meta = (ClampMin = "0.0", Units = "deg/s", DisplayName = "伤害盒转向速度"))
	float DamageBoxTurnSpeed = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|FirePillar|Damage", meta = (ClampMin = "0.05"))
	float TickInterval = 0.25f;

private:
	FTimerHandle DamageTimerHandle;
	TWeakObjectPtr<AActor> OverlappingPlayer;
	bool bHasYaw = false;
	float CurrentYaw = 0.f;
	float DesiredYaw = 0.f;
};
