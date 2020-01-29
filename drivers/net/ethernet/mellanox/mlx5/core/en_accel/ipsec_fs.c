// SPDX-License-Identifier: GPL-2.0 OR Linux-OpenIB
/* Copyright (c) 2020, Mellanox Technologies inc. All rights reserved. */

#include <linux/netdevice.h>
#include "accel/ipsec_offload.h"
#include "ipsec_fs.h"
#include "accel_fs.h"
#include "fs_core.h"

#ifdef CONFIG_MLX5_EN_IPSEC
#define NUM_IPSEC_FTE BIT(15)
#define NUM_IPSEC_FG 1

int mlx5e_ipsec_fs_rx_inline_remove(struct mlx5e_priv *priv, enum mlx5e_traffic_types type)
{
	struct mlx5e_accel_proto *prot;

	/* The netdev unreg already happened, so all offloaded rule are already removed */
	if (!priv->fs.accel.prot[type])
		return 0;
	prot = priv->fs.accel.prot[type];

	if (prot->miss_rule) {
		mlx5_del_flow_rules(priv->fs.accel.prot[type]->miss_rule);
		prot->miss_rule = NULL;
	}

	if (prot->miss_group) {
		mlx5_destroy_flow_group(priv->fs.accel.prot[type]->miss_group);
		prot->miss_group = NULL;
	}

	if (prot->ft) {
		mlx5_destroy_flow_table(priv->fs.accel.prot[type]->ft);
		prot->ft = NULL;
	}

	return 0;
}

int mlx5e_ipsec_fs_is_supported(struct mlx5e_priv *priv, enum mlx5e_traffic_types type)
{
	return mlx5_is_ipsec_device(priv->mdev);
}

int mlx5e_ipsec_fs_rx_inline_init(struct mlx5e_priv *priv, enum mlx5e_traffic_types type)
{
	int inlen = MLX5_ST_SZ_BYTES(create_flow_group_in);
	struct mlx5_flow_table_attr ft_attr = {};
	struct mlx5_flow_group *miss_group;
	struct mlx5_flow_handle *miss_rule;
	MLX5_DECLARE_FLOW_ACT(flow_act);
	struct mlx5_flow_spec *spec;
	struct mlx5_flow_table *ft;
	u32 *flow_group_in;
	int err = 0;

	flow_group_in = kvzalloc(inlen, GFP_KERNEL);
	spec = kvzalloc(sizeof(*spec), GFP_KERNEL);
	if (!flow_group_in || !spec) {
		err = -ENOMEM;
		goto out_alloc;
	}

	/* Create FT */
	ft_attr.max_fte = NUM_IPSEC_FTE;
	ft_attr.level = MLX5E_ACCEL_FS_FT_LEVEL;
	ft_attr.prio = MLX5E_NIC_PRIO;
	ft_attr.autogroup.num_reserved_entries = 1;
	ft_attr.autogroup.max_num_groups = NUM_IPSEC_FG;
	ft = mlx5_create_auto_grouped_flow_table(priv->fs.ns, &ft_attr);
	if (IS_ERR(ft)) {
		netdev_err(priv->netdev, "fail to create ipsec rx ft, type=%d\n", type);
		err = PTR_ERR(ft);
		goto out_alloc;
	}
	priv->fs.accel.prot[type]->ft = ft;

	/* Create miss_group */
	MLX5_SET(create_flow_group_in, flow_group_in, start_flow_index, ft->max_fte - 1);
	MLX5_SET(create_flow_group_in, flow_group_in, end_flow_index, ft->max_fte - 1);
	miss_group = mlx5_create_flow_group(ft, flow_group_in);
	if (IS_ERR(miss_group)) {
		netdev_err(priv->netdev, "fail to create ipsec rx miss_group, type=%d\n", type);
		err = PTR_ERR(miss_group);
		goto err_steering;
	}
	priv->fs.accel.prot[type]->miss_group = miss_group;

	/* Create miss rule */
	miss_rule = mlx5_add_flow_rules(ft, spec, &flow_act, &priv->fs.accel.prot[type]->default_dest, 1);
	if (IS_ERR(miss_rule)) {
		netdev_err(priv->netdev, "fail to create ipsec rx miss_rule, type=%d\n", type);
		err = PTR_ERR(miss_rule);
		goto err_steering;
	}
	priv->fs.accel.prot[type]->miss_rule = miss_rule;

	goto out_alloc;

err_steering:
	mlx5e_ipsec_fs_rx_inline_remove(priv, type);

out_alloc:
	kfree(flow_group_in);
	kfree(spec);
	return err;
}
#else
int mlx5e_ipsec_fs_rx_inline_remove(struct mlx5e_priv *priv, enum mlx5e_traffic_types type)
{
	return 0;
}

int mlx5e_ipsec_fs_is_supported(struct mlx5e_priv *priv, enum mlx5e_traffic_types type)
{
	return 0;
}

int mlx5e_ipsec_fs_rx_inline_init(struct mlx5e_priv *priv, enum mlx5e_traffic_types type)
{
	return 0;
}
#endif /* #ifdef CONFIG_MLX5_EN_IPSEC */
