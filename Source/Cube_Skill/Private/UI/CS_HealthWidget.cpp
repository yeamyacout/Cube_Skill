// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/CS_HealthWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CS_HealthComponent.h"
#include "Components/ProgressBar.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"   // GetPlayerCharacter 返回 ACharacter*，必须完整类型才能当 AActor* 用
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

// 血量 UI 专用的日志分类，Output Log 里过滤 "CS_HealthUI" 即可。
DEFINE_LOG_CATEGORY_STATIC(LogCSHealthUI, Log, All);


/**
 * 控件创建时（≈ Unity 的 OnEnable）
 *
 * 三件事：
 *   1. 确保拿到血条控件
 *   2. 订阅血量百分比事件（角色可能还没生成，交给定时器重试）
 *   3. 先按当前百分比画一次，否则会一直显示满血直到第一次受伤
 */
void UCS_HealthWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// ---------- 1. 拿血条控件 ----------
	// BindWidgetOptional 是按变量名自动绑的，名字对不上就是 nullptr。
	// 这里做一层兜底：自动找控件树里第一个 ProgressBar，让"忘了改名"也能跑起来。
	if (!HealthBar && WidgetTree)
	{
		WidgetTree->ForEachWidget([this](UWidget* Widget)
		{
			if (!HealthBar)
			{
				HealthBar = Cast<UProgressBar>(Widget);
			}
		});

		if (HealthBar)
		{
			UE_LOG(LogCSHealthUI, Warning,
				TEXT("设计器里没有名为 HealthBar 的控件，已自动改用 \"%s\"。建议把它的变量名改成 HealthBar。"),
				*HealthBar->GetName());
		}
		else
		{
			UE_LOG(LogCSHealthUI, Error,
				TEXT("控件树里找不到任何 ProgressBar，血条不会更新。"));
		}
	}

	// ---------- 2. 订阅事件 ----------
	TryBindHealth();

	// ---------- 3. 补一次初值 ----------
	ForceRefresh();

	// 还没绑上说明角色还没 Possess：每 0.1 秒重试一次，绑上就停。
	// （不用 NativeTick，原因见头文件里的说明）
	if (!CachedHealth && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			BindRetryTimer, this, &UCS_HealthWidget::RetryBind, 0.1f, /*bLoop=*/true);
	}
}


/**
 * 定时器回调：反复尝试绑定，直到成功
 */
void UCS_HealthWidget::RetryBind()
{
	TryBindHealth();

	if (CachedHealth)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(BindRetryTimer);
		}

		// 绑定成功后再刷一次，保证显示的是最新血量
		ForceRefresh();
	}
}


/**
 * 找到玩家身上的 HealthComponent 并订阅 OnHealthPercentChanged
 */
void UCS_HealthWidget::TryBindHealth()
{
	if (CachedHealth)
	{
		return;
	}

	// GetOwningPlayerPawn ≈ Unity 里"拿到本地玩家角色"
	AActor* OwnerActor = GetOwningPlayerPawn();

	// 兜底：控件如果没设 Owning Player，就直接取 0 号玩家角色
	if (!IsValid(OwnerActor))
	{
		OwnerActor = UGameplayStatics::GetPlayerCharacter(this, 0);
	}

	if (!IsValid(OwnerActor))
	{
		return;
	}

	// FindComponentByClass ≈ Unity 的 GetComponent<T>()
	UCS_HealthComponent* Health = OwnerActor->FindComponentByClass<UCS_HealthComponent>();
	if (!IsValid(Health))
	{
		// 组件不存在：多半是玩家蓝图里没挂，或者换成了别的角色类
		if (bLogDebug)
		{
			UE_LOG(LogCSHealthUI, Warning, TEXT("%s 身上没有找到 HealthComponent"), *GetNameSafe(OwnerActor));
		}
		return;
	}

	CachedHealth = Health;

	// AddDynamic ≈ Unity 的 +=
	CachedHealth->OnHealthPercentChanged.AddDynamic(this, &UCS_HealthWidget::HandleHealthPercentChanged);

	if (bLogDebug)
	{
		UE_LOG(LogCSHealthUI, Log, TEXT("已绑定 %s 的血量百分比事件"), *GetNameSafe(OwnerActor));
	}
}


/**
 * 用组件当前百分比立刻刷新一次
 */
void UCS_HealthWidget::ForceRefresh()
{
	if (!CachedHealth)
	{
		return;
	}

	// 直接查组件拿当前值，不依赖"事件有没有发过"
	SetBarPercent(CachedHealth->GetHealthPercent());
}


/**
 * 血量百分比变化回调（受伤 / 治疗 / 自动回复 / 复活都会走到这里）
 */
void UCS_HealthWidget::HandleHealthPercentChanged(float HealthPercent)
{
	SetBarPercent(HealthPercent);

	if (bLogDebug)
	{
		UE_LOG(LogCSHealthUI, Log, TEXT("血条更新：%.0f%%"), HealthPercent * 100.f);
	}
}


/**
 * 真正写血条（统一在这里钳制，防止外部传来奇怪的值）
 */
void UCS_HealthWidget::SetBarPercent(float Percent)
{
	if (!HealthBar)
	{
		return;
	}

	HealthBar->SetPercent(FMath::Clamp(Percent, 0.f, 1.f));
}


/**
 * 控件销毁（≈ Unity 的 OnDisable）
 *
 * 动态委托内部存的是弱引用，控件销毁后即使不解绑，广播时也会跳过它、不会崩。
 * 但规范上还是解绑：避免 CachedHealth 持有无效引用，也方便以后换绑定方式。
 */
void UCS_HealthWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindRetryTimer);
	}

	UnbindHealth();
	Super::NativeDestruct();
}


void UCS_HealthWidget::UnbindHealth()
{
	if (CachedHealth)
	{
		CachedHealth->OnHealthPercentChanged.RemoveDynamic(this, &UCS_HealthWidget::HandleHealthPercentChanged);
		CachedHealth = nullptr;
	}
}
