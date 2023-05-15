#include <linux/module.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/of.h>

/* Meta Information */
MODULE_LICENSE("GPL-2.0");
MODULE_AUTHOR("Lantronix <lantronix@lantronix.com>");
MODULE_DESCRIPTION("QED sysfs info");
MODULE_VERSION("1.0");

#define SYSFS_QEDFS "qedfs"
#define OF_QED_SERIALNO_PROP "qed,serialno"

static struct kobject *qedfs_kobj;
static const char *qed_serialno;

static ssize_t qedsn_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
       return sprintf(buf, qed_serialno);
}

static struct kobj_attribute qedfs_attr = __ATTR(qed_serialno, S_IRUGO, qedsn_show, 0);

static int __init qedfs_init(void)
{
        struct device_node *node = of_find_node_with_property(NULL, OF_QED_SERIALNO_PROP);
        if (!node) {
                pr_err("qedfs-driver: can't find node with \"%s\" property\n", OF_QED_SERIALNO_PROP);
                return -ENOMEM;
        }

        if (of_property_read_string(node, OF_QED_SERIALNO_PROP, &qed_serialno)) {
                pr_err("qedfs-driver: can't read \"%s\" property\n", OF_QED_SERIALNO_PROP);
                of_node_put(node);
                return -ENOMEM;
        }

        of_node_put(node);
        /* Creating the folder qedfs */
        qedfs_kobj = kobject_create_and_add(SYSFS_QEDFS, kernel_kobj);
        if (!qedfs_kobj) {
                pr_err("qedfs-driver: Cannot create kobj %s\n", SYSFS_QEDFS);
                return -ENOMEM;
        }
        /* Create the sysfs file qed_serialno */
        if (sysfs_create_file(qedfs_kobj, &qedfs_attr.attr)) {
                pr_err("qedfs-driver: Cannot create sysfs file qed_serialno\n");
                kobject_put(qedfs_kobj);
                return -ENOMEM;
        }
        return 0;
}

static void __exit qedfs_exit(void) {
        sysfs_remove_file(qedfs_kobj, &qedfs_attr.attr);
        kobject_put(qedfs_kobj);
}

module_init(qedfs_init);
module_exit(qedfs_exit);
