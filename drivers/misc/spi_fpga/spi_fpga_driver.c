// SPDX-License-Identifier: GPL-2.0
/*
 *  spi_fpga_drivers.c - QED spi fgpa driver.
 *
 *  Copyright (C) 2021 Lantronix Inc.
 *
 */

#include <asm/unaligned.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/firmware.h>

#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/mod_devicetable.h>
#include <linux/gpio/consumer.h>

#include "ad7768.h"
#include "fpga.h"
#include "ecp5.h"

struct fpga_data {
        struct spi_device *spi_cfg;
        struct spi_device *spi_fw;
        struct spi_board_info cfg_info;
        struct spi_board_info fw_info;
        struct gpio_desc *power;
        struct gpio_desc *reset;
        struct gpio_desc *programn;
        struct gpio_desc *initn;
        struct gpio_desc *done;
        struct gpio_desc *nconfig;
        struct kobject *fpga_kobj;
        struct mutex lock;
        /* adc clock rate */
        unsigned long clock_rate;
        unsigned int sampling_freq;
        enum ad7768_power_modes power_mode;
        enum fpga_cfg cfg_mode;
        __be16 d16;
        uint8_t slice_enabled;
        enum fpga_type type;
};

static int get_id(struct fpga_data *);

static int get_window_size(struct fpga_data *);
static int set_window_size(struct fpga_data *, uint8_t);

static int get_test_mode(const struct fpga_data *);
static int set_test_mode1(const struct fpga_data *);
static int set_test_mode2(const struct fpga_data *);
static int set_test_mode_disable(const struct fpga_data *);

static int set_pps_enable(const struct fpga_data *);
static int set_pps_disable(const struct fpga_data *);

static int set_cfg_normal(struct fpga_data *);
static int set_cfg_adc0(struct fpga_data *);
static int set_cfg_adc1(struct fpga_data *);

static int get_soft_reset(struct fpga_data *);
static int assert_soft_reset(struct fpga_data *);
static int release_soft_reset(struct fpga_data *);

static int get_slices_enabled(struct fpga_data *);
static int set_slices_enabled(struct fpga_data *, unsigned char);

static int get_irq_offset(struct fpga_data *);
static int set_irq_offset(struct fpga_data *, uint8_t);

static int get_ch_irq_mask_hi(struct fpga_data *);
static int set_ch_irq_mask_hi(struct fpga_data *, uint8_t);
static int get_ch_irq_mask_low(struct fpga_data *);
static int set_ch_irq_mask_low(struct fpga_data *, uint8_t);

static int get_ch_overflow_hi(struct fpga_data *);
static int get_ch_overflow_low(struct fpga_data *);
static int get_ch_underflow_hi(struct fpga_data *);
static int get_ch_underflow_low(struct fpga_data *);

static int get_stat(struct fpga_data *);
static int clear_stat(struct fpga_data *);

static int get_pps_data(struct fpga_data *, struct fpga_pps_dbg *);

static int get_adc_reset(struct fpga_data *);
static int adc_reset(struct fpga_data *, uint32_t);
static int adc_reset_assert(struct fpga_data *);
static int adc_reset_deassert(struct fpga_data *);

static int ad7768_set_sampling_freq(struct fpga_data *, unsigned int);
static int ad7768_get_sampling_freq(struct fpga_data *);

static int ad7768_set_power_mode(struct fpga_data *, unsigned int);
static int ad7768_get_power_mode(struct fpga_data *);

static int ad7768_set_filter_type(struct fpga_data *, unsigned int);
static int ad7768_get_filter_type(struct fpga_data *);

static int ad7768_set_channel_standby(struct fpga_data *, unsigned char);
static int ad7768_get_channel_standby(struct fpga_data *);

static int ad7768_get_revision(struct fpga_data *);
static int ad7768_get_interface_mode(struct fpga_data *);

static int ad7768_read_register(struct fpga_data *, uint8_t);

#define QED_SPI_FPGA_MAX_ITER 50
#define QED_SPI_FPGA_MAX_ECP5_ITER 70

static ssize_t id_show(struct device *dev,
                       struct device_attribute *attr,
                       char *buf)
{
        int ret = get_id(dev_get_drvdata(dev));
        if (ret < 0) {
                dev_err(dev, "Failed to get fpga id\n");
                return -ENODEV;
        }
        return sprintf(buf, "%02x\n", ret);
}

static DEVICE_ATTR_RO(id);

static ssize_t test_mode_show(struct device *dev,
                              struct device_attribute *attr,
                              char *buf)
{
        int ret = get_test_mode(dev_get_drvdata(dev));
        if (ret < 0) {
                dev_err(dev, "Failed to get fpga test mode\n");
                return -ENODEV;
        }
        return sprintf(buf, "test mode: %s, PPS Alignment %s\n",
                       (ret & (FPGA_TEST_MODE2 | FPGA_TEST_MODE1)) == 2 ?
                       "mode2" : (((ret & (FPGA_TEST_MODE2 | FPGA_TEST_MODE1)) == 1)
                        ? "mode1" : "normal"),
                       ((ret & FPGA_TEST_MODE_DEFAULT) >> 4) ?
                       "enabled" : "disabled");
}

static ssize_t test_mode_store(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf,
                               size_t count)
{
        int test_mode;
        int ret;
        sscanf(buf,"%d",&test_mode);
        pr_debug("test_mode_store %d\n", test_mode);

        if (test_mode == 1) {
                ret = set_test_mode1(dev_get_drvdata(dev));
                if (ret < 0) {
                        dev_err(dev, "Failed to get fpga test mode1\n");
                }
        }
        else if (test_mode == 2) {
                ret = set_test_mode2(dev_get_drvdata(dev));
                if (ret < 0) {
                        dev_err(dev, "Failed to get fpga test mode2\n");
                }
        }
        else  {
                ret = set_test_mode_disable(dev_get_drvdata(dev));
                if (ret < 0) {
                        dev_err(dev, "Failed to get fpga test disable\n");
                }
        }
        return count;
}

static DEVICE_ATTR_RW(test_mode);

static ssize_t cfg_cfg_show(struct device *dev,
                            struct device_attribute *attr,
                            char *buf)
{
        struct fpga_data *pd = dev_get_drvdata(dev);

        return sprintf(buf, "cfg_cfg %s\n",
                       pd->cfg_mode == FPGA_CFG_MODE_CFG_ADC0 ? "adc0"
                       : (pd->cfg_mode == FPGA_CFG_MODE_CFG_ADC1 ? "adc1" : "normal"));
}

static ssize_t cfg_cfg_store(struct device *dev,
                             struct device_attribute *attr,
                             const char *buf,
                             size_t count)
{
        int config, ret;

        sscanf(buf,"%d",&config);

        if (config == FPGA_CFG_MODE_CFG_ADC0) {
                ret = set_cfg_adc0(dev_get_drvdata(dev));
        }
        else if (config == FPGA_CFG_MODE_CFG_ADC1) {
                ret = set_cfg_adc1(dev_get_drvdata(dev));
        }
        else  {
                ret = set_cfg_normal(dev_get_drvdata(dev));
        }

        if (ret < 0) {
                dev_err(dev, "Failed to set config mode %d\n", config);
                return -ENODEV;
        }

        return count;
}

static DEVICE_ATTR_RW(cfg_cfg);

static ssize_t soft_reset_show(struct device *dev,
                               struct device_attribute *attr,
                               char *buf)
{
        int ret = get_soft_reset(dev_get_drvdata(dev));
        if (ret < 0) {
                dev_err(dev, "Failed to get fpga soft reset\n");
                return -ENODEV;
        }
        return sprintf(buf, "soft reset: %s\n",
                       (ret & 0x01) ? "asserted" : "de-asserted");
}

static ssize_t soft_reset_store(struct device *dev,
                                struct device_attribute *attr,
                                const char *buf,
                                size_t count)
{
        int soft_reset;
        int ret;

        sscanf(buf,"%d",&soft_reset);

        if (soft_reset == 0) {
                ret = release_soft_reset(dev_get_drvdata(dev));
                if (ret < 0) {
                        dev_err(dev, "Failed to release soft reset\n");
                        return -ENODEV;
                }
        }
        else  {
                ret = assert_soft_reset(dev_get_drvdata(dev));
                if (ret < 0) {
                        dev_err(dev, "Failed to assert soft reset\n");
                        return -ENODEV;
                }
        }
        return count;
}

static DEVICE_ATTR_RW(soft_reset);

static ssize_t adc_reset_show(struct device *dev,
                              struct device_attribute *attr,
                              char *buf)
{
        int ret = get_adc_reset(dev_get_drvdata(dev));
        if (ret < 0) {
                dev_err(dev, "Failed to get fpga adc reset\n");
                return -ENODEV;
        }
        return sprintf(buf, "adc reset: %s\n",
                       (ret & 0x02) ? "asserted" : "de-asserted");
}

static ssize_t adc_reset_store(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf,
                               size_t count)
{
        int adc_reset;
        int ret;

        sscanf(buf,"%d",&adc_reset);

        if (adc_reset == 0) {
                ret = adc_reset_deassert(dev_get_drvdata(dev));
                if (ret < 0) {
                        dev_err(dev, "Failed to assert adc reset\n");
                        return -ENODEV;
                }
        }
        else  {
                ret = adc_reset_assert(dev_get_drvdata(dev));
                if (ret < 0) {
                        dev_err(dev, "Failed to de-assert adc reset\n");
                        return -ENODEV;
                }
        }
        return count;
}

static DEVICE_ATTR_RW(adc_reset);

static ssize_t slices_enable_show(struct device *dev,
                                  struct device_attribute *attr,
                                  char *buf)
{
        int ret = get_slices_enabled(dev_get_drvdata(dev));

        if (ret < 0) {
                dev_err(dev, "Failed to get fpga soft reset\n");
                return -ENODEV;
        }
        return sprintf(buf, "slice 3: %s,"
                       " slice 2: %s,"
                       " slice 1: %s,"
                       " slice 0: %s\n",
                       (ret & 0x08) ? "enabled" : "disabled",
                       (ret & 0x04) ? "enabled" : "disabled",
                       (ret & 0x02) ? "enabled" : "disabled",
                       (ret & 0x01) ? "enabled" : "disabled");
}

static ssize_t slices_enable_store(struct device *dev,
                                   struct device_attribute *attr,
                                   const char *buf,
                                   size_t count)
{
        int slices_enabled;
        int ret;

        sscanf(buf,"%d", &slices_enabled);

        if (slices_enabled >= 0) {
                ret = set_slices_enabled(dev_get_drvdata(dev), slices_enabled);
                if (ret < 0) {
                        dev_err(dev, "Failed to set slices enabled\n");
                return -ENODEV;
                }
        }
        return count;
}

static DEVICE_ATTR_RW(slices_enable);

static ssize_t pps_dbg_show(struct device *dev,
                            struct device_attribute *attr,
                            char *buf)
{
        struct fpga_pps_dbg data = {0};

        int ret = get_pps_data(dev_get_drvdata(dev), &data);

        if (ret < 0) {
                dev_err(dev, "Failed to get fpga pps data\n");
                return -ENODEV;
        }
        return sprintf(buf, "slice_3_err: %+04d"
                       " slice_2_err: %+04d"
                       " slice_1_err: %+04d"
                       " slice_0_err: %+04d"
                       " freq_err_threshold: %03u"
                       " sync_err_threshold: %03u"
                       " pps_phase_offset: %+04d"
                       " freq_monitor_delta: %+04d\n",
                       data.slice_3_err,
                       data.slice_2_err,
                       data.slice_1_err,
                       data.slice_0_err,
                       data.freq_err_threshold, /* unsigned */
                       data.sync_err_threshold, /* unsigned */
                       data.pps_phase_offset,
                       data.freq_monitor_delta
                       );
}

static DEVICE_ATTR_RO(pps_dbg);

