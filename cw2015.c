/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * CW2015 IC driver function
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-12-28     QG           first version
 */
#include <rtthread.h>
#include <rtdevice.h>
#include "cw2015.h"
#define DBG_ENABLE
#define DBG_SECTION_NAME   "cw2015"
#define DBG_LEVEL                   DBG_LOG
#define DBG_COLOR
#include <rtdbg.h>

struct cw_battery  cw_bat;

/*内部使用变量定义*/
static cw_device_t dev;
static uint8_t no_charger_full_jump =0;
static uint32_t allow_no_charger_full =0;
static uint32_t allow_charger_always_zero =0;
static uint8_t if_quickstart =0;
static uint8_t reset_loop =0;

/*内部使用函数定义*/
static int cw_write(rt_uint8_t reg, rt_uint8_t data);//写
static int cw_read(rt_uint8_t reg, rt_uint8_t *buf); //读
static int8_t cw_por(void);                          //芯片重启
static int cw_config(void);                          //芯片参数配置
static int cw_update_config_info(void);              //芯片参数更新
static int cw_get_capacity(void);                    //获取电池容量
static int cw_get_vcell(void);                       //获取电池电压
#ifdef CW2015_USE_AUTO_UPDATE
static void cw_work(void *parameter);                //电池更新线程
#endif

static int cw_write(rt_uint8_t reg, rt_uint8_t data)
{
    rt_uint8_t buf[2];

    buf[0] = reg;
    buf[1] = data;

    return (rt_i2c_master_send(dev->i2c, CW2015_ADDR, 0, buf, 2) == 2) ? 0 : 1;
}
static int cw_read(rt_uint8_t reg, rt_uint8_t *buf)
{
    struct rt_i2c_msg msgs[2];

    msgs[0].addr = CW2015_ADDR;
    msgs[0].flags = RT_I2C_WR;
    msgs[0].buf = &reg;
    msgs[0].len = 1;

    msgs[1].addr = CW2015_ADDR;
    msgs[1].flags = RT_I2C_RD;
    msgs[1].buf = buf;
    msgs[1].len = 1;
    return (rt_i2c_transfer(dev->i2c, msgs, 2) == 2) ? 0:1;
}
static int8_t cw_por(void)
{
    int8_t ret = 0;
    uint8_t reset_val = 0;
    reset_val = MODE_SLEEP;
    ret = cw_write(REG_MODE, reset_val);
    if (ret)
        return -1;
    rt_thread_mdelay(1);

    reset_val = MODE_NORMAL;
    ret = cw_write(REG_MODE, reset_val);
    if (ret)
        return -1;
    rt_thread_mdelay(1);

    ret = cw_config();
    if (ret)
        return ret;
    return 0;
}

static int cw_config(void)
{
    unsigned ret;
    uint8_t i;
    uint8_t reg_val = MODE_NORMAL;
    /* wake up cw2015/13 from sleep mode */
    ret = cw_write(REG_MODE, reg_val);
    if(ret)
    {
        LOG_E("wakeup fail");
        return 1;
    }
    /* check ATHD if not right */
    ret = cw_read(REG_CONFIG, &reg_val);
    if(ret)
    {
        return 1;
    }
    if((reg_val & 0xf8) != ATHD)
    {
        reg_val &= 0x07;    /* clear ATHD */
        reg_val |= ATHD;    /* set ATHD */
        ret = cw_write(REG_CONFIG, reg_val);
        if(ret)
        {
            return 1;
        }
    }
    /* check config_update_flag if not right */
    ret = cw_read(REG_CONFIG, &reg_val);
    if(ret)
    {
        return 1;
    }
    if(!(reg_val & CONFIG_UPDATE_FLG))
    {
        LOG_I("update flag for new battery info need set_1");
        ret = cw_update_config_info();
        if(ret)
        {
            return ret;
        }
    }
    else
    {
        for(i = 0; i < SIZE_BATINFO; i++)
        {
            ret = cw_read(REG_BATINFO +i, &reg_val);
            if(ret)
            {
                return 1;
            }
            if(cw_bat_config_info[i] != reg_val)
            {
                break;
            }
        }
        if(i != SIZE_BATINFO)
        {
            LOG_I("update flag for new battery info need set_2");
            ret = cw_update_config_info();
            if(ret)
            {
                return ret;
            }
        }
    }
    /* check SOC if not eqaul 255 */
    for (i = 0; i < 30; i++) {
        ret = cw_read(REG_SOC, &reg_val);
        if (ret)
            return 1;
        else if (reg_val <= 100)
            break;
        rt_thread_mdelay(100);
    }
    if (i >=30) {
        reg_val = MODE_SLEEP;
        ret = cw_write(REG_MODE, reg_val);
        return 4;
    }
    return 0;
}
/**
 * 更新IC内的电池profile信息，当IC VDD掉电后再上电会执行
 *
 * @param none
 * @return 1：i2c读写错 2：芯片处于sleep模式 3：写入的profile与读出的不一致
 */
