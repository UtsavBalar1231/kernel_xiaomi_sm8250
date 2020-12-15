#include <linux/fs.h>
extern const struct file_operations sfops;
extern ssize_t sdevice_read(struct file *, char *, size_t, loff_t *);
extern ssize_t sdevice_write(struct file *, const char *, size_t, loff_t *);
