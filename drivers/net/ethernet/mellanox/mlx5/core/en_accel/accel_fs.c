// SPDX-License-Identifier: GPL-2.0 OR Linux-OpenIB
/* Copyright (c) 2020, Mellanox Technologies inc. All rights reserved. */

#include <linux/netdevice.h>
#include "accel_fs.h"
#include "fs_core.h"

typedef int (*mlx5e_accel_prot_cb)(struct mlx5e_priv *priv, enum mlx5e_traffic_types type);

struct mlx5e_accel_proto_func {
	mlx5e_accel_prot_cb init;
	mlx5e_accel_prot_cb remove;
	mlx5e_accel_prot_cb is_supported;
};

static struct mlx5e_accel_proto_func proto_funcs[MLX5E_NUM_TT] = {};

void mlx5e_accel_fs_ref_prot(struct mlx5e_priv *priv, enum mlx5e_traffic_types type, int change)
{
	struct mlx5_flow_destination dest = {};
	struct mlx5e_accel_proto *prot;
	u32 prev_refcnt;

	prot = priv->fs.accel.prot[type];

	mutex_lock(&prot->prot_mutex);
	prev_refcnt = prot->refcnt;
	prot->refcnt += change;

	/* connect */
	if (prev_refcnt == 0 && prot->refcnt == 1) {
		dest.type = MLX5_FLOW_DESTINATION_TYPE_FLOW_TABLE;
		dest.ft = prot->ft;
		mlx5e_ttc_fwd_dest(priv, type, &dest);
	}

	/* disconnect */
	if (prev_refcnt == 1 && prot->refcnt == 0)
		mlx5e_ttc_fwd_dest(priv, type, &prot->default_dest);
	mutex_unlock(&prot->prot_mutex);
}

void mlx5e_accel_fs_destroy_tables(struct mlx5e_priv *priv)
{
	struct mlx5e_accel_proto_func *proto;
	struct mlx5e_accel_proto *prot;
	enum mlx5e_traffic_types i;

	for (i = 0; i < MLX5E_NUM_TT; i++) {
		if (!priv->fs.accel.prot[i])
			continue;

		proto = &proto_funcs[i];

		prot = priv->fs.accel.prot[i];
		proto->remove(priv, i);
		priv->fs.accel.prot[i] = NULL;
		kfree(prot);
	}
}

int mlx5e_accel_fs_create_tables(struct mlx5e_priv *priv)
{
	struct mlx5e_accel_proto_func *proto;
	enum mlx5e_traffic_types i;
	int err;

	for (i = 0; i < MLX5E_NUM_TT; i++) {
		proto = &proto_funcs[i];
		if (!proto->is_supported)
			continue;
		if (!proto->is_supported(priv, i))
			continue;

		priv->fs.accel.prot[i] = kzalloc(sizeof(struct mlx5e_accel_proto), GFP_KERNEL);
		if (!priv->fs.accel.prot[i]) {
			err = -ENOMEM;
			goto out_err;
		}
		mlx5e_ttc_get_default_dest(priv, i, &priv->fs.accel.prot[i]->default_dest);
		mutex_init(&priv->fs.accel.prot[i]->prot_mutex);

		err = proto->init(priv, i);
		if (err) {
			kfree(priv->fs.accel.prot[i]);
			priv->fs.accel.prot[i] = NULL;
			goto out_err;
		}
	}

	return 0;

out_err:
	mlx5e_accel_fs_destroy_tables(priv);
	return err;
}
