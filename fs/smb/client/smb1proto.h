/* SPDX-License-Identifier: LGPL-2.1 */
/*
 *
 *   Copyright (c) International Business Machines  Corp., 2002,2008
 *   Author(s): Steve French (sfrench@us.ibm.com)
 *
 */
#ifndef _SMB1PROTO_H
#define _SMB1PROTO_H

struct cifs_unix_set_info_args {
	__u64	ctime;
	__u64	atime;
	__u64	mtime;
	__u64	mode;
	kuid_t	uid;
	kgid_t	gid;
	dev_t	device;
};

#ifdef CONFIG_CIFS_ALLOW_INSECURE_LEGACY

/*
 * cifssmb.c
 */
int CIFSTCon(const unsigned int xid, struct cifs_ses *ses, const char *tree,
	     struct cifs_tcon *tcon, const struct nls_table *nls_codepage);

/*
 * smb1ops.c
 */
extern struct smb_version_operations smb1_operations;
extern struct smb_version_values smb1_values;
void reset_cifs_unix_caps(unsigned int xid, struct cifs_tcon *tcon,
			  struct cifs_sb_info *cifs_sb,
			  struct smb3_fs_context *ctx);

/*
 * smb1transport.c
 */
bool cifs_check_trans2(struct mid_q_entry *mid, struct TCP_Server_Info *server,
		       char *buf, int malformed);

#endif /* CONFIG_CIFS_ALLOW_INSECURE_LEGACY */
#endif /* _SMB1PROTO_H */
