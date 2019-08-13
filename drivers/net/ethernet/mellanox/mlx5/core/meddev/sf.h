/* SPDX-License-Identifier: GPL-2.0 OR Linux-OpenIB */
/* Copyright (c) 2019 Mellanox Technologies */

#ifndef __MLX5_SF_H__
#define __MLX5_SF_H__

#include <linux/mlx5/driver.h>
#include <linux/mlx5/eswitch.h>

struct mlx5_sf_table {
	phys_addr_t base_address;
	/* Protects sfs life cycle and sf enable/disable flows */
	struct mutex lock;
	u16 max_sfs;
	u16 log_sf_bar_size;
};

static inline bool mlx5_core_is_sf_supported(const struct mlx5_core_dev *dev)
{
	return MLX5_ESWITCH_MANAGER(dev) &&
	       MLX5_CAP_GEN(dev, max_num_sf_partitions) &&
	       MLX5_CAP_GEN(dev, sf);
}

#ifdef CONFIG_MLX5_MDEV
int mlx5_sf_table_init(struct mlx5_core_dev *dev,
		       struct mlx5_sf_table *sf_table);
void mlx5_sf_table_cleanup(struct mlx5_core_dev *dev,
			   struct mlx5_sf_table *sf_table);
#endif

#endif
