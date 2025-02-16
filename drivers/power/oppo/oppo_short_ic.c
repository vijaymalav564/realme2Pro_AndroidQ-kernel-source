/************************************************************************************
** 
** CONFIG_PRODUCT_REALME_RMX1801
** Copyright (C), 2008-2012, OPPO Mobile Comm Corp., Ltd
**
** Description:
**          for oppo short IC solution
**
** Version: 1.0
** Date created: 21:03:46, 05/24/2018
** Author: Tongfeng.Huang@ProDrv.CHG
**
** --------------------------- Revision History: ------------------------------------------------------------
* <version>           <date>                <author>                                <desc>
* Revision 1.0        2018-05-24        Tongfeng.Huang@ProDrv.CHG            Created for new architecture
************************************************************************************************************/

#include <asm/uaccess.h>
#include <linux/proc_fs.h>
#ifdef CONFIG_OPPO_CHARGER_MTK
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <asm/atomic.h>

#include <linux/xlog.h>
#if defined(CONFIG_OPPO_CHARGER_MTK6763) || defined(CONFIG_OPPO_CHARGER_MTK6771)
#include <upmu_common.h>
#include <mt-plat/mtk_gpio.h>
#include <linux/dma-mapping.h>

#include <mt-plat/battery_meter.h>
#include <linux/module.h>
#include <soc/oppo/device_info.h>
#else
#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>
#include <linux/dma-mapping.h>

#include <mach/battery_meter.h>
#include <soc/oppo/device_info.h>
#endif

#else
#include <linux/i2c.h>
#include <linux/debugfs.h>
#include <linux/gpio.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/bitops.h>
#include <linux/mutex.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <soc/oppo/device_info.h>
#endif
#include "oppo_charger.h"
#include "oppo_short_ic.h"
#include "oppo_vooc.h"
#ifdef CONFIG_OPPO_SHORT_IC_CHECK

static struct oppo_short_ic *short_ic_chip = NULL;

#define I2C_ERROR_RETRY_CNT			5