static int cw_update_config_info(void)
{
    uint8_t ret = 0;
    uint8_t i;
    uint8_t reset_val;
    uint8_t reg_val;
    /* make sure no in sleep mode */
    ret = cw_read(REG_MODE, &reg_val);
    if(ret)
    {
        return 1;
    }
    if((reg_val & MODE_SLEEP_MASK) == MODE_SLEEP)
    {
        return 2;
    }
    /* update new battery info */
    for(i = 0; i < SIZE_BATINFO; i++)
    {
        reg_val = cw_bat_config_info[i];
        ret = cw_write(REG_BATINFO+i, reg_val);
        if(ret)
        {
            return 1;
        }
    }
    /* readback & check */
    for(i = 0; i < SIZE_BATINFO; i++)
    {
        ret = cw_read(REG_BATINFO+i, &reg_val);
        if(ret)
        {
            return 1;
        }
        if(reg_val != cw_bat_config_info[i])
        {
            return 3;
        }
    }
    /* set cw2015/cw2013 to use new battery info */
    ret = cw_read(REG_CONFIG, &reg_val);
    if(ret)
    {
        return 1;
    }
    reg_val |= CONFIG_UPDATE_FLG;   /* set UPDATE_FLAG */
    reg_val &= 0x07;                /* clear ATHD */
    reg_val |= ATHD;                /* set ATHD */
    ret = cw_write(REG_CONFIG, reg_val);
    if(ret)
    {
        return 1;
    }
    /* reset */
    reset_val = MODE_NORMAL;
    reg_val = MODE_RESTART;
    ret = cw_write(REG_MODE, reg_val);
    if(ret)
    {
        return 1;
    }
    rt_thread_mdelay(1);
    ret = cw_write(REG_MODE, reset_val);
    if(ret)
    {
        return 1;
    }
    return 0;
}
int8_t cw_set_athd(uint8_t new_athd)
{
    int8_t ret = 0;
    uint8_t reg_val;
    new_athd = new_athd << 3;
    reg_val &= 0x07;    /* clear ATHD */
    reg_val |= new_athd;    /* set new ATHD */
    ret = cw_write(REG_CONFIG, reg_val);
    if(ret)
    {
        return -1;
    }
    return 0;
}
static int cw_get_capacity(void)
{
    int8_t ret = 0;
    uint8_t allow_capacity;
    uint8_t reg_val;
    uint8_t cw_capacity;

    if(ret!=RT_EOK)
    {
        return -1;
    }
    ret = cw_read(REG_SOC, &reg_val);
    if(ret)
    {
        return -1;
    }
    cw_capacity = reg_val;
    if ((cw_capacity < 0) || (cw_capacity > 100)) {
        reset_loop++;
        if (reset_loop >5) {
            ret = cw_por(); //por ic
            if(ret)
                return -1;
            reset_loop =0;
        }
        return cw_bat.capacity;
    } else {
        reset_loop =0;
    }
    if(((cw_bat.usb_online == 1) && (cw_capacity == (cw_bat.capacity - 1)))
            || ((cw_bat.usb_online == 0) && (cw_capacity == (cw_bat.capacity + 1))))
    {
        if(!((cw_capacity == 0 && cw_bat.capacity <= 2)||(cw_capacity == 100 && cw_bat.capacity == 99)))
        {
            cw_capacity = cw_bat.capacity;
        }
    }
    if((cw_bat.usb_online == 1) && (cw_capacity >= 95) && (cw_capacity <= cw_bat.capacity) )
    {
        // avoid not charge full
        allow_no_charger_full++;
        if(allow_no_charger_full >= BATTERY_UP_MAX_CHANGE)
        {
            allow_capacity = cw_bat.capacity + 1;
            cw_capacity = (allow_capacity <= 100) ? allow_capacity : 100;
            no_charger_full_jump =1;
            allow_no_charger_full =0;
        }
        else if(cw_capacity <= cw_bat.capacity)
        {
            cw_capacity = cw_bat.capacity;
        }
    }
    else if((cw_bat.usb_online == 0) && (cw_capacity <= cw_bat.capacity ) && (cw_capacity >= 90) && (no_charger_full_jump == 1))
    {
        // avoid battery level jump to CW_BAT
        if(cw_bat.usb_online == 0)
            allow_no_charger_full++;
        if(allow_no_charger_full >= BATTERY_DOWN_MIN_CHANGE)
        {
            allow_capacity = cw_bat.capacity - 1;
            allow_no_charger_full =0;
            if (cw_capacity >= allow_capacity)
            {
                no_charger_full_jump =0;
            }
            else
            {
                cw_capacity = (allow_capacity > 0) ? allow_capacity : 0;
            }
        }
        else if(cw_capacity <= cw_bat.capacity)
        {
            cw_capacity = cw_bat.capacity;
        }
    }
    else
    {
        allow_no_charger_full =0;
    }
    if((cw_bat.usb_online > 0) && (cw_capacity == 0))
    {
        allow_charger_always_zero++;
        if((allow_charger_always_zero >= BATTERY_DOWN_MIN_CHANGE_SLEEP) && (if_quickstart == 0))
        {
            ret = cw_por(); //por ic
            if(ret) {
                return -1;
            }
            if_quickstart = 1;
            allow_charger_always_zero =0;
        }
    }
    else if((if_quickstart == 1)&&(cw_bat.usb_online == 0))
    {
        if_quickstart = 0;
    }

    return(cw_capacity);
}
static int cw_get_vcell()
{
    uint8_t ret = 0;
    uint8_t get_ad_times = 0;
    uint8_t reg_val[2] = {0, 0};
    unsigned long ad_value = 0;
    uint32_t ad_buff = 0;
    uint32_t ad_value_min = 0;
    uint32_t ad_value_max = 0;

    for(get_ad_times = 0; get_ad_times < 3; get_ad_times++)
    {
        ret = cw_read(REG_VCELL, &reg_val[0]);
        if(ret)
        {
            return 1;
        }
        ret = cw_read(REG_VCELL + 1, &reg_val[1]);
        if(ret)
        {
            return 1;
        }
        ad_buff = (reg_val[0] << 8) + reg_val[1];

        if(get_ad_times == 0)
        {
            ad_value_min = ad_buff;
            ad_value_max = ad_buff;
        }
        if(ad_buff < ad_value_min)
        {
            ad_value_min = ad_buff;
        }
        if(ad_buff > ad_value_max)
        {
            ad_value_max = ad_buff;
        }
        ad_value += ad_buff;
    }
    ad_value -= ad_value_min;
    ad_value -= ad_value_max;
    ad_value = ad_value  * 305 / 1000;
    return(ad_value);
}
/**
 * 释放alrt_pin，该函数在触发低电量中断后必须调用
 *
 * @param none
 * @return -1：i2c读写出错
 */
