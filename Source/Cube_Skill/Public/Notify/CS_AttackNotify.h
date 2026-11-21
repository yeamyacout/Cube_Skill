// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "CS_AttackNotify.generated.h"

/**
 * UCS_AttackNotify - 攻击动画的"投掷"时机通知
 *
 * 作用：
 *   在攻击动画的指定帧插入本通知，动画播到该帧时才会真正生成方块。
 *   这样投掷动作与挥手动作就能精确对齐。
 *
 * 使用方式：
 *   1. 打开攻击动画（AS_Attack），在 Notifies 轨道上添加本通知
 *   2. 拖到第 30 帧（30fps 下约 1.0 秒）的位置
 *
 * 为什么用 AnimNotify 而不是在 C++ 里计时：
 *   · 动画可能被加速/减速播放，Notify 会自动跟随实际播放进度
 *   · 动画被打断时不会误触发
 *   · 时机由美术/动画同学在编辑器里可视化调整，不用改代码
 */
UCLASS()
class CUBE_SKILL_API UCS_AttackNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	/**
	 * 通知触发时调用
	 * @param MeshComp   播放动画的骨骼网格组件（其 Owner 就是角色）
	 * @param Animation  当前播放的动画序列
	 * @param EventReference 通知事件引用
	 */
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	/** 通知在编辑器里显示的名称 */
	virtual FString GetNotifyName_Implementation() const override;
};
