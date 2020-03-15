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
struct mlx5e_ipsec_rx_err {
	struct mlx5_flow_table *ft_rx_err;
	struct mlx5_flow_handle *copy_fte;
	struct mlx5_modify_hdr *copy_modify_hdr;
};

/* IPsec RX flow steering */
static int ipsec_add_copy_action_rule(struct mlx5e_priv *priv,
				      struct mlx5e_accel_proto *prot,
				      struct mlx5e_ipsec_rx_err *rx_err)
{
	u8 action[MLX5_UN_SZ_BYTES(set_add_copy_action_in_auto)] = {};
	struct mlx5_core_dev *mdev = priv->mdev;
	struct mlx5_flow_act flow_act = {};
	struct mlx5_modify_hdr *modify_hdr;
	struct mlx5_flow_handle *fte;
	struct mlx5_flow_spec *spec;
	int err = 0;

	spec = kzalloc(sizeof(*spec), GFP_KERNEL);
	if (!spec)
		return -ENOMEM;

	/* Action to copy 7 bit ipsec_syndrome to regB[0:6] */
	MLX5_SET(copy_action_in, action, action_type, MLX5_ACTION_TYPE_COPY);
	MLX5_SET(copy_action_in, action, src_field, MLX5_ACTION_IN_FIELD_IPSEC_SYNDROME);
	MLX5_SET(copy_action_in, action, src_offset, 0);
	MLX5_SET(copy_action_in, action, length, 7);
	MLX5_SET(copy_action_in, action, dst_field, MLX5_ACTION_IN_FIELD_METADATA_REG_B);
	MLX5_SET(copy_action_in, action, dst_offset, 0);

	modify_hdr = mlx5_modify_header_alloc(mdev, MLX5_FLOW_NAMESPACE_KERNEL,
					      1, action);

	if (IS_ERR(modify_hdr)) {
		netdev_err(priv->netdev, "fail to alloc ipsec copy modify_header_id\n");
		err = PTR_ERR(modify_hdr);
		goto out_spec;
	}

	/* create fte */
	flow_act.action = MLX5_FLOW_CONTEXT_ACTION_MOD_HDR |
			  MLX5_FLOW_CONTEXT_ACTION_FWD_DEST;
	flow_act.modify_hdr = modify_hdr;
	fte = mlx5_add_flow_rules(rx_err->ft_rx_err, spec, &flow_act, &prot->default_dest, 1);
	if (IS_ERR(fte)) {
		err = PTR_ERR(fte);
		netdev_err(priv->netdev, "fail to add ipsec rx err copy rule err=%d\n", err);
		goto out;
	}

	rx_err->copy_fte = fte;
	rx_err->copy_modify_hdr = modify_hdr;

out:
	if (err)
		mlx5_modify_header_dealloc(mdev, modify_hdr);
out_spec:
	kfree(spec);
	return err;
}

static void ipsec_del_copy_action_rule(struct mlx5e_priv *priv, struct mlx5e_ipsec_rx_err *rx_err)
{
	if (rx_err->copy_fte) {
		mlx5_del_flow_rules(rx_err->copy_fte);
		rx_err->copy_fte = NULL;
	}

	if (rx_err->copy_modify_hdr) {
		mlx5_modify_header_dealloc(priv->mdev, rx_err->copy_modify_hdr);
		rx_err->copy_modify_hdr = NULL;
	}
}

static void ipsec_destroy_rx_err_ft(struct mlx5e_priv *priv, struct mlx5e_ipsec_rx_err *rx_err)
{
	ipsec_del_copy_action_rule(priv, rx_err);

	if (rx_err->ft_rx_err) {
		mlx5_destroy_flow_table(rx_err->ft_rx_err);
		rx_err->ft_rx_err = NULL;
	}
}

static int create_rx_inline_err_ft(struct mlx5e_priv *priv,
				   struct mlx5e_accel_proto *prot,
				   struct mlx5e_ipsec_rx_err *rx_err)
{
	struct mlx5_flow_table_attr ft_attr = {};
	struct mlx5_flow_table *ft;
	int err;

