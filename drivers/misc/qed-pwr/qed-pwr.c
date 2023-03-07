// SPDX-License-Identifier: GPL-2.0
/*
 *  qed-prw.c - QED power managment driver.
 *
 *  Copyright (C) 2022 Lantronix Inc.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <asm/unaligned.h>
#include <linux/crc32.h>
#include <linux/kdev_t.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/delay.h>
#include <linux/gpio.h>

#include <linux/of.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/of_platform.h>
#include <linux/mod_devicetable.h>
#include <linux/gpio/consumer.h>

#define GPIO_OF "pwr"

#define VSC_RESET_OF "vsc,reset"
#define WIFI_EN_OF "qed,wifi_en"
#define BT_EN_OF "qed,bt_en"
#define VSC_RESET_OF "vsc,reset"
#define LTE_EN_OF "qed,lte_en"

#define SYSFS_QED_PWR "qed-pwr"
#define SYSFS_PWR "pwr"


#define LTE_PWR_OFF_MAX_ITER 35
#define BT_DELAY 35000 /* ms */
#define LTE_DELAY 25000 /* ms */

#define	QED_PWR_IOCTL_BASE     'Q'

#define	QED_PWR_WIFI_GETSTATUS _IOR(QED_PWR_IOCTL_BASE, 0, int)
#define	QED_PWR_BT_GETSTATUS _IOR(QED_PWR_IOCTL_BASE, 1, int)
#define	QED_PWR_LTE_GETSTATUS _IOR(QED_PWR_IOCTL_BASE, 2, int)

#define	QED_PWR_WIFI_SET _IOW(QED_PWR_IOCTL_BASE, 3, int)
#define	QED_PWR_BT_SET _IOW(QED_PWR_IOCTL_BASE, 4, int)
#define	QED_PWR_LTE_SET _IOW(QED_PWR_IOCTL_BASE, 5, int)

#if defined _LTE_DEBUG
#undef _LTE_DEBUG
#endif

enum pwr_gpios_idx {
        ETH_EN = 0,
        ETH_PCIE_EN,
        BTWIFI_VDD,
        BTWIFI_VDDIO,
        BT_EN,
        WIFI_EN,
        LTE_PWR,
        LTE_PWRMON,
        LTE_SW_RDY,
        LTE_USB_FORCE_BOOT,
        LTE_FAST_SHDN,
        LTE_SHDN,
        LTE_GPIO4,
        LTE_GPIO_SPARE,
        BT_PCM_CLK,
        BT_PCM_DIN,
        BT_PCM_SYNC,
        BT_PCM_DOUT,
        END_GPIOS,
};

static int gpio_config [END_GPIOS] = {
        GPIOD_OUT_LOW,
        GPIOD_OUT_LOW,
        GPIOD_OUT_LOW,
        GPIOD_OUT_LOW,
        GPIOD_OUT_LOW,
        GPIOD_OUT_LOW,
#if IS_ENABLED(CONFIG_QED_BTWIFI_PWR_COMPLIANCE_TEST)
        GPIOD_IN, /* lte pwr */
        GPIOD_IN, /* lte pwrmon */
        GPIOD_IN, /* lte sw ready */
        GPIOD_IN, /* lte usb force */
        GPIOD_IN, /* lte fast shdn */
        GPIOD_IN, /* lte shdn */
        GPIOD_IN, /* lte gpio4 */
        GPIOD_IN, /* lte gpio spare */
#else
        GPIOD_OUT_LOW, /* lte pwr */
        GPIOD_IN, /* lte pwrmon */
        GPIOD_IN, /* lte sw ready */
        GPIOD_OUT_LOW, /* lte usb force */
        GPIOD_OUT_LOW, /* lte fast shdn */
        GPIOD_OUT_LOW, /* lte shdn */
        GPIOD_OUT_LOW, /* lte gpio4 */
        GPIOD_OUT_LOW, /* lte gpio spare */
#endif
        GPIOD_OUT_LOW, /* BT_PCM_CLI */
        GPIOD_IN, /* bt pcm din */
        GPIOD_OUT_LOW,
        GPIOD_OUT_LOW,
};

static char __maybe_unused gpio_names [END_GPIOS][32] = {
        "ETH_EN",
        "ETH_PCIE_EN",
        "BTWIFI_VDD",
        "BTWIFI_VDDIO",
        "BT_EN",
        "WIFI_EN",
        "LTE_PWR",
        "LTE_PWRMON",
        "LTE_SW_RDY",
        "LTE_USB_FORCE_BOOT",
        "LTE_FAST_SHDN",
        "LTE_SHDN",
        "LTE_GPIO4",
        "LTE_GPIO_SPARE",
        "BT_PCM_CLK",
        "BT_PCM_DIN",
        "BT_PCM_SYNC",
        "BT_PCM_DOUT",
};


