// SPDX-License-Identifier: GPL-2.0 OR Linux-OpenIB
// Copyright (c) 2018-19 Mellanox Technologies

#include <linux/kernel.h>
#include <linux/module.h>
#include "eswitch.h"
#include "sf.h"
#include "mlx5_core.h"

static int
mlx5_cmd_query_sf_partitions(struct mlx5_core_dev *mdev, u32 *out, int outlen)
{
	u32 in[MLX5_ST_SZ_DW(query_sf_partitions_in)] = {};

	/* Query sf partitions */
	MLX5_SET(query_sf_partitions_in, in, opcode,
		 MLX5_CMD_OP_QUERY_SF_PARTITION);
	return mlx5_cmd_exec(mdev, in, sizeof(in), out, outlen);
}

int mlx5_sf_table_init(struct mlx5_core_dev *dev,
		       struct mlx5_sf_table *sf_table)
{
	void *sf_parts;
	int n_support;
	int outlen;
	u32 *out;
	int err;

	outlen = MLX5_ST_SZ_BYTES(query_sf_partitions_out) + MLX5_ST_SZ_BYTES(sf_partition);
	out = kvzalloc(outlen, GFP_KERNEL);
	if (!out)
		return -ENOMEM;

	mutex_init(&sf_table->lock);
	/* SFs BAR is implemented in PCI BAR2 */
	sf_table->base_address = pci_resource_start(dev->pdev, 2);

	/* Query first partition */
	err = mlx5_cmd_query_sf_partitions(dev, out, outlen);
	if (err)
		goto free_outmem;

	n_support = MLX5_GET(query_sf_partitions_out, out, num_sf_partitions);
	sf_parts = MLX5_ADDR_OF(query_sf_partitions_out, out, sf_partition);
	sf_table->max_sfs = 1 << MLX5_GET(sf_partition, sf_parts, log_num_sf);
	sf_table->log_sf_bar_size =
		MLX5_GET(sf_partition, sf_parts, log_sf_bar_size);

	mlx5_core_dbg(dev, "supported partitions(%d)\n", n_support);
	mlx5_core_dbg(dev, "SF_part(0) log_num_sf(%d) log_sf_bar_size(%d)\n",
		      sf_table->max_sfs, sf_table->log_sf_bar_size);

free_outmem:
	kvfree(out);
	return err;
}

void mlx5_sf_table_cleanup(struct mlx5_core_dev *dev,
			   struct mlx5_sf_table *sf_table)
{
	mutex_destroy(&sf_table->lock);
}
