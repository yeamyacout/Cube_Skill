// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/CS_HealthComponent.h"

// AActor：需要访问 GetOwner()
#include "GameFramework/Actor.h"


/**
 * 构造函数
 *
 * 注意事项：
 *   构造函数在编辑器里也会被调用（用来创建 CDO，即类默认对象），
 *   所以这里只能设置"配置性质"的默认值，绝不能访问 World、Owner 或其他运行时对象。
 */
UCS_HealthComponent::UCS_HealthComponent()
{
	// 允许 Tick，但下面 bStartWithTickEnabled = false 表示默认不运行。
	// 真正的开启时机在 BeginPlay（只有启用自动回复时才打开）。
	PrimaryComponentTick.bCanEverTick = true;

	// 关键性能优化：
	// 关闭后组件完全不参与每帧更新，没有 Tick 函数调用的开销。
	// 对于没有自动回复的血量组件（绝大多数情况），这是明显的节省。
	PrimaryComponentTick.bStartWithTickEnabled = false;
}


/**
 * 游戏开始时调用
 *
 * 完整流程：
 *   1. 修正 MaxHealth 的非法值
 *   2. 初始化 CurrentHealth
 *   3. 绑定 Owner 的标准受击事件（让 ApplyDamage 也能驱动本组件）
 *   4. 按配置决定是否开启 Tick
 *   5. 广播一次初始血量，方便 UI 在开局就能取到正确数据
 */
void UCS_HealthComponent::BeginPlay()
{
	Super::BeginPlay();

	// 防御性处理：编辑器里有可能配置出 0 或负数，钳到最小 1。
	// 因为后面 GetHealthPercent 会拿 MaxHealth 做除数，必须保证它 > 0。
	MaxHealth = FMath::Max(1.f, MaxHealth);

	// 出生即满血
	CurrentHealth = MaxHealth;

	// 与 UE 标准伤害系统对接。
	// OnTakeAnyDamage 是 AActor 上的一个多播委托，任何 UGameplayStatics::ApplyDamage
	// 或伤害区域(ApplyRadialDamage)最终都会触发它。
	// 绑定之后，外部系统不需要知道本组件存在，也能正常扣血。
	if (AActor* Owner = GetOwner())
	{
		Owner->OnTakeAnyDamage.AddDynamic(this, &UCS_HealthComponent::HandleOwnerTakeAnyDamage);
	}

	// 只有开启自动回复才需要每帧计算，否则保持 Tick 关闭以节省性能。
	SetComponentTickEnabled(bEnableRegen);

	// 开局广播一次。
	// 理由：UI 通常在角色 Spawn 后才创建并绑定事件，
	// 如果不主动广播一次，血条会一直显示空直到第一次受伤。
	// Delta 传 0 表示"这是初始状态，不是变化"。
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth, 0.f);
}


/**
 * 每帧更新（仅在 bEnableRegen 开启时执行实际逻辑）
 *
 * 自动回复的状态机：
 *   受伤 -> TimeSinceLastDamage 清零
 *   每帧  TimeSinceLastDamage 累加
 *   当 TimeSinceLastDamage >= RegenDelay 且未满血 -> 开始按秒回血
 */
void UCS_HealthComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                        FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 未开启回复，或已死亡（死人不需要回血），直接返回。
	if (!bEnableRegen || bIsDead)
	{
		return;
	}

	// 累加距离上次受伤的时间，用于判断冷却是否结束。
	TimeSinceLastDamage += DeltaTime;

	// 条件：冷却结束 + 没满血（满血时不必白白广播事件）
	if (TimeSinceLastDamage >= RegenDelay && CurrentHealth < MaxHealth)
	{
		// 按帧积分：每秒回 RegenPerSecond，这一帧就回 RegenPerSecond * DeltaTime。
		// 这样在不同帧率下回复速度保持一致（不会因为帧率高就回得快）。
		const float RegenAmount = RegenPerSecond * DeltaTime;

		// 统一走内部入口，它会负责钳制到 MaxHealth 并广播事件。
		ChangeHealthInternal(CurrentHealth + RegenAmount, nullptr);
	}
}


/**
 * 造成伤害
 *
 * 返回值是"实际扣除的血量"，这个设计很重要：
 *   - 血量不足时，返回的是剩余血量而非传入值
 *   - 已死亡时不重复扣血，返回 0
 * 调用方可以据此判断"是否真的打到了"（比如播放受击动画）。
 */