struct qed_pwr_data {
        struct platform_device *pdev;
        struct kobject *qed_kobj;
        struct miscdevice mdev;
        struct mutex lock;
        struct delayed_work bt_pwr_on;
        struct delayed_work lte_pwr_on;
        struct gpio_desc *qed_gpios[END_GPIOS];
};
#ifdef _LTE_DEBUG
static struct task_struct *monitor_thread;
#endif

static int get(struct device *, int);
static int set(struct device *, int, bool);

static int set_bt_enable(struct device *, bool);
static int get_bt_status(struct device *);

static int set_wifi_enable(struct device *, bool);
static int get_wifi_status(struct device *);

static int reset_vsc(struct device *);

static int set_lte_power(struct device *, bool);
static int get_lte_power(struct device *);

static int set_lte_fast_shdn(struct device *, bool);

static int set_lte_shdn(struct device *, bool);

static int set_lte_usb_force_boot(struct device *, bool);
static int get_lte_usb_force_boot(struct device *);

static int set_lte_gpio4(struct device *, bool);
static int get_lte_gpio4(struct device *);

static int set_lte_gpio_spare(struct device *, bool);
static int get_lte_gpio_spare(struct device *);

static int get_lte_pwrmon(struct device *);
static int get_lte_sw_ready(struct device *);

static void qed_pwr_lte_off(struct platform_device *pdev);

static ssize_t bt_pwr_show(struct device *dev,
                           struct device_attribute *attr,
                           char *buf)
{
        int ret = get_bt_status(dev);
        if (ret < 0) {
                dev_err(dev, "Failed to get bt status\n");
                return -ENODEV;
        }
        return sprintf(buf, "%d\n", ret);
}

static ssize_t bt_pwr_store(struct device *dev,
                            struct device_attribute *attr,
                            const char *buf,
                            size_t count)
{
        int pwr;
        int ret;
        sscanf(buf,"%d",&pwr);
        dev_dbg(dev, "bt power: %d\n", pwr);

        ret = set_bt_enable(dev, !!pwr);
        if (ret < 0) {
                dev_err(dev, "Failed to %s bluetooth\n",
                                !!pwr ? "enable": "disable");
        }
        return count;
}

static DEVICE_ATTR_RW(bt_pwr);

static ssize_t wifi_pwr_show(struct device *dev,
                             struct device_attribute *attr,
                             char *buf)
{
        int ret = get_wifi_status(dev);
        if (ret < 0) {
                dev_err(dev, "Failed to get wifi status\n");
                return -ENODEV;
        }
        return sprintf(buf, "%d\n", ret);
}

static ssize_t wifi_pwr_store(struct device *dev,
                              struct device_attribute *attr,
                              const char *buf,
                              size_t count)
{
        int pwr;
        int ret;
        sscanf(buf,"%d",&pwr);
        dev_dbg(dev, "wifi power: %d\n", pwr);
        ret = set_wifi_enable(dev, !!pwr);

        if (ret < 0) {
                dev_err(dev, "Failed to enable/disable wifi\n");
        }
        return count;
}

static DEVICE_ATTR_RW(wifi_pwr);

static ssize_t lte_pwr_show(struct device *dev,
                            struct device_attribute *attr,
                            char *buf)
{
        int ret = get_lte_sw_ready(dev);
        if (ret < 0) {
                dev_err(dev, "Failed to get lte sw ready status\n");
                return -ENODEV;
        }
        return sprintf(buf, "%d\n", ret);
}

static ssize_t lte_pwr_store(struct device *dev,
                             struct device_attribute *attr,
                             const char *buf,
                             size_t count)
{
        int pwr;
        int ret;
        sscanf(buf,"%d",&pwr);
        dev_dbg(dev, "lte power: %d\n", pwr);

        ret = set_lte_power(dev, !!pwr);
        if (ret < 0) {
                dev_err(dev, "Failed to power on/off lte\n");
        }
        return count;
}

static DEVICE_ATTR_RW(lte_pwr);

static ssize_t lte_usb_force_boot_show(struct device *dev,
                                       struct device_attribute *attr,
                                       char *buf)
{
        int ret = get_lte_usb_force_boot(dev);
        if (ret < 0) {
                dev_err(dev, "Failed to get lte usb force boot status\n");
                return -ENODEV;
        }
        return sprintf(buf, "%d\n", ret);
}