static ssize_t window_size_show(struct device *dev,
                                struct device_attribute *attr,
                                char *buf)
{
        int ret = get_window_size(dev_get_drvdata(dev));
        if (ret < 0) {
                dev_err(dev, "Failed to get fpga window size\n");
                return -ENODEV;
        }
        return sprintf(buf, "%d\n", ret);
}

static ssize_t window_size_store(struct device *dev,
                                 struct device_attribute *attr,
                                 const char *buf,
                                 size_t count)
{
        int window_size;
        int ret;

        sscanf(buf,"%d",&window_size);
        dev_dbg(dev, "window_size %d\n", window_size);

        if (window_size > 0) {
                ret = set_window_size(dev_get_drvdata(dev), window_size);
                if (ret < 0) {
                        dev_err(dev, "Failed to set window size\n");
                        return -ENODEV;
                }
        }
        else  {
                dev_err(dev, "Failed to set window size\n");
                return -ENODEV;
        }
        return count;
}

static DEVICE_ATTR_RW(window_size);

static ssize_t irq_offset_show(struct device *dev,
                               struct device_attribute *attr,
                               char *buf)
{
        int ret = get_irq_offset(dev_get_drvdata(dev));
        if (ret < 0) {
                dev_err(dev, "Failed to get fpga irq offset\n");
                return -ENODEV;
        }
        return sprintf(buf, "%d\n", ret);
}

static ssize_t irq_offset_store(struct device *dev,
                                struct device_attribute *attr,
                                const char *buf,
                                size_t count)
{
        int irq_offset;
        int ret;

        sscanf(buf,"%d",&irq_offset);
        pr_debug("irq_offset %d\n", irq_offset);

        if (irq_offset > 0) {
                ret = set_irq_offset(dev_get_drvdata(dev), irq_offset);
                if (ret < 0) {
                        dev_err(dev, "Failed to set irq offset\n");
                        return -ENODEV;
                }
        }
        else  {
                dev_err(dev, "Failed to set irq offset\n");
                return -ENODEV;
        }
        return count;
}

static DEVICE_ATTR_RW(irq_offset);

static ssize_t sampling_freq_show(struct device *dev,
                                  struct device_attribute *attr,
                                  char *buf)
{
        int ret = ad7768_get_sampling_freq(dev_get_drvdata(dev));
        if (ret < 0) {
                dev_err(dev, "Failed to get ad7768 samping freq\n");
                return -ENODEV;
        }
        return sprintf(buf, "%u\n", ret);
}

static ssize_t sampling_freq_store(struct device *dev,
                                   struct device_attribute *attr,
                                   const char *buf,
                                   size_t count)
{
        int sampling_freq, ret, i;

        sscanf(buf,"%d",&sampling_freq);
        pr_debug("sampling_freq %d\n", sampling_freq);

        for (i = 0; i < ARRAY_SIZE(ad7768_sampl_freq_avail); i++) {
                pr_debug("sampling_freq %d:%d\n", sampling_freq, ad7768_sampl_freq_avail[i]);
                if (sampling_freq == ad7768_sampl_freq_avail[i]) {
                        pr_debug("avail sampling_freq %d\n", sampling_freq);
                        break;
                }
        }

        if (i >= ARRAY_SIZE(ad7768_sampl_freq_avail)) {
                dev_err(dev, "Sampling rate is out of bound %d\n", sampling_freq);
                return -ENODEV;
        }

        if (sampling_freq > 0) {
                ret = ad7768_set_sampling_freq(dev_get_drvdata(dev), sampling_freq);
                if (ret < 0) {
                        dev_err(dev, "Failed to set ad7768 sampling frequency\n");
                        return -ENODEV;
                }
        }
        else  {
                dev_err(dev, "Failed to set ad7768 sampling frequency\n");
                return -ENODEV;
        }
        return count;
}

static DEVICE_ATTR_RW(sampling_freq);

static ssize_t power_mode_show(struct device *dev,
                               struct device_attribute *attr,
                               char *buf)
{
        int ret;

        ret = ad7768_get_power_mode(dev_get_drvdata(dev));
        if (ret < 0) {
                dev_err(dev, "Failed to get ad7768 power mode\n");
                return -ENODEV;
        }
        return sprintf(buf, "%u\n", ret);
}

static ssize_t power_mode_store(struct device *dev,
                                struct device_attribute *attr,
                                const char *buf,
                                size_t count)
{
        int power_mode, ret;

        sscanf(buf,"%d",&power_mode);

        if (power_mode >= 0) {
                ret = ad7768_set_power_mode(dev_get_drvdata(dev), power_mode);
                if (ret < 0) {
                        dev_err(dev, "Failed to set ad7768 power mode\n");
                        return -ENODEV;
                }
        }
        else  {
                dev_err(dev, "Failed to set ad7768 power mode\n");
                return -ENODEV;
        }
        return count;
}

static DEVICE_ATTR_RW(power_mode);

static ssize_t filter_type_show(struct device *dev,
                                struct device_attribute *attr,
                                char *buf)
{
        int ret;

        ret = ad7768_get_filter_type(dev_get_drvdata(dev));
        if (ret < 0) {
                dev_err(dev, "Failed to get ad7768 power mode\n");
                return -ENODEV;
        }
        return sprintf(buf, "%02x\n", ret);
}

static ssize_t filter_type_store(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf,
                               size_t count)
{
        int filter_type, ret;

        sscanf(buf,"%d",&filter_type);

        if (filter_type >= 0) {
                ret = ad7768_set_filter_type(dev_get_drvdata(dev), filter_type);
                if (ret < 0) {
                        dev_err(dev, "Failed to set ad7768 filter type\n");
                        return -ENODEV;
                }
        }
        else  {
                dev_err(dev, "Failed to set ad7768 filter type\n");
                return -ENODEV;
        }
        return count;
}

static DEVICE_ATTR_RW(filter_type);

static ssize_t adc_revision_show(struct device *dev,
                                 struct device_attribute *attr,
                                 char *buf)
{
        int ret;

        ret = ad7768_get_revision(dev_get_drvdata(dev));
        if (ret < 0) {
                dev_err(dev, "Failed to get ad7768 revision\n");
                return -ENODEV;
        }
        return sprintf(buf, "adc %02x\n", ret);
}

static DEVICE_ATTR_RO(adc_revision);

static ssize_t fpga_stat_show(struct device *dev,
                              struct device_attribute *attr,
                              char *buf)
{
        int ret;

        ret = get_stat(dev_get_drvdata(dev));
        if (ret < 0) {
                dev_err(dev, "Failed to get fpga stat\n");
                return -ENODEV;
        }
        return sprintf(buf, "fatal: %01lx frq error: %01x"
                       " out sync: %01lx"
                       " underflow: %01lx"
                       " overflow: %01lx"
                       " wait pps : %01lx\n",
                       (ret & FPGA_STAT_FATAL) >> 7,
                       (ret & FPGA_STAT_FRQ_ERROR) >> 6,
                       (ret & FPGA_STAT_OUT_SYNC) >> 5,
                       (ret & FPGA_STAT_UNDERFLOW) >> 4,
                       (ret & FPGA_STAT_OVERFLOW) >> 3,
                       (ret & FPGA_STAT_WAIT_PPS) >> 0
                       );
}

static ssize_t fpga_stat_store(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf,
                               size_t count)
{
        int ret;

        ret = clear_stat(dev_get_drvdata(dev));
        if (ret < 0) {
                dev_err(dev, "Failed to clear fpga stat\n");
                return -ENODEV;
        }

        return count;
}

static DEVICE_ATTR_RW(fpga_stat);

static ssize_t fpga_reset_store(struct device *dev,
                                struct device_attribute *attr,
                                const char *buf,
                                size_t count)
{
        struct fpga_data *pd = dev_get_drvdata(dev);
        gpiod_set_value(pd->reset, 0);
        msleep(50);
        gpiod_set_value(pd->reset, 1);

        return count;
}

static DEVICE_ATTR_WO(fpga_reset);

static ssize_t write_reg_show(struct device *dev,
                              struct device_attribute *attr,
                              char *buf)
{
        return sprintf(buf, "send cmd to fpga\n");
}

static ssize_t write_reg_store(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf,
                               size_t count)
{

        uint32_t addr = 0, val = 0;

        struct fpga_data *pd = dev_get_drvdata(dev);
        sscanf(buf,"%x %x",&addr, &val);
        pr_debug("write cmd: addr %02x, val %02x\n", addr, val);
        if (addr != 0) {
                uint16_t tx = cpu_to_be16(((addr & 0x7F) << 8) | (0x00ff & val));
                pr_debug("send cmd: Data to set be_to_cpu: address: %02x\n", ((uint8_t *)&tx)[0]);
                pr_debug("send cmd: Data to set be_to_cpu: value: %02x\n", ((uint8_t *)&tx)[1]);
                mutex_lock(&pd->lock);
                spi_write(pd->spi_cfg, &tx, sizeof(tx));
                mutex_unlock(&pd->lock);
                return count;
        }

        else  {
                dev_err(dev, "Failed to write to reg %02x value %02x\n", addr, val);
                return -ENODEV;
        }
}

static DEVICE_ATTR_RW(write_reg);

static ssize_t read_reg_store(struct device *dev,
                              struct device_attribute *attr,
                              const char *buf,
                              size_t count)
{
    uint32_t addr = 0, val = 0;
    struct fpga_data *pd = dev_get_drvdata(dev);

    sscanf(buf,"%x %x",&addr, &val);

    pr_debug("read reg: addr %02x, val %02x\n", addr, val);
    if (addr != 0) {
        uint16_t rx = 0x00;
        uint16_t tx = cpu_to_be16((AD7768_WR_FLAG_MSK(addr) << 8));

        struct spi_transfer t[] = {
            {
                .tx_buf = &tx,
                .len = 2,
                .cs_change = 0, /* do not hold low */
                .bits_per_word = 8,
                .rx_buf = &rx,
            },
        };

        int ret;

        pr_debug("read reg: Data to send  %02x %02x\n", ((uint8_t *)&tx)[0], ((uint8_t *)&tx)[1]);

        mutex_lock(&pd->lock);
        ret = spi_sync_transfer(pd->spi_cfg, t, ARRAY_SIZE(t));
        mutex_unlock(&pd->lock);
        if (ret < 0)
            return -ENODEV;

        pr_debug("read reg: %02x\n", be16_to_cpu(rx));
        return count;
    }
    else  {
        dev_err(dev, "Failed to write to reg %02x value %02x\n", addr, val);
        return -ENODEV;
    }
}

static DEVICE_ATTR_WO(read_reg);

static ssize_t irq_mask_high_show(struct device *dev,
                                  struct device_attribute *attr,
                                  char *buf)
{
        int ret;

        ret = get_ch_irq_mask_hi(dev_get_drvdata(dev));
        if (ret < 0) {
                dev_err(dev, "Failed to get fpga irq high\n");
                return -ENODEV;
        }
        return sprintf(buf, "ch15: %01lx ch14: %01lx"
                       " ch13: %01lx"
                       " ch12: %01lx"
                       " ch11: %01lx"
                       " ch10: %01lx"
                       " ch9: %01lx"
                       " ch8: %01lx\n",
                       (ret & FPGA_IRQ_MSK_HI_CH15) >> 7,
                       (ret & FPGA_IRQ_MSK_HI_CH14) >> 6,
                       (ret & FPGA_IRQ_MSK_HI_CH13) >> 5,
                       (ret & FPGA_IRQ_MSK_HI_CH12) >> 4,
                       (ret & FPGA_IRQ_MSK_HI_CH11) >> 3,
                       (ret & FPGA_IRQ_MSK_HI_CH10) >> 2,
                       (ret & FPGA_IRQ_MSK_HI_CH9) >> 1,
                       (ret & FPGA_IRQ_MSK_HI_CH8) >> 0
                       );
}