static void oppo_short_ic_init_work_func(struct work_struct *work)
{
        struct oppo_short_ic *chip = short_ic_chip;
        int rc = 0;
        int retry_cnt = 0;
        int chip_id = 0;
        int volt_threshold = 0;

        chg_err("oppo_short_ic init work start\n");

        if (!chip){
                chg_err("ERROR: oppo_short_ic is NULL, return\n");
                return;
        }

        rc = i2c_smbus_read_byte_data(chip->client, OPPO_SHORT_IC_CHIP_ID_REG);
        while(rc < 0 && retry_cnt < 5){
                usleep_range(5000, 5000 + 100);
                rc = i2c_smbus_read_byte_data(chip->client, OPPO_SHORT_IC_CHIP_ID_REG);
                retry_cnt++;
        }
        if(rc < 0){
                chip->b_oppo_short_ic_exist = false;
                chg_err("ERROR: oppo_short_ic is not exist rc[%d], retry_cnt[%d], return\n", rc, retry_cnt);
                return;
        }		
        chip->b_oppo_short_ic_exist = true;
        chg_err("oppo_short_ic,0x00_reg, 0xD*, 0xE*, 0xF*, ID [0x%02X]\n", rc);
        chip_id = rc & 0xF0;

        if(chip_id != 0xD0 && chip_id != 0xE0 && chip_id != 0xF0){
                chip->b_factory_id_get = false;
                chg_err("ERROR: oppo_short_ic factory ERROR, chip_id[0x%02X]\n", chip_id);
                return;
        }
        chip->b_factory_id_get = true;

        rc = i2c_smbus_read_byte_data(chip->client, OPPO_SHORT_IC_TEMP_VOLT_DROP_THRESH_REG);
        if(rc < 0){
                chip->b_volt_drop_set = false;
                chg_err("ERROR: oppo_short_ic OPPO_SHORT_IC_TEMP_VOLT_DROP_THRESD read err, return\n");
                return;
        }
        volt_threshold = rc;
        chg_err("oppo_short_ic, 0x02_reg, volt_threshold [0x%02X]\n", volt_threshold);
        if(volt_threshold != OPPO_SHORT_IC_TEMP_VOLT_DROP_THRESH_VAL){
                rc = i2c_smbus_write_byte_data(chip->client, OPPO_SHORT_IC_TEMP_VOLT_DROP_THRESH_REG, OPPO_SHORT_IC_TEMP_VOLT_DROP_THRESH_VAL);
                if(rc < 0){
                        chip->b_volt_drop_set = false;
                        chg_err("ERROR: oppo_short_ic OPPO_SHORT_IC_TEMP_VOLT_DROP_THRESD write err, return\n");
                        return;
                }
                retry_cnt = 0;
                while(rc != OPPO_SHORT_IC_TEMP_VOLT_DROP_THRESH_VAL && retry_cnt < 3){
                        usleep_range(5000, 5000 + 100);
                        rc = i2c_smbus_read_byte_data(chip->client, OPPO_SHORT_IC_TEMP_VOLT_DROP_THRESH_REG);
                        retry_cnt++;
                }
                if(rc != OPPO_SHORT_IC_TEMP_VOLT_DROP_THRESH_VAL){
                        chip->b_volt_drop_set = false;
                        chg_err("ERROR: oppo_short_ic can not set threshold to 0x43, return\n");
                        return;
                }
        }
        chip->b_volt_drop_set = true;

        rc = i2c_smbus_read_byte_data(chip->client, OPPO_SHORT_IC_WORK_MODE_REG);
        if(rc < 0){
                chip->b_work_mode_set = false;
                chg_err("ERROR: oppo_short_ic OPPO_SHORT_IC_WORK_MODE_REG read err, return\n");
                return;
        }
        chg_err("oppo_short_ic, 0x03_reg, work mode [0x%02X]\n", rc);
        retry_cnt = 0;
        while((rc & 0x80) != 0x80 && retry_cnt < 5){
                i2c_smbus_write_byte_data(chip->client, 0x05, 0x00);
                i2c_smbus_write_byte_data(chip->client, 0x06, 0xA5);
                i2c_smbus_write_byte_data(chip->client, 0x05, 0x80);

                usleep_range(5000, 5000 + 100);
                rc = i2c_smbus_read_byte_data(chip->client, OPPO_SHORT_IC_WORK_MODE_REG);
                retry_cnt++;
        }
        if((rc & 0x80) != 0x80){
                chip->b_work_mode_set = false;
                chg_err("ERROR: oppo_short_ic can not set normal work mode to 0x80, return\n");
                return;
        }		
        chip->b_work_mode_set = true;

        rc = i2c_smbus_read_byte_data(chip->client, OPPO_SHORT_IC_OTP_REG);
        if(rc < 0){
                chg_err("ERROR: oppo_short_ic can not get OTP state, return false\n");
                return ;
        }
        chg_err("oppo_short_ic end OTP state rc[0x%02X]\n", rc);

}

int oppo_short_ic_set_volt_threshold(struct oppo_chg_chip *chip)
{
        struct oppo_short_ic *oppo_short_chip = NULL;
        int rc = 0;
        u8 new_threshold = 0;

        oppo_short_chip = short_ic_chip;
        if(oppo_short_chip == NULL || chip == NULL){
                chg_err("ERROR: oppo_short_ic is not ready, return\n");
                return 0;
        }

        if(oppo_short_chip->b_oppo_short_ic_exist == false){
                chg_err("ERROR: oppo_short_ic is not exist, return 0\n");
                return 0;
        }

        if (oppo_vooc_get_allow_reading() == false) {
                return 0;
        }

        new_threshold = chip->short_c_batt.ic_volt_threshold;
        rc = i2c_smbus_write_byte_data(oppo_short_chip->client, OPPO_SHORT_IC_TEMP_VOLT_DROP_THRESH_REG, new_threshold);
        if(rc < 0){
                chg_err("ERROR: oppo_short_ic OPPO_SHORT_IC_TEMP_VOLT_DROP_THRESD write err, return\n");
        }
        oppo_short_chip->volt_drop_threshold = new_threshold;
        chg_err("oppo_short_ic new_threshold[0x%02X]\n", new_threshold);

        rc = i2c_smbus_read_byte_data(oppo_short_chip->client, OPPO_SHORT_IC_TEMP_VOLT_DROP_THRESH_REG);
        chg_err("oppo_short_ic,0x02_reg, new_threshold value [0x%02X]\n", rc);

        return rc;
}