static ssize_t lte_usb_force_boot_store(struct device *dev,
                                        struct device_attribute *attr,
                                        const char *buf,
                                        size_t count)
{
        int pwr;
        int ret;
        sscanf(buf,"%d",&pwr);
        dev_dbg(dev, "lte force boot: %d\n", pwr);

        ret = set_lte_usb_force_boot(dev, !!pwr);
        if (ret < 0) {
                dev_err(dev, "Failed to usb force boot assert/de-assert lte\n");
        }
        return count;
}

static DEVICE_ATTR_RW(lte_usb_force_boot);

static ssize_t lte_fast_shdn_store(struct device *dev,
                                   struct device_attribute *attr,
                                   const char *buf,
                                   size_t count)
{
        int pwr;
        int ret;
        sscanf(buf,"%d",&pwr);
        dev_dbg(dev, "lte fast shutdown: %d\n", pwr);

        ret = set_lte_fast_shdn(dev, !!pwr);
        if (ret < 0) {
                dev_err(dev, "Failed to assert/de-assert fast shutdown\n");
        }
        return count;
}

static DEVICE_ATTR_WO(lte_fast_shdn);

static ssize_t lte_shdn_store(struct device *dev,
                              struct device_attribute *attr,
                              const char *buf,
                              size_t count)
{
        int pwr;
        int ret;
        sscanf(buf,"%d",&pwr);
        dev_dbg(dev, "lte fast shutdown: %d\n", pwr);

        ret = set_lte_shdn(dev, !!pwr);
        if (ret < 0) {
                dev_err(dev, "Failed to assert/de-assert fast shutdown\n");
        }
        return count;
}

static DEVICE_ATTR_WO(lte_shdn);

static ssize_t lte_gpio4_show(struct device *dev,
                              struct device_attribute *attr,
                              char *buf)
{
        int ret = get_lte_gpio4(dev);
        if (ret < 0) {
                dev_err(dev, "Failed to get lte gpio4 status\n");
                return -ENODEV;
        }
        return sprintf(buf, "%d\n", ret);
}

static ssize_t lte_gpio4_store(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf,
                               size_t count)
{
        int pwr;
        int ret;
        sscanf(buf,"%d",&pwr);
        dev_dbg(dev, "lte fast shutdown: %d\n", pwr);

        ret = set_lte_gpio4(dev, !!pwr);
        if (ret < 0) {
                dev_err(dev, "Failed to asert/de-assert gpio4\n");
        }
        return count;
}

static DEVICE_ATTR_RW(lte_gpio4);

static ssize_t lte_gpio_spare_show(struct device *dev,
                                   struct device_attribute *attr,
                                   char *buf)
{
        int ret = get_lte_gpio_spare(dev);
        if (ret < 0) {
                dev_err(dev, "Failed to get lte gpio4 status\n");
                return -ENODEV;
        }
        return sprintf(buf, "%d\n", ret);
}

static ssize_t lte_gpio_spare_store(struct device *dev,
                                    struct device_attribute *attr,
                                    const char *buf,
                                    size_t count)
{
        int pwr;
        int ret;
        sscanf(buf,"%d",&pwr);
        dev_dbg(dev, "lte fast shutdown: %d\n", pwr);

        ret = set_lte_gpio_spare(dev, !!pwr);
        if (ret < 0) {
                dev_err(dev, "Failed to assert/de-assert gpio spare\n");
        }
        return count;
}

static DEVICE_ATTR_RW(lte_gpio_spare);

static ssize_t lte_pwrmon_show(struct device *dev,
                               struct device_attribute *attr,
                               char *buf)
{
        int ret = get_lte_pwrmon(dev);
        if (ret < 0) {
                dev_err(dev, "Failed to get pwrmon status\n");
                return -ENODEV;
        }
        return sprintf(buf, "%d\n", ret);
}

static DEVICE_ATTR_RO(lte_pwrmon);

static ssize_t lte_sw_ready_show(struct device *dev,
                                 struct device_attribute *attr,
                                 char *buf)
{
        int ret = get_lte_sw_ready(dev);
        if (ret < 0) {
                dev_err(dev, "Failed to get sw ready status\n");
                return -ENODEV;
        }
        return sprintf(buf, "%d\n", ret);
}

static DEVICE_ATTR_RO(lte_sw_ready);

