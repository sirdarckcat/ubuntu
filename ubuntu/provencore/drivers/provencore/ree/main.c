// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, ProvenRun S.A.S
 */
/**
 * @file
 * @brief
 * @author Henri Chataing
 * @date December 19th, 2017 (creation)
 * @copyright (c) 2017-2021, Prove & Run and/or its affiliates.
 *   All rights reserved.
 */

#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/semaphore.h>
#include <linux/smp.h> /* get_cpu, put_cpu */
#include <linux/types.h>
#include <linux/version.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/slab.h>

#ifdef CONFIG_PROVENCORE_DTS_CONFIGURATION
#include <linux/of_address.h>
#include <linux/of_irq.h>
#else
#ifdef CONFIG_IRQ_DOMAIN
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#endif /* CONFIG_IRQ_DOMAIN */
#endif /* CONFIG_PROVENCORE_DTS_CONFIGURATION */

#include <asm/ioctl.h>

#include "internal.h"
#include "shm.h"
#include "ree.h"
#include "session.h"
#include "smc.h"

#ifndef CONFIG_PROVENCORE_DTS_CONFIGURATION
#ifndef CONFIG_PROVENCORE_NON_SECURE_IRQ
/* Default SGI used by ProvenCore to notify the non-secure world. */
#define CONFIG_PROVENCORE_NON_SECURE_IRQ  14
#endif

#ifdef CONFIG_IRQ_DOMAIN
#ifndef CONFIG_PROVENCORE_IRQ_DOMAIN_NUMBER
/* Default number is for unused SGI domain: used in interrupt controller to
 * handle CONFIG_PROVENCORE_NON_SECURE_IRQ as a generic IRQ...
 */
#define CONFIG_PROVENCORE_IRQ_DOMAIN_NUMBER 2
#endif

/* Minimal num for SPI index */
#define LINUX_MIN_SPI           32

/* Maximal num for SGI index */
#define LINUX_MAX_SGI           16

/* Check NS-->S IRQ setup is valid:
 *  - CONFIG_PROVENCORE_IRQ_DOMAIN_NUMBER set to 0 means IRQ is a SPI
 *  - CONFIG_PROVENCORE_IRQ_DOMAIN_NUMBER set to 2 means IRQ is a SGI
 */
_Static_assert((
    ((CONFIG_PROVENCORE_IRQ_DOMAIN_NUMBER == 0) && (CONFIG_PROVENCORE_NON_SECURE_IRQ >= LINUX_MIN_SPI)) ||
    ((CONFIG_PROVENCORE_IRQ_DOMAIN_NUMBER == 2) && (CONFIG_PROVENCORE_NON_SECURE_IRQ <= LINUX_MAX_SGI)) ),
    "invalid secure to non secure IRQ setup");
#endif /* CONFIG_IRQ_DOMAIN */
#endif /* !CONFIG_PROVENCORE_DTS_CONFIGURATION */

#define TZ_IOCTL_ALLOC              1
#define TZ_IOCTL_FREE               2
#define TZ_IOCTL_SEND_OBSOLETE      3  /* To be replaced by TZ_IOCTL_SEND_REQ+TZ_IOCTL_GET_REQ+TZ_IOCTL_WAIT_REQ */
#define TZ_IOCTL_CONFIG_PROHIBITED  4  /* TZ_IOCTL_CONFIG_SID is the only available config method */
#define TZ_IOCTL_STATUS             5
#define TZ_IOCTL_CONFIG_SID         6
#define TZ_IOCTL_SEND_EXT_OBSOLETE  7 /* To be replaced by TZ_IOCTL_SEND_REQ+TZ_IOCTL_GET_REQ+TZ_IOCTL_WAIT_REQ */
#define TZ_IOCTL_VERSION            8224

#define TZ_IOCTL_SEND_RESP          8
#define TZ_IOCTL_GET_RESP           9
#define TZ_IOCTL_WAIT_RESP          10
#define TZ_IOCTL_SEND_REQ           11
#define TZ_IOCTL_GET_REQ            12
#define TZ_IOCTL_WAIT_REQ           13
#define TZ_IOCTL_CANCEL_REQ         14
#define TZ_IOCTL_SEND_SIGNAL        15
#define TZ_IOCTL_GET_SIGNAL         16
#define TZ_IOCTL_WAIT_SIGNAL        17
#define TZ_IOCTL_WAIT_EVENT         18
#define TZ_IOCTL_GET_PENDING_EVENTS 19

