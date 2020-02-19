#include <linux/kobject.h>

extern bool capacitive_keys_enabled;
extern struct kobject *touchpanel_kobj;

struct tp_common_ops {
	ssize_t (*show)(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf);
	ssize_t (*store)(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count);
};

int tp_common_set_capacitive_keys_ops(struct tp_common_ops *ops);
int tp_common_set_double_tap_ops(struct tp_common_ops *ops);
int tp_common_set_reversed_keys_ops(struct tp_common_ops *ops);