static ssize_t irq_mask_high_store(struct device *dev,
                                   struct device_attribute *attr,
                                   const char *buf,
                                   size_t count)
{
        int mask;
        int ret;

        sscanf(buf,"%d",&mask);
        pr_debug("irq high mask %01x\n", mask);

        if (mask >=0) {
                ret = set_ch_irq_mask_hi(dev_get_drvdata(dev), mask);
                if (ret < 0) {
                        dev_err(dev, "Failed to set irq high\n");
                        return -ENODEV;
                }
        }
        else  {
                dev_err(dev, "Failed to set irq high\n");
                return -ENODEV;
        }
        return count;
}

static DEVICE_ATTR_RW(irq_mask_high);

static ssize_t irq_mask_low_show(struct device *dev,
                                 struct device_attribute *attr,
                                 char *buf)
{
        int ret;

        ret = get_ch_irq_mask_low(dev_get_drvdata(dev));
        if (ret < 0) {
                dev_err(dev, "Failed to get fpga irq high\n");
                return -ENODEV;
        }
        return sprintf(buf, "ch7: %01lx ch6: %01lx"
                       " ch5: %01lx"
                       " ch4: %01lx"
                       " ch3: %01lx"
                       " ch2: %01lx"
                       " ch1: %01lx"
                       " ch0: %01lx\n",
                       (ret & FPGA_IRQ_MSK_LOW_CH7) >> 7,
                       (ret & FPGA_IRQ_MSK_LOW_CH6) >> 6,
                       (ret & FPGA_IRQ_MSK_LOW_CH5) >> 5,
                       (ret & FPGA_IRQ_MSK_LOW_CH4) >> 4,
                       (ret & FPGA_IRQ_MSK_LOW_CH3) >> 3,
                       (ret & FPGA_IRQ_MSK_LOW_CH2) >> 2,
                       (ret & FPGA_IRQ_MSK_LOW_CH1) >> 1,
                       (ret & FPGA_IRQ_MSK_LOW_CH0) >> 0
                       );
}

static ssize_t irq_mask_low_store(struct device *dev,
                                  struct device_attribute *attr,
                                  const char *buf,
                                  size_t count)
{
        int mask;
        int ret;

        sscanf(buf,"%d",&mask);
        pr_debug("irq low mask %01x\n", mask);

        if (mask >=0) {
                ret = set_ch_irq_mask_low(dev_get_drvdata(dev), mask);
                if (ret < 0) {
                        dev_err(dev, "Failed to set irq low\n");
                        return -ENODEV;
                }
        }
        else  {
                dev_err(dev, "Failed to set irq low\n");
                return -ENODEV;
        }
        return count;
}

static DEVICE_ATTR_RW(irq_mask_low);

static ssize_t pps_show(struct device *dev,
                        struct device_attribute *attr,
                        char *buf)
{
        int ret = get_test_mode(dev_get_drvdata(dev));
        if (ret < 0) {
                dev_err(dev, "Failed to get fpga test mode\n");
                return -ENODEV;
        }
        return sprintf(buf, "PPS Alignment: %s\n",
                       ((ret & FPGA_TEST_MODE_DEFAULT) >> 4) ?
                       "enabled" : "disabled");
}

static ssize_t pps_store(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf,
                               size_t count)
{
        int pps;
        int ret;
        sscanf(buf,"%d",&pps);
        pr_debug("pps: %d\n", pps);

        if (pps == 0) {
                ret = set_pps_disable(dev_get_drvdata(dev));
                if (ret < 0) {
                        dev_err(dev, "Failed to get disable pps\n");
                }
        }
        else  {
                ret = set_pps_enable(dev_get_drvdata(dev));
                if (ret < 0) {
                        dev_err(dev, "Failed to get enable pps\n");
                }
        }
        return count;
}

static DEVICE_ATTR_RW(pps);

static ssize_t interface_config_show(struct device *dev,
                                     struct device_attribute *attr,
                                     char *buf)
{
        int ret;

        ret = ad7768_get_interface_mode(dev_get_drvdata(dev));
        if (ret < 0) {
                dev_err(dev, "Failed to get ad7768 interface mode\n");
                return -ENODEV;
        }
        return sprintf(buf, "%u\n", ret);
}

static DEVICE_ATTR_RO(interface_config);

static ssize_t adc_reg_show(struct device *dev,
                            struct device_attribute *attr,
                            char *buf)
{
        int ret, i;

        char buf_l[512] = {0};
        struct fpga_data *pd = dev_get_drvdata(dev);

        for (i = 0; i < 0x0a; i++)
        {
                ret = ad7768_read_register(pd, i);
                if (ret < 0) {
                        dev_err(dev, "Failed to get ad7768 register %02x\n", i);
                        return -ENODEV;
                }
                sprintf(buf_l + strlen(buf_l), " reg_%02x: %02x", i, ret);
        }
        return sprintf(buf, "%s\n", buf_l);
}

static DEVICE_ATTR_RO(adc_reg);

static ssize_t adc_channel_standby_show(struct device *dev,
                                        struct device_attribute *attr,
                                        char *buf)
{
        int ret;

        ret = ad7768_get_channel_standby(dev_get_drvdata(dev));
        if (ret < 0) {
                dev_err(dev, "Failed to get ad7768 power mode\n");
                return -ENODEV;
        }
        return sprintf(buf, "%02x\n", ret);
}

static ssize_t adc_channel_standby_store(struct device *dev,
                                         struct device_attribute *attr,
                                         const char *buf,
                                         size_t count)
{
        int standby, ret;

        sscanf(buf,"%d", &standby);
        pr_debug("standby value: %02x\n", standby);

        if (standby >= 0) {
                ret = ad7768_set_channel_standby(dev_get_drvdata(dev), standby);
                if (ret < 0) {
                        dev_err(dev, "Failed to set ad7768 channel standby\n");
                        return -ENODEV;
                }
        }
        else  {
                dev_err(dev, "Failed to set ad7768 filter type\n");
                return -ENODEV;
        }
        return count;
}

static DEVICE_ATTR_RW(adc_channel_standby);

static ssize_t overflow_high_show(struct device *dev,
                                 struct device_attribute *attr,
                                 char *buf)
{
        int ret = get_ch_overflow_hi(dev_get_drvdata(dev));
        if (ret < 0) {
                dev_err(dev, "Failed to get fpga ch overflow high\n");
                return -ENODEV;
        }
        return sprintf(buf, "%02x\n", ret);
}

static DEVICE_ATTR_RO(overflow_high);

static ssize_t overflow_low_show(struct device *dev,
                                 struct device_attribute *attr,
                                 char *buf)
{
        int ret = get_ch_overflow_low(dev_get_drvdata(dev));
        if (ret < 0) {
                dev_err(dev, "Failed to get fpga ch overflow low\n");
                return -ENODEV;
        }
        return sprintf(buf, "%02x\n", ret);
}

static DEVICE_ATTR_RO(overflow_low);

static ssize_t underflow_high_show(struct device *dev,
                                   struct device_attribute *attr,
                                   char *buf)
{
        int ret = get_ch_underflow_hi(dev_get_drvdata(dev));
        if (ret < 0) {
                dev_err(dev, "Failed to get fpga ch underflow high\n");
                return -ENODEV;
        }
        return sprintf(buf, "%02x\n", ret);
}

static DEVICE_ATTR_RO(underflow_high);

static ssize_t underflow_low_show(struct device *dev,
                                  struct device_attribute *attr,
                                  char *buf)
{
        int ret = get_ch_underflow_low(dev_get_drvdata(dev));
        if (ret < 0) {
                dev_err(dev, "Failed to get fpga ch underflow low\n");
                return -ENODEV;
        }
        return sprintf(buf, "%02x\n", ret);
}

static DEVICE_ATTR_RO(underflow_low);

struct attribute *fpga_attrs[] = {
	&dev_attr_id.attr,
        &dev_attr_test_mode.attr,
        &dev_attr_cfg_cfg.attr,
        &dev_attr_pps.attr,
        &dev_attr_soft_reset.attr,
        &dev_attr_slices_enable.attr,
        &dev_attr_window_size.attr,
        &dev_attr_irq_offset.attr,
        &dev_attr_fpga_stat.attr,
        &dev_attr_fpga_reset.attr,
        &dev_attr_sampling_freq.attr,
        &dev_attr_power_mode.attr,
        &dev_attr_filter_type.attr,
        &dev_attr_adc_revision.attr,
        &dev_attr_write_reg.attr,
        &dev_attr_read_reg.attr,
        &dev_attr_adc_reg.attr,
        &dev_attr_irq_mask_high.attr,
        &dev_attr_irq_mask_low.attr,
        &dev_attr_interface_config.attr,
        &dev_attr_adc_reset.attr,
        &dev_attr_adc_channel_standby.attr,
        &dev_attr_overflow_high.attr,
        &dev_attr_overflow_low.attr,
        &dev_attr_underflow_high.attr,
        &dev_attr_underflow_low.attr,
        &dev_attr_pps_dbg.attr,
	NULL,
};

const struct attribute_group fpga_attr_group = {
	.attrs = fpga_attrs,
};

static const struct attribute_group *fpga_attr_groups[] = {
        &fpga_attr_group,
        NULL,
};

static int ecp5_spi_cmd_a(struct spi_device *spi_dev, unsigned char cmd,
                          unsigned int *val)
{
        unsigned char rx[8] = {0};
        unsigned char tx[8] = {0};

        struct spi_transfer t[] = {
                {
                        .tx_buf = &tx,
                        .len = 8,
                        .cs_change = 0, /* do not hold low */
                        .bits_per_word = 8,
                        .rx_buf = &rx,
                },
        };

        int ret;

        tx[0] = cmd;

        ret = spi_sync_transfer(spi_dev, t, ARRAY_SIZE(t));
        if (ret < 0) {
                pr_debug("Read: Failed to send in tx %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
                         tx[0], tx[1], tx[2], tx[3], tx[4], tx[5], tx[6], tx[7]);
                return ret;
        }
        pr_debug("ecp5: Read: rx: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
                 rx[0], rx[1], rx[2], rx[3], rx[4], rx[5], rx[6], rx[7]);

        *val = get_unaligned_be32(&rx[4]);

        return ret;
}

static int ecp5_spi_cmd_c(struct spi_device *spi_dev, unsigned char cmd)
{
        uint32_t tx = cmd;

        return spi_write(spi_dev, &tx, sizeof(tx));
}

static int ecp5_spi_write_fw_stream(struct spi_device *spi_dev,
                                    const u8 * const data, unsigned int size)
{
        int rc;
        u8 *ptr = NULL;
        uint32_t burst = CMD_LSC_BITSTREAM_BURST;

        ptr = kzalloc(size + sizeof(burst), GFP_KERNEL);
        if (!ptr) {
                pr_err("NO MEM ENOMEM\n");
                return -ENOMEM;
        }

        memcpy(ptr, &burst, sizeof(burst));
        memcpy(ptr + sizeof(burst), data, size);

        rc = spi_write(spi_dev, (void*)ptr, size + sizeof(burst));
        kfree(ptr);
        return rc;
}

/* for cmd_clear and cmd_refresh */
static int __maybe_unused ecp5_spi_cmd_d(struct spi_device *spi_dev, unsigned char cmd)
{
        uint32_t tx = cmd;

        if (spi_write(spi_dev, &tx, sizeof(tx)) != 0) {
                pr_err("Write: Failed to write: %02x%02x%02x%02x\n",
                       ((uint8_t *)&tx)[0], ((uint8_t *)&tx)[1],
                 ((uint8_t *)&tx)[2], ((uint8_t *)&tx)[3]);
                return -1;
        }

        msleep(20);
        return 0;
}