float UCS_HealthComponent::ApplyDamage(float DamageAmount, AActor* DamageCauser)
{
	// 已死亡不再重复结算；非正数伤害视为无效（避免被"负伤害"当成治疗）。
	if (bIsDead || DamageAmount <= 0.f)
	{
		return 0.f;
	}

	// 记录伤害前的血量，算完后再做差，这样能准确知道"真实掉了多少血"。
	const float HealthBefore = CurrentHealth;

	// 钳制到 [0, MaxHealth]：
	//   减去伤害后可能为负 -> 钳到 0
	//   MaxHealth 上限主要防 SetHealth 之类的情况
	const float HealthAfter = FMath::Clamp(CurrentHealth - DamageAmount, 0.f, MaxHealth);

	// 真实伤害 = 变化量（不是传入值）
	const float ActualDamage = HealthBefore - HealthAfter;

	// 理论上只可能在已满血又被"治疗"时出现，但仍然防御一下。
	if (ActualDamage <= 0.f)
	{
		return 0.f;
	}

	// 应用新血量
	CurrentHealth = HealthAfter;

	// 重置自动回复的冷却计时：刚挨打，回血要重新等待 RegenDelay。
	TimeSinceLastDamage = 0.f;

	// 广播血量变化事件。Delta 传负数表示掉血，
	// UI 可以据此触发受击闪红等表现。
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth, -ActualDamage);

	// 血量归零 -> 进入死亡流程
	if (CurrentHealth <= 0.f)
	{
		HandleDeath(DamageCauser);
	}

	// 返回真实伤害值，供调用方使用
	return ActualDamage;
}


/**
 * 治疗回血
 *
 * 与 ApplyDamage 对称：返回"实际回复量"。
 * 满血时返回 0（不会广播多余事件），已死亡时也不能被"奶回来"（需走 ResetHealth）。
 */
float UCS_HealthComponent::Heal(float HealAmount)
{
	// 已死亡不接受治疗；非正数视为无效
	if (bIsDead || HealAmount <= 0.f)
	{
		return 0.f;
	}

	const float HealthBefore = CurrentHealth;

	// 加上治疗量后钳制上限，不会超过 MaxHealth
	const float HealthAfter = FMath::Clamp(CurrentHealth + HealAmount, 0.f, MaxHealth);

	// 真实回复量
	const float ActualHeal = HealthAfter - HealthBefore;

	// 已经满血 -> 没有变化，直接返回，避免广播无意义事件
	if (ActualHeal <= 0.f)
	{
		return 0.f;
	}

	CurrentHealth = HealthAfter;

	// 广播：Delta 为正表示回血
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth, ActualHeal);

	return ActualHeal;
}


/**
 * 直接设置当前血量
 *
 * 与 Heal/ApplyDamage 的区别：这个函数无视方向，可以直接设成任意值，
 * 常用于读档、调试、关卡脚本。
 * 注意它同样会触发死亡判定（设置到 0 会死）。
 */
void UCS_HealthComponent::SetHealth(float NewHealth)
{
	// 已死亡就不再接受修改，避免出现"死了还能改血量"的诡异状态。
	// 想让死者复活请用 ResetHealth。
	if (bIsDead)
	{
		return;
	}

	// 交给统一入口处理钳制、广播和死亡判定
	ChangeHealthInternal(NewHealth, nullptr);
}


/**
 * 满血重置 / 复活
 *
 * 与 SetHealth(MaxHealth) 的区别：
 *   SetHealth 在死亡状态下无效，而 ResetHealth 会主动清除死亡标记。
 * 所以复活流程必须用这个。
 */
void UCS_HealthComponent::ResetHealth()
{
	// 清除死亡状态，让组件重新可用
	bIsDead = false;

	// 重置受击冷却（否则刚复活就立刻开始回血，虽然也无害但语义更清晰）
	TimeSinceLastDamage = 0.f;

	// 计算变化量后设为满血
	const float Delta = MaxHealth - CurrentHealth;
	CurrentHealth = MaxHealth;

	// 广播（即使本来就是满血也会广播一次，让 UI 状态同步）
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth, Delta);
}


