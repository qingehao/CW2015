# CW2015 软件包

## 1 介绍

CW2015是一款小尺寸，无需检流电阻的锂电池电量计量芯片。芯片持续监测电池在充电/放电状态下的电压，运行专利“FastCali”电量计算法，结合电池建模信息，可准确计算电池剩余电量。CW2015适用于包括锂锰，锂钴和聚合物等多种类型的锂电池应用。

### 1.1 目录结构

| 名称 | 说明 |
| ---- | ---- |
| cw2015.h | 电量计IC使用头文件 |
| cw2015.c | 电量计IC使用源代码 |
| SConscript | RT-Thread 默认的构建脚本 |
| README.md | 软件包使用说明 |
| CW2015_datasheet.pdf | 官方数据手册 |

### 1.2 许可证

CW2015 软件包遵循  Apache-2.0 许可，详见 LICENSE 文件。

### 1.3 依赖

依赖 `RT-Thread I2C` 设备驱动框架。

## 2 获取软件包

使用 CW2015 软件包需要在 RT-Thread 的包管理器中选择它，具体路径如下：

```markdown
RT-Thread online packages
    peripheral libraries and drivers  --->
        [*] cw2015: fuel gauging system IC for Lithium-ion(Li+) Battery  --->
        [*] Enable auto update battery information                                               Version (latest)  --->
```


每个功能的配置说明如下：

- `cw2015: fuel gauging system IC for Lithium-ion(Li+) Battery`：选择使用 `cw2015` 软件包；
- `Enable auto update battery information  `：使用独立线程自动更新电池容量及电压。
- `Version`：配置软件包版本，默认最新版本。

然后让 RT-Thread 的包管理器自动更新，或者使用 `pkgs --update` 命令更新包到 BSP 中。

## 3 使用 CW2015 软件包

按照前文介绍，获取 `CW2015` 软件包后，就可以按照 下文提供的 API 使用传感器 `cw2015` 与 `Finsh/MSH` 命令进行测试，详细内容如下。

### 3.1 API

#### 3.1.1  电池信息结构体

```c
struct cw_battery {
	uint8_t usb_online;
	uint32_t capacity;
	uint32_t voltage;
	uint8_t alt;
};
```

```C
extern struct cw_battery  cw_bat;
```

该结构体在cw2015.h中使用extern声明，用户在引用头文件后便可使用该结构体。

| 参数    | 描述                      |
| :----- | :----------------------- |
| usb_online | 是否插入充电器，需由用户应用更新此值。 |
| capacity | 电池容量，范围 1% - 100%，单位 % |
| voltage | 电池电压，单位 0.001 V |
| alt | 暂未用到 |

#### 3.1.2  初始化

int cw_init(void);

初始化设备

| 返回     | 描述       |
| :------- | :--------- |
| =RT_EOK  | 初始化成功 |
| !=RT_EOK | 初始化失败 |

#### 3.1.3 更新电池电压

```c
int8_t cw_update_vol(void)
```

更新一次电池电压值，并赋值到cw_bat.voltage中

| 返回 | 描述 |
| :--- | :--- |
| 1    | 失败 |
| 0    | 成功 |

#### 3.1.4 更新电池容量

```C
cw_update_capacity(void)
```

更新一次电池电压值，并赋值到cw_bat.capacity中

| 参数 | 描述 |
| :--- | :--- |
| 1    | 失败 |
| 0    | 成功 |

#### 3.1.5 释放ALRT_PIN

```c
uint8_t cw_release_alrt_pin(void)
```

释放引脚中断，该函数在触发低电量中断后必须调用。

| 返回 | 描述 |
| :--- | :--- |
| -1   | 失败 |
| !=-1 | 成功 |

#### 3.1.6 设置ATHD值

```c
int8_t cw_set_athd(uint8_t new_athd);
```

设置新的ATHD值

| 参数     | 描述       |
| :------- | :--------- |
| new_athd | 新的athd值 |
| **返回** | **描述**   |
| 0        | 成功       |
| -1       | 失败       |

### 3.2 Finsh/MSH 测试命令

cw2015 软件包提供了丰富的测试命令，项目只要在 RT-Thread 上开启 Finsh/MSH 功能即可。在输入battery后便可得到详细信息

```c
msh >battery
battery get vol   - get battery voltage
battery get cap   - get battery capacity
battery set athd  - set battery ATHD
msh >battery get vol
battery voltage:3.825 V
msh >battery get cap
battery capacity:66%
```

## 4 注意事项

cw2015电量计使用前需拿到自己电池的建模信息，这个模型信息在cw2015.h中。

```c
/*电池建模信息，客户拿到自己电池匹配的建模信息后请替换*/
static unsigned char cw_bat_config_info[SIZE_BATINFO] = {
0X15,0X7E,0X7C,0X5C,0X64,0X6A,0X65,0X5C,0X55,0X53,0X56,0X61,0X6F,0X66,0X50,0X48,
0X43,0X42,0X40,0X43,0X4B,0X5F,0X75,0X7D,0X52,0X44,0X07,0XAE,0X11,0X22,0X40,0X56,
0X6C,0X7C,0X85,0X86,0X3D,0X19,0X8D,0X1B,0X06,0X34,0X46,0X79,0X8D,0X90,0X90,0X46,
0X67,0X80,0X97,0XAF,0X80,0X9F,0XAE,0XCB,0X2F,0X00,0X64,0XA5,0XB5,0X11,0XD0,0X11
};
```

该模型信息与电路原理图，具体PCB走线的长度，线阻都是有关系的，不可通用。

## 5 联系方式

* 维护：qinge wx:17864194640
* 主页：https://github.com/qingehao