static int fpga_spi_reg_read(const struct fpga_data *pd, unsigned int addr,
                             unsigned char *val)
{
        uint16_t rx = 0x00;
        uint16_t tx = cpu_to_be16((AD7768_WR_FLAG_MSK(addr) << 8));

        struct spi_transfer t[] = {
                {
                        .tx_buf = &tx,
                        .len = 2,
                        .cs_change = 0, /* do not hold low */
                        .bits_per_word = 8,
                        .rx_buf = &rx,
                },
        };

        int ret;

        pr_debug("Read: Data to send in tx %04x\n", tx);

        ret = spi_sync_transfer(pd->spi_cfg, t, ARRAY_SIZE(t));
        if (ret < 0)
                return ret;

        pr_debug("Read: Data received: %02x%02x\n", ((uint8_t *)&rx)[0], ((uint8_t *)&rx)[1]);

        *val = be16_to_cpu(rx);

        return ret;
}

static int fpga_spi_reg_write(const struct fpga_data *pd,
                              unsigned int addr,
                              unsigned char val)
{
        uint16_t tx = cpu_to_be16(((addr & 0x7F) << 8) | (0x00ff & val));
        pr_debug("Write: Data to set be_to_cpu : %02x%02x\n",
               ((uint8_t *)&tx)[0], ((uint8_t *)&tx)[1]);

        return spi_write(pd->spi_cfg, &tx, sizeof(tx));
}

static int fpga_spi_write_mask(const struct fpga_data *pd,
                               unsigned int addr,
                               unsigned long int mask,
                               unsigned char val)
{
        unsigned char regval;
        int ret;

        ret = fpga_spi_reg_read(pd, addr, &regval);
        if (ret < 0) {
                pr_err("%s: Failed to read %02x\n", __FUNCTION__, addr);
                return ret;
        }
        regval &= ~mask;
        regval |= val;

        return fpga_spi_reg_write(pd, addr, regval);
}

static int ad7768_spi_reg_read(struct fpga_data *pd, unsigned int addr,
                               unsigned int *val)
{
        struct spi_transfer t[] = {
                {
                        .tx_buf = &pd->d16,
                        .len = 2,
                        .cs_change = 1, /* hold low */
                        .bits_per_word = 8,
                }, {
                        .rx_buf = &pd->d16,
                        .len = 2,
                        .bits_per_word = 8,
                },
        };

        int ret;

        pd->d16 = cpu_to_be16((AD7768_WR_FLAG_MSK(addr) << 8));
        ((unsigned char*)&pd->d16)[1] = 0xaa;

        pr_debug("ad7768_spi_reg_read: Data to send pd->d16 addr %02x val: %02x\n",
                0x7f & ((unsigned char*)&pd->d16)[0], ((unsigned char*)&pd->d16)[1]);

        ret = spi_sync_transfer(pd->spi_cfg, t, ARRAY_SIZE(t));
        if (ret < 0)
                return ret;

        *val = be16_to_cpu(pd->d16);
        pr_debug("ad7768_spi_reg_read: Data recieved %04x\n", *val);

        return ret;
}

static int ad7768_spi_reg_write(struct fpga_data *pd,
                                unsigned int addr,
                                unsigned int val)
{
        pd->d16 = cpu_to_be16(((addr & 0x7F) << 8) | val);

        pr_debug("ad7768_spi_reg_write:  addr: %02x val: %02x\n",
                 ((unsigned char*)&pd->d16)[0], ((unsigned char*)&pd->d16)[1]);
        return spi_write(pd->spi_cfg, &pd->d16, sizeof(pd->d16));
}

static int ad7768_spi_write_mask(struct fpga_data *pd,
                                 unsigned int addr,
                                 unsigned long int mask,
                                 unsigned int val)
{
        unsigned int regval;
        int ret;
        short local_mask = ~mask;

        pr_debug("%s\n", __FUNCTION__);
        ret = ad7768_spi_reg_read(pd, addr, &regval);
        if (ret < 0)
                return ret;

        pr_debug("write mask: to %02x data received %04x, mask %02x\n", addr, regval, local_mask);
        regval &= ~mask;
        pr_debug("write mask: data masked %04x, mask %02x \n", regval, local_mask);
        regval |= val;
        pr_debug("write mask: data to write masked %02x, mask %02x value %02x\n", regval, local_mask, val);

        return ad7768_spi_reg_write(pd, addr, regval);
}

int get_id(struct fpga_data *pd)
{
        unsigned char regval;
        int ret;

        mutex_lock(&pd->lock);
        ret = fpga_spi_reg_read(pd, FPGA_ID, &regval);
        mutex_unlock(&pd->lock);
        if (ret < 0) {
                pr_err( "Failed to read FPGA ID\n");
                return ret;
        }
        pr_debug("FPGA ID = 0x%02x\n", regval);
        return regval;
}

int get_window_size(struct fpga_data *pd)
{
        unsigned char regval;
        int ret;

        mutex_lock(&pd->lock);
        ret = fpga_spi_reg_read(pd, FPGA_WINDOW_SIZE, &regval);
        mutex_unlock(&pd->lock);

        if (ret < 0) {
                pr_err( "Failed to read FPGA window size\n");
                return ret;
        }

        pr_debug( "FPGA window size = 0x%02x\n", regval);
        return regval;
}

int set_window_size(struct fpga_data *pd, uint8_t size)
{
        int rc;

        pr_debug( "SET FPGA window size = %u\n", size);
        mutex_lock(&pd->lock);
        rc = fpga_spi_reg_write(pd, FPGA_WINDOW_SIZE, size);
        mutex_unlock(&pd->lock);

        return rc;
}

int get_irq_offset(struct fpga_data *pd)
{
        unsigned char regval;
        int ret;

        mutex_lock(&pd->lock);
        ret = fpga_spi_reg_read(pd, FPGA_IRQ_OFFSET, &regval);
        mutex_unlock(&pd->lock);

        if (ret < 0) {
                pr_err( "Failed to read irq offset\n");
                return ret;
        }
        pr_debug( "FPGA irq offset: 0x%02x\n", regval);
        return regval;
}

int set_irq_offset(struct fpga_data *pd, uint8_t offset)
{
        int rc;

        pr_debug( "Set irq offset: %u\n", offset);

        mutex_lock(&pd->lock);
        rc = fpga_spi_reg_write(pd, FPGA_IRQ_OFFSET, offset);
        mutex_unlock(&pd->lock);

        return rc;
}

int get_ch_irq_mask_hi(struct fpga_data *pd)
{
        unsigned char regval;
        int rc;

        mutex_lock(&pd->lock);
        rc = fpga_spi_reg_read(pd, FPGA_IRQ_MSK_HI, &regval);
        mutex_unlock(&pd->lock);
        if (rc < 0) {
                pr_err( "Failed to read FPGA irq mask high\n");
                return rc;
        }
        pr_debug( "FPGA irq mask high: 0x%02x\n", regval);
        return regval;
}

int set_ch_irq_mask_hi(struct fpga_data *pd, uint8_t mask)
{
        int rc;

        pr_debug( "SET FPGA irq mask high: %u\n", mask);
        mutex_lock(&pd->lock);
        rc = fpga_spi_reg_write(pd, FPGA_IRQ_MSK_HI, mask);
        mutex_unlock(&pd->lock);
        return rc;
}

int get_ch_irq_mask_low(struct fpga_data *pd)
{
        unsigned char regval;
        int rc;

        mutex_lock(&pd->lock);
        rc = fpga_spi_reg_read(pd, FPGA_IRQ_MSK_LOW, &regval);
        mutex_unlock(&pd->lock);
        if (rc < 0) {
                pr_err( "Failed to read FPGA irq mask low\n");
                return rc;
        }
        pr_debug( "FPGA irq mask low: 0x%02x\n", regval);
        return regval;
}

int set_ch_irq_mask_low(struct fpga_data *pd, uint8_t mask)
{
        int rc;

        pr_debug( "SET FPGA irq mask low: %u\n", mask);
        mutex_lock(&pd->lock);
        rc = fpga_spi_reg_write(pd, FPGA_IRQ_MSK_LOW, mask);
        mutex_unlock(&pd->lock);
        return rc;
}

int get_ch_overflow_hi(struct fpga_data *pd)
{
        unsigned char regval;
        int rc;

        mutex_lock(&pd->lock);
        rc = fpga_spi_reg_read(pd, FPGA_OVERFLOW_HI, &regval);
        mutex_unlock(&pd->lock);
        if (rc < 0) {
                pr_err( "Failed to read FPGA ch overflow high\n");
                return rc;
        }
        pr_debug( "FPGA underflow high: 0x%02x\n", regval);
        return regval;
}

int get_ch_overflow_low(struct fpga_data *pd)
{
        unsigned char regval;
        int rc;

        mutex_lock(&pd->lock);
        rc = fpga_spi_reg_read(pd, FPGA_OVERFLOW_LOW, &regval);
        mutex_unlock(&pd->lock);

        if (rc < 0) {
                pr_err( "Failed to read FPGA ch overlfow low\n");
                return rc;
        }
        pr_debug( "FPGA underflow low: 0x%02x\n", regval);
        return regval;
}

int get_ch_underflow_hi(struct fpga_data *pd)
{
        unsigned char regval;
        int ret;

        mutex_lock(&pd->lock);
        ret = fpga_spi_reg_read(pd, FPGA_UNDERFLOW_HI, &regval);
        mutex_unlock(&pd->lock);

        if (ret < 0) {
                pr_err( "Failed to read FPGA ch underflow high\n");
                return ret;
        }
        pr_debug( "FPGA underflow high: 0x%02x\n", regval);
        return regval;
}

int get_ch_underflow_low(struct fpga_data *pd)
{
        unsigned char regval;
        int ret;

        mutex_lock(&pd->lock);
        ret = fpga_spi_reg_read(pd, FPGA_UNDERFLOW_LOW, &regval);
        mutex_unlock(&pd->lock);

        if (ret < 0) {
                pr_err( "Failed to read FPGA ch underflow low\n");
                return ret;
        }
        pr_debug( "FPGA underflow low: 0x%02x\n", regval);
        return regval;
}

int get_test_mode(const struct fpga_data *pd)
{
        unsigned char regval;
        int ret;

        ret = fpga_spi_reg_read(pd, FPGA_TEST_MODE, &regval);
        if (ret < 0) {
                pr_err( "Failed to read FPGA irq mask low\n");
                return ret;
        }
        pr_debug( "FPGA test mode: %lu, PPS alignment %lu\n",
                 regval & (FPGA_TEST_MODE2 | FPGA_TEST_MODE1),
                 (regval & FPGA_TEST_MODE_DEFAULT) >> 4);
        return regval;
}

int set_test_mode1(const struct fpga_data *state)
{
        pr_debug( "SET FPGA test mode1: %lu\n", FPGA_TEST_MODE1);

        /* PPS aligment is to be disabled */
        return fpga_spi_reg_write(state, FPGA_TEST_MODE, FPGA_TEST_MODE1);
}

int set_test_mode2(const struct fpga_data *state)
{
        pr_debug( "SET FPGA test mode1: %lu\n", FPGA_TEST_MODE2);
        /* PPS aligment is to be disabled */
        return fpga_spi_reg_write(state, FPGA_TEST_MODE, FPGA_TEST_MODE2);
}

int set_test_mode_disable(const struct fpga_data *state)
{
        pr_debug( "SET FPGA test mode disable: %lu\n", FPGA_TEST_MODE_DEFAULT);
        /* PPS aligment is to be enabled */
        return fpga_spi_reg_write(state, FPGA_TEST_MODE, FPGA_TEST_MODE_DEFAULT);
}

int set_pps_enable(const struct fpga_data *state)
{
        /* struct fpga_data *pd = get_data(); */
        pr_debug( "SET FPGA PPS enable: %01lx\n", FPGA_TEST_MODE_PPS_SET);
        /* PPS aligment to be enabled */
        return fpga_spi_write_mask(state, FPGA_TEST_MODE, FPGA_TEST_MODE_PPS_MSK, FPGA_TEST_MODE_PPS_SET);
}

int set_pps_disable(const struct fpga_data *state)
{
        pr_debug( "SET FPGA PPS disable: %01lx\n", FPGA_TEST_MODE_PPS_UNSET);
        /* PPS aligment is to be disabled, normal mode enabled */
        return fpga_spi_write_mask(state, FPGA_TEST_MODE, FPGA_TEST_MODE_PPS_MSK, FPGA_TEST_MODE_PPS_UNSET);
}