/**
 * 血量百分比
 *
 * 做除零保护：MaxHealth 理论上有 ClampMin 保护不会为 0，
 * 但万一被代码改成 0（比如直接改内存或蓝图设置异常）也不能崩溃。
 */
float UCS_HealthComponent::GetHealthPercent() const
{
	return MaxHealth > 0.f ? FMath::Clamp(CurrentHealth / MaxHealth, 0.f, 1.f) : 0.f;
}


/**
 * 统一的血量修改入口（内部函数）
 *
 * 为什么需要它？
 *   ApplyDamage、Heal、SetHealth、自动回复四条路径最终都汇聚到这里，
 *   保证"钳制 -> 比较 -> 赋值 -> 广播 -> 死亡判定"这一套流程完全一致，
 *   不会出现某条路径漏掉广播或漏掉死亡判定的情况。
 */
void UCS_HealthComponent::ChangeHealthInternal(float NewHealth, AActor* DamageCauser)
{
	// 先钳制到合法范围
	const float Clamped = FMath::Clamp(NewHealth, 0.f, MaxHealth);

	// 计算净变化量
	const float Delta = Clamped - CurrentHealth;

	// 没变化就不做任何事（避免刷屏式广播，比如满血时自动回复每帧都广播）
	if (FMath::IsNearlyZero(Delta))
	{
		return;
	}

	// 应用
	CurrentHealth = Clamped;

	// 广播变化
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth, Delta);

	// 归零则死亡。注意这里额外判断 !bIsDead，
	// 防止在已死亡状态下重复进入 HandleDeath 流程。
	if (CurrentHealth <= 0.f && !bIsDead)
	{
		HandleDeath(DamageCauser);
	}
}


/**
 * 死亡处理
 *
 * 顺序：
 *   1. 置死亡标记（同时作为重入守卫）
 *   2. 关闭 Tick（死者不需要回血）
 *   3. 广播 OnDeath，把处理权交给外部（蓝图/C++ 监听者）
 *
 * 注意：本组件不负责销毁 Actor。
 *       需要销毁时，请监听 OnDeath 事件自行处理（可以自行决定延迟多久、
 *       播完死亡动画再销毁等），这样组件保持通用、不侵入具体的死亡表现。
 */
void UCS_HealthComponent::HandleDeath(AActor* DamageCauser)
{
	// 重入守卫：多条路径（ApplyDamage / ChangeHealthInternal）都可能调到这，
	// 靠 bIsDead 保证 OnDeath 只广播一次。
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;

	// 死亡后不再需要每帧回血计算
	SetComponentTickEnabled(false);

	// 广播死亡事件，交由外部处理后续（死亡动画、销毁、掉落等）
	OnDeath.Broadcast(DamageCauser);
}


/**
 * Owner 受到 UE 标准伤害时的回调（BeginPlay 中绑定到 Owner->OnTakeAnyDamage）
 *
 * 有了这个桥接，以下两种写法效果完全一致：
 *   1) HealthComp->ApplyDamage(25.f, Instigator)              // 直接调组件
 *   2) UGameplayStatics::ApplyDamage(Actor, 25.f, ...);       // 引擎标准 API
 *
 * 意义：任何第三方系统（爆炸、陷阱、火焰伤害区域、其他角色的攻击）
 * 都可以用引擎标准方式造成伤害，而不需要知道本项目用的是自定义血量组件。
 *
 * 参数说明（由引擎的 FTakeAnyDamageSignature 决定，顺序和类型不能改）：
 *   DamagedActor  - 受伤的 Actor（就是本组件的 Owner）
 *   Damage        - 伤害数值
 *   DamageType    - 伤害类型类（物理/火焰/可自定义，本组件暂不区分）
 *   InstigatedBy  - 发起伤害的控制器（用于判定击杀归属）
 *   DamageCauser  - 实际造成伤害的 Actor（传给 OnDeath 事件）
 */
void UCS_HealthComponent::HandleOwnerTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
	AController* InstigatedBy, AActor* DamageCauser)
{
	// 直接转交给统一的伤害入口。
	// 这里忽略 DamageType 和 InstigatedBy，因为当前需求不区分伤害类型；
	// 将来若要做抗性/减伤，就在这里根据 DamageType 做修正后再调用 ApplyDamage。
	ApplyDamage(Damage, DamageCauser);
}