struct attribute *qed_pwr_attrs[] = {
        &dev_attr_bt_pwr.attr,
        &dev_attr_wifi_pwr.attr,
        &dev_attr_lte_pwr.attr,
        &dev_attr_lte_usb_force_boot.attr,
        &dev_attr_lte_fast_shdn.attr,
        &dev_attr_lte_shdn.attr,
        &dev_attr_lte_gpio4.attr,
        &dev_attr_lte_gpio_spare.attr,
        &dev_attr_lte_pwrmon.attr,
        &dev_attr_lte_sw_ready.attr,
        NULL,
};

const struct attribute_group qed_pwr_attr_group = {
        .attrs = qed_pwr_attrs,
};

static const struct attribute_group *qed_pwr_attr_groups[] = {
        &qed_pwr_attr_group,
        NULL,
};

int get(struct device *dev, int gpio)
{
        int rc;
        struct qed_pwr_data *pd = dev_get_drvdata(dev);

        if (gpio >= END_GPIOS || gpio < ETH_EN)
                return -EINVAL;
        mutex_lock(&pd->lock);
        rc = gpiod_get_value(pd->qed_gpios[gpio]);
        dev_dbg(dev, "%s: %s:%d\n", __func__, gpio_names[gpio],
                gpiod_get_value(pd->qed_gpios[gpio]));
        mutex_unlock(&pd->lock);
        return rc;
}

int set(struct device *dev, int gpio, bool on)
{
        struct qed_pwr_data *pd = dev_get_drvdata(dev);

        if (gpio >= END_GPIOS || gpio < ETH_EN) {
                dev_err(dev, "%s: invalid gpio %d\n", __func__, gpio);
                return -EINVAL;
        }

        mutex_lock(&pd->lock);
        do {
                int state = gpiod_get_value(pd->qed_gpios[gpio]);
                if (on) {
                        if (state)
                                break;

                } else {
                        if (!state)
                                break;
                }
                gpiod_set_value(pd->qed_gpios[gpio], on);
                dev_dbg(dev, "%s: %s:%d\n", __func__, gpio_names[gpio],
                        gpiod_get_value(pd->qed_gpios[gpio]));
        } while (false);

        mutex_unlock(&pd->lock);
        return 0;
}

static void btwifi_on(struct device *dev, bool on)
{
        struct qed_pwr_data *pd = dev_get_drvdata(dev);

        if (on) {
                gpiod_set_value(pd->qed_gpios[BTWIFI_VDD], 0);
                usleep_range(50, 51);
                gpiod_set_value(pd->qed_gpios[BTWIFI_VDDIO], 0);
                gpiod_set_value(pd->qed_gpios[BTWIFI_VDDIO], 1);
                usleep_range(50, 51);
                gpiod_set_value(pd->qed_gpios[BTWIFI_VDD], 1);
        } else {
                gpiod_set_value(pd->qed_gpios[BTWIFI_VDD], 0);
                usleep_range(100, 101);
                gpiod_set_value(pd->qed_gpios[BTWIFI_VDDIO], 0);
        }
}

int set_bt_enable(struct device *dev, bool enable)
{
        struct qed_pwr_data *pd = dev_get_drvdata(dev);
        int state;

        mutex_lock(&pd->lock);
        state = gpiod_get_value(pd->qed_gpios[BT_EN]);

        do {
                if (enable) {
                    if (state) {
                        dev_dbg(dev, "%s:%s: %d already enabled\n", __func__, gpio_names[BT_EN],
                                gpiod_get_value(pd->qed_gpios[BT_EN]));
                        break;
                    }
                        if (!gpiod_get_value(pd->qed_gpios[BTWIFI_VDDIO]) ||
                                        !gpiod_get_value(pd->qed_gpios[BTWIFI_VDD])) {
                                btwifi_on(dev, true);
                        }

                }
                else {
                        if (!state) {
                                dev_dbg(dev, "%s:%s: %d already disabled\n", __func__, gpio_names[BT_EN],
                                gpiod_get_value(pd->qed_gpios[BT_EN]));
                                break;
                        }
                }

                gpiod_set_value(pd->qed_gpios[BT_EN], enable);

                if (!enable && !gpiod_get_value(pd->qed_gpios[WIFI_EN])) {
                                btwifi_on(dev, false);
                }
        } while (false);

        mutex_unlock(&pd->lock);
        return 0;

}

int get_bt_status(struct device *dev)
{
        return get(dev, BT_EN);
}

