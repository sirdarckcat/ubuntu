/*
 * INTEL CONFIDENTIAL
 *
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * This software and the related documents are Intel copyrighted materials,
 * and your use of them is governed by the express license under which they
 * were provided to you ("LICENSE"). Unless the LICENSE provides otherwise,
 * you may not use, modify, copy, publish, distribute, disclose or transmit
 * this software or the related documents without Intel's prior written
 * permission.
 *
 * This software and the related documents are provided as is, with no
 * express or implied warranties, other than those that are expressly
 * stated in the License.
 *
 */
/* SPDX-License-Identifier: MIT */
/*
 * Copyright(c) 2019 - 2021 Intel Corporation.
 *
 */

#if !defined(__TRACE_NL_H) || defined(TRACE_HEADER_MULTI_READ)
#define __TRACE_NL_H

#include <linux/tracepoint.h>
#include <linux/trace_seq.h>

#include "iaf_drv.h"
#include "ops.h"
#include "netlink.h"

#undef TRACE_SYSTEM
#define TRACE_SYSTEM iaf_nl

DECLARE_EVENT_CLASS(iaf_nl_template,
		    TP_PROTO(enum cmd_op r_cmd_op, u32 len, u32 snd_seq),
		    TP_ARGS(r_cmd_op, len, snd_seq),
		    TP_STRUCT__entry(__field(enum cmd_op, r_cmd_op)
				     __field(u32, len)
				     __field(u32, snd_seq)
				    ),
		    TP_fast_assign(__entry->r_cmd_op = r_cmd_op;
				   __entry->len = len;
				   __entry->snd_seq = snd_seq;
				  ),
		    TP_printk("cmd op %u len %u snd_seq %u",
			      __entry->r_cmd_op,
			      __entry->len,
			      __entry->snd_seq
			     )
		    );

DEFINE_EVENT(iaf_nl_template, nl_rsp,
	     TP_PROTO(enum cmd_op r_cmd_op, u32 len, u32 snd_seq),
	     TP_ARGS(r_cmd_op, len, snd_seq)
	    );

DEFINE_EVENT(iaf_nl_template, nl_req,
	     TP_PROTO(enum cmd_op r_cmd_op, u32 len, u32 snd_seq),
	     TP_ARGS(r_cmd_op, len, snd_seq)
	    );

#endif /* __TRACE_NL_H */

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE trace_nl
#include <trace/define_trace.h>