/**
 * @brief Parameter vector for the \ref TZ_IOCTL_SEND_EXT_OBSOLETE request.
 */
struct pnc_send_params_obsolete
{
    uint32_t type;      /**< Input request type */
    uint32_t flags;     /**< Input request flags */
    uint32_t timeout;   /**< Optional input request timeout */
    uint32_t status;    /**< Output status code. */
};

typedef struct pnc_send_params_obsolete pnc_send_params_obsolete_t;

/**
 * Module parameter. Set the number of physical pages to be allocated by
 *  \ref pnc_init.
 */
static unsigned int order = 9;

/**
 * Page frame number (pfn) of the first page in the allocated shared memory
 * range.
 */
static unsigned long _base = 0;
static void *_vbase;
static unsigned long _nr_pages;

/** /dev/trustzone device registration information */
static bool _device_registered = false;

/** Secure IRQ (possibly virtual). */
unsigned int _irq;

/* ========================================================================== *
 *   File operations                                                          *
 * ========================================================================== */


#ifndef VM_RESERVED
#define VM_RESERVED   (VM_DONTEXPAND | VM_DONTDUMP)
#endif

static void pnc_vma_open(struct vm_area_struct *vma)
{
}

static void pnc_vma_close(struct vm_area_struct *vma)
{
}

static struct vm_operations_struct pnc_mmap_vm_ops = {
        .open =     pnc_vma_open,
        .close =    pnc_vma_close,
};

static int pnc_miscdev_mmap(struct file *filp, struct vm_area_struct *vma)
{
    pnc_session_t *s = filp->private_data;
    unsigned long mem_offset, mem_nr_pages;
    unsigned long offset, nr_pages;
    unsigned long i, pfn;
    int r;

    offset = vma->vm_pgoff;
    nr_pages = (vma->vm_end - vma->vm_start) >> PAGE_SHIFT;

    if (pnc_session_get_mem_offset(s, &mem_offset, &mem_nr_pages) < 0) {
        pr_err("(%s) no configured memory range\n", __func__);
        return -ENODEV;
    }

    pr_debug("(%s) pid=%d\n", __func__, current->pid);

    if ((vma->vm_flags & VM_SHARED) == 0) {
        pr_err("(%s) mapping must be shared\n", __func__);
        return -EINVAL;
    }
    if (offset >= mem_nr_pages || offset + nr_pages > mem_nr_pages) {
        pr_err("(%s) mapping out of bounds\n", __func__);
        return -EINVAL;
    }

    pr_debug("(%s) [%#.8lx - %#.8lx]", __func__,
        (mem_offset + offset) << PAGE_SHIFT,
        (mem_offset + offset + nr_pages) << PAGE_SHIFT);
    pr_debug("    => [%#.8lx - %#.8lx]\n", vma->vm_start, vma->vm_end);

    vma->vm_ops = &pnc_mmap_vm_ops;
    vma->vm_flags |= VM_RESERVED;

    pfn = _base + mem_offset + offset;
    for (i = 0; i < nr_pages; i++, pfn++) {
        r = vm_insert_page(vma, vma->vm_start + i * PAGE_SIZE, pfn_to_page(pfn));
        if (r != 0) {
            pr_err("(%s) failed to insert page (%d)\n", __func__, r);
            return r;
        }
    }
    return 0;
}

static int pnc_miscdev_open(struct inode *inode, struct file *filp)
{
    return pnc_session_open_with_flags((pnc_session_t **)&filp->private_data,
        filp->f_flags);
}

static int pnc_miscdev_release(struct inode *inode, struct file *filp)
{
    pnc_session_close(filp->private_data);
    filp->private_data = NULL;
    return 0;
}

/**
 * !!!!!!!!!!!!!! Obsolete starting REEV3 !!!!!!!!!!!!!!!!!!!!!!
 *
 * @brief Handle a send request with extended parameters.
 * @param s             User session
 * @param params        User virtual address of the parameter vector
 * @return              - O on success
 *                      - -EINVAL if the parameters in \p params could not be
 *                          accessed
 *                      - an error code otherwise
 */
static int pnc_send_ext(pnc_session_t *s, pnc_send_params_obsolete_t __user *params)
{
    pnc_send_params_obsolete_t loc_params;
    int res;

    /* Read input parameters. */
    res = copy_from_user(&loc_params, params, sizeof(loc_params));
    if (res != 0) {
        return -EINVAL;
    }

    /* Send the request to provencore. */
    res = pnc_session_request(s, loc_params.type, loc_params.flags,
                              loc_params.timeout, &loc_params.status);
    if (res != 0) {
        return res;
    }

    /* Write return values. */
    res = copy_to_user(params, &loc_params, sizeof(loc_params));
    if (res != 0) {
        return -EINVAL;
    }

    return 0;
}

/**
 * @brief Parameter vector for ioctl needing it..
 */
typedef struct pnc_ioctl_params {
    uint32_t sent;      /**< Sent value */
    uint32_t returned;  /**< Returned value */
    uint32_t timeout;   /**< Optional timeout to wait for event(s), NO_TIMEOUT otherwise */
} pnc_ioctl_params_t;

static long pnc_miscdev_ioctl(struct file *filp, unsigned int cmd,
                             unsigned long arg)
{
    pnc_session_t *s = filp->private_data;
    int ret = 0;
    uint32_t val;
    pnc_ioctl_params_t ioctl_params;

    pr_debug("(%s) cmd=%d arg=%ld pid=%d tgid=%d\n", __func__, cmd, arg,
        current->pid, current->tgid);

    switch (cmd & 0xffff) {
        case TZ_IOCTL_VERSION:
            ret = pnc_session_get_version(s, &val);
            if (ret == 0) {
                ret = copy_to_user((void *)arg, &val, sizeof(uint32_t));
                if (ret != 0) {
                    pr_err("(%s) TZ_IOCTL_VERSION copy failure (%d).\n",
                        __func__, ret);
                }
            }
            break;
        case TZ_IOCTL_ALLOC:
            ret = pnc_session_alloc(s, arg);
            break;
        case TZ_IOCTL_CONFIG_SID:
            ret = pnc_session_config(s, arg);
            break;
        case TZ_IOCTL_SEND_OBSOLETE:
            ret = pnc_session_request(s, arg, cmd >> 16, 0, NULL);
            break;
        case TZ_IOCTL_SEND_EXT_OBSOLETE:
            ret = pnc_send_ext(s, (void *)arg);
            break;
        case TZ_IOCTL_CONFIG_PROHIBITED:
            /* Legacy. Should never happen if NS userland API is used... */
            pr_err("This config method is prohibited.\n");
            pr_err("Use TZ_IOCTL_CONFIG_SID instead for session configuration.\n");
            ret = -ENOTSUPP;
            break;
        case TZ_IOCTL_SEND_RESP:
            ret = pnc_session_send_response(s, arg);
            break;
        case TZ_IOCTL_GET_RESP:
            ret = pnc_session_get_response(s, &val);
            if (ret == 0) {
                ret = copy_to_user((void *)arg, &val, sizeof(uint32_t));
                if (ret != 0) {
                    pr_err("(%s) TZ_IOCTL_GET_RESP copy failure (%d).\n",
                        __func__, ret);
                }
            }
            break;
        case TZ_IOCTL_WAIT_RESP:
            ret = copy_from_user(&ioctl_params, (void *)arg, sizeof(ioctl_params));
            if (ret == 0) {
                ret = pnc_session_wait_response(s, &ioctl_params.returned,
                    ioctl_params.timeout);
                if (ret == 0) {
                    ret = copy_to_user((void *)arg, &ioctl_params, sizeof(ioctl_params));
                    if (ret != 0) {
                        pr_err("(%s) TZ_IOCTL_WAIT_RESP copy 2 failure (%d).\n",
                            __func__, ret);
                    }
                }
            } else {
                pr_err("(%s) TZ_IOCTL_WAIT_RESP copy 1 failure (%d).\n",
                    __func__, ret);
            }
            break;
        case TZ_IOCTL_SEND_REQ:
            ret = pnc_session_send_request(s, arg);
            break;
        case TZ_IOCTL_GET_REQ:
            ret = pnc_session_get_request(s, &val);
            if (ret == 0) {
                ret = copy_to_user((void *)arg, &val, sizeof(uint32_t));
                if (ret != 0) {
                    pr_err("(%s) TZ_IOCTL_GET_REQ copy failure (%d).\n",
                        __func__, ret);
                }
            }
            break;
        case TZ_IOCTL_WAIT_REQ:
            ret = copy_from_user(&ioctl_params, (void *)arg, sizeof(ioctl_params));
            if (ret == 0) {
                ret = pnc_session_wait_request(s, &ioctl_params.returned,
                    ioctl_params.timeout);
                if (ret == 0) {
                    ret = copy_to_user((void *)arg, &ioctl_params, sizeof(ioctl_params));
                    if (ret != 0) {
                        pr_err("(%s) TZ_IOCTL_WAIT_REQ copy 2 failure (%d).\n",
                            __func__, ret);
                    }
                }
            } else {
                pr_err("(%s) TZ_IOCTL_WAIT_REQ copy 1 failure (%d).\n",
                    __func__, ret);
            }
            break;
        case TZ_IOCTL_CANCEL_REQ:
            ret = copy_from_user(&ioctl_params, (void *)arg, sizeof(ioctl_params));
            if (ret == 0) {
                ret = pnc_session_cancel_request(s, &ioctl_params.returned,
                        ioctl_params.timeout);
                if (ret == REQUEST_CANCEL_RESPONSE) {
                    ret = copy_to_user((void *)arg, &ioctl_params, sizeof(ioctl_params));
                    if (ret == 0) {
                        ret = REQUEST_CANCEL_RESPONSE;
                    } else {
                        pr_err("(%s) TZ_IOCTL_CANCEL_REQ copy 1 failure (%d).\n",
                            __func__, ret);
                    }
                }
            } else {
                pr_err("(%s) TZ_IOCTL_CANCEL_REQ copy 2 failure (%d).\n",
                    __func__, ret);
            }
            break;
        case TZ_IOCTL_SEND_SIGNAL:
            ret = pnc_session_send_signal(s, arg);
            break;
        case TZ_IOCTL_GET_SIGNAL:
            ret = pnc_session_get_signal(s, &val);
            if (ret == 0) {
                ret = copy_to_user((void *)arg, &val, sizeof(uint32_t));
                if (ret != 0) {
                    pr_err("(%s) TZ_IOCTL_GET_SIGNAL copy failure (%d).\n",
                        __func__, ret);
                }
            }
            break;
        case TZ_IOCTL_WAIT_SIGNAL:
            ret = copy_from_user(&ioctl_params, (void *)arg, sizeof(ioctl_params));
            if (ret == 0) {
                ret = pnc_session_wait_signal(s, &ioctl_params.returned,
                        ioctl_params.timeout);
                if (ret == 0) {
                    ret = copy_to_user((void *)arg, &ioctl_params, sizeof(ioctl_params));
                    if (ret != 0) {
                        pr_err("(%s) TZ_IOCTL_WAIT_SIGNAL copy 2 failure (%d).\n",
                            __func__, ret);
                    }
                }
            } else {
                pr_err("(%s) TZ_IOCTL_WAIT_SIGNAL copy 1 failure (%d).\n",
                    __func__, ret);
            }
            break;
        case TZ_IOCTL_WAIT_EVENT:
            ret = copy_from_user(&ioctl_params, (void *)arg, sizeof(ioctl_params));
            if (ret == 0) {
                ret = pnc_session_wait_event(s, &ioctl_params.returned,
                        ioctl_params.sent, ioctl_params.timeout);
                if (ret == 0) {
                    ret = copy_to_user((void *)arg, &ioctl_params, sizeof(ioctl_params));
                    if (ret != 0) {
                        pr_err("(%s) TZ_IOCTL_WAIT_EVENT copy 2 failure (%d).\n",
                            __func__, ret);
                    }
                }
            } else {
                pr_err("(%s) TZ_IOCTL_WAIT_EVENT copy 1 failure (%d).\n",
                    __func__, ret);
            }
            break;
        case TZ_IOCTL_GET_PENDING_EVENTS:
            ret = pnc_session_get_pending_events(s, &ioctl_params.returned);
            if (ret == 0) {
                ret = copy_to_user((void *)arg, &ioctl_params, sizeof(ioctl_params));
                if (ret != 0) {
                    pr_err("(%s) TZ_IOCTL_GET_PENDING_EVENTS copy failure (%d).\n",
                        __func__, ret);
                }
            }
            break;
        default:
            ret = -ENOTTY;
            break;
    }
    return ret;
}

static __poll_t pnc_miscdev_poll(struct file *filp, poll_table *wait)
{
    pnc_session_t *s = filp->private_data;

    pr_debug("(%s)\n", __func__);

    return pnc_session_poll_wait(s, filp, wait);
}


static const struct file_operations pnc_miscdev_fops = {
    .owner = THIS_MODULE,
    .open = pnc_miscdev_open,
    .release = pnc_miscdev_release,
    .mmap = pnc_miscdev_mmap,
    .unlocked_ioctl = pnc_miscdev_ioctl,
    .poll = pnc_miscdev_poll,
};


/* ========================================================================== *
 *   Module initialisation                                                    *
 * ========================================================================== */


static struct miscdevice pnc_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "trustzone",
    .fops = &pnc_miscdev_fops,
};

#ifdef CONFIG_PROVENCORE_DTS_CONFIGURATION

/**
 * @brief Lookup the location of the reserved memory in the boot parameters.
 * @param paddr          Updated with the start address of the reserved memory
 * @param size          Updated with the size of the reserved memory
 * @param irq           Updated with the virtual irq line triggered by pnc
 * @return              - 0 on success
 *                      - -ENOENT if no compatible region is found
 */
static int pnc_find_reserved_mem_bootargs(unsigned long *paddr, u64 *size,
                                         int *irq)
{
    (void)paddr;
    (void)size;
    (void)irq;
    /* @todo */
    return -ENOENT;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,15,0)

/**
 * @brief Lookup the location of the reserved memory in the reserved-memory
 *  DTB node.
 * @param paddr         Updated with the start address of the reserved memory
 * @param size          Updated with the size of the reserved memory
 * @param irq           Updated with the virtual irq line triggered by pnc
 * @return              - 0 on success
 *                      - -ENOENT if no compatible region is found
 */
static int pnc_find_reserved_mem_of(unsigned long *paddr, u64 *size, int *irq)
{
    struct device_node *node, *node2;
    struct resource res;
    int ret = 0;

    node = of_find_node_by_path("/reserved-memory");
    if (!node) {
        pr_err("(%s) failed to locate reserved-memory node\n", __func__);
        return -ENOENT;
    }

    node2 = of_find_compatible_node(node, NULL, "pnc,reserved");
    if (!node2) {
        pr_err("(%s) failed to locate pnc,reserved compatible node\n", __func__);
        ret = -ENOENT;
        goto put_node;
    }

    /* Supports only one range in the node reg property. */
    if (of_address_to_resource(node2, 0, &res)) {
        pr_err("(%s) failed to read reg field in pnc,reserved node\n",
            __func__);
        ret = -ENOENT;
        goto put_node2;
    }

    *paddr = res.start;
    *size = resource_size(&res);
    *irq = irq_of_parse_and_map(node2, 0);

    pr_info("(%s) paddr size irq : 0x%lx 0x%llx %d\n", __func__,  *paddr,
        (unsigned long long)*size, *irq);

put_node2:
    of_node_put(node2);
put_node:
    of_node_put(node);
    return ret;
}

#endif /* >= v3.15.0 */

/**
 * @brief Lookup the location of the reserved memory in the DTB.
 * @param pfn           Updated with the pfn of the first page in the region
 * @param nr_pages      Updated with the number of reserved pages
 * @param irq           Updated with the virtual irq line triggered by pnc
 * @return              - 0 on success
 *                      - -ENOENT if no compatible region is found
 */
static int pnc_find_reserved_mem(unsigned long *pfn, unsigned long *nr_pages, int *irq)
{
    unsigned long paddr;
    unsigned long p, pfn_last;
    u64 size;
    int ret;
    int local_irq;

    ret = pnc_find_reserved_mem_bootargs(&paddr, &size, &local_irq);
    if (ret == 0) {
        goto found;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,15,0)
    ret = pnc_find_reserved_mem_of(&paddr, &size, &local_irq);
    if (ret == 0) {
        goto found;
    }
#endif
    return -ENOENT;

found:
    /*
     * Make sure the struct page objects are available for all pages in the
     * region.
     * Note: pfn_valid is undefined for arm and arm64 if
     * !defined(CONFIG_HAVE_ARCH_PFN_VALID).
     */

    pfn_last = (paddr + size - 1) / PAGE_SIZE;
    for (p = paddr / PAGE_SIZE; p <= pfn_last; ++p) {
        if (!pfn_valid(p)) {
            pr_err("(%s) the reserved memory is not in the memory map\n",
                __func__);
            return -EINVAL;
        }
    }

    if (local_irq == 0) {
        pr_err("(%s) could not find irq field in the reserved memory.", __func__);
        return -EINVAL;
    }

    *pfn = paddr / PAGE_SIZE;
    *nr_pages = size / PAGE_SIZE;
    *irq = local_irq;
    return 0;
}

#else

#ifdef CONFIG_IRQ_DOMAIN

#ifndef CONFIG_PROVENCORE_INTERRUPT_CONTROLLER_COMPATIBLE_NODE
#define CONFIG_PROVENCORE_INTERRUPT_CONTROLLER_COMPATIBLE_NODE "arm,gic-v3"
#endif

#ifndef CONFIG_PROVENCORE_INTERRUPT_CONTROLLER_NODE_NAME
#define CONFIG_PROVENCORE_INTERRUPT_CONTROLLER_NODE_NAME "interrupt-controller"
#endif

static unsigned int pnc_create_sgi(irq_hw_number_t hwirq)
{
    struct of_phandle_args args;
    struct device_node *ic;
    unsigned int virq;

    /*
     * Check irqchip/irq-gic-v3.c for new compatibilities
     * added with IRQCHIP_DECLARE
     */
    ic = of_find_compatible_node(NULL, NULL,
            CONFIG_PROVENCORE_INTERRUPT_CONTROLLER_COMPATIBLE_NODE);
    if (ic != NULL)
        goto create_sgi;

    /*
     * Caution : the node is not always named 'interrupt-controller'
     * Other option would be to find nodes with compatibility
     * in the list
     *      arm,cortex-a15-gic
     *      arm,cortex-a9-gic
     *      ..
     * (full list in irqchip/irq-gic.c)
     */
    ic = of_find_node_by_name(NULL,
            CONFIG_PROVENCORE_INTERRUPT_CONTROLLER_NODE_NAME);

    if (ic == NULL) {
        pr_warn("pnc: (pnc_create_sgi) failed to locate %s and %s\n",
                CONFIG_PROVENCORE_INTERRUPT_CONTROLLER_COMPATIBLE_NODE,
                CONFIG_PROVENCORE_INTERRUPT_CONTROLLER_NODE_NAME);
        pr_warn("pnc: (pnc_create_sgi) other options can be found in"
                "the file irqchip/irq-gic.c or the platform's dtb file");
        return 0;
    }

    if (ic == NULL) {
        pr_warn("(%s) failed to locate interrupt controller node\n", __func__);
        return 0;
    }

create_sgi:
    args.np = ic;
    args.args_count = 3;
    args.args[0] = CONFIG_PROVENCORE_IRQ_DOMAIN_NUMBER;
    args.args[1] = hwirq;
    args.args[1] -= (hwirq >= LINUX_MIN_SPI) ? LINUX_MIN_SPI:0;
#ifdef CONFIG_PROVENCORE_IRQ_TYPE_LEVEL_HIGH
    args.args[2] = IRQ_TYPE_LEVEL_HIGH;
#else
    if (IS_ENABLED(CONFIG_ARM_GIC_V3)) {
        args.args[2] = IRQ_TYPE_EDGE_RISING;
    } else {
        args.args[2] = IRQ_TYPE_LEVEL_HIGH;
    }
#endif

    virq = irq_create_of_mapping(&args);

    pr_info("(%s) mapped hw sgi %lu to desc %u\n", __func__, hwirq, virq);

    of_node_put(ic);
    return virq;
}

#endif /* CONFIG_IRQ_DOMAIN */

#endif /* CONFIG_PROVENCORE_DTS_CONFIGURATION */

DECLARE_WORK(_sync_work, pnc_sessions_sync);

static int __init pnc_init(void)
{
    unsigned long p, pfn;
    int ret;
#ifndef CONFIG_PROVENCORE_DTS_CONFIGURATION
    struct page *page = NULL;
#endif
    struct page **shmem = NULL;
    struct pnc_smc_params params;

    pr_info("module init (0x%x)\n", REE_VERSION);

#ifdef CONFIG_PROVENCORE_DTS_CONFIGURATION
    /*
     * Lookup the reserved memory area defined in the DTB.
     * For linux version >=3.15, the region may be defined as a range
     * with compatibility pnc,reserved in the reserved-memory section.
     * For earlier versions, the region must be defined through the
     * boot parameters using memmap=<SIZE>$<ADDRESS>.
     */
    ret = pnc_find_reserved_mem(&pfn, &_nr_pages, &_irq);
    if (ret != 0) {
        pr_err("(%s) failed to locate reserved memory\n", __func__);
        ret = -ENOMEM;
        goto err_0;
    }
    if (_nr_pages < REE_RESERVED_PAGES) {
        pr_err("(%s) reserved memory is too small\n", __func__);
        ret = -EINVAL;
        goto err_0;
    }

    _base = pfn;
    pr_info("(%s) found %ld reserved pages\n", __func__, _nr_pages);
    pr_info("    physaddr: 0x%lx\n", _base << PAGE_SHIFT);
#else
    /*
     * Allocate (1 << order) contiguous pages in kernel memory. By default,
     * only the first page is ref counted and all the pages are handled as
     * one.
     * Since the mmap operation needs individual page ref counts in order
     * to work correctly (can corrupt page counts otherwise), split_page()
     * is called to split the higher order allocated area.
     * NB: as a result, the pages must be freed one by one.
     */
    if (order < 8) {
        pr_err("(%s) selected order is too small (min. 8)\n", __func__);
        goto err_0;
    }
    page = alloc_pages(GFP_KERNEL | __GFP_ZERO, order);
    if (page == NULL) {
        pr_err("(%s) failed to allocate contiguous memory\n", __func__);
        ret = -ENOMEM;
        goto err_0;
    }
    _nr_pages = 1 << order;
    if (_nr_pages < REE_RESERVED_PAGES) {
        pr_err("(%s) reserved memory is too small\n", __func__);
        ret = -EINVAL;
        goto err_0;
    }
    _base = page_to_pfn(page);
    split_page(page, order);

    pr_info("(%s) successfully allocated %ld pages\n", __func__, _nr_pages);
    pr_info("    physaddr: 0x%lx\n", _base << PAGE_SHIFT);

#ifdef CONFIG_IRQ_DOMAIN
    /* Bind the trustzone IRQ. */
    _irq = pnc_create_sgi(CONFIG_PROVENCORE_NON_SECURE_IRQ);
    if (!_irq) {
        pr_err("(%s) failed to allocate SGI descriptor\n", __func__);
        ret = -EINVAL;
        goto err_3;
    }
#else
    _irq = CONFIG_PROVENCORE_NON_SECURE_IRQ;
#endif /* CONFIG_IRQ_DOMAIN */
#endif /* CONFIG_PROVENCORE_DTS_CONFIGURATION */

    /*
     * Map the full memory. Because of the kernel API the kernel may need
     * to access the shared memory directly.
     */
    shmem = kmalloc(_nr_pages * sizeof(struct page *), GFP_KERNEL);
    if (shmem == NULL) {
        pr_err("(%s) failed to allocate shmem pages\n", __func__);
        goto err_1;
    }
    for (p = 0, pfn = _base; p < _nr_pages; p++, pfn++) {
        shmem[p] = pfn_to_page(pfn);
    }
    _vbase = vmap(shmem, _nr_pages, VM_RESERVED | VM_MAP, PAGE_KERNEL);
    kfree(shmem);
    if (_vbase == NULL) {
        pr_err("(%s) failed to map the shared memory\n", __func__);
        goto err_1;
    }
    pr_info("(%s) successfully mapped %lu shared memory pages\n", __func__,
        _nr_pages);
    pr_info("    virtaddr: 0x%p\n", _vbase);

    /* Initialise the block allocator. */
    ret = pnc_shm_init(_vbase, (_base << PAGE_SHIFT), _nr_pages);
    if (ret) {
        pr_err("(%s) failed to initialise block allocator\n", __func__);
        goto err_2;
    }
    pr_info("(%s) successfully initialised block allocator\n", __func__);

    /* Initialise the ring buffers and the session handles. */
    ret = pnc_sessions_init();
    if (ret) {
        pr_err("(%s) failed to init sessions framework (%d)\n", __func__, ret);
        goto err_3;
    }

    /* Request the trustzone IRQ. */
    ret = request_irq(_irq, pnc_session_interrupt_handler, IRQF_SHARED, "tzirq",
                  &pnc_device);
    if (ret) {
        pr_err("(%s) failed to request SGI %u to notify Secure World (%d)\n",
            __func__, _irq, ret);
        goto err_3;
    }
    pr_info("(%s) successfully registered IRQ %d. Hook at %p\n", __func__, _irq,
        pnc_session_interrupt_handler);

    /* Init SMC framework */
    ret = pnc_smc_init();
    if (ret) {
        pr_err("(%s) SMC init failure.\n", __func__);
        goto err_4;
    }

    /* Schedule a work that will wait for end of synchro with S */
    schedule_work(&_sync_work);

    /* Forward SHM geometry to the secure monitor */
    pnc_shm_forward();

    /* Trigger 1st valid action from NS: this 1st action is the signal for
     * secure REE application that SHM init is ended in the non secure world */
    memset(&params, 0, sizeof(struct pnc_smc_params));
    params.a0 = SMC_ACTION_FROM_NS;
    pnc_sched_smc(&params);

    return 0;

err_4:
    free_irq(_irq, &pnc_device);
err_3:
    pnc_shm_exit();
err_2:
    vunmap(_vbase);
err_1:
#ifndef CONFIG_PROVENCORE_DTS_CONFIGURATION
    for (p = 0, pfn = _base; p < _nr_pages; p++, pfn++)
        __free_page(pfn_to_page(pfn));
#endif /* !CONFIG_PROVENCORE_DTS_CONFIGURATION */
err_0:
    _base = 0;
    return ret;
}

static void pnc_exit(void)
{
#ifndef CONFIG_PROVENCORE_DTS_CONFIGURATION
    unsigned long p, pfn;
#endif

    pr_info("module exit\n");

    if (_base == 0)
        return;

    /* We shouldn't receive any S irq anymore */
    free_irq(_irq, &pnc_device);
    /* We can release any process waiting for S readyness... */
    pnc_sessions_release();
    /* ...then flush remaining works... */
    flush_work(&_sync_work);
    pnc_sessions_exit();
    /* ...and clean remaining resources */
    pnc_smc_exit();
    pnc_shm_exit();
    vunmap(_vbase);
#ifndef CONFIG_PROVENCORE_DTS_CONFIGURATION
    for (p = 0, pfn = _base; p < _nr_pages; p++, pfn++)
        __free_page(pfn_to_page(pfn));
    _base = 0;
#endif /* !CONFIG_PROVENCORE_DTS_CONFIGURATION */
    /* No need to protect _device_registered as the only thread that sets this
     * variable has stopped working thanks to flush_work(&_sync_work) above */
    if (_device_registered)
        misc_deregister(&pnc_device);
}

int register_device(void)
{
    int ret = misc_register(&pnc_device);
    if (ret) {
        pr_err("(%s) failed to register misc device\n", __func__);
        return ret;
    }
    /* No need to protect _device_registered here as the only thread that will
     * read this variable will ensure register_device() has finished before
     * accessing _device_registered */
    _device_registered = true;
    return ret;
}

module_init(pnc_init);
module_exit(pnc_exit);
module_param(order, uint, S_IRUGO);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Provencore REE driver");
MODULE_AUTHOR("Provenrun");
