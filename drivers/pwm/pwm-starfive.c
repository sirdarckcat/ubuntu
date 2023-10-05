// SPDX-License-Identifier: GPL-2.0
/*
 * PWM driver for the StarFive JH71x0 SoC
 *
 * Copyright (C) 2018-2023 StarFive Technology Co., Ltd.
 */

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/reset.h>
#include <linux/slab.h>

/* Access PTC register (CNTR, HRC, LRC and CTRL) */
#define REG_PTC_BASE_ADDR_SUB(base, N)	((base) + (((N) > 3) ? \
					(((N) % 4) * 0x10 + (1 << 15)) : ((N) * 0x10)))
#define REG_PTC_RPTC_CNTR(base, N)	(REG_PTC_BASE_ADDR_SUB(base, N))
#define REG_PTC_RPTC_HRC(base, N)	(REG_PTC_BASE_ADDR_SUB(base, N) + 0x4)
#define REG_PTC_RPTC_LRC(base, N)	(REG_PTC_BASE_ADDR_SUB(base, N) + 0x8)
#define REG_PTC_RPTC_CTRL(base, N)	(REG_PTC_BASE_ADDR_SUB(base, N) + 0xC)

/* PTC_RPTC_CTRL register bits*/
#define PTC_EN      BIT(0)
#define PTC_ECLK    BIT(1)
#define PTC_NEC     BIT(2)
#define PTC_OE      BIT(3)
#define PTC_SIGNLE  BIT(4)
#define PTC_INTE    BIT(5)
#define PTC_INT     BIT(6)
#define PTC_CNTRRST BIT(7)
#define PTC_CAPTE   BIT(8)

struct starfive_pwm_ptc_device {
	struct pwm_chip chip;
	struct clk *clk;
	struct reset_control *rst;
	void __iomem *regs;
	u32 clk_rate; /* PWM APB clock frequency */
};

static inline struct starfive_pwm_ptc_device *
chip_to_starfive_ptc(struct pwm_chip *chip)

{
	return container_of(chip, struct starfive_pwm_ptc_device, chip);
}

static int starfive_pwm_ptc_get_state(struct pwm_chip *chip,
				      struct pwm_device *dev,
				      struct pwm_state *state)
{
	struct starfive_pwm_ptc_device *pwm = chip_to_starfive_ptc(chip);
	u32 period_data, duty_data, ctrl_data;

	period_data = readl(REG_PTC_RPTC_LRC(pwm->regs, dev->hwpwm));
	duty_data = readl(REG_PTC_RPTC_HRC(pwm->regs, dev->hwpwm));
	ctrl_data = readl(REG_PTC_RPTC_CTRL(pwm->regs, dev->hwpwm));

	state->period = DIV_ROUND_CLOSEST_ULL((u64)period_data * NSEC_PER_SEC, pwm->clk_rate);
	state->duty_cycle = DIV_ROUND_CLOSEST_ULL((u64)duty_data * NSEC_PER_SEC, pwm->clk_rate);
	state->polarity = PWM_POLARITY_INVERSED;
	state->enabled = (ctrl_data & PTC_EN) ? true : false;

	return 0;
}

static int starfive_pwm_ptc_apply(struct pwm_chip *chip,
				  struct pwm_device *dev,
				  const struct pwm_state *state)
{
	struct starfive_pwm_ptc_device *pwm = chip_to_starfive_ptc(chip);
	u32 period_data, duty_data, ctrl_data = 0;

	if (state->polarity != PWM_POLARITY_INVERSED)
		return -EINVAL;

	period_data = DIV_ROUND_CLOSEST_ULL(state->period * pwm->clk_rate,
					    NSEC_PER_SEC);
	duty_data = DIV_ROUND_CLOSEST_ULL(state->duty_cycle * pwm->clk_rate,
					  NSEC_PER_SEC);

	writel(period_data, REG_PTC_RPTC_LRC(pwm->regs, dev->hwpwm));
	writel(duty_data, REG_PTC_RPTC_HRC(pwm->regs, dev->hwpwm));
	writel(0,  REG_PTC_RPTC_CNTR(pwm->regs, dev->hwpwm));

	ctrl_data = readl(REG_PTC_RPTC_CTRL(pwm->regs, dev->hwpwm));
	if (state->enabled)
		writel(ctrl_data | PTC_EN | PTC_OE, REG_PTC_RPTC_CTRL(pwm->regs, dev->hwpwm));
	else
		writel(ctrl_data & ~(PTC_EN | PTC_OE), REG_PTC_RPTC_CTRL(pwm->regs, dev->hwpwm));

	return 0;
}

