#ifndef __CW2015_H__
#define __CW2015_H__
#include "rtthread.h"
#define CW2015_ADDR             0x62

/*CW2015 寄存器定义*/
#define REG_VERSION             0x0
#define REG_VCELL               0x2
#define REG_SOC                 0x4
#define REG_RRT_ALERT           0x6
#define REG_CONFIG              0x8
#define REG_MODE                0xA
#define REG_BATINFO             0x10
#define MODE_SLEEP_MASK         (0x3<<6)
#define MODE_SLEEP              (0x3<<6)
#define MODE_NORMAL             (0x0<<6)
#define MODE_QUICK_START        (0x3<<4)
#define MODE_RESTART            (0xf<<0)
#define CONFIG_UPDATE_FLG       (0x1<<1)
#define ATHD                    (0x0<<3)        //ATHD = 0%

/*CW2015 参数定义*/
#define BATTERY_UP_MAX_CHANGE         720
#define BATTERY_DOWN_MIN_CHANGE       60
#define BATTERY_DOWN_MIN_CHANGE_SLEEP 1800
#define SIZE_BATINFO                  64

/*电池建模信息，客户拿到自己电池匹配的建模信息后请替换*/
static unsigned char cw_bat_config_info[SIZE_BATINFO] = {
0X15,0X7E,0X7C,0X5C,0X64,0X6A,0X65,0X5C,0X55,0X53,0X56,0X61,0X6F,0X66,0X50,0X48,
0X43,0X42,0X40,0X43,0X4B,0X5F,0X75,0X7D,0X52,0X44,0X07,0XAE,0X11,0X22,0X40,0X56,
0X6C,0X7C,0X85,0X86,0X3D,0X19,0X8D,0X1B,0X06,0X34,0X46,0X79,0X8D,0X90,0X90,0X46,
0X67,0X80,0X97,0XAF,0X80,0X9F,0XAE,0XCB,0X2F,0X00,0X64,0XA5,0XB5,0X11,0XD0,0X11
};

/*设备结构体定义*/
struct cw_device
{
    struct rt_i2c_bus_device *i2c;
    rt_thread_t thread;
    rt_uint32_t period;
    rt_mutex_t lock;

};
typedef struct cw_device *cw_device_t;
/*电池信息结构体*/
struct cw_battery {
    uint8_t usb_online;
    uint32_t capacity;
    uint32_t voltage;
    uint8_t alt;
};
typedef struct CW_BATTERY *cw_battery_t;

/*外部使用变量定义*/
extern struct cw_battery  cw_bat;

/*外部使用函数声明*/
int cw_init(void);                    //初始化函数
int8_t cw_update_vol(void);           //更新电池电压
int8_t cw_update_capacity(void);      //更新电池容量
uint8_t cw_release_alrt_pin(void);    //释放alrt pin,触发低电量中断后必须调用
int8_t cw_set_athd(uint8_t new_athd); //设置ATHD

#endif