int set_wifi_enable(struct device *dev, bool enable)
{
        struct qed_pwr_data *pd = dev_get_drvdata(dev);
        int state;
        mutex_lock(&pd->lock);
        state = gpiod_get_value(pd->qed_gpios[WIFI_EN]);

        do {
                if (enable) {
                        if (state) {
                                dev_dbg(dev, "%s:%s\n", __func__, "WIFI_EN already enabled");
                                break;
                        }
                        if (!gpiod_get_value(pd->qed_gpios[BTWIFI_VDDIO]) ||
                            !gpiod_get_value(pd->qed_gpios[BTWIFI_VDD])) {
                                btwifi_on(dev, true);
                        }

                } else {
                        if (!state) {
                                dev_dbg(dev, "%s:%s\n", __func__, "WIFI_EN already disabled");
                                break;
                        }
                }

                gpiod_set_value(pd->qed_gpios[WIFI_EN], enable);

                if (!enable && !gpiod_get_value(pd->qed_gpios[BT_EN])) {
                                btwifi_on(dev, false);
                }
        } while (false);

        mutex_unlock(&pd->lock);
        return 0;
}

int get_wifi_status(struct device *dev)
{
        dev_err(dev, "%s\n", __func__);
        return get(dev, WIFI_EN);
}

int reset_vsc(struct device *dev)
{
        struct qed_pwr_data *pd = dev_get_drvdata(dev);
	struct gpio_desc *gpod;

        mutex_lock(&pd->lock);
	gpod = devm_gpiod_get(dev, VSC_RESET_OF, GPIOD_OUT_LOW);

	if (IS_ERR(gpod)) {
                dev_err(dev, "Failed to get of "VSC_RESET_OF" gpio\n");
		return 1;
	}

	msleep(1);
	devm_gpiod_put(dev, gpod);

        mutex_unlock(&pd->lock);
        return 0;
}

int set_lte_power(struct device *dev, bool on)
{
        int ret = 0;
        int state = get_lte_sw_ready(dev);

        if (state < 0) {
                dev_err(dev, "Failed to get sw ready status\n");
                return -ENODEV;
        }

        if (on) {
                dev_dbg(dev, "%s: *** set_lte_power -> on\n", __func__);
                if (state) {
                        dev_info(dev, "%s modem already enabled\n", __func__);
                } else {
                        dev_dbg(dev, "%s: *** Power On: set LTE_PWR true, 1.5 sec delay\n", __func__);
                        ret = set(dev, LTE_PWR, true);
                        msleep(1500);
                        dev_dbg(dev, "%s: *** Power On: set LTE_PWR false\n", __func__);
                        ret = set(dev, LTE_PWR, false);
                }
        } else {
                dev_dbg(dev, "*** set_lte_power -> off\n");
                if (!state) {
                        dev_info(dev, "%s: modem already disabled\n", __func__);
                } else {
                        dev_dbg(dev, "*** Power Off: set LTE_PWR true, 2.8 sec delay\n");
                        ret = set(dev, LTE_PWR, true);
                        msleep(2800);
                        dev_dbg(dev, "*** Power Off: set LTE_PWR false\n");
                        ret = set(dev, LTE_PWR, false);
                }
        }
        return ret;
}

int __maybe_unused get_lte_power(struct device *dev)
{
        return get(dev, LTE_SW_RDY);
}

int set_lte_usb_force_boot(struct device *dev, bool on)
{
        return set(dev, LTE_USB_FORCE_BOOT, on);
}

int get_lte_usb_force_boot(struct device *dev)
{
        return get(dev, LTE_USB_FORCE_BOOT);
}

int set_lte_fast_shdn(struct device *dev, bool __maybe_unused on)
{
        set(dev, LTE_FAST_SHDN, true);
        msleep(10);
        return set(dev, LTE_FAST_SHDN, false);
}

int set_lte_shdn(struct device *dev, bool __maybe_unused on)
{
        set(dev, LTE_SHDN, true);
        msleep(210);
        return set(dev, LTE_SHDN, false);
}

int set_lte_gpio4(struct device *dev, bool on)
{
        return set(dev, LTE_GPIO4, on);
}

int get_lte_gpio4(struct device *dev)
{
        return get(dev, LTE_GPIO4);
}

int set_lte_gpio_spare(struct device *dev, bool on)
{
        return set(dev, LTE_GPIO_SPARE, on);
}

int get_lte_gpio_spare(struct device *dev)
{
        return get(dev, LTE_GPIO_SPARE);
}

int get_lte_pwrmon(struct device *dev)
{
        return get(dev, LTE_PWRMON);
}