bool oppo_short_ic_is_exist(struct oppo_chg_chip *chip)
{
        struct oppo_short_ic *oppo_short_chip = NULL;

        oppo_short_chip = short_ic_chip;
        if(oppo_short_chip == NULL){
                chg_err("ERROR: oppo_short_ic is not ready, return\n");
                return true;
        }
        return oppo_short_chip->b_oppo_short_ic_exist;
}

int oppo_short_ic_get_otp_error_value(struct oppo_chg_chip *chip)
{
        int rc = 0;
        struct oppo_short_ic *oppo_short_chip = NULL;

        oppo_short_chip = short_ic_chip;
        if(oppo_short_chip == NULL){
                chg_err("ERROR: oppo_short_ic is not ready, return\n");
                return 0;
        }

        if(oppo_short_chip->b_oppo_short_ic_exist == false){
                ///chg_err("ERROR: oppo_short_ic is not exist, return 0\n");
                return 0;
        }

        if (oppo_vooc_get_allow_reading() == false) {
                return 0;
        }

        rc = i2c_smbus_read_byte_data(oppo_short_chip->client, OPPO_SHORT_IC_OTP_REG);
        if(rc < 0){
                chg_err("ERROR: oppo_short_ic can not get OTP state, return 0\n");
                return 0;
        }
        oppo_short_chip->otp_error_value= rc;
        ///chg_err("oppo_short_ic OTP state rc[0x%02X]\n", rc);

        return oppo_short_chip->otp_error_value;
}

bool oppo_short_ic_otp_check(void)
{
        struct oppo_short_ic *chip = short_ic_chip;
        int rc = 0;
        int otp_st = 0;
        static bool pre_otp_st = true;

        if(chip == NULL){
                return true;
        }

        if(chip->b_oppo_short_ic_exist == false){
                ///chg_err("ERROR: oppo_short_ic is not exist, return true\n");
                return true;
        }
        if (atomic_read(&short_ic_chip->suspended) == 1) {
                chg_err("ERROR: oppo_short_ic is suspended, return true\n");
                return pre_otp_st;
        }

        if(chip->b_factory_id_get == false
            || chip->b_volt_drop_set == false
            || chip->b_work_mode_set == false){
                chg_err("ERROR: oppo_short_ic some information is ERROR, return false\n");
                pre_otp_st = false;
                return false;
        }

        if (oppo_vooc_get_allow_reading() == false) {
                return pre_otp_st;
        }

        rc = i2c_smbus_read_byte_data(chip->client, OPPO_SHORT_IC_OTP_REG);
        if(rc < 0){
                chip->otp_error_cnt ++;
                if (chip->otp_error_cnt >= 3) {
                        chg_err("ERROR: oppo_short_ic can not get OTP state, return false\n");
                        pre_otp_st = false;
                        return false;
                }
        } else {
                chg_err("oppo_short_ic OTP state rc[0x%02X]\n", rc);
                otp_st = rc & 0xF0;
                if(otp_st != 0x00){
                        if(chip->otp_error_cnt < 3) {
                                chip->otp_error_cnt++;
                        } else{
                                chip->otp_error_value = rc;
                                pre_otp_st = false;
                                return false;
                        }
                } else {
                        chip->otp_error_cnt = 0;
                }
        }

        pre_otp_st = true;
        return true;
}

static void oppo_short_ic_shutdown(struct i2c_client *client)
{
        //msleep(80);
        return;
}