uint8_t cw_release_alrt_pin(void)
{
    int8_t ret = 0;
    uint8_t reg_val;
    uint8_t alrt;
    ret = cw_read(REG_RRT_ALERT, &reg_val);
    if (ret) {
        return -1;
    }
    alrt = reg_val & 0x80;
    reg_val = reg_val & 0x7f;
    ret = cw_write(REG_RRT_ALERT, reg_val);
    if(ret) {
        return -1;
    }
    return alrt;
}
/**
 * 更新一次电池容量
 *
 * @param none
 * @return 1：获取互斥锁失败 0：更新成功
 */
int8_t cw_update_capacity(void)
{
    int8_t ret=0;
    int cw_capacity;
    ret=rt_mutex_take(dev->lock,RT_WAITING_FOREVER);
    if(ret!=RT_EOK)
    {
        return 1;
    }
    cw_capacity = cw_get_capacity();
    rt_mutex_release(dev->lock);
    if((cw_capacity >= 0) && (cw_capacity <= 100) && (cw_bat.capacity != cw_capacity))
    {
        cw_bat.capacity = cw_capacity;
    }
    return 0;
}
/**
 * 更新一次电池电压
 *
 * @param none
 * @return 1：获取互斥锁失败 0：更新成功
 */
int8_t cw_update_vol(void)
{
    int8_t ret=0;
    uint32_t cw_voltage;
    ret=rt_mutex_take(dev->lock,RT_WAITING_FOREVER);
    if(ret!=RT_EOK)
    {
        return 1;
    }
    cw_voltage = cw_get_vcell();
    rt_mutex_release(dev->lock);
    if(cw_voltage == 1) {
        cw_bat.voltage = cw_bat.voltage;
    } else if(cw_bat.voltage != cw_voltage)
    {
        cw_bat.voltage = cw_voltage;
    }
    return 0;
}
/**
 * 初始化函数
 *
 * @param none
 * @return
 */
