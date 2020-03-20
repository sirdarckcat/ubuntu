// SPDX-License-Identifier: GPL-2.0 OR Linux-OpenIB
// Copyright (c) 2018-19 Mellanox Technologies

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io-mapping.h>
#include <linux/mlx5/driver.h>
#include <linux/bitmap.h>
#include "eswitch.h"
#include "sf.h"
#include "mlx5_core.h"
#include "eswitch.h"

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

	sf_table->sf_id_bitmap = bitmap_zalloc(sf_table->max_sfs, GFP_KERNEL);
	if (!sf_table->sf_id_bitmap) {
		err = -ENOMEM;
		goto free_outmem;
	}

free_outmem:
	kvfree(out);
	return err;
}

void mlx5_sf_table_cleanup(struct mlx5_core_dev *dev,
			   struct mlx5_sf_table *sf_table)
{
	mutex_destroy(&sf_table->lock);
	bitmap_free(sf_table->sf_id_bitmap);
}

static int mlx5_cmd_alloc_sf(struct mlx5_core_dev *mdev, u16 function_id)
{
	u32 out[MLX5_ST_SZ_DW(alloc_sf_out)] = {};
	u32 in[MLX5_ST_SZ_DW(alloc_sf_in)] = {};

	MLX5_SET(alloc_sf_in, in, opcode, MLX5_CMD_OP_ALLOC_SF);
	MLX5_SET(alloc_sf_in, in, function_id, function_id);

	return mlx5_cmd_exec(mdev, in, sizeof(in), out, sizeof(out));
}

static int mlx5_cmd_dealloc_sf(struct mlx5_core_dev *mdev, u16 function_id)
{
	u32 out[MLX5_ST_SZ_DW(dealloc_sf_out)] = {};
	u32 in[MLX5_ST_SZ_DW(dealloc_sf_in)] = {};

	MLX5_SET(dealloc_sf_in, in, opcode, MLX5_CMD_OP_DEALLOC_SF);
	MLX5_SET(dealloc_sf_in, in, function_id, function_id);

	return mlx5_cmd_exec(mdev, in, sizeof(in), out, sizeof(out));
}

static const struct devlink_ops sf_devlink_ops = {};

static int alloc_sf_id(struct mlx5_sf_table *sf_table, u16 *sf_id)
{
	int ret = 0;
	u16 idx;

	mutex_lock(&sf_table->lock);
	idx = find_first_zero_bit(sf_table->sf_id_bitmap, sf_table->max_sfs);
	if (idx == sf_table->max_sfs) {
		ret = -ENOSPC;
		goto done;
	}
	bitmap_set(sf_table->sf_id_bitmap, idx, 1);
	*sf_id = idx;
done:
	mutex_unlock(&sf_table->lock);
	return ret;
}

static void free_sf_id(struct mlx5_sf_table *sf_table, u16 sf_id)
{
	mutex_lock(&sf_table->lock);
	bitmap_clear(sf_table->sf_id_bitmap, sf_id, 1);
	mutex_unlock(&sf_table->lock);
}

static u16 mlx5_sf_hw_id(const struct mlx5_core_dev *coredev, u16 sf_id)
{
	return mlx5_sf_base_id(coredev) + sf_id;
}

/* Perform SF allocation using parent device BAR. */
struct mlx5_sf *
mlx5_sf_alloc(struct mlx5_core_dev *coredev, struct mlx5_sf_table *sf_table,
	      struct device *dev)
{
	struct devlink *devlink;
	phys_addr_t base_addr;
	struct mlx5_sf *sf;
	u16 hw_function_id;
	u16 sf_id;
	int ret;

	devlink = devlink_alloc(&sf_devlink_ops, sizeof(*sf));
	if (!devlink)
		return ERR_PTR(-ENOMEM);

	ret = alloc_sf_id(sf_table, &sf_id);
	if (ret)
		goto id_err;

	hw_function_id = mlx5_sf_hw_id(coredev, sf_id);
	ret = mlx5_cmd_alloc_sf(coredev, hw_function_id);
	if (ret)
		goto alloc_sf_err;

	ret = mlx5_core_enable_sf_hca(coredev, hw_function_id);
	if (ret)
		goto enable_err;

	ret = mlx5_eswitch_setup_sf_vport(coredev->priv.eswitch, hw_function_id);
	if (ret)
		goto vport_err;

	sf = devlink_priv(devlink);
	sf->idx = sf_id;
	sf->dev.device = dev;
	sf->dev.pdev = coredev->pdev;
	sf->dev.coredev_type = MLX5_COREDEV_SF;
	base_addr = sf_table->base_address +
			(sf_id << (sf_table->log_sf_bar_size + 12));
	sf->dev.bar_addr = base_addr;
	sf->dev.iseg_base = base_addr;

	ret = devlink_register(devlink, dev);
	if (ret)
		goto devlink_err;

	return sf;

devlink_err:
	mlx5_eswitch_cleanup_sf_vport(coredev->priv.eswitch, hw_function_id);
vport_err:
	mlx5_core_disable_sf_hca(coredev, hw_function_id);
enable_err:
	mlx5_cmd_dealloc_sf(coredev, hw_function_id);
alloc_sf_err:
	free_sf_id(sf_table, sf_id);
id_err:
	devlink_free(devlink);
	return ERR_PTR(ret);
}

void mlx5_sf_free(struct mlx5_core_dev *coredev, struct mlx5_sf_table *sf_table,
		  struct mlx5_sf *sf)
{
	struct devlink *devlink = priv_to_devlink(sf);
	u16 hw_function_id;

	hw_function_id = mlx5_sf_hw_id(coredev, sf->idx);
	devlink_unregister(devlink);
	mlx5_eswitch_cleanup_sf_vport(coredev->priv.eswitch, hw_function_id);
	mlx5_core_disable_sf_hca(coredev, hw_function_id);
	mlx5_cmd_dealloc_sf(coredev, hw_function_id);
	free_sf_id(sf_table, sf->idx);
	devlink_free(devlink);
}

u16 mlx5_get_free_sfs(struct mlx5_core_dev *dev, struct mlx5_sf_table *sf_table)
{
	u16 free_sfs = 0;

	if (!mlx5_core_is_sf_supported(dev))
		return 0;

	mutex_lock(&sf_table->lock);
	if (sf_table->sf_id_bitmap)
		free_sfs = sf_table->max_sfs -
				bitmap_weight(sf_table->sf_id_bitmap,
					      sf_table->max_sfs);
	mutex_unlock(&sf_table->lock);
	return free_sfs;
}

u16 mlx5_core_max_sfs(const struct mlx5_core_dev *dev,
		      const struct mlx5_sf_table *sf_table)
{
	return mlx5_core_is_sf_supported(dev) ? sf_table->max_sfs : 0;
}