int get_lte_sw_ready(struct device *dev)
{
        return get(dev, LTE_SW_RDY);
}

static const struct of_device_id qed_pwr_of_match[] = {
        {.compatible = "ltx,qed-power"},
        {}
};

MODULE_DEVICE_TABLE(of, qed_pwr_of_match);

#ifdef _LTE_DEBUG
static int monitor_thread_f(void *pv)
{
        struct device *dev = (struct device *)pv;
        struct qed_pwr_data *pd = dev_get_drvdata(dev);
        int cur_on_off = 2, cur_sw_rdy = 2, cur_powerm = 2;
        int cur_shdwn = 2, cur_fast_shdwn = 2;

        while(!kthread_should_stop()) {
            int on_off = gpiod_get_value(pd->qed_gpios[LTE_PWR]);
            int powerm = gpiod_get_value(pd->qed_gpios[LTE_PWRMON]);
            int sw_rdy = gpiod_get_value(pd->qed_gpios[LTE_SW_RDY]);
            int shdwn = gpiod_get_value(pd->qed_gpios[LTE_SHDN]);
            int fast_shdwn = gpiod_get_value(pd->qed_gpios[LTE_FAST_SHDN]);

            if (cur_on_off != on_off) {
                cur_on_off = on_off;
                dev_err(dev, "%s: on_off gpio %d\n", __func__,  cur_on_off);
            }

            if (cur_powerm != powerm) {
                cur_powerm = powerm;
                dev_err(dev, "%s: powerm gpio %d\n", __func__,  cur_powerm);
            }

            if (cur_sw_rdy != sw_rdy) {
                cur_sw_rdy = sw_rdy;
                dev_err(dev, "%s: sw_rdy gpio %d\n", __func__,  cur_sw_rdy);
            }

            if (cur_shdwn != shdwn) {
                cur_shdwn = shdwn;
                dev_err(dev, "%s: shdwn gpio %d\n", __func__,  cur_shdwn);
            }

            if (cur_fast_shdwn != fast_shdwn) {
                cur_fast_shdwn = fast_shdwn;
                dev_err(dev, "%s: fast_shdwn gpio %d\n", __func__,  cur_fast_shdwn);
            }

            msleep(10);
        }
    return 0;
}
#endif

static void bt_pwr_f(struct work_struct *work)
{
        struct delayed_work *dw  = to_delayed_work(work);
        struct qed_pwr_data *data = container_of(dw, struct qed_pwr_data, bt_pwr_on);
        struct device __maybe_unused *dev = container_of((void*)data, struct device, driver_data);
        set_bt_enable(&data->pdev->dev, true);
}

static void lte_pwr_f(struct work_struct *work)
{
        struct delayed_work *dw  = to_delayed_work(work);
        struct qed_pwr_data *data = container_of(dw, struct qed_pwr_data, lte_pwr_on);
        struct device __maybe_unused *dev = container_of((void*)data, struct device, driver_data);
        set_lte_power(&data->pdev->dev, true);
}

static long qed_pwr_ioctl(struct file *file, unsigned int cmd,
                          unsigned long arg)
{
        struct miscdevice *mdev = file->private_data;
        struct qed_pwr_data *data = container_of(mdev, struct qed_pwr_data, mdev);
        struct device *dev = &data->pdev->dev;
        int ret, opt;
        int __user *argp = (int __user *)arg;

        if (IS_ENABLED(CONFIG_QED_BTWIFI_PWR_COMPLIANCE_TEST))
                return -ENOTTY;
        switch (cmd) {
                case QED_PWR_WIFI_GETSTATUS:
                        ret = put_user(get_wifi_status(dev), argp);
                        return (ret < 0 ? ret : 0);
                case QED_PWR_WIFI_SET:
                        if (get_user(opt, (int *)arg))
                                return -EFAULT;
                        return set_wifi_enable(dev, !!opt);
                case QED_PWR_LTE_GETSTATUS:
                        ret = put_user(get_lte_power(dev), argp);
                        return (ret < 0 ? ret : 0);
                case QED_PWR_LTE_SET:
                        if (get_user(opt, (int *)arg))
                                return -EFAULT;
                        return set_lte_power(dev, !!opt);
                case QED_PWR_BT_GETSTATUS:
                        ret = put_user(get_bt_status(dev), argp);
                        return (ret < 0 ? ret : 0);
                case QED_PWR_BT_SET:
                        if (get_user(opt, (int *)arg))
                                return -EFAULT;
                        return set_bt_enable(dev, !!opt);
                default:
                        return -ENOTTY;
        }
}