static const struct pwm_ops starfive_pwm_ptc_ops = {
	.get_state	= starfive_pwm_ptc_get_state,
	.apply		= starfive_pwm_ptc_apply,
	.owner		= THIS_MODULE,
};

static int starfive_pwm_ptc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct starfive_pwm_ptc_device *pwm;
	struct pwm_chip *chip;
	int ret;

	pwm = devm_kzalloc(dev, sizeof(*pwm), GFP_KERNEL);
	if (!pwm)
		return -ENOMEM;

	chip = &pwm->chip;
	chip->dev = dev;
	chip->ops = &starfive_pwm_ptc_ops;
	chip->npwm = 8;
	chip->of_pwm_n_cells = 3;

	pwm->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(pwm->regs))
		return dev_err_probe(dev, PTR_ERR(pwm->regs),
				     "Unable to map IO resources\n");

	pwm->clk = devm_clk_get_enabled(dev, NULL);
	if (IS_ERR(pwm->clk))
		return dev_err_probe(dev, PTR_ERR(pwm->clk),
				     "Unable to get pwm's clock\n");

	pwm->rst = devm_reset_control_get_exclusive(dev, NULL);
	if (IS_ERR(pwm->rst))
		return dev_err_probe(dev, PTR_ERR(pwm->rst),
				     "Unable to get pwm's reset\n");

	ret = reset_control_deassert(pwm->rst);
	if (ret) {
		dev_err(dev, "Failed to enable clock for pwm: %d\n", ret);
		return ret;
	}

	pwm->clk_rate = clk_get_rate(pwm->clk);
	if (pwm->clk_rate <= 0) {
		dev_warn(dev, "Failed to get APB clock rate\n");
		return -EINVAL;
	}

	ret = devm_pwmchip_add(dev, chip);
	if (ret < 0) {
		dev_err(dev, "Cannot register PTC: %d\n", ret);
		clk_disable_unprepare(pwm->clk);
		reset_control_assert(pwm->rst);
		return ret;
	}

	platform_set_drvdata(pdev, pwm);

	return 0;
}

static int starfive_pwm_ptc_remove(struct platform_device *dev)
{
	struct starfive_pwm_ptc_device *pwm = platform_get_drvdata(dev);

	reset_control_assert(pwm->rst);
	clk_disable_unprepare(pwm->clk);

	return 0;
}

static const struct of_device_id starfive_pwm_ptc_of_match[] = {
	{ .compatible = "starfive,jh7100-pwm" },
	{ .compatible = "starfive,jh7110-pwm" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, starfive_pwm_ptc_of_match);

static struct platform_driver starfive_pwm_ptc_driver = {
	.probe = starfive_pwm_ptc_probe,
	.remove = starfive_pwm_ptc_remove,
	.driver = {
		.name = "pwm-starfive-ptc",
		.of_match_table = starfive_pwm_ptc_of_match,
	},
};
module_platform_driver(starfive_pwm_ptc_driver);

MODULE_AUTHOR("Jieqin Chen");
MODULE_AUTHOR("Hal Feng <hal.feng@starfivetech.com>");
MODULE_DESCRIPTION("StarFive PWM PTC driver");
MODULE_LICENSE("GPL");