	ft_attr.max_fte = 1;
	ft_attr.autogroup.max_num_groups = 1;
	ft_attr.level = MLX5E_ACCEL_FS_ERR_FT_LEVEL;
	ft_attr.prio = MLX5E_NIC_PRIO;
	ft = mlx5_create_auto_grouped_flow_table(priv->fs.ns, &ft_attr);
	if (IS_ERR(ft)) {
		netdev_err(priv->netdev, "fail to create ipsec rx inline ft\n");
		return PTR_ERR(ft);
	}

	rx_err->ft_rx_err = ft;
	err = ipsec_add_copy_action_rule(priv, prot, rx_err);
	if (err)
		goto out_err;

	return 0;

out_err:
	mlx5_destroy_flow_table(ft);
	rx_err->ft_rx_err = NULL;
	return err;
}

static void ipsec_rx_inline_priv_remove(struct mlx5e_priv *priv, enum mlx5e_traffic_types type)
{
	struct mlx5e_ipsec_rx_err *rx_err;
	struct mlx5e_accel_proto *prot;

	if (!priv->fs.accel.prot[type])
		return;
	prot = priv->fs.accel.prot[type];

	if (prot->proto_priv) {
		rx_err = (struct mlx5e_ipsec_rx_err *)priv->fs.accel.prot[type]->proto_priv;
		ipsec_destroy_rx_err_ft(priv, rx_err);
		kfree(rx_err);
		prot->proto_priv = NULL;
	}
}

static int ipsec_rx_inline_priv_init(struct mlx5e_priv *priv, enum mlx5e_traffic_types type)
{
	struct mlx5e_ipsec_rx_err *rx_err;
	struct mlx5e_accel_proto *prot;
	int err;

	rx_err = kvzalloc(sizeof(*rx_err), GFP_KERNEL);
	if (!rx_err)
		return -ENOMEM;

	prot = priv->fs.accel.prot[type];
	err = create_rx_inline_err_ft(priv, prot, rx_err);
	if (err)
		goto out_err;

	prot->proto_priv = rx_err;
	return 0;

out_err:
	kfree(rx_err);
	return err;
}