int get_stat(struct fpga_data *pd)
{
        unsigned char regval;
        int ret;

        mutex_lock(&pd->lock);
        if (pd->cfg_mode != FPGA_CFG_MODE_CFG_NORMAL) {
                mutex_unlock(&pd->lock);
                return -EAGAIN;
        }

        ret = fpga_spi_reg_read(pd, FPGA_STAT, &regval);
        mutex_unlock(&pd->lock);

        if (ret < 0) {
                pr_err( "Failed to read FPGA irq mask low\n");
                return ret;
        }
        return regval;
}

int clear_stat(struct fpga_data *pd)
{
        unsigned char regval;
        int ret;

        mutex_lock(&pd->lock);

        if (pd->cfg_mode != FPGA_CFG_MODE_CFG_NORMAL) {
                mutex_unlock(&pd->lock);
                return -EAGAIN;
        }

        ret = fpga_spi_reg_write(pd, FPGA_STAT, 0);

        mutex_unlock(&pd->lock);
        if (ret < 0) {
                pr_err( "Failed to write FPGA stat clear\n");
                return ret;
        }
        return regval;
}

int get_soft_reset(struct fpga_data *pd)
{
        unsigned char regval;
        int ret;

        mutex_lock(&pd->lock);

        if (pd->cfg_mode != FPGA_CFG_MODE_CFG_NORMAL) {
                mutex_unlock(&pd->lock);
                return -EAGAIN;
        }

        ret = fpga_spi_reg_read(pd, FPGA_SOFT_RESET, &regval);

        mutex_unlock(&pd->lock);
        if (ret < 0) {
                pr_err( "Failed to read FPGA soft reset\n");
                return ret;
        }

        pr_debug( "FPGA GET SOFT RESET = 0x%02lx\n", regval & FPGA_SOFT_RESET_MSK);
        return (regval & FPGA_SOFT_RESET_MSK);
}

int assert_soft_reset(struct fpga_data *pd)
{
        int rc;

        mutex_lock(&pd->lock);
        if (pd->cfg_mode != FPGA_CFG_MODE_CFG_NORMAL) {
                mutex_unlock(&pd->lock);
                return -EAGAIN;
        }

        rc = fpga_spi_write_mask(pd, FPGA_SOFT_RESET, FPGA_SOFT_RESET_MSK, FPGA_SOFT_RESET_SET);
        mutex_unlock(&pd->lock);

        return rc;
}

int release_soft_reset(struct fpga_data *pd)
{
        int rc;
        mutex_lock(&pd->lock);
        if (pd->cfg_mode != FPGA_CFG_MODE_CFG_NORMAL) {
                mutex_unlock(&pd->lock);
                return -EAGAIN;
        }

        rc = fpga_spi_write_mask(pd, FPGA_SOFT_RESET, FPGA_SOFT_RESET_MSK, FPGA_SOFT_RESET_RELEASE);
        mutex_unlock(&pd->lock);

        return rc;
}

int get_slices_enabled(struct fpga_data *pd)
{
        unsigned char regval;
        int ret;

        mutex_lock(&pd->lock);

        if (pd->cfg_mode != FPGA_CFG_MODE_CFG_NORMAL) {
                mutex_unlock(&pd->lock);
                return -EAGAIN;
        }

        ret = fpga_spi_reg_read(pd, FPGA_SOFT_RESET, &regval);
        mutex_unlock(&pd->lock);

        if (ret < 0) {
                pr_err( "Failed to read FPGA soft reset\n");
                return ret;
        }

        pr_debug( "FPGA soft reset register = 0x%02x\n", regval >> 4);
        return (regval >> 4);
}

int set_slices_enabled(struct fpga_data *pd, unsigned char slices_enabled)
{
        int rc;
        unsigned char regval;

        mutex_lock(&pd->lock);

        if (pd->cfg_mode != FPGA_CFG_MODE_CFG_NORMAL) {
                mutex_unlock(&pd->lock);
                return -EAGAIN;
        }

        rc = fpga_spi_reg_read(pd, FPGA_SOFT_RESET, &regval);
        if (rc < 0) {
                pr_err( "Failed to read FPGA soft reset\n");
                mutex_unlock(&pd->lock);
                return rc;
        }
        pr_debug( "slices enable read 0x%02x\n", regval);

        if (!(regval & 0x1)) {
                pr_err( "Soft reset is deasserted, can't change the slice enabled\n");
                mutex_unlock(&pd->lock);
                return -EAGAIN;
        }

        pr_debug( "slices enable write with mask  val: %02x\n", slices_enabled);
        pr_debug( "slices enable write with mask  val << 4: %02x\n", slices_enabled << 4);
        rc = fpga_spi_write_mask(pd, FPGA_SOFT_RESET, FPGA_SOFT_RESET_SLICE_MSK, 0);

        if (rc < 0) {
                pr_err( "Failed to write FPGA soft reset\n");
                mutex_unlock(&pd->lock);
                return rc;
        }

        rc = fpga_spi_write_mask(pd, FPGA_SOFT_RESET, FPGA_SOFT_RESET_SLICE_MSK, slices_enabled << 4);
        mutex_unlock(&pd->lock);

        return rc;
}

int set_cfg_adc0(struct fpga_data *pd)
{
        mutex_lock(&pd->lock);
        if (pd->cfg_mode != FPGA_CFG_MODE_CFG_ADC0) {
                int ret = fpga_spi_reg_write(pd, FPGA_CFG_MODE,
                                             FPGA_CFG_MODE_CFG_ADC0 | FPGA_CFG_MODE_CFG_SPI);
                if (ret < 0) {
                        pr_err("Failed to set cfg mode adc0\n");
                        mutex_unlock(&pd->lock);
                        return ret;
                }
                pd->cfg_mode = FPGA_CFG_MODE_CFG_ADC0;

        }
        else {
                pr_debug("Already in cfg mode adc0\n");
        }

        mutex_unlock(&pd->lock);
        return 0;
}

int set_cfg_adc1(struct fpga_data *pd)
{
        mutex_lock(&pd->lock);
        if (pd->cfg_mode != FPGA_CFG_MODE_CFG_ADC1) {
                int ret = fpga_spi_reg_write(pd, FPGA_CFG_MODE,
                                             FPGA_CFG_MODE_CFG_ADC1 | FPGA_CFG_MODE_CFG_SPI);
                if (ret < 0) {
                        pr_err("Failed to set cfg mode adc1\n");
                        mutex_unlock(&pd->lock);
                        return ret;
                }
                pd->cfg_mode = FPGA_CFG_MODE_CFG_ADC1;

        }
        else {
                pr_debug("Already in cfg mode adc1\n");
        }
        mutex_unlock(&pd->lock);
        return 0;
}

int set_cfg_normal(struct fpga_data *pd)
{
        mutex_lock(&pd->lock);
        if (pd->cfg_mode != FPGA_CFG_MODE_CFG_NORMAL) {
                int ret = fpga_spi_reg_write(pd, FPGA_CFG_MODE,
                                             FPGA_CFG_MODE_CFG_NORMAL | FPGA_CFG_MODE_CFG_SPI);
                if (ret < 0) {
                        pr_err("Failed to set cfg mode normal\n");
                        mutex_unlock(&pd->lock);
                        return ret;
                }
                pd->cfg_mode = FPGA_CFG_MODE_CFG_NORMAL;

        }
        else {
                pr_debug("Already in cfg mode normal\n");
        }
        mutex_unlock(&pd->lock);
        return 0;
}

int get_adc_reset(struct fpga_data *pd)
{
        unsigned char regval;
        int ret;

        if (pd->cfg_mode != FPGA_CFG_MODE_CFG_NORMAL) {
                return -EAGAIN;
        }

        ret = fpga_spi_reg_read(pd, FPGA_SOFT_RESET, &regval);
        if (ret < 0) {
                pr_err( "Failed to read FPGA soft reset\n");
                return ret;
        }

        pr_debug( "FPGA get adc reset = 0x%02lx\n", regval & FPGA_ADC_RESET_MSK);
        return regval & FPGA_ADC_RESET_MSK;
}

int adc_reset(struct fpga_data *pd, uint32_t reset)
{
        int rc;
        unsigned char regval;

        mutex_lock(&pd->lock);
        if (pd->cfg_mode != FPGA_CFG_MODE_CFG_NORMAL) {
                mutex_unlock(&pd->lock);
                return -EAGAIN;
        }

        rc = fpga_spi_reg_read(pd, FPGA_SOFT_RESET, &regval);
        if (rc < 0) {
                pr_err( "Failed to read FPGA soft reset\n");
                mutex_unlock(&pd->lock);
                return rc;
        }

        if (!(regval & 0x1)) {
                mutex_unlock(&pd->lock);
                pr_err( "Soft reset is deasserted, can't change the adc reset\n");
                return -EAGAIN;
        }

        pr_debug( "adc reset write with mask value: 0x%02x\n", reset);
        rc = fpga_spi_write_mask(pd, FPGA_SOFT_RESET, FPGA_ADC_RESET_MSK, reset);
        mutex_unlock(&pd->lock);

        return rc;
}

int adc_reset_assert(struct fpga_data *pd)
{
        return adc_reset(pd, FPGA_ADC_RESET_SET);
}

int adc_reset_deassert(struct fpga_data *pd)
{
        return adc_reset(pd, FPGA_ADC_RESET_RELEASE);
}

int get_pps_data(struct fpga_data *pd, struct fpga_pps_dbg *data)
{
        unsigned char regval;
        int ret;

        mutex_lock(&pd->lock);
        if (pd->cfg_mode != FPGA_CFG_MODE_CFG_NORMAL) {
                mutex_unlock(&pd->lock);
                return -EAGAIN;
        }

        memset(data, 0, sizeof(struct fpga_pps_dbg));

        ret = fpga_spi_reg_read(pd, FPGA_PPS_PHASE_OFFSET, &regval);
        if (ret < 0) {
                pr_err( "Failed to read FPGA pps phase offset\n");
                mutex_unlock(&pd->lock);
                return ret;
        }

        data->pps_phase_offset = (int8_t)regval;

        ret = fpga_spi_reg_read(pd, FPGA_FREQ_MONITOR_DELTA, &regval);
        if (ret < 0) {
                pr_err( "Failed to read FPGA freq monitor delta\n");
                mutex_unlock(&pd->lock);
                return ret;
        }

        data->freq_monitor_delta = (int8_t)regval;

        ret = fpga_spi_reg_read(pd, FPGA_SYNC_ERROR_3, &regval);
        if (ret < 0) {
                pr_err( "Failed to read FPGA sync error 3\n");
                mutex_unlock(&pd->lock);
                return ret;
        }

        data->slice_3_err = (int8_t)regval;

        ret = fpga_spi_reg_read(pd, FPGA_SYNC_ERROR_2, &regval);
        if (ret < 0) {
                pr_err( "Failed to read FPGA sync error 2\n");
                mutex_unlock(&pd->lock);
                return ret;
        }

        data->slice_2_err = (int8_t)regval;

        ret = fpga_spi_reg_read(pd, FPGA_SYNC_ERROR_1, &regval);
        if (ret < 0) {
                pr_err( "Failed to read FPGA sync error 1\n");
                mutex_unlock(&pd->lock);
                return ret;
        }

        data->slice_1_err = (int8_t)regval;

        ret = fpga_spi_reg_read(pd, FPGA_SYNC_ERROR_0, &regval);
        if (ret < 0) {
                pr_err( "Failed to read FPGA sync error 0\n");
                mutex_unlock(&pd->lock);
                return ret;
        }

        data->slice_0_err = (int8_t)regval;

        ret = fpga_spi_reg_read(pd, FPGA_FREQ_ERROR_TRH, &regval);
        if (ret < 0) {
                pr_err( "Failed to read FPGA freq error threshold\n");
                mutex_unlock(&pd->lock);
                return ret;
        }

        data->freq_err_threshold = regval;

        ret = fpga_spi_reg_read(pd, FPGA_SYNC_ERROR_TRH, &regval);
        if (ret < 0) {
                pr_err( "Failed to read FPGA sync error threshold\n");
                mutex_unlock(&pd->lock);
                return ret;
        }

        data->sync_err_threshold = regval;

        mutex_unlock(&pd->lock);
        return ret;
}

static int ad7768_sync(struct fpga_data *pd)
{
        int ret;

        if (pd->cfg_mode == FPGA_CFG_MODE_CFG_NORMAL) {
                pr_err("Failed to sync ad7768, fpga is in normal mode\n");
                return -EAGAIN;
        }

        ret = ad7768_spi_write_mask(pd, AD7768_DATA_CONTROL,
                                    AD7768_DATA_CONTROL_SPI_SYNC_MSK,
                                    AD7768_DATA_CONTROL_SPI_SYNC_CLEAR);
        if (ret < 0) {
                pr_err("Failed to sync clear\n");
                return ret;
        }

        return ad7768_spi_write_mask(pd,  AD7768_DATA_CONTROL,
                                     AD7768_DATA_CONTROL_SPI_SYNC_MSK,
                                     AD7768_DATA_CONTROL_SPI_SYNC);
}

static int ad7768_set_clk_divs(struct fpga_data *pd,
                               unsigned int mclk_div,
                               unsigned int freq)
{
        unsigned int mclk, dclk_div, dec, div;
        unsigned int result = 0;
        int ret = 0;

        mclk = pd->clock_rate;
        pr_debug("clock rate %u\n", mclk);

        for (dclk_div = 0; dclk_div < 4 ; dclk_div++) {
                for (dec = 0; dec < ARRAY_SIZE(ad7768_dec_rate); dec++) {
                        div = mclk_div *
                                (1 <<  (3 - dclk_div)) *
                                ad7768_dec_rate[dec];

                        result = DIV_ROUND_CLOSEST_ULL(mclk, div);
                        if (freq == result)
                                break;
                }
        }

        if (freq != result) {
                pr_err("freq != result, %u != %u\n", freq, result);
                return -EINVAL;
        }
        pr_debug("clock div: %u\n", dclk_div);

        ret = ad7768_spi_write_mask(pd, AD7768_INTERFACE_CFG,
                                    AD7768_INTERFACE_CFG_DCLK_DIV_MSK,
                                    /* AD7768_INTERFACE_CFG_DCLK_DIV_MODE(3 - dclk_div)); */
                                    AD7768_INTERFACE_CFG_DCLK_DIV_MODE(3));
        if (ret < 0)
                return ret;

        ret = ad7768_spi_write_mask(pd, AD7768_CH_MODE,
                                     AD7768_CH_MODE_DEC_RATE_MSK,
                                     AD7768_CH_MODE_DEC_RATE_MODE(dec));
        if (ret < 0) {
                pr_err("Failed to set decimation rate on ch mode A : %u\n", AD7768_CH_MODE_DEC_RATE_MODE(dec));
                return ret;
        }
        return ad7768_spi_write_mask(pd, AD7768_CH_MODE_B,
                                     AD7768_CH_MODE_DEC_RATE_MSK,
                                     AD7768_CH_MODE_DEC_RATE_MODE(dec));
}

int ad7768_set_power_mode(struct fpga_data *pd, unsigned int mode)
{
        unsigned int regval;
        int ret;
        /* Check if this mode supports the current sampling rate */

        mutex_lock(&pd->lock);

        if (pd->cfg_mode == FPGA_CFG_MODE_CFG_NORMAL) {
                pr_err("Failed to set ad7768 power mode, fpga is in normal mode\n");
                mutex_unlock(&pd->lock);
                return -EINVAL;
        }

        if (pd->sampling_freq > ad7768_sampling_rates[mode][AD7768_MAX_RATE] ||
            pd->sampling_freq < ad7768_sampling_rates[mode][AD7768_MIN_RATE]) {
                pr_err("Failed to set ad7768 power mode, sampling freq is out of range %u\n", pd->sampling_freq);
                mutex_unlock(&pd->lock);
                return -EINVAL;
        }

        regval = AD7768_POWER_MODE_POWER_MODE(mode);
        pr_debug("setting ad7768 power mode 0x%08x\n", regval);
        ret = ad7768_spi_write_mask(pd, AD7768_POWER_MODE,
                                    AD7768_POWER_MODE_POWER_MODE_MSK,
                                    regval);
        if (ret < 0) {
                pr_err("Failed to set ad7768 power mode, spi write mask\n");
                mutex_unlock(&pd->lock);
                return ret;
        }
        /* The values for the powermode correspond for mclk div */
        ret = ad7768_spi_write_mask(pd, AD7768_POWER_MODE,
                                    AD7768_POWER_MODE_MCLK_DIV_MSK,
                                    AD7768_POWER_MODE_MCLK_DIV_MODE(mode));
        if (ret < 0) {
                pr_err("Failed to set ad7768 power mode, spi write div\n");
                mutex_unlock(&pd->lock);
                return ret;
        }

        ret = ad7768_set_clk_divs(pd, ad7768_mclk_divs[mode],
                                  pd->sampling_freq);
        if (ret < 0) {
                mutex_unlock(&pd->lock);
                return ret;
        }

        ret = ad7768_sync(pd);
        if (ret < 0) {
                mutex_unlock(&pd->lock);
                pr_err("Failed to set ad7768 power mode, sync\n");
                return ret;
        }

        pd->power_mode = mode;
        pr_debug("power mode %u\n", mode);

        mutex_unlock(&pd->lock);
        return ret;
}

int ad7768_get_interface_mode(struct fpga_data *pd)
{
        unsigned int regval;
        int ret;

        mutex_lock(&pd->lock);
        if (pd->cfg_mode == FPGA_CFG_MODE_CFG_NORMAL) {
                pr_err("Failed to read ad7768 power mode, fpga is in normal mode\n");
                mutex_unlock(&pd->lock);
                return -EAGAIN;
        }

        ret = ad7768_spi_reg_read(pd, AD7768_INTERFACE_CFG, &regval);
        mutex_unlock(&pd->lock);

        if (ret < 0) {
                pr_err("Failed to read ad7768 interface confguiration\n");
                return ret;
        }
        pr_debug("interface mode: 0x%08x\n", regval);

        return regval;
}

int ad7768_read_register(struct fpga_data *pd, uint8_t reg)
{
        unsigned int regval;
        int ret;

        mutex_lock(&pd->lock);
        if (pd->cfg_mode == FPGA_CFG_MODE_CFG_NORMAL) {
                pr_err("Failed to read ad7768 reg %02x, fpga is in normal mode\n", reg);
                mutex_unlock(&pd->lock);
                return -EAGAIN;
        }

        if (reg > 0x59) {
                mutex_unlock(&pd->lock);
                return -EAGAIN;
        }

        ret = ad7768_spi_reg_read(pd, reg, &regval);
        mutex_unlock(&pd->lock);

        if (ret < 0) {
                pr_err("Failed to read ad7768 register %02x\n", reg);
                return ret;
        }
        pr_debug("reg: 0x%02x value: 0x%02x\n", reg, regval);

        return regval;
}

int ad7768_get_power_mode(struct fpga_data *pd)
{
        unsigned int regval;
        int ret;

        mutex_lock(&pd->lock);
        if (pd->cfg_mode == FPGA_CFG_MODE_CFG_NORMAL) {
                pr_err("Failed to read ad7768 power mode, fpga is in normal mode\n");
                mutex_unlock(&pd->lock);
                return -EAGAIN;
        }

        ret = ad7768_spi_reg_read(pd, AD7768_POWER_MODE, &regval);
        if (ret < 0) {
                pr_err("Failed to read ad7768 power mode\n");
                mutex_unlock(&pd->lock);
                return ret;
        }

        pd->power_mode = AD7768_POWER_MODE_GET_POWER_MODE(regval);

        mutex_unlock(&pd->lock);
        return pd->power_mode;
}

int ad7768_set_filter_type(struct fpga_data *pd, unsigned int filter)
{
        int ret;

        mutex_lock(&pd->lock);

        if (pd->cfg_mode == FPGA_CFG_MODE_CFG_NORMAL) {
                pr_err("Failed to set ad7768 filter type, fpga is in normal mode\n");
                mutex_unlock(&pd->lock);
                return -EAGAIN;
        }

        pr_info("writing filter: addr: %02x, mask %02x, filter type %02x\n",
               AD7768_CH_MODE,
               AD7768_CH_MODE_FILTER_TYPE_MSK,
               AD7768_CH_MODE_FILTER_TYPE_MODE(filter));
        ret = ad7768_spi_write_mask(pd, AD7768_CH_MODE,
                                    AD7768_CH_MODE_FILTER_TYPE_MSK,
                                    AD7768_CH_MODE_FILTER_TYPE_MODE(filter));
        if (ret < 0) {
                pr_err("Failed to set ad7768 filter type, spi write\n");
                mutex_unlock(&pd->lock);
                return ret;
        }

        ret = ad7768_sync(pd);

        mutex_unlock(&pd->lock);
        return ret;
}

int ad7768_get_filter_type(struct fpga_data *pd)
{
        unsigned int filter;
        int ret;

        mutex_lock(&pd->lock);
        if (pd->cfg_mode == FPGA_CFG_MODE_CFG_NORMAL) {
                pr_err("Failed to get ad7768 filter type, fpga is in normal mode\n");
                mutex_unlock(&pd->lock);
                return -EAGAIN;
        }

        ret = ad7768_spi_reg_read(pd, AD7768_CH_MODE, &filter);
        mutex_unlock(&pd->lock);

        if (ret < 0) {
                pr_err("Failed to get ad7768 filter type, spi read\n");
                return ret;
        }

        return AD7768_CH_MODE_GET_FILTER_TYPE(filter);
}

int ad7768_set_channel_standby(struct fpga_data *pd, unsigned char ch_mask)
{
        int ret;

        mutex_lock(&pd->lock);
        if (pd->cfg_mode == FPGA_CFG_MODE_CFG_NORMAL) {
                pr_err("Failed to set ad7768 filter type, fpga is in normal mode\n");
                mutex_unlock(&pd->lock);
                return -EAGAIN;
        }

        pr_err("Writing channel standby: addr: %02x, channel mask %02x\n",
               AD7768_CH_STANDBY,
               ch_mask);
        ret = ad7768_spi_reg_write(pd, AD7768_CH_STANDBY,
                                    ch_mask);
        mutex_unlock(&pd->lock);
        if (ret < 0) {
                pr_err("Failed to set ad7768 channel standby, spi write\n");
                return ret;
        }

        return ad7768_sync(pd);
}

int ad7768_get_channel_standby(struct fpga_data *pd)
{
        unsigned int standby;
        int ret;

        mutex_lock(&pd->lock);
        if (pd->cfg_mode == FPGA_CFG_MODE_CFG_NORMAL) {
                pr_err("Failed to get ad7768 filter type, fpga is in normal mode\n");
                mutex_unlock(&pd->lock);
                return -EAGAIN;
        }

        pr_debug("Reading channel standby: addr: %02x\n",
               AD7768_CH_STANDBY);
        ret = ad7768_spi_reg_read(pd, AD7768_CH_STANDBY, &standby);
        mutex_unlock(&pd->lock);
        if (ret < 0) {
                pr_err("Failed to get ad7768 channel standby, spi read\n");
                return ret;
        }

        return standby;
}

int ad7768_get_revision(struct fpga_data *pd)
{
        unsigned int revision;
        int ret;

        mutex_lock(&pd->lock);
        if (pd->cfg_mode == FPGA_CFG_MODE_CFG_NORMAL) {
                pr_err("Failed to get ad7768 revision, fpga is in normal mode\n");
                mutex_unlock(&pd->lock);
                return -EAGAIN;
        }

        ret = ad7768_spi_reg_read(pd, AD7768_REVISION, &revision);
        mutex_unlock(&pd->lock);

        if (ret < 0) {
                pr_err("Failed to get ad7768 revision, spi read\n");
                return ret;
        }

        return revision;
}

