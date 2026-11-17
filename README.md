# Cube_Skill

内容规范：

    代码要加入到符合类型的文件夹中

    上传代码时，要写清提交的内容，更新或新增了什么

    代码的开头使用 CS 作为前缀（项目缩写）
    
    在UE编辑器中创建了内容，需要使用特定前缀，比如

    蓝图        BP_         BP_Player
    控件蓝图    WBP_        WBP_SkillBar
    蓝图接口    BPI_        BPI_Damageable
    动画蓝图    ABP_        ABP_Player
    蒙太奇      AM_         AM_Cast
    动画序列    AS_         AS_Idle
    混合空间    BS_         BS_Locomotion
    材质        M_          M_Cube
    材质实例    MI_         MI_Cube_Red
    材质函数    MF_         MF_Pulse
    贴图        T_          T_Cube_D
    静态网格    SM_         SM_Cube
    骨骼网格    SK_         SK_Player
    Niagara    NS_          NS_Hit
    数据表      DT_        DT_Skill
    枚举        E_         E_SkillState
    关卡        L_         L_Test
    输入动作    IA_         IA_Cast
    输入映射    IMC_        IMC_Default
    行为树      BT_         BT_Enemy
    黑板        BB_         BB_Enemy

项目需求：

    能力：
        玩家：        

            玩家初始拥有一个方块，贴在手上插槽
    
            左键丢出方块，方块会向前飞行，可以推出敌人（无法直接击杀）
            当距离过远，或碰到墙，会自动回到玩家手中
    
            击杀敌人可以获取敌人的能力，通过方块颜色区分
    
            当选中某个能力时，释放能力的那一面会发出粒子特效
    
            可以通过旋转方块，决定能力释放朝向（旋转简化为三个面，前后、左右、上下）

        敌人：
            可以让玩家和某个物体或敌人进行交换位置

    UI：
        右下角会显示获得的能力方块，通过颜色区分

        当玩家选中某个能力时，手中的方块也会跟着变颜色

    关卡设置：
        方块可以给门上色，上色后可以传送到该颜色的门后