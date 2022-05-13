#include <linux/blk-crypto.h>
#include <linux/blkdev.h>
#include <linux/device-mapper.h>
#include <linux/kernel.h>
#include <linux/log2.h>
#include <linux/math.h>
#include <linux/minmax.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>

#define DM_MSG_PREFIX	"blk-crypto"

#define SECTOR_SIZE	(1 << SECTOR_SHIFT)

static const struct blk_crypto_cipher {
	const char *name;
	enum blk_crypto_mode_num mode_num;
	int key_size;
} blk_crypto_ciphers[] = {
	{
		.name = "aes-xts-plain64",
		.mode_num = BLK_ENCRYPTION_MODE_AES_256_XTS,
		.key_size = 64,
	}, {
		.name = "aes-cbc-essiv",
	        .mode_num = BLK_ENCRYPTION_MODE_AES_128_CBC_ESSIV,
		.key_size = 16,
	}, {
		.name = "xchacha12,aes-adiantum-plain64",
		.mode_num = BLK_ENCRYPTION_MODE_ADIANTUM,
		.key_size = 32,
	},
};

enum flags { DM_BLK_CRYPTO_SUSPENDED, DM_BLK_CRYPTO_KEY_VALID };

struct dm_blk_crypto_config {
	struct dm_dev *dev;
	sector_t start;
	char *cipher_string;
	unsigned int data_unit_size;
	unsigned char dun_shift;
	u64 dun_offset;
	struct blk_crypto_key key;
	bool allow_fallback;
	unsigned long flags;
};

static const struct blk_crypto_cipher *
lookup_cipher(const char *cipher_string)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(blk_crypto_ciphers); i++) {
		if (strcmp(cipher_string, blk_crypto_ciphers[i].name) == 0)
			return &blk_crypto_ciphers[i];
	}
	return NULL;
}

static int get_key(u8 *key, size_t len, char *key_string)
{
	int sz;

	sz = min(strlen(key_string) >> 1, len);

	if (hex2bin(key, key_string, sz) != 0)
		return -EINVAL;

	return sz;
}

static int blk_crypto_evict_and_wipe_key(struct dm_blk_crypto_config *bcc)
{
	int err;

	err = blk_crypto_evict_key(bdev_get_queue(bcc->dev->bdev), &bcc->key);
	if (err == -ENOKEY)
		err = 0;
	if (err)
		DMWARN("Failed to evict crypto key: %d", err);

	memzero_explicit(bcc->key.raw, sizeof(bcc->key.raw));
	clear_bit(DM_BLK_CRYPTO_KEY_VALID, &bcc->flags);

	return err;
}

static void blk_crypto_dtr(struct dm_target *ti)
{
	struct dm_blk_crypto_config *bcc = ti->private;

	if (bcc->dev) {
		blk_crypto_evict_and_wipe_key(bcc);
		dm_put_device(ti, bcc->dev);
	}

	kfree(bcc->cipher_string);
	kfree(bcc);
}

static int blk_crypto_ctr_optional(struct dm_target *ti,
				  unsigned int argc, char **argv)
{
	struct dm_blk_crypto_config *bcc = ti->private;
	struct dm_arg_set as;
	static const struct dm_arg _args[] = {
		{0, 3, "Invalid number of feature args"},
	};
	unsigned int opt_params;
	const char *opt_string;
	char dummy;
	int err;

	as.argc = argc;
	as.argv = argv;

	err = dm_read_arg_group(_args, &as, &opt_params, &ti->error);
	if (err)
		return err;

	while (opt_params--) {
		opt_string = dm_shift_arg(&as);
		if (!opt_string) {
			ti->error = "Not enough feature arguments";
			return -EINVAL;
		}
		if (!strcmp(opt_string, "allow_discards")) {
			ti->num_discard_bios = 1;
		} else if (!strcmp(opt_string, "allow_fallback")) {
			bcc->allow_fallback = true;
		} else if (sscanf(opt_string, "data_unit_size:%u%c", &bcc->data_unit_size, &dummy) == 1) {
			if (bcc->data_unit_size < SECTOR_SIZE ||
			    bcc->data_unit_size > 4096 ||
			    !is_power_of_2(bcc->data_unit_size)) {
				ti->error = "Invalid data_unit_size";
				return -EINVAL;
			}
			if (ti->len & ((bcc->data_unit_size >> SECTOR_SHIFT) - 1)) {
				ti->error = "Device size is not a multiple of data_unit_size";
				return -EINVAL;
			}
		} else {
			ti->error = "Invalid feature arguments";
			return -EINVAL;
		}
	}

	return 0;
}

static int blk_crypto_ctr(struct dm_target *target,
			 unsigned int argc, char **argv)
{
	struct dm_blk_crypto_config *bcc;
	const struct blk_crypto_cipher *cipher;
	unsigned int key_size;
	u8 key[BLK_CRYPTO_MAX_KEY_SIZE];
	u64 max_dun;
	unsigned int dun_bytes;
	char dummy;
	int err = -EINVAL;

	bcc = kzalloc(sizeof(*bcc), GFP_KERNEL);
	if (!bcc) {
		target->error = "Out of memory";
		return -ENOMEM;
	}
	target->private = bcc;

	if (argc < 5) {
		target->error = "Not enough arguments";
		goto bad;
	}

	/* optional arguments */
	bcc->data_unit_size = SECTOR_SIZE;
	if (argc > 5) {
		err = blk_crypto_ctr_optional(target, argc - 5, &argv[5]);
		if (err)
			goto bad;
	}

	bcc->dun_shift = ilog2(bcc->data_unit_size) - SECTOR_SHIFT;

	/* <cipher> */
	bcc->cipher_string = kstrdup(argv[0], GFP_KERNEL);
	if (!bcc->cipher_string) {
		target->error = "Out of memory";
		err = -ENOMEM;
		goto bad;
	}

	cipher = lookup_cipher(bcc->cipher_string);
	if (!cipher) {
		target->error = "Unsupported cipher";
		goto bad;
	}

	/* <key> */
	key_size = get_key(key, sizeof(key), argv[1]);
	if (key_size < 0) {
		err = key_size;
		target->error = "Invalid key string";
		goto bad;
	}
	if (key_size != cipher->key_size) {
		target->error = "Invalid keysize";
		goto bad;
	}

	/* <dun_offset> */
	if (sscanf(argv[2], "%llu%c", &bcc->dun_offset, &dummy) != 1) {
		target->error = "Invalid dun_offset";
		goto bad;
	}

	/* <dev_path> */
	err = dm_get_device(target, argv[3], dm_table_get_mode(target->table),
			    &bcc->dev);
	if (err) {
		target->error = "Device lookup failed";
		goto bad;
	}

	/* <start> */
	if (sscanf(argv[4], "%llu%c", &bcc->start, &dummy) != 1) {
		target->error = "Invalid device start sector";
		goto bad;
	}

	/* Initialize the key. */
	max_dun = (target->len >> (bcc->dun_shift + SECTOR_SHIFT)) + bcc->dun_offset;
	if (max_dun < bcc->dun_offset)
		dun_bytes = sizeof(sector_t) + 1;
	else
		dun_bytes = DIV_ROUND_UP(fls64(max_dun), 8);

	err = blk_crypto_init_key(&bcc->key, key, cipher->mode_num,
				  dun_bytes, bcc->data_unit_size);
	if (err) {
		target->error = "Error initializing blk_crypto_key";
		goto bad;
	}

	err = blk_crypto_start_using_key(&bcc->key,
					 bdev_get_queue(bcc->dev->bdev),
					 bcc->allow_fallback);
	if (err) {
		target->error = "Error starting to use blk_crypto_key";
		goto bad;
	}
	set_bit(DM_BLK_CRYPTO_KEY_VALID, &bcc->flags);

	target->num_flush_bios = 1;

	err = 0;
	goto out;

bad:
	blk_crypto_dtr(target);
out:
	memzero_explicit(key, sizeof(key));
	return err;
}

static int blk_crypto_map(struct dm_target *ti, struct bio *bio)
{
	const struct dm_blk_crypto_config *bcc = ti->private;
	sector_t sector_in_target;
	u64 dun[BLK_CRYPTO_DUN_ARRAY_SIZE] = { 0 };

	/* Map the bio to the underlying device. */
	bio_set_dev(bio, bcc->dev->bdev);

	/*
	 * If the bio is a device-level request which doesn't target a specific
	 * sector, there's nothing more to do.
	 */
	if (bio_sectors(bio) == 0)
		return DM_MAPIO_REMAPPED;

	/*
	 * Ensure that bio is a multiple of the encryption data unit size and is
	 * aligned to this size as defined in IO hints.
	 */
	if (unlikely((bio->bi_iter.bi_sector & ((bcc->data_unit_size >> SECTOR_SHIFT) - 1)) != 0))
		return DM_MAPIO_KILL;

	if (unlikely(bio->bi_iter.bi_size & (bcc->data_unit_size - 1)))
		return DM_MAPIO_KILL;

	/* Map the bio's sector to the underlying device. */
	sector_in_target = dm_target_offset(ti, bio->bi_iter.bi_sector);
	bio->bi_iter.bi_sector = bcc->start + sector_in_target;

	/*
	 * If the bio doesn't have any data (e.g. if it's a DISCARD request),
	 * there's nothing more to do.
	 */
	if (!bio_has_data(bio))
		return DM_MAPIO_REMAPPED;

	/*
	 * Else, dm-blk-crypt needs to set this bio's encryption context.
	 * It must not already have one.
	 */
	if (WARN_ON_ONCE(bio_has_crypt_ctx(bio)))
		return DM_MAPIO_KILL;

	/*
	 * Calculate the DUN for this encryption sector. Note that
	 * sector_in_target refers to 512-byte sectors and we checked
	 * earlier that this was aligned to the data unit size.
	 */
	dun[0] = sector_in_target >> bcc->dun_shift;
	dun[0] += bcc->dun_offset;
	if (dun[0] < bcc->dun_offset)
		dun[1] = 1;

	bio_crypt_set_ctx(bio, &bcc->key, dun, GFP_NOIO);

	return DM_MAPIO_REMAPPED;
}

static void blk_crypto_postsuspend(struct dm_target *ti)
{
	struct dm_blk_crypto_config *bcc = ti->private;

	set_bit(DM_BLK_CRYPTO_SUSPENDED, &bcc->flags);
}

static int blk_crypto_preresume(struct dm_target *ti)
{
	struct dm_blk_crypto_config *bcc = ti->private;

	if (!test_bit(DM_BLK_CRYPTO_KEY_VALID, &bcc->flags)) {
		DMERR("aborting resume - key is not set.");
		return -EAGAIN;
	}

	return 0;
}

static void blk_crypto_resume(struct dm_target *ti)
{
	struct dm_blk_crypto_config *bcc = ti->private;

	clear_bit(DM_BLK_CRYPTO_SUSPENDED, &bcc->flags);
}

static void blk_crypto_status(struct dm_target *ti, status_type_t status_type,
			     unsigned status_flags, char *result,
			     unsigned maxlen)
{
	const struct dm_blk_crypto_config *bcc = ti->private;
	unsigned int i, sz = 0;
	int num_feature_args = 0;

	switch (status_type) {
	case STATUSTYPE_INFO:
		result[0] = '\0';
		break;

	case STATUSTYPE_TABLE:
		DMEMIT("%s ", bcc->cipher_string);

		for (i = 0; i < bcc->key.size; i++)
			DMEMIT("%02x", bcc->key.raw[i]);

		DMEMIT(" %llu %s %llu", (unsigned long long)bcc->dun_offset,
		       bcc->dev->name, (unsigned long long)bcc->start);

		num_feature_args += !!ti->num_discard_bios;
		num_feature_args += !!bcc->allow_fallback;
		if (bcc->data_unit_size != SECTOR_SIZE)
			num_feature_args += 1;
		if (num_feature_args != 0) {
			DMEMIT(" %d", num_feature_args);
			if (ti->num_discard_bios)
				DMEMIT(" allow_discards");
			if (bcc->allow_fallback)
				DMEMIT(" allow_fallback");
			if (bcc->data_unit_size != SECTOR_SIZE)
				DMEMIT(" data_unit_size:%u", bcc->data_unit_size);
		}
		break;
	case STATUSTYPE_IMA:
		DMEMIT_TARGET_NAME_VERSION(ti->type);
		DMEMIT(",allow_discards=%c", ti->num_discard_bios ? 'y' : 'n');
		DMEMIT(",using_fallback=%c",
		       blk_crypto_config_supported(bdev_get_queue(bcc->dev->bdev),
						   &bcc->key.crypto_cfg, false) ?
		       'n' : 'y');
		if (bcc->data_unit_size != SECTOR_SIZE)
			DMEMIT(",data_unit_size=%d", bcc->data_unit_size);
		if (bcc->cipher_string)
			DMEMIT(",cipher_string=%s", bcc->cipher_string);

		DMEMIT(",key_size=%u", bcc->key.size);
		DMEMIT(";");
		break;
	}
}

static int blk_crypto_message(struct dm_target *ti, unsigned argc, char **argv,
			     char *result, unsigned maxlen)
{
	struct dm_blk_crypto_config *bcc = ti->private;
	int key_size;

	if (argc < 2)
		goto error;

	if (!strcasecmp(argv[0], "key")) {
		if (!test_bit(DM_BLK_CRYPTO_SUSPENDED, &bcc->flags)) {
			DMWARN("not suspended during key manipulation.");
			return -EINVAL;
		}
		if (argc == 3 && !strcasecmp(argv[1], "set")) {
			key_size = get_key(bcc->key.raw, sizeof(bcc->key.raw), argv[2]);
			if (key_size < 0 || key_size != bcc->key.size) {
				memzero_explicit(bcc->key.raw, sizeof(bcc->key.raw));
				return -EINVAL;
			}

			set_bit(DM_BLK_CRYPTO_KEY_VALID, &bcc->flags);
			return 0;
		}
		if (argc == 2 && !strcasecmp(argv[1], "wipe")) {
			return blk_crypto_evict_and_wipe_key(bcc);
		}
	}

error:
	DMWARN("unrecognised message received.");
	return -EINVAL;
}

static int blk_crypto_iterate_devices(struct dm_target *ti,
				     iterate_devices_callout_fn fn, void *data)
{
	const struct dm_blk_crypto_config *bcc = ti->private;

	return fn(ti, bcc->dev, bcc->start, ti->len, data);
}

static void blk_crypto_io_hints(struct dm_target *ti,
			       struct queue_limits *limits)
{
	const struct dm_blk_crypto_config *bcc = ti->private;
	const unsigned int data_unit_size = bcc->data_unit_size;

	limits->logical_block_size =
		max_t(unsigned short, limits->logical_block_size, data_unit_size);
	limits->physical_block_size =
		max_t(unsigned int, limits->physical_block_size, data_unit_size);
	limits->io_min = max_t(unsigned int, limits->io_min, data_unit_size);
}

static struct target_type blk_crypto_target = {
	.name			= "blk-crypto",
	.features		= DM_TARGET_PASSES_INTEGRITY,
	.version		= {0, 0, 1},
	.module			= THIS_MODULE,
	.ctr			= blk_crypto_ctr,
	.dtr			= blk_crypto_dtr,
	.map			= blk_crypto_map,
	.postsuspend		= blk_crypto_postsuspend,
	.preresume		= blk_crypto_preresume,
	.resume			= blk_crypto_resume,
	.status			= blk_crypto_status,
	.message		= blk_crypto_message,
	.iterate_devices	= blk_crypto_iterate_devices,
	.io_hints		= blk_crypto_io_hints,
};

static int __init dm_blk_crypto_init(void)
{
	return dm_register_target(&blk_crypto_target);
}

static void __exit dm_blk_crypto_exit(void)
{
	dm_unregister_target(&blk_crypto_target);
}

module_init(dm_blk_crypto_init);
module_exit(dm_blk_crypto_exit);

MODULE_AUTHOR("Chris Coulson <chris.coulson@canonical.com>");
MODULE_DESCRIPTION(DM_NAME " target for transparent encryption / decryption using an inline crypto engine");
MODULE_LICENSE("GPL");