int ad7768_set_sampling_freq(struct fpga_data *pd, unsigned int freq)
{
        int power_mode = -1;
        unsigned int i, j;
        int ret = 0;

        if (!freq)
                return -EINVAL;

        if (pd->cfg_mode == FPGA_CFG_MODE_CFG_NORMAL) {
                pr_err("Failed to set ad7768 sampling freq, fpga is in normal mode\n");
                return -EAGAIN;
        }

        mutex_lock(&pd->lock);

        for (i = 0; i < AD7768_NUM_CONFIGS; i++) {
                for (j = 0; j < AD7768_CONFIGS_PER_MODE; j++) {
                        if (freq == ad7768_sampling_rates[i][j]) {
                                power_mode = i;
                                break;
                        }
                }
        }

        if (power_mode == -1) {
                ret = -EINVAL;
                pr_err("Power mode -1\n");
                goto freq_err;
        }
        else {
                pr_info("Power mode %d\n", power_mode);
        }

        ret = ad7768_set_clk_divs(pd, ad7768_mclk_divs[power_mode], freq);
        if (ret < 0) {
                pr_err("Clock divisor %d\n", power_mode);
                goto freq_err;
        }

        pd->sampling_freq = freq;

        mutex_unlock(&pd->lock);

        /* locking in set power mode */
        ret = ad7768_set_power_mode(pd, power_mode);
        if (ret < 0)
                goto freq_err;
freq_err:

        return ret;
}

static int ad7768_get_sampling_freq(struct fpga_data *pd)
{
        return pd->sampling_freq;
}

int fpga_ecp5_get_id(struct fpga_data *pd)
{
        unsigned char regval;
        int ret;

        mutex_lock(&pd->lock);
        ret = fpga_spi_reg_read(pd, FPGA_ID, &regval);
        mutex_unlock(&pd->lock);

        if (ret < 0) {
                pr_err( "Failed to read FPGA ID\n");
                return ret;
        }
        pr_debug("FPGA ID = 0x%02x\n", regval);
        return regval;
}

static int spi_init_fw (struct platform_device *pdev)
{
        int retries = 0;
        struct device *dev = &pdev->dev;
        struct fpga_data *pd = platform_get_drvdata(pdev);
        uint32_t sleep_step = 1;

        gpiod_set_value(pd->power, 0);
        msleep(2);
        gpiod_set_value(pd->power, 1);

        while (retries < QED_SPI_FPGA_MAX_ECP5_ITER) {
                msleep(sleep_step);
                if (!gpiod_get_value(pd->initn))
                        break;
                ++retries;
        }

        if (retries == QED_SPI_FPGA_MAX_ECP5_ITER) {
                dev_err(dev, "Retries \"INITN pin to low level\" limit reached in %u ms\n",
                        retries * sleep_step);
                return -1;
        }

        gpiod_set_value(pd->programn, 1);

        retries = 0;
        while (retries < QED_SPI_FPGA_MAX_ECP5_ITER) {
                msleep(sleep_step);
                if (gpiod_get_value(pd->initn)) {
                        break;
                }
                ++retries;
        }

        if (retries == QED_SPI_FPGA_MAX_ECP5_ITER) {
                dev_err(dev, "Retries \"INITN pin to high\" limit reached in %u ms\n",
                        retries * sleep_step);
                return -1;
        }

        msleep(5);
        return 0;
}

static int spi_deinit_fw (struct platform_device *pdev)
{
        struct fpga_data *pd = platform_get_drvdata(pdev);
        gpiod_set_value(pd->programn, 0);
        gpiod_set_value(pd->power, 0);
        return 0;
}

static int decode_status (struct device *dev, uint32_t status)
{
        uint32_t mask = 0x01;
        uint32_t mask_3 = 0x07;
        uint8_t shift = 0;
        uint8_t res;
        int rc = 0;

        dev_dbg(dev, "Status:%08x\n",
                 status);
        res = (status >> shift++) & mask;

        if (res)
                dev_dbg(dev, "Status: Transparent Mode: Yes\n");
        else
                dev_dbg(dev, "Status: Transparent Mode: No\n");

        res = ((status >> shift++) & mask_3);

        if (!res)
                dev_dbg(dev, "Status: Config Target Selection: SRAM\n");
        else if (res == 0x1)
                dev_dbg(dev, "Status: Config Target Selection: eFuse\n");
        else
                dev_dbg(dev, "Status: Config Target Selection: UNKNOWN\n");

        shift += 2;
        res = (status >> shift++) & mask;

        if (res)
                dev_dbg(dev, "Status: JTAC Active: Yes\n");
        else
                dev_dbg(dev, "Status: JTAC Active: No\n");

        res = (status >> shift++) & mask;

        if (res)
                dev_dbg(dev, "Status: PWD Protections: Yes\n");
        else
                dev_dbg(dev, "Status: PWD Protections: No\n");

        res = (status >> shift++) & mask;
        if (res)
                dev_dbg(dev, "Status: Internal use 1\n");
        else
                dev_dbg(dev, "Status: Internal use 0\n");

        res = (status >> shift++) & mask;
        if (res)
                dev_dbg(dev, "Status: Decrypt Enable: Yes\n");
        else
                dev_dbg(dev, "Status: Decrypt Enable: No\n");

        res = (status >> shift++) & mask;
        if (res)
                dev_dbg(dev, "Status: DONE: Yes/Set\n");
        else
                dev_dbg(dev, "Status: DONE: No/Not set\n");

        res = (status >> shift++) & mask;
        if (res)
                dev_dbg(dev, "Status: ISC Enabled: Yes\n");
        else
                dev_dbg(dev, "Status: ISC Enabled: No\n");

        res = (status >> shift++) & mask;
        if (res)
                dev_dbg(dev, "Status: Write Enabled: Yes\n");
        else
                dev_dbg(dev, "Status: Write Enabled: No\n");

        res = (status >> shift++) & mask;
        if (res)
                dev_dbg(dev, "Status: Read Enabled: Yes\n");
        else
                dev_dbg(dev, "Status: Read Enabled: No\n");

        res = (status >> shift++) & mask;
        if (res)
                dev_dbg(dev, "Status: Busy Flag: Yes\n");
        else
                dev_dbg(dev, "Status: Busy Flag: No\n");

        res = (status >> shift++) & mask;
        if (res) {
                dev_info(dev, "Status: Fail Flag: Yes\n");
                rc = -1;
        }
        else
                dev_dbg(dev, "Status: Fail Flag: No\n");

        res = (status >> shift++) & mask;
        if (res)
                dev_dbg(dev, "Status: FEA OTP: Yes\n");
        else
                dev_dbg(dev, "Status: FEA OTP: No\n");

        res = (status >> shift++) & mask;
        if (res)
                dev_dbg(dev, "Status: Decrypt Only: Yes\n");
        else
                dev_dbg(dev, "Status: Decrypt Only: No\n");

        res = (status >> shift++) & mask;
        if (res)
                dev_dbg(dev, "Status: PWD Enable: Yes\n");
        else
                dev_dbg(dev, "Status: PWD Enable: No\n");

        res = (status >> shift++) & mask;
        if (res)
                dev_dbg(dev, "Status: Internal use 1\n");
        else
                dev_dbg(dev, "Status: Internal use 0\n");

        res = (status >> shift++) & mask;
        if (res)
                dev_dbg(dev, "Status: Internal use 1\n");
        else
                dev_dbg(dev, "Status: Internal use 0\n");

        res = (status >> shift++) & mask;
        if (res)
                dev_dbg(dev, "Status: Internal use 1\n");
        else
                dev_dbg(dev, "Status: Internal use 0\n");

        res = (status >> shift++) & mask;
        if (res)
                dev_dbg(dev, "Status: Encrypt Preamble: Yes\n");
        else
                dev_dbg(dev, "Status: Encrypt Preamble: No\n");

        res = (status >> shift++) & mask;
        if (res)
                dev_dbg(dev, "Status: Std Preamble: Yes\n");
        else
                dev_dbg(dev, "Status: Std Preamble: No\n");

        res = (status >> shift++) & mask;
        if (res) {
                dev_err(dev, "Status: SPIm Fail 1: Yes\n");
                rc = -1;
        }
        else
                dev_dbg(dev, "Status: SPIm Fail 1: No\n");

        res = (status >> shift++) & mask_3;
        switch (res) {
                case 0:
                        dev_dbg(dev, "Status: BSE status Code: "
                                 "NONE\n");
                        break;
                case 0x1:
                        rc = -1;
                        dev_info(dev, "Status: BSE status Code: "
                                 "ID status\n");
                        break;
                case 0x2:
                        rc = -1;
                        dev_info(dev, "Status: BSE status Code: "
                                 "CMD status: illegal command\n");
                        break;
                case 0x3:
                        rc = -1;
                        dev_info(dev, "Status: BSE status Code: "
                                 "CRC status\n");
                        break;
                case 0x4:
                        rc = -1;
                        dev_info(dev, "Status: BSE status Code: "
                                 "PRMB status - preabmle status\n");
                        break;
                case 0x5:
                        rc = -1;
                        dev_info(dev, "Status: BSE status Code: "
                                 "ABRT status - configuration aborted by the user\n");
                        break;
                case 0x6:
                        rc = -1;
                        dev_info(dev,
                                 "Status: BSE status Code: "
                                 "OVFL status - data overflow status\n");
                        break;
                case 0x7:
                        rc = -1;
                        dev_info(dev,
                                 "Status: BSE status Code: "
                                 "SDM status - bitstream pass the size of the SRAM array\n");
                        break;
                default:
                        rc = -1;
                        dev_info(dev,
                                 "Status: BSE status Code: "
                                 "UNKNOWN\n");
                        break;
        }

        shift += 2;
        res = (status >> shift++) & mask;

        if (res) {
                rc = -1;
                dev_info(dev, "Status: Execution status: Yes\n");
        }
        else
                dev_dbg(dev, "Status: Execution status: No\n");

        res = (status >> shift++) & mask;

        if (res) {
                rc = -1;
                dev_info(dev, "Status: ID status: "
                         "ID mismatch with Verify_ID command: Yes\n");
        }

        res = (status >> shift++) & mask;

        if (res) {
                rc = -1;
                dev_info(dev, "Status: Invalid Command: Yes\n");
        }

        res = (status >> shift++) & mask;

        if (res) {
                rc = -1;
                dev_info(dev, "Status: SED status: Yes\n");
        }

        res = (status >> shift++) & mask;

        if (res)
                dev_dbg(dev, "Status: Bypass Mode: Yes\n");
        else
                dev_dbg(dev, "Status: Bypass Mode: No\n");

        res = (status >> shift++) & mask;

        if (res)
                dev_dbg(dev, "Status: Flow Through Mode: Yes\n");
        else
                dev_dbg(dev, "Status: Flow Through Mode: No\n");

        return rc;
}

static int status_done (uint32_t status)
{
        uint32_t mask = 0x100;
        return !(status & mask);
}

