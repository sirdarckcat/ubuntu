/* SPDX-License-Identifier: GPL-2.0 OR Linux-OpenIB */
/* Copyright (c) 2019 Mellanox Technologies */

#ifndef __MLX5_SF_H__
#define __MLX5_SF_H__

#include <linux/mlx5/driver.h>
#include <linux/mlx5/eswitch.h>

static inline bool mlx5_core_is_sf_supported(const struct mlx5_core_dev *dev)
{
	return MLX5_ESWITCH_MANAGER(dev) &&
	       MLX5_CAP_GEN(dev, max_num_sf_partitions) &&
	       MLX5_CAP_GEN(dev, sf);
}

#endif