static int oppo_short_ic_driver_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
        struct oppo_short_ic *chip;

        chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
        if (!chip) {
                dev_err(&client->dev, "Couldn't allocate memory\n");
                return -ENOMEM;
        }

        chip->client = client;
        chip->dev = &client->dev;
        i2c_set_clientdata(client, chip);

        chip->b_oppo_short_ic_exist = false;
        chip->b_factory_id_get = true;
        chip->b_volt_drop_set = true;
        chip->b_work_mode_set = true;
        chip->otp_error_cnt = 0;
        chip->otp_error_value = 0;
        chip->volt_drop_threshold = OPPO_SHORT_IC_TEMP_VOLT_DROP_THRESH_VAL;

        short_ic_chip = chip;
        INIT_DELAYED_WORK(&chip->oppo_short_ic_init_work, oppo_short_ic_init_work_func);
        msleep(80);
        schedule_delayed_work(&chip->oppo_short_ic_init_work, 200);

        chg_debug("oppo_short_ic success\n");
        return 0;
}


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
static int short_ic_pm_resume(struct device *dev)
{
        if (!short_ic_chip) {
                return 0;
        }
        atomic_set(&short_ic_chip->suspended, 0);
        return 0;
}

static int short_ic_pm_suspend(struct device *dev)
{
        if (!short_ic_chip) {
                return 0;
        }
        atomic_set(&short_ic_chip->suspended, 1);
        return 0;
}

static const struct dev_pm_ops short_ic_pm_ops = {
        .resume                = short_ic_pm_resume,
        .suspend                = short_ic_pm_suspend,
};
#else /*(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))*/
static int short_ic_resume(struct i2c_client *client)
{
        if (!short_ic_chip) {
            return 0;
        }
        atomic_set(&short_ic_chip->suspended, 0);
        return 0;
}

static int short_ic_suspend(struct i2c_client *client, pm_message_t mesg)
{
        if (!short_ic_chip) {
            return 0;
        }
        atomic_set(&short_ic_chip->suspended, 1);
        return 0;
}
#endif /*(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))*/


/**********************************************************
  *
  *   [platform_driver API]
  *
  *********************************************************/
static const struct of_device_id oppo_short_ic_match[] = {
        { .compatible = "oppo,oppo_short-ic"},
        { },
};

static const struct i2c_device_id oppo_short_ic_id[] = {
        { "oppo_short-ic", 0},
        {},
};
MODULE_DEVICE_TABLE(i2c, oppo_short_ic_id);

struct i2c_driver oppo_short_ic_i2c_driver = {
        .driver        = {
                .name = "oppo_short-ic",
                .owner        = THIS_MODULE,
                .of_match_table = oppo_short_ic_match,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
                .pm                = &short_ic_pm_ops,
#endif

        },
        .probe = oppo_short_ic_driver_probe,
        .shutdown        = oppo_short_ic_shutdown,
        .id_table = oppo_short_ic_id,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0))
        .resume         = short_ic_resume,
        .suspend        = short_ic_suspend,
#endif

};

static int __init oppo_short_ic_subsys_init(void)
{
        int ret = 0;
        chg_debug(" init start\n");

        if (i2c_add_driver(&oppo_short_ic_i2c_driver) != 0) {
                chg_err(" failed to register oppo_short_ic i2c driver.\n");
        } else {
                chg_debug(" Success to register oppo_short_ic i2c driver.\n");
        }
        return ret;
}


subsys_initcall(oppo_short_ic_subsys_init);
MODULE_DESCRIPTION("Driver for oppo short ic");
MODULE_LICENSE("GPL v2");

#else
int oppo_short_ic_set_volt_threshold(struct oppo_chg_chip *chip)
{
        return 0;
}
bool oppo_short_ic_is_exist(struct oppo_chg_chip *chip)
{
        return false;
}

bool oppo_short_ic_otp_check(void)
{
        return true;
}
int oppo_short_ic_get_otp_error_value(struct oppo_chg_chip *chip)
{
        return 0;
}
#endif