static int firmware_load (struct platform_device *pdev)
{
        struct device *dev = &pdev->dev;
        const struct firmware *fw = NULL;
        const char *fw_name = ECP5_FM;
        struct  spi_master *fw_master;
        unsigned char cmd;
        unsigned int val;

        struct fpga_data *state = platform_get_drvdata(pdev);

        int rc = request_firmware(&fw, fw_name, dev);

        if (rc) {
                dev_err(dev, "Failed request firmware\n");
                return -1;
        }

        rc = spi_init_fw(pdev);

        if (rc) {
                dev_err(dev, "spi init fw failed\n");
                goto err_release_fw;
        }

        fw_master = spi_busnum_to_master(state->fw_info.bus_num);

        if (!fw_master) {
                spi_deinit_fw(pdev);
                rc = -1;
                dev_err(dev, "SPI fw_master not found.\n");
                goto err_release_fw;
        }

        state->spi_fw = spi_new_device(fw_master, &state->fw_info);

        if (!state->spi_fw) {
                spi_deinit_fw(pdev);
                rc = -1;
                dev_err(dev, "Failed to create slave.\n");
                goto err_release_fw;
        }

        do {
                rc = spi_setup(state->spi_fw);
                if (rc) {
                        dev_err(dev, "Failed to setup slave.\n");
                        break;
                }

                cmd = CMD_READ_ID;
                rc = ecp5_spi_cmd_a(state->spi_fw, cmd, &val);

                if (rc) {
                        dev_err(dev, "Failed to send command %02x\n", cmd);
                        break;
                }

                dev_info(dev, "ecp5 id: 0x%08x", val);

                if ((val != LFE5U_45_ID) && (val != LFE5UM_45_ID)) {
                        dev_err(dev,
                                "Read FPGA_ID: 0x%08x does "
                                "not match LFE5U_45_ID: 0x%08x "
                                "nor LFE5UM_45_ID: 0x%08x",
                                val, LFE5U_45_ID, LFE5UM_45_ID);

                        rc = -1;
                        break;
                }

                cmd = CMD_READ_STATUS;
                rc = ecp5_spi_cmd_a(state->spi_fw, cmd, &val);

                if (rc) {
                        dev_err(dev, "Failed to send command %02x\n", cmd);
                        break;
                }

                dev_dbg(dev, "ecp5 status: %08x", val);

                rc = decode_status(dev, val);

                if (rc || val) {
                        dev_err(dev, "ecp5 status: %08x Error\n", val);
                        rc = -1;
                        break;
                }

                cmd = CMD_ISC_ENABLE;
                rc = ecp5_spi_cmd_c(state->spi_fw, cmd);

                if (rc) {
                        dev_err(dev, "Failed to send command %02x\n", cmd);
                        break;
                }

                rc = ecp5_spi_write_fw_stream(state->spi_fw, fw->data, fw->size);

                if (rc) {
                        dev_err(dev, "Failed to write fw stream %p size %zu\n", fw->data, fw->size);
                        break;
                }

                cmd = CMD_ISC_DISABLE;
                rc = ecp5_spi_cmd_c(state->spi_fw, cmd);

                if (rc) {
                        dev_err(dev, "Failed to send command %02x\n", cmd);
                        break;
                }

                /* gpio done is 1 here */
                cmd = CMD_READ_STATUS;
                rc = ecp5_spi_cmd_a(state->spi_fw, cmd, &val);

                if (rc) {
                        dev_err(dev, "Failed to send command %02x\n", cmd);
                        break;
                }

                rc = decode_status(dev, val);

                if (rc) {
                        dev_err(dev, "ecp5 status: %08x Error\n", val);
                        break;
                }
                rc = status_done(val);
                dev_dbg(dev, "ecp5 status: %08x done: %d\n", val, rc);
                dev_dbg(dev, "ecp5 DONE: %d\n", gpiod_get_value(state->done));
                dev_info(dev, "ecp5 status: %08x done: %d\n", val, rc);
                dev_info(dev, "ecp5 DONE pin: %d\n", gpiod_get_value(state->done));
        } while (0);

        if (state->spi_fw) {
                dev_info(dev, "spi_unregister_device\n");
                spi_unregister_device(state->spi_fw);
        }
err_release_fw:
        release_firmware(fw);

        return rc;
}

static const struct of_device_id fpga_of_match[] = {
        {.compatible = "ltx,fpga-device", .data = (void *)FPGA_INTEL},
        {.compatible = "ltx,fpga-device-ecp5", .data = (void *)FPGA_LATTICE},
        {}
};

MODULE_DEVICE_TABLE(of, fpga_of_match);

static int fpga_spi_probe(struct platform_device *pdev)
{
        int     rc;
        struct  spi_master *cfg_master;
        struct device *dev = &pdev->dev;
        struct fpga_data *state;
        const struct of_device_id *of_id;
        uint8_t spi_bus;
        uint8_t retries = 0;

        state = devm_kzalloc(&pdev->dev, sizeof(*state), GFP_KERNEL);
        if (!state) {
                dev_err(dev, "Failed to allocate private data\n");
                return (-ENOMEM);
        }

        platform_set_drvdata(pdev, state);
        state->clock_rate = 32768000;
        state->cfg_mode = FPGA_CFG_MODE_CFG_NORMAL;

        state->cfg_info.max_speed_hz = 1000000;
        state->cfg_info.chip_select = FPGA_CFG_SPI_CS;
        state->cfg_info.mode = SPI_MODE_0;

        state->fw_info.max_speed_hz = 50000000;
        state->fw_info.chip_select = FPGA_CFG_SPI_CS;
        state->fw_info.mode = SPI_MODE_0;

        if (!pdev->dev.of_node) {
                dev_err(dev, "Failed to get of data\n");
                goto err_of_dev;
        }

        of_id = of_match_node(fpga_of_match, dev->of_node);

        state->type = (enum fpga_type)of_id->data;

        rc = of_property_read_u8(dev->of_node, "fpga-config-bus", &spi_bus);

        if (rc) {
                dev_warn(dev, "Failed to read config bus number, using default: %d\n", rc);
                state->cfg_info.bus_num = FPGA_CFG_SPI_DEV_CFG;
        } else {
                state->cfg_info.bus_num = spi_bus;
                dev_info(dev, "Config bus number from of: %d\n", spi_bus);
        }

        state->power = devm_gpiod_get_index(&pdev->dev, "fpga", 0, GPIOD_OUT_LOW);

        if (IS_ERR(state->power)) {
                dev_err(dev, "Failed to get of power gpio\n");
                goto err_of_dev;
        }

        state->reset = devm_gpiod_get_index(&pdev->dev, "fpga", 1, GPIOD_OUT_LOW);

        if (IS_ERR(state->reset)) {
                dev_err(dev, "Failed to get of reset gpio\n");
                goto err_of_dev;
        }

        state->done = devm_gpiod_get_index(&pdev->dev, "fpga", 2, GPIOD_IN);
        if (IS_ERR(state->done)) {
                dev_info(dev, "Failed to get of done gpio\n");
        }

        if (state->type == FPGA_INTEL) {
                dev_dbg(dev, "fpga intel\n");
                state->nconfig = devm_gpiod_get_index(&pdev->dev, "fpga", 3, GPIOD_OUT_HIGH);
                if (IS_ERR(state->nconfig)) {
                        dev_info(dev, "Failed to get of nconfig gpio\n");
                        goto err_of_dev;
                }
        }

        if (state->type == FPGA_LATTICE) {
                dev_dbg(dev, "fpga lattice\n");

                state->programn = devm_gpiod_get_index(&pdev->dev, "fpga", 3, GPIOD_OUT_LOW);
                if (IS_ERR(state->programn)) {
                        dev_info(dev, "Failed to get of progamn gpio\n");
                        goto err_of_dev;
                }

                state->initn = devm_gpiod_get_index(&pdev->dev, "fpga", 4, GPIOD_IN);
                if (IS_ERR(state->initn)) {
                        dev_info(dev, "Failed to get of initn gpio\n");
                        goto err_of_dev;
                }

                if (IS_ERR(state->programn)
                    || IS_ERR(state->initn)
                    || IS_ERR(state->done)) {
                        dev_err(dev, "fpga lattice can't get required gpios\n");
                        goto err_of_dev;
                }

                rc = of_property_read_u8(dev->of_node, "fpga-fw-bus", &spi_bus);

                if (rc) {
                        state->fw_info.bus_num = FPGA_CFG_SPI_DEV_CFG_LATTICE;
                        dev_warn(dev, "Failed to read firmware load bus number, using default %d\n",
                                 state->fw_info.bus_num);
                } else {
                        state->fw_info.bus_num = spi_bus;
                }

                rc = firmware_load(pdev);

                if (rc) {
                        dev_err(dev, "fpga lattice failed to load firmware: %d\n", rc);
                        goto err_of_dev;
                }
        }

        state->fpga_kobj = kobject_create_and_add("fpga_sysfs", NULL);
        if (!state->fpga_kobj) {
                dev_err(dev, "Cannot create kobj\n");
                goto err_of_dev;
        }

        if (sysfs_create_groups(&pdev->dev.kobj, fpga_attr_groups)) {
                dev_err(dev, "Cannot create sysfs file......\n");
                goto err_sysfs;
        }

        if (sysfs_create_link(state->fpga_kobj, &pdev->dev.kobj, "qed")) {
                dev_err(dev, "Cannot create sysfs link......\n");
                goto err_sysfsgroup;
        }

        /* conf QEd */
        cfg_master = spi_busnum_to_master(state->cfg_info.bus_num);
        if (!cfg_master) {
                dev_err(dev, "SPI cfg_master not found.\n");
                goto err_sysfslink;
        }

        /* create a new slave device for cfg */
        state->spi_cfg = spi_new_device(cfg_master, &state->cfg_info);
        if (!state->spi_cfg) {
                dev_err(dev, "Failed to create slave.\n");
                goto err_sysfslink;
        }

        rc = spi_setup(state->spi_cfg);
        if (rc) {
                dev_err(dev, "Failed to setup slave.\n");
                goto err_spi;
        }
        /* power on fpga */
        gpiod_set_value(state->power, 1);

        if (state->type == FPGA_INTEL) {
                uint32_t sleep = 1;
                /* wait until configuration is done */
                while (retries < QED_SPI_FPGA_MAX_ITER) {
                        msleep(sleep);
                        if (gpiod_get_value(state->done)) {
                                break;
                        }
                        ++retries;
                }

                if (retries == QED_SPI_FPGA_MAX_ITER) {
                        dev_err(dev, "Intel fpga configuration max retries reached: %d\n",
                                retries);
                        goto err_spi;
                }
                else
                        dev_dbg(dev, "intel fpga configured in %u ms\n",
                                (retries * sleep));
        }

        mutex_init(&state->lock);
        gpiod_set_value(state->reset, 1);

        msleep(50);

        rc = get_id(state);
        if (rc < 0) {
                dev_err(dev, "Failed to get fpga id\n");
                goto err_mutex;
        }

        dev_info(dev, "SPI driver Registered: FPGA ID: %02x\n", rc);
        return 0;

err_mutex:
        mutex_destroy(&state->lock);
err_spi:
        spi_unregister_device(state->spi_cfg);
err_sysfslink:
        sysfs_remove_link(&pdev->dev.kobj, "qed");
err_sysfsgroup:
        sysfs_remove_group(&pdev->dev.kobj, &fpga_attr_group);
err_sysfs:
        kobject_put(state->fpga_kobj);
err_of_dev:
        return -ENODEV;
}

static int fpga_spi_remove(struct platform_device *pdev)
{
        struct device *dev = &pdev->dev;
        struct fpga_data *pd = platform_get_drvdata(pdev);

        if (pd->type == FPGA_LATTICE) {
            spi_deinit_fw(pdev);
        }

        mutex_destroy(&pd->lock);

        if (pd->spi_cfg) {
                spi_unregister_device(pd->spi_cfg);
        }


        kobject_put(pd->fpga_kobj);
        sysfs_remove_group(&pdev->dev.kobj, &fpga_attr_group);
        sysfs_remove_link(&pdev->dev.kobj, "qed");
        dev_info(dev, "SPI driver Unregistered\n");

        return 0;
}

static struct platform_driver fpga_plat_driver = {
        .driver = {
                .name   = "spi_fpga_dev",
                .owner  = THIS_MODULE,
                .of_match_table = of_match_ptr(fpga_of_match),
        },
        .probe = fpga_spi_probe,
        .remove = fpga_spi_remove,
};

module_platform_driver(fpga_plat_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Lantronix <lantronix@lantronix.com>");
MODULE_DESCRIPTION("An FPGA QED driver");
MODULE_VERSION("1.01");