int mlx5e_ipsec_fs_rx_inline_remove(struct mlx5e_priv *priv, enum mlx5e_traffic_types type)
{
	struct mlx5e_accel_proto *prot;

	/* The netdev unreg already happened, so all offloaded rule are already removed */
	if (!priv->fs.accel.prot[type])
		return 0;
	prot = priv->fs.accel.prot[type];

	ipsec_rx_inline_priv_remove(priv, type);

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

	err = ipsec_rx_inline_priv_init(priv, type);
	if (err)
		return err;

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

/* IPsec TX flow steering */
int mlx5e_ipsec_create_tx_ft(struct mlx5e_priv *priv)
{
	struct mlx5_flow_table_attr ft_attr = {};
	struct mlx5e_ipsec *ipsec = priv->ipsec;
	struct mlx5_flow_table *ft;

	if (!mlx5_is_ipsec_device(priv->mdev) || !MLX5_IPSEC_DEV(priv->mdev) || !ipsec)
		return 0;

	priv->fs.egress_ns = mlx5_get_flow_namespace(priv->mdev, MLX5_FLOW_NAMESPACE_EGRESS_KERNEL);
	if (!priv->fs.egress_ns)
		return -EOPNOTSUPP;

	ft_attr.max_fte = NUM_IPSEC_FTE;
	ft_attr.autogroup.max_num_groups = NUM_IPSEC_FG;
	ft = mlx5_create_auto_grouped_flow_table(priv->fs.egress_ns, &ft_attr);
	if (IS_ERR(ft)) {
		netdev_err(priv->netdev, "fail to create ipsec tx ft\n");
		return PTR_ERR(ft);
	}
	ipsec->ft_tx = ft;
	return 0;
}

void mlx5e_ipsec_destroy_tx_ft(struct mlx5e_priv *priv)
{
	struct mlx5e_ipsec *ipsec = priv->ipsec;

	if (!ipsec || IS_ERR_OR_NULL(ipsec->ft_tx))
		return;

	mlx5_destroy_flow_table(ipsec->ft_tx);
	ipsec->ft_tx = NULL;
}

/* IPsec XFRM */
static void ipsec_setup_fte_common(struct mlx5_accel_esp_xfrm_attrs *attrs,
				   u32 ipsec_obj_id,
				   struct mlx5_flow_spec *spec,
				   struct mlx5_flow_act *flow_act)
{
	u8 ip_version = attrs->is_ipv6 ? 6 : 4;

	spec->match_criteria_enable = MLX5_MATCH_OUTER_HEADERS | MLX5_MATCH_MISC_PARAMETERS;

	/* ip_version */
	MLX5_SET_TO_ONES(fte_match_param, spec->match_criteria, outer_headers.ip_version);
	MLX5_SET(fte_match_param, spec->match_value, outer_headers.ip_version, ip_version);

	/* Non fragmented */
	MLX5_SET_TO_ONES(fte_match_param, spec->match_criteria, outer_headers.frag);
	MLX5_SET(fte_match_param, spec->match_value, outer_headers.frag, 0);

	/* ESP header */
	MLX5_SET_TO_ONES(fte_match_param, spec->match_criteria, outer_headers.ip_protocol);
	MLX5_SET(fte_match_param, spec->match_value, outer_headers.ip_protocol, IPPROTO_ESP);

	/* SPI number */
	MLX5_SET_TO_ONES(fte_match_param, spec->match_criteria, misc_parameters.outer_esp_spi);
	MLX5_SET(fte_match_param, spec->match_value, misc_parameters.outer_esp_spi, cpu_to_be32(attrs->spi));

	if (ip_version == 4) {
		memcpy(MLX5_ADDR_OF(fte_match_param, spec->match_value,
				    outer_headers.src_ipv4_src_ipv6.ipv4_layout.ipv4),
		       &attrs->saddr.a4, 4);
		memcpy(MLX5_ADDR_OF(fte_match_param, spec->match_value,
				    outer_headers.dst_ipv4_dst_ipv6.ipv4_layout.ipv4),
		       &attrs->daddr.a4, 4);
		MLX5_SET_TO_ONES(fte_match_param, spec->match_criteria,
				 outer_headers.src_ipv4_src_ipv6.ipv4_layout.ipv4);
		MLX5_SET_TO_ONES(fte_match_param, spec->match_criteria,
				 outer_headers.dst_ipv4_dst_ipv6.ipv4_layout.ipv4);
	} else {
		memcpy(MLX5_ADDR_OF(fte_match_param, spec->match_value,
				    outer_headers.src_ipv4_src_ipv6.ipv6_layout.ipv6),
		       &attrs->saddr.a6, 16);
		memcpy(MLX5_ADDR_OF(fte_match_param, spec->match_value,
				    outer_headers.dst_ipv4_dst_ipv6.ipv6_layout.ipv6),
		       &attrs->daddr.a6, 16);
		memset(MLX5_ADDR_OF(fte_match_param, spec->match_criteria,
				    outer_headers.src_ipv4_src_ipv6.ipv6_layout.ipv6),
		       0xff, 16);
		memset(MLX5_ADDR_OF(fte_match_param, spec->match_criteria,
				    outer_headers.dst_ipv4_dst_ipv6.ipv6_layout.ipv6),
		       0xff, 16);
	}

	flow_act->ipsec_obj_id = ipsec_obj_id;
	flow_act->flags |= FLOW_ACT_NO_APPEND;
}

static int ipsec_add_rx_rule(struct mlx5e_priv *priv, struct mlx5e_ipsec_sa_entry *sa_entry)
{
	struct mlx5_accel_esp_xfrm_attrs *attrs = &sa_entry->xfrm->attrs;
	u8 action[MLX5_UN_SZ_BYTES(set_add_copy_action_in_auto)] = {};
	struct mlx5_ipsec_sa_ctx *sa_ctx = sa_entry->hw_context;
	struct mlx5_core_dev *mdev = priv->mdev;
	struct mlx5_flow_destination dest = {};
	struct mlx5_flow_act flow_act = {};
	struct mlx5_modify_hdr *modify_hdr;
	struct mlx5e_accel_proto *prot;
	enum mlx5e_traffic_types type;
	struct mlx5_flow_handle *rule;
	struct mlx5_flow_spec *spec;
	int err = 0;

	type = attrs->is_ipv6 ? MLX5E_TT_IPV6_IPSEC_ESP : MLX5E_TT_IPV4_IPSEC_ESP;
	prot = priv->fs.accel.prot[type];
	/* Fail safe check if ESP FTs are not initialized */
	if (!prot->ft)
		return 0;

	if (!mlx5_is_ipsec_device(mdev))
		return 0;

	spec = kvzalloc(sizeof(*spec), GFP_KERNEL);
	if (!spec) {
		err = -ENOMEM;
		goto out_err;
	}

	ipsec_setup_fte_common(attrs, sa_ctx->ipsec_obj_id, spec, &flow_act);

	/* Set 1  bit ipsec marker */
	/* Set 24 bit ipsec_obj_id */
	MLX5_SET(set_action_in, action, action_type, MLX5_ACTION_TYPE_SET);
	MLX5_SET(set_action_in, action, field, MLX5_ACTION_IN_FIELD_METADATA_REG_B);
	MLX5_SET(set_action_in, action, data, (sa_ctx->ipsec_obj_id << 1) | 0x1);
	MLX5_SET(set_action_in, action, offset, 7);
	MLX5_SET(set_action_in, action, length, 25);

	modify_hdr = mlx5_modify_header_alloc(mdev, MLX5_FLOW_NAMESPACE_KERNEL, 1, action);

	if (IS_ERR(modify_hdr)) {
		err = PTR_ERR(modify_hdr);
		netdev_err(priv->netdev, "fail to alloc ipsec set modify_header_id err=%d\n", err);
		modify_hdr = NULL;
		goto out_err;
	}

	flow_act.action = MLX5_FLOW_CONTEXT_ACTION_FWD_DEST |
			  MLX5_FLOW_CONTEXT_ACTION_IPSEC_DECRYPT |
			  MLX5_FLOW_CONTEXT_ACTION_MOD_HDR;
	dest.type = MLX5_FLOW_DESTINATION_TYPE_FLOW_TABLE;
	flow_act.modify_hdr = modify_hdr;

	dest.ft = ((struct mlx5e_ipsec_rx_err *)(prot->proto_priv))->ft_rx_err;
	rule = mlx5_add_flow_rules(prot->ft, spec, &flow_act, &dest, 1);

	if (IS_ERR(rule)) {
		err = PTR_ERR(rule);
		netdev_err(priv->netdev, "fail to add ipsec rule attrs->action=0x%x, err=%d\n", attrs->action, err);
		goto out_err;
	}

	sa_ctx->ipsec_rule.rule = rule;
	sa_ctx->ipsec_rule.set_modify_hdr = modify_hdr;
	mlx5e_accel_fs_ref_prot(priv, type, 1);

	goto out;

out_err:
	if (modify_hdr)
		mlx5_modify_header_dealloc(mdev, modify_hdr);

out:
	kvfree(spec);
	return err;
}

static int ipsec_add_tx_rule(struct mlx5e_priv *priv, struct mlx5e_ipsec_sa_entry *sa_entry)
{
	struct mlx5_accel_esp_xfrm_attrs *attrs = &sa_entry->xfrm->attrs;
	struct mlx5_ipsec_sa_ctx *sa_ctx = sa_entry->hw_context;
	struct mlx5_core_dev *mdev = priv->mdev;
	struct mlx5_flow_act flow_act = {};
	struct mlx5_flow_handle *rule;
	struct mlx5_flow_spec *spec;
	int err = 0;

	if (!mlx5_is_ipsec_device(mdev))
		return 0;

	spec = kvzalloc(sizeof(*spec), GFP_KERNEL);
	if (!spec) {
		err = -ENOMEM;
		goto out;
	}

	ipsec_setup_fte_common(attrs, sa_ctx->ipsec_obj_id, spec, &flow_act);

	/* Add IPsec indicator in metadata_reg_a */
	spec->match_criteria_enable |= MLX5_MATCH_MISC_PARAMETERS_2;
	MLX5_SET(fte_match_param, spec->match_criteria, misc_parameters_2.metadata_reg_a,
		 MLX5_ETH_WQE_FT_META_IPSEC);
	MLX5_SET(fte_match_param, spec->match_value, misc_parameters_2.metadata_reg_a,
		 MLX5_ETH_WQE_FT_META_IPSEC);

	flow_act.action = MLX5_FLOW_CONTEXT_ACTION_ALLOW |
			  MLX5_FLOW_CONTEXT_ACTION_IPSEC_ENCRYPT;
	rule = mlx5_add_flow_rules(priv->ipsec->ft_tx, spec, &flow_act, NULL, 0);
	if (IS_ERR(rule)) {
		err = PTR_ERR(rule);
		netdev_err(priv->netdev, "fail to add ipsec rule attrs->action=0x%x, err=%d\n", attrs->action, err);
		goto out;
	}

	sa_ctx->ipsec_rule.rule = rule;

out:
	kvfree(spec);
	return err;
}

int mlx5e_ipsec_fs_add_rule(void *context)
{
	struct mlx5e_ipsec_sa_entry *sa_entry = (struct mlx5e_ipsec_sa_entry *)context;
	struct mlx5_accel_esp_xfrm_attrs *attrs = &sa_entry->xfrm->attrs;
	struct mlx5e_priv *priv = (struct mlx5e_priv *)attrs->priv;

	if (attrs->action == MLX5_ACCEL_ESP_ACTION_DECRYPT)
		return ipsec_add_rx_rule(priv, sa_entry);
	else
		return ipsec_add_tx_rule(priv, sa_entry);
}

void mlx5e_ipsec_fs_del_rule(void *context)
{
	struct mlx5e_ipsec_sa_entry *sa_entry = (struct mlx5e_ipsec_sa_entry *)context;
	struct mlx5_accel_esp_xfrm_attrs *attrs = &sa_entry->xfrm->attrs;
	struct mlx5e_priv *priv = (struct mlx5e_priv *)attrs->priv;
	struct mlx5_ipsec_sa_ctx *sa_ctx = sa_entry->hw_context;

	if (attrs->action == MLX5_ACCEL_ESP_ACTION_DECRYPT)
		mlx5e_accel_fs_ref_prot(priv,
					attrs->is_ipv6 ?
					MLX5E_TT_IPV6_IPSEC_ESP : MLX5E_TT_IPV4_IPSEC_ESP,
					-1);

	if (sa_ctx->ipsec_rule.rule) {
		mlx5_del_flow_rules(sa_ctx->ipsec_rule.rule);
		sa_ctx->ipsec_rule.rule = NULL;
	}

	if (sa_ctx->ipsec_rule.set_modify_hdr) {
		mlx5_modify_header_dealloc(priv->mdev, sa_ctx->ipsec_rule.set_modify_hdr);
		sa_ctx->ipsec_rule.set_modify_hdr = NULL;
	}
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

int mlx5e_ipsec_create_tx_ft(struct mlx5e_priv *priv)
{
	return 0;
}

void mlx5e_ipsec_destroy_tx_ft(struct mlx5e_priv *priv) {}

#endif /* #ifdef CONFIG_MLX5_EN_IPSEC */
