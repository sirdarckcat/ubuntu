/* SPDX-License-Identifier: GPL-2.0 OR Linux-OpenIB */
/* Copyright (c) 2020, Mellanox Technologies inc. All rights reserved. */

#ifndef __MLX5E_ACCEL_FS_H__
#define __MLX5E_ACCEL_FS_H__

#ifdef CONFIG_MLX5_ACCEL

#include "en.h"
void mlx5e_accel_fs_ref_prot(struct mlx5e_priv *priv, enum mlx5e_traffic_types type, int change);

int mlx5e_accel_fs_create_tables(struct mlx5e_priv *priv);
void mlx5e_accel_fs_destroy_tables(struct mlx5e_priv *priv);
#else
int mlx5e_accel_fs_create_tables(struct mlx5e_priv *priv) { return 0; }
void mlx5e_accel_fs_destroy_tables(struct mlx5e_priv *priv) {}
#endif

#endif /* __MLX5E_ACCEL_FS_H__ */