static long qed_pwr_read(struct file *filp, char __user *buf, size_t count, loff_t *off)
{
        struct miscdevice *mdev = filp->private_data;
        struct qed_pwr_data *data = container_of(mdev, struct qed_pwr_data, mdev);
        return simple_read_from_buffer(buf, count, off, "qed power\n", 10);
}

static const struct file_operations qed_pwr_fops = {
        .owner  = THIS_MODULE,
        .read   = qed_pwr_read,
        .unlocked_ioctl   = qed_pwr_ioctl,
};

static int qed_pwr_probe(struct platform_device *pdev)
{
        struct device *dev = &pdev->dev;
        struct qed_pwr_data *data;
        int i, ret;

        data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
        if (!data) {
                dev_err(dev, "Failed to allocate private data\n");
                return (-ENOMEM);
        }

        platform_set_drvdata(pdev, data);

        if (!pdev->dev.of_node) {
                dev_err(dev, "Failed to get of data\n");
                goto err_of_dev;
        }

        data->pdev = pdev;

        data->qed_gpios[ETH_PCIE_EN] = devm_gpiod_get_index(&pdev->dev, GPIO_OF, ETH_PCIE_EN, GPIOD_OUT_HIGH);

        if (IS_ERR(data->qed_gpios[ETH_PCIE_EN])) {
                dev_err(dev, "Failed to get of eth_pcie_en gpio\n");
                goto err_of_dev;
        }

        usleep_range(10, 11);

        /* enable VDD on i226 */
        data->qed_gpios[ETH_EN] = devm_gpiod_get_index(&pdev->dev, GPIO_OF, ETH_EN, GPIOD_OUT_HIGH);

        if (IS_ERR(data->qed_gpios[BTWIFI_VDD])) {
                dev_err(dev, "Failed to get of eth_en gpio\n");
                goto err_of_dev;
        }

        usleep_range(50, 51);
        /* enable pcie on i226 */
        gpiod_set_value(data->qed_gpios[ETH_PCIE_EN], 0);

        for (i = BTWIFI_VDD; i < END_GPIOS; i++) {

                data->qed_gpios[i] = devm_gpiod_get_index(&pdev->dev, GPIO_OF, i, gpio_config[i]);
                if (IS_ERR(data->qed_gpios[i])) {
                        dev_err(dev, "%s: Failed to get gpio idx %d\n", __func__, i);
                        goto err_of_dev;
                }

                if (IS_ENABLED(CONFIG_QED_BTWIFI_PWR_COMPLIANCE_TEST))
                    dev_info(dev, "%s: set %s gpio to %s\n", __func__,
                        gpio_names[i],
                        (gpio_config[i] == GPIOD_OUT_LOW) ? "OUTPUT LOW" : "INPUT");
                else
                    dev_dbg(dev, "%s: set %s gpio to %s\n", __func__,
                        gpio_names[i],
                        (gpio_config[i] == GPIOD_OUT_LOW) ? "OUTPUT LOW" : "INPUT");
        }
#ifdef _LTE_DEBUG
        monitor_thread = kthread_run(monitor_thread_f, dev, "on_off thread");
        if(monitor_thread) {
            dev_info(dev, "monitor thread success ...\n");
        } else {
            dev_err(dev, "Cannot create monitor thread\n");
        }
#endif

        if (sysfs_create_groups(&pdev->dev.kobj, qed_pwr_attr_groups)) {
                dev_err(dev, "Cannot create sysfs file......\n");
                goto err_of_dev;
        }

        data->qed_kobj = kobject_create_and_add(SYSFS_QED_PWR, NULL);
        if (!data->qed_kobj) {
                dev_err(dev, "Cannot create kobj\n");
                goto err_sysfs_group;
        }

        if (sysfs_create_link(data->qed_kobj, &pdev->dev.kobj, SYSFS_PWR)) {
                dev_err(dev, "Cannot create sysfs link......\n");
                goto err_kobject;
        }

        mutex_init(&data->lock);

        if (of_property_read_bool(dev->of_node, WIFI_EN_OF)) {
                set_wifi_enable(dev, true);
        }

        if (of_property_read_bool(dev->of_node, VSC_RESET_OF)) {
		reset_vsc(dev);
        }

        if (of_property_read_bool(dev->of_node, LTE_EN_OF)) {
                INIT_DELAYED_WORK(&data->lte_pwr_on, lte_pwr_f);
                schedule_delayed_work(&data->lte_pwr_on, msecs_to_jiffies(LTE_DELAY));
        }

        if (of_property_read_bool(dev->of_node, BT_EN_OF)) {
                INIT_DELAYED_WORK(&data->bt_pwr_on, bt_pwr_f);
                schedule_delayed_work(&data->bt_pwr_on, msecs_to_jiffies(BT_DELAY));
        }


        data->mdev.minor = MISC_DYNAMIC_MINOR;
        data->mdev.name = "qed_pwr";
        data->mdev.fops = &qed_pwr_fops;
        data->mdev.parent = NULL;
        ret = misc_register(&data->mdev);
        if (ret) {
                dev_err(dev, "Failed to register misc qed-pwr\n");
                return ret;
        }

        dev_dbg(dev, "%s: %d\n", gpio_names[BTWIFI_VDD], gpiod_get_value(data->qed_gpios[BTWIFI_VDD]));
        dev_dbg(dev, "%s: %d\n", gpio_names[BTWIFI_VDDIO], gpiod_get_value(data->qed_gpios[BTWIFI_VDDIO]));
        dev_dbg(dev, "%s: %d\n", gpio_names[WIFI_EN], gpiod_get_value(data->qed_gpios[WIFI_EN]));
        dev_dbg(dev, "%s: %d\n", gpio_names[BTWIFI_VDD], gpiod_get_value(data->qed_gpios[BTWIFI_VDD]));
        dev_dbg(dev, "%s: %d\n", gpio_names[BTWIFI_VDDIO], gpiod_get_value(data->qed_gpios[BTWIFI_VDDIO]));
        dev_dbg(dev, "%s: %d\n", gpio_names[LTE_PWR], gpiod_get_value(data->qed_gpios[LTE_PWR]));
        dev_dbg(dev, "%s: %d\n", gpio_names[LTE_SW_RDY], gpiod_get_value(data->qed_gpios[LTE_SW_RDY]));
        dev_info(dev, "QED power driver registered\n");
        return 0;

err_kobject:
        kobject_put(data->qed_kobj);
err_sysfs_group:
        sysfs_remove_group(&pdev->dev.kobj, &qed_pwr_attr_group);
err_of_dev:
#ifdef _LTE_DEBUG
        kthread_stop(monitor_thread);
#endif
        return -ENODEV;
}