int cw_init()
{
    int8_t err=0;
    char *i2c_bus_name=CW2015_DEVICE_NAME;
    uint8_t error_num=0;
    dev = rt_calloc(1, sizeof(struct cw_device));
    if (dev == RT_NULL)
    {
        LOG_E("can't allocate memory for cw device on '%s' ", i2c_bus_name);
        rt_free(dev);
        return RT_NULL;
    }
    dev->i2c = rt_i2c_bus_device_find(i2c_bus_name);
    if (dev->i2c == RT_NULL)
    {
        LOG_E("can't find cw device on '%s' ", i2c_bus_name);
        rt_free(dev);
        return RT_NULL;
    }
    dev->lock = rt_mutex_create("mutex_cw", RT_IPC_FLAG_FIFO);
    if (dev->lock == RT_NULL)
    {
        LOG_E("can't create mutex for cw device on '%s' ", i2c_bus_name);
        rt_free(dev);

        return RT_NULL;
    }
    rt_mutex_take(dev->lock,RT_WAITING_FOREVER);
    while( (err=cw_config()) > 0 )
    {
        LOG_E("config fail,err code=%d",err);
        error_num++;
        if(error_num>20)
        {
            return RT_ERROR;
        }
        rt_thread_mdelay(100);
    }
    rt_mutex_release(dev->lock);
    cw_bat.usb_online = 0;
    cw_bat.capacity = 2;
    cw_bat.voltage = 0;
    cw_bat.alt = 0;
#ifdef CW2015_USE_AUTO_UPDATE
    dev->thread = rt_thread_create("cw_battery", cw_work, (void *)dev, 470, 15, 10);
    if (dev->thread != RT_NULL)
    {
        rt_thread_startup(dev->thread);
    }
#endif
    return RT_EOK;
}
INIT_APP_EXPORT(cw_init);

#ifdef CW2015_USE_AUTO_UPDATE
static void cw_work(void *parameter)
{
    while(1)
    {
        cw_update_capacity(); //更新电池容量
        cw_update_vol();      //更新电池电压
        rt_thread_mdelay(1000);
    }
}
#endif

/*MSH 命令测试函数*/
rt_err_t battery(int argc, char *argv[])
{
    if(argc>1)
    {
        if (!rt_strcmp(argv[1], "get"))
        {
            if (argc > 2)
            {
                if (!rt_strcmp(argv[2], "vol"))
                {
                    if(cw_update_vol())
                    {
                        LOG_E("cw get mutex lock fail");
                        return RT_EBUSY;
                    }
                    rt_kprintf("battery voltage:%d.%d V\n",cw_bat.voltage/1000,cw_bat.voltage%1000);
                }
                else if(!rt_strcmp(argv[2], "cap"))
                {
                    if(cw_update_capacity())
                    {
                        LOG_E("cw get mutex lock fail");
                        return RT_EBUSY;
                    }
                    rt_kprintf("battery voltage:%d%\n",cw_bat.capacity);
                }
                else
                {
                    rt_kprintf("illegal parameter,only support get vol and cap\n");
                }
            } else
            {
                rt_kprintf("battery get vol   - get battery voltage\n");
                rt_kprintf("battery get cap   - get battery capacity\n");
            }
        }
        else if(!rt_strcmp(argv[1], "set"))
        {
            if(argc>2)
            {
                if (!rt_strcmp(argv[2], "athd"))
                {

                }
                else
                {
                    rt_kprintf("illegal parameter,only support set athd\n");
                }
            }
            else
            {
                rt_kprintf("battery set athd  - set battery ATHD\n");
            }
        }
    } else
    {
        rt_kprintf("battery get vol   - get battery voltage\n");
        rt_kprintf("battery get cap   - get battery capacity\n");
        rt_kprintf("battery set athd  - set battery ATHD\n");
    }
    return RT_EOK;
}
MSH_CMD_EXPORT(battery,get battery soc);
