/* SPDX-License-Identifier: GPL-2.0 OR Linux-OpenIB */
/* Copyright (c) 2020, Mellanox Technologies inc. All rights reserved. */

#ifndef __MLX5_IPSEC_STEERING_H__
#define __MLX5_IPSEC_STEERING_H__

#include "en.h"
#include "ipsec.h"
#include "accel/ipsec_offload.h"
#include "en/fs.h"

int mlx5e_ipsec_fs_rx_inline_init(struct mlx5e_priv *priv, enum mlx5e_traffic_types type);
int mlx5e_ipsec_fs_rx_inline_remove(struct mlx5e_priv *priv, enum mlx5e_traffic_types type);
int mlx5e_ipsec_fs_is_supported(struct mlx5e_priv *priv, enum mlx5e_traffic_types type);

#endif /* __MLX5_IPSEC_STEERING_H__ */