void qed_pwr_lte_off(struct platform_device *pdev) {
        struct device *dev = &pdev->dev;
        struct qed_pwr_data *pd = platform_get_drvdata(pdev);
        int retries = 0;
        uint32_t sleep = 50;

        if (IS_ENABLED(CONFIG_QED_BTWIFI_PWR_COMPLIANCE_TEST))
                return;

        set_lte_shdn(dev, true);
        while (retries < LTE_PWR_OFF_MAX_ITER) {
                if (!gpiod_get_value(pd->qed_gpios[LTE_SW_RDY])
                    && !gpiod_get_value(pd->qed_gpios[LTE_PWRMON])) {
                        dev_dbg(dev, "modem is off\n");
                        break;
                }
                ++retries;
                msleep(sleep);
        }

        if (LTE_PWR_OFF_MAX_ITER <= retries) {
                dev_err(dev, "modem is still on after %u ms\n",
                        (retries * sleep));
        }
}

static int qed_pwr_remove(struct platform_device *pdev)
{
        struct device *dev = &pdev->dev;
        struct qed_pwr_data *pd = platform_get_drvdata(pdev);

        misc_deregister(&pd->mdev);
        flush_scheduled_work();
        qed_pwr_lte_off(pdev);
#ifdef _LTE_DEBUG
        kthread_stop(monitor_thread);
#endif
        mutex_destroy(&pd->lock);

        kobject_put(pd->qed_kobj);
        sysfs_remove_group(&pdev->dev.kobj, &qed_pwr_attr_group);
        dev_info(dev, "qed-pwr driver Unregistered\n");

        return 0;
}

static struct platform_driver qed_pwr_plat_driver = {
        .driver = {
                .name   = "qed_pwr",
                .owner  = THIS_MODULE,
                .of_match_table = of_match_ptr(qed_pwr_of_match),
        },
        .probe = qed_pwr_probe,
        .remove = qed_pwr_remove,
};

static int __init qed_pwr_init(void)
{
        return platform_driver_register(&qed_pwr_plat_driver);
}

arch_initcall(qed_pwr_init);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Lantronix <lantronix@lantronix.com>");
MODULE_DESCRIPTION("QED Power driver");
MODULE_VERSION("1.01");
