/* Copyright (c) 2019-2020 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * RMNET Data Smart Hash Workqueue Generic Netlink Functions
 *
 */

#include "rmnet_shs_wq_mem.h"
#include <linux/proc_fs.h>
#include <linux/refcount.h>

MODULE_LICENSE("GPL v2");

struct proc_dir_entry *shs_proc_dir;

/* Fixed arrays to copy to userspace over netlink */
struct rmnet_shs_wq_cpu_cap_usr_s rmnet_shs_wq_cap_list_usr[MAX_CPUS];
struct rmnet_shs_wq_gflows_usr_s rmnet_shs_wq_gflows_usr[RMNET_SHS_MAX_USRFLOWS];
struct rmnet_shs_wq_ssflows_usr_s rmnet_shs_wq_ssflows_usr[RMNET_SHS_MAX_USRFLOWS];
struct rmnet_shs_wq_netdev_usr_s rmnet_shs_wq_netdev_usr[RMNET_SHS_MAX_NETDEVS];

struct list_head gflows   = LIST_HEAD_INIT(gflows);   /* gold flows */
struct list_head ssflows  = LIST_HEAD_INIT(ssflows);  /* slow start flows */
struct list_head cpu_caps = LIST_HEAD_INIT(cpu_caps); /* capacities */

struct rmnet_shs_mmap_info *cap_shared;
struct rmnet_shs_mmap_info *gflow_shared;
struct rmnet_shs_mmap_info *ssflow_shared;
struct rmnet_shs_mmap_info *netdev_shared;

/* Static Functions and Definitions */
static void rmnet_shs_vm_open(struct vm_area_struct *vma)
{
	return;
}

static void rmnet_shs_vm_close(struct vm_area_struct *vma)
{
	return;
}

static int rmnet_shs_vm_fault_caps(struct vm_fault *vmf)
{
	struct page *page = NULL;
	struct rmnet_shs_mmap_info *info;

	rmnet_shs_wq_ep_lock_bh();
	if (cap_shared) {
		info = (struct rmnet_shs_mmap_info *) vmf->vma->vm_private_data;
		if (info->data) {
			page = virt_to_page(info->data);
			get_page(page);
			vmf->page = page;
		} else {
			rmnet_shs_wq_ep_unlock_bh();
			return VM_FAULT_SIGSEGV;
		}
	} else {
		rmnet_shs_wq_ep_unlock_bh();
		return VM_FAULT_SIGSEGV;
	}
	rmnet_shs_wq_ep_unlock_bh();

	return 0;
}


static int rmnet_shs_vm_fault_g_flows(struct vm_fault *vmf)
{
	struct page *page = NULL;
	struct rmnet_shs_mmap_info *info;

	rmnet_shs_wq_ep_lock_bh();
	if (gflow_shared) {
		info = (struct rmnet_shs_mmap_info *) vmf->vma->vm_private_data;
		if (info->data) {
			page = virt_to_page(info->data);
			get_page(page);
			vmf->page = page;
		} else {
			rmnet_shs_wq_ep_unlock_bh();
			return VM_FAULT_SIGSEGV;
		}
	} else {
		rmnet_shs_wq_ep_unlock_bh();
		return VM_FAULT_SIGSEGV;

	}
	rmnet_shs_wq_ep_unlock_bh();

	return 0;
}

static int rmnet_shs_vm_fault_ss_flows(struct vm_fault *vmf)
{
	struct page *page = NULL;
	struct rmnet_shs_mmap_info *info;

	rmnet_shs_wq_ep_lock_bh();
	if (ssflow_shared) {
		info = (struct rmnet_shs_mmap_info *) vmf->vma->vm_private_data;
		if (info->data) {
			page = virt_to_page(info->data);
			get_page(page);
			vmf->page = page;
		} else {
			rmnet_shs_wq_ep_unlock_bh();
			return VM_FAULT_SIGSEGV;
		}
	} else {
		rmnet_shs_wq_ep_unlock_bh();
		return VM_FAULT_SIGSEGV;
	}
	rmnet_shs_wq_ep_unlock_bh();

	return 0;
}

static int rmnet_shs_vm_fault_netdev(struct vm_fault *vmf)
{
	struct page *page = NULL;
	struct rmnet_shs_mmap_info *info;

	rmnet_shs_wq_ep_lock_bh();
	if (netdev_shared) {
		info = (struct rmnet_shs_mmap_info *) vmf->vma->vm_private_data;
		if (info->data) {
			page = virt_to_page(info->data);
			get_page(page);
			vmf->page = page;
		} else {
			rmnet_shs_wq_ep_unlock_bh();
			return VM_FAULT_SIGSEGV;
		}
	} else {
		rmnet_shs_wq_ep_unlock_bh();
		return VM_FAULT_SIGSEGV;
	}
	rmnet_shs_wq_ep_unlock_bh();

	return 0;
}


static const struct vm_operations_struct rmnet_shs_vm_ops_caps = {
	.close = rmnet_shs_vm_close,
	.open = rmnet_shs_vm_open,
	.fault = rmnet_shs_vm_fault_caps,
};

static const struct vm_operations_struct rmnet_shs_vm_ops_g_flows = {
	.close = rmnet_shs_vm_close,
	.open = rmnet_shs_vm_open,
	.fault = rmnet_shs_vm_fault_g_flows,
};

static const struct vm_operations_struct rmnet_shs_vm_ops_ss_flows = {
	.close = rmnet_shs_vm_close,
	.open = rmnet_shs_vm_open,
	.fault = rmnet_shs_vm_fault_ss_flows,
};

static const struct vm_operations_struct rmnet_shs_vm_ops_netdev = {
	.close = rmnet_shs_vm_close,
	.open = rmnet_shs_vm_open,
	.fault = rmnet_shs_vm_fault_netdev,
};

static int rmnet_shs_mmap_caps(struct file *filp, struct vm_area_struct *vma)
{
	vma->vm_ops = &rmnet_shs_vm_ops_caps;
	vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP;
	vma->vm_private_data = filp->private_data;

	return 0;
}

static int rmnet_shs_mmap_g_flows(struct file *filp, struct vm_area_struct *vma)
{
	vma->vm_ops = &rmnet_shs_vm_ops_g_flows;
	vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP;
	vma->vm_private_data = filp->private_data;

	return 0;
}

static int rmnet_shs_mmap_ss_flows(struct file *filp, struct vm_area_struct *vma)
{
	vma->vm_ops = &rmnet_shs_vm_ops_ss_flows;
	vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP;
	vma->vm_private_data = filp->private_data;

	return 0;
}

static int rmnet_shs_mmap_netdev(struct file *filp, struct vm_area_struct *vma)
{
	vma->vm_ops = &rmnet_shs_vm_ops_netdev;
	vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP;
	vma->vm_private_data = filp->private_data;

	return 0;
}

static int rmnet_shs_open_caps(struct inode *inode, struct file *filp)
{
	struct rmnet_shs_mmap_info *info;

	rm_err("%s", "SHS_MEM: rmnet_shs_open - entry\n");

	rmnet_shs_wq_ep_lock_bh();
	if (!cap_shared) {
		info = kzalloc(sizeof(struct rmnet_shs_mmap_info), GFP_ATOMIC);
		if (!info)
			goto fail;

		info->data = (char *)get_zeroed_page(GFP_ATOMIC);
		if (!info->data) {
			kfree(info);
			goto fail;
		}

		cap_shared = info;
		refcount_set(&cap_shared->refcnt, 1);
		rm_err("SHS_MEM: virt_to_phys = 0x%llx cap_shared = 0x%llx\n",
		       (unsigned long long)virt_to_phys((void *)info),
		       (unsigned long long)virt_to_phys((void *)cap_shared));
	} else {
		refcount_inc(&cap_shared->refcnt);
	}

	filp->private_data = cap_shared;
	rmnet_shs_wq_ep_unlock_bh();

	rm_err("%s", "SHS_MEM: rmnet_shs_open - OK\n");

	return 0;

fail:
	rmnet_shs_wq_ep_unlock_bh();
	rm_err("%s", "SHS_MEM: rmnet_shs_open - FAILED\n");
	return -ENOMEM;
}

static int rmnet_shs_open_g_flows(struct inode *inode, struct file *filp)
{
	struct rmnet_shs_mmap_info *info;

	rm_err("%s", "SHS_MEM: rmnet_shs_open g_flows - entry\n");

	rmnet_shs_wq_ep_lock_bh();
	if (!gflow_shared) {
		info = kzalloc(sizeof(struct rmnet_shs_mmap_info), GFP_ATOMIC);
		if (!info)
			goto fail;

		info->data = (char *)get_zeroed_page(GFP_ATOMIC);
		if (!info->data) {
			kfree(info);
			goto fail;
		}

		gflow_shared = info;
		refcount_set(&gflow_shared->refcnt, 1);
		rm_err("SHS_MEM: virt_to_phys = 0x%llx gflow_shared = 0x%llx\n",
		       (unsigned long long)virt_to_phys((void *)info),
		       (unsigned long long)virt_to_phys((void *)gflow_shared));
	} else {
		refcount_inc(&gflow_shared->refcnt);
	}

	filp->private_data = gflow_shared;
	rmnet_shs_wq_ep_unlock_bh();

	return 0;

fail:
	rmnet_shs_wq_ep_unlock_bh();
	rm_err("%s", "SHS_MEM: rmnet_shs_open - FAILED\n");
	return -ENOMEM;
}

static int rmnet_shs_open_ss_flows(struct inode *inode, struct file *filp)
{
	struct rmnet_shs_mmap_info *info;

	rm_err("%s", "SHS_MEM: rmnet_shs_open ss_flows - entry\n");

	rmnet_shs_wq_ep_lock_bh();
	if (!ssflow_shared) {
		info = kzalloc(sizeof(struct rmnet_shs_mmap_info), GFP_ATOMIC);
		if (!info)
			goto fail;

		info->data = (char *)get_zeroed_page(GFP_ATOMIC);
		if (!info->data) {
			kfree(info);
			goto fail;
		}

		ssflow_shared = info;
		refcount_set(&ssflow_shared->refcnt, 1);
		rm_err("SHS_MEM: virt_to_phys = 0x%llx ssflow_shared = 0x%llx\n",
		       (unsigned long long)virt_to_phys((void *)info),
		       (unsigned long long)virt_to_phys((void *)ssflow_shared));
	} else {
		refcount_inc(&ssflow_shared->refcnt);
	}

	filp->private_data = ssflow_shared;
	rmnet_shs_wq_ep_unlock_bh();

	return 0;

fail:
	rmnet_shs_wq_ep_unlock_bh();
	rm_err("%s", "SHS_MEM: rmnet_shs_open - FAILED\n");
	return -ENOMEM;
}

static int rmnet_shs_open_netdev(struct inode *inode, struct file *filp)
{
	struct rmnet_shs_mmap_info *info;

	rm_err("%s", "SHS_MEM: rmnet_shs_open netdev - entry\n");

	rmnet_shs_wq_ep_lock_bh();
	if (!netdev_shared) {
		info = kzalloc(sizeof(struct rmnet_shs_mmap_info), GFP_ATOMIC);
		if (!info)
			goto fail;

		info->data = (char *)get_zeroed_page(GFP_ATOMIC);
		if (!info->data) {
			kfree(info);
			goto fail;
		}

		netdev_shared = info;
		refcount_set(&netdev_shared->refcnt, 1);
		rm_err("SHS_MEM: virt_to_phys = 0x%llx netdev_shared = 0x%llx\n",
		       (unsigned long long)virt_to_phys((void *)info),
		       (unsigned long long)virt_to_phys((void *)netdev_shared));
	} else {
		refcount_inc(&netdev_shared->refcnt);
	}

	filp->private_data = netdev_shared;
	rmnet_shs_wq_ep_unlock_bh();

	return 0;

fail:
	rmnet_shs_wq_ep_unlock_bh();
	return -ENOMEM;
}

static ssize_t rmnet_shs_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
	/*
	 * Decline to expose file value and simply return benign value
	 */
	return RMNET_SHS_READ_VAL;
}

static ssize_t rmnet_shs_write(struct file *filp, const char __user *buf, size_t len, loff_t *off)
{
	/*
	 * Returning zero here would result in echo commands hanging
	 * Instead return len and simply decline to allow echo'd values to 
	 * take effect
	 */
	return len;
}

static int rmnet_shs_release_caps(struct inode *inode, struct file *filp)
{
	struct rmnet_shs_mmap_info *info;

	rm_err("%s", "SHS_MEM: rmnet_shs_release - entry\n");

	rmnet_shs_wq_ep_lock_bh();
	if (cap_shared) {
		info = filp->private_data;
		if (refcount_read(&info->refcnt) <= 1) {
			free_page((unsigned long)info->data);
			kfree(info);
			cap_shared = NULL;
			filp->private_data = NULL;
		} else {
			refcount_dec(&info->refcnt);
		}
	}
	rmnet_shs_wq_ep_unlock_bh();

	return 0;
}

static int rmnet_shs_release_g_flows(struct inode *inode, struct file *filp)
{
	struct rmnet_shs_mmap_info *info;

	rm_err("%s", "SHS_MEM: rmnet_shs_release - entry\n");

	rmnet_shs_wq_ep_lock_bh();
	if (gflow_shared) {
		info = filp->private_data;
		if (refcount_read(&info->refcnt) <= 1) {
			free_page((unsigned long)info->data);
			kfree(info);
			gflow_shared = NULL;
			filp->private_data = NULL;
		} else {
			refcount_dec(&info->refcnt);
		}
	}
	rmnet_shs_wq_ep_unlock_bh();

	return 0;
}

static int rmnet_shs_release_ss_flows(struct inode *inode, struct file *filp)
{
	struct rmnet_shs_mmap_info *info;

	rm_err("%s", "SHS_MEM: rmnet_shs_release - entry\n");

	rmnet_shs_wq_ep_lock_bh();
	if (ssflow_shared) {
		info = filp->private_data;
		if (refcount_read(&info->refcnt) <= 1) {
			free_page((unsigned long)info->data);
			kfree(info);
			ssflow_shared = NULL;
			filp->private_data = NULL;
		} else {
			refcount_dec(&info->refcnt);
		}
	}
	rmnet_shs_wq_ep_unlock_bh();

	return 0;
}

static int rmnet_shs_release_netdev(struct inode *inode, struct file *filp)
{
	struct rmnet_shs_mmap_info *info;

	rm_err("%s", "SHS_MEM: rmnet_shs_release netdev - entry\n");

	rmnet_shs_wq_ep_lock_bh();
	if (netdev_shared) {
		info = filp->private_data;
		if (refcount_read(&info->refcnt) <= 1) {
			free_page((unsigned long)info->data);
			kfree(info);
			netdev_shared = NULL;
			filp->private_data = NULL;
		} else {
			refcount_dec(&info->refcnt);
		}
	}
	rmnet_shs_wq_ep_unlock_bh();

	return 0;
}

static const struct file_operations rmnet_shs_caps_fops = {
	.owner   = THIS_MODULE,
	.mmap    = rmnet_shs_mmap_caps,
	.open    = rmnet_shs_open_caps,
	.release = rmnet_shs_release_caps,
	.read    = rmnet_shs_read,
	.write   = rmnet_shs_write,
};

static const struct file_operations rmnet_shs_g_flows_fops = {
	.owner   = THIS_MODULE,
	.mmap    = rmnet_shs_mmap_g_flows,
	.open    = rmnet_shs_open_g_flows,
	.release = rmnet_shs_release_g_flows,
	.read    = rmnet_shs_read,
	.write   = rmnet_shs_write,
};

static const struct file_operations rmnet_shs_ss_flows_fops = {
	.owner   = THIS_MODULE,
	.mmap    = rmnet_shs_mmap_ss_flows,
	.open    = rmnet_shs_open_ss_flows,
	.release = rmnet_shs_release_ss_flows,
	.read    = rmnet_shs_read,
	.write   = rmnet_shs_write,
};

static const struct file_operations rmnet_shs_netdev_fops = {
	.owner   = THIS_MODULE,
	.mmap    = rmnet_shs_mmap_netdev,
	.open    = rmnet_shs_open_netdev,
	.release = rmnet_shs_release_netdev,
	.read    = rmnet_shs_read,
	.write   = rmnet_shs_write,
};

/* Global Functions */
/* Add a flow to the slow start flow list */
void rmnet_shs_wq_ssflow_list_add(struct rmnet_shs_wq_hstat_s *hnode,
				 struct list_head *ss_flows)
{
	struct rmnet_shs_wq_ss_flow_s *ssflow_node;

	if (!hnode || !ss_flows) {
		rmnet_shs_crit_err[RMNET_SHS_WQ_INVALID_PTR_ERR]++;
		return;
	}

	ssflow_node = kzalloc(sizeof(*ssflow_node), GFP_ATOMIC);
	if (ssflow_node != NULL) {
		ssflow_node->avg_pps = hnode->avg_pps;
		ssflow_node->cpu_num = hnode->current_cpu;
		ssflow_node->hash = hnode->hash;
		ssflow_node->rx_pps = hnode->rx_pps;
		ssflow_node->rx_bps = hnode->rx_bps;

		list_add(&ssflow_node->ssflow_list, ss_flows);
	} else {
		rmnet_shs_crit_err[RMNET_SHS_WQ_NODE_MALLOC_ERR]++;
	}
}

/* Clean up slow start flow list */
void rmnet_shs_wq_cleanup_ss_flow_list(struct list_head *ss_flows)
{
	struct rmnet_shs_wq_ss_flow_s *ssflow_node;
	struct list_head *ptr, *next;

	if (!ss_flows) {
		rmnet_shs_crit_err[RMNET_SHS_WQ_INVALID_PTR_ERR]++;
		return;
	}

	list_for_each_safe(ptr, next, ss_flows) {
		ssflow_node = list_entry(ptr,
					struct rmnet_shs_wq_ss_flow_s,
					ssflow_list);
		if (!ssflow_node)
			continue;

		list_del_init(&ssflow_node->ssflow_list);
		kfree(ssflow_node);
	}
}

/* Add a flow to the gold flow list */
void rmnet_shs_wq_gflow_list_add(struct rmnet_shs_wq_hstat_s *hnode,
				 struct list_head *gold_flows)
{
	struct rmnet_shs_wq_gold_flow_s *gflow_node;

	if (!hnode || !gold_flows) {
		rmnet_shs_crit_err[RMNET_SHS_WQ_INVALID_PTR_ERR]++;
		return;
	}

	if (!rmnet_shs_is_lpwr_cpu(hnode->current_cpu)) {
		gflow_node = kzalloc(sizeof(*gflow_node), GFP_ATOMIC);
		if (gflow_node != NULL) {
			gflow_node->avg_pps = hnode->avg_pps;
			gflow_node->cpu_num = hnode->current_cpu;
			gflow_node->hash = hnode->hash;
			gflow_node->rx_pps = hnode->rx_pps;

			list_add(&gflow_node->gflow_list, gold_flows);
		} else {
			rmnet_shs_crit_err[RMNET_SHS_WQ_NODE_MALLOC_ERR]++;
		}
	}
}

/* Clean up gold flow list */
void rmnet_shs_wq_cleanup_gold_flow_list(struct list_head *gold_flows)
{
	struct rmnet_shs_wq_gold_flow_s *gflow_node;
	struct list_head *ptr, *next;

	if (!gold_flows) {
		rmnet_shs_crit_err[RMNET_SHS_WQ_INVALID_PTR_ERR]++;
		return;
	}

	list_for_each_safe(ptr, next, gold_flows) {
		gflow_node = list_entry(ptr,
					struct rmnet_shs_wq_gold_flow_s,
					gflow_list);
		if (!gflow_node)
			continue;

		list_del_init(&gflow_node->gflow_list);
		kfree(gflow_node);
	}
}

/* Add a cpu to the cpu capacities list */
void rmnet_shs_wq_cpu_caps_list_add(
				struct rmnet_shs_wq_rx_flow_s *rx_flow_tbl_p,
				struct rmnet_shs_wq_cpu_rx_pkt_q_s *cpu_node,
				struct list_head *cpu_caps)
{
	u64 pps_uthresh, pps_lthresh = 0;
	struct rmnet_shs_wq_cpu_cap_s *cap_node;
	int flows = 0;

	if (!cpu_node || !cpu_caps) {
		rmnet_shs_crit_err[RMNET_SHS_WQ_INVALID_PTR_ERR]++;
		return;
	}

	flows = rx_flow_tbl_p->cpu_list[cpu_node->cpu_num].flows;

	pps_uthresh = rmnet_shs_cpu_rx_max_pps_thresh[cpu_node->cpu_num];
	pps_lthresh = rmnet_shs_cpu_rx_min_pps_thresh[cpu_node->cpu_num];

	cap_node = kzalloc(sizeof(*cap_node), GFP_ATOMIC);
	if (cap_node == NULL) {
		rmnet_shs_crit_err[RMNET_SHS_WQ_NODE_MALLOC_ERR]++;
		return;
	}

	cap_node->cpu_num = cpu_node->cpu_num;

	/* No flows means capacity is upper threshold */
	if (flows <= 0) {
		cap_node->pps_capacity = pps_uthresh;
		cap_node->avg_pps_capacity = pps_uthresh;
		cap_node->bps = 0;
		list_add(&cap_node->cpu_cap_list, cpu_caps);
		return;
	}

	/* Instantaneous PPS capacity */
	if (cpu_node->rx_pps < pps_uthresh) {
		cap_node->pps_capacity =
			pps_uthresh - cpu_node->rx_pps;
	} else {
		cap_node->pps_capacity = 0;
	}

	/* Average PPS capacity */
	if (cpu_node->avg_pps < pps_uthresh) {
		cap_node->avg_pps_capacity =
			pps_uthresh - cpu_node->avg_pps;
	} else {
		cap_node->avg_pps_capacity = 0;
	}

	cap_node->bps = cpu_node->rx_bps;

	list_add(&cap_node->cpu_cap_list, cpu_caps);
}

/* Clean up cpu capacities list */
/* Can reuse this memory since num cpus doesnt change */
void rmnet_shs_wq_cleanup_cpu_caps_list(struct list_head *cpu_caps)
{
	struct rmnet_shs_wq_cpu_cap_s *cap_node;
	struct list_head *ptr, *next;

	if (!cpu_caps) {
		rmnet_shs_crit_err[RMNET_SHS_WQ_INVALID_PTR_ERR]++;
		return;
	}

	list_for_each_safe(ptr, next, cpu_caps) {
		cap_node = list_entry(ptr,
					struct rmnet_shs_wq_cpu_cap_s,
					cpu_cap_list);
		if (!cap_node)
			continue;

		list_del_init(&cap_node->cpu_cap_list);
		kfree(cap_node);
	}
}

/* Converts the kernel linked list to an array. Then memcpy to shared mem
 * > The cpu capacity linked list is sorted: highest capacity first
 *     | cap_0 | cap_1 | cap_2 | ... | cap_7 |
 */
void rmnet_shs_wq_mem_update_cached_cpu_caps(struct list_head *cpu_caps)
{
	struct rmnet_shs_wq_cpu_cap_s *cap_node;

	uint16_t idx = 0;

	if (!cpu_caps) {
		rm_err("%s", "SHS_SCAPS: CPU Capacities List is NULL");
		return;
	}

	rm_err("%s", "SHS_SCAPS: Sorted CPU Capacities:");
	list_for_each_entry(cap_node, cpu_caps, cpu_cap_list) {
		if (!cap_node)
			continue;

		if (idx >= MAX_CPUS)
			break;

		rm_err("SHS_SCAPS: > cpu[%d] with pps capacity = %llu | "
		       "avg pps cap = %llu bps = %llu",
		       cap_node->cpu_num, cap_node->pps_capacity,
		       cap_node->avg_pps_capacity, cap_node->bps);

		rmnet_shs_wq_cap_list_usr[idx].avg_pps_capacity = cap_node->avg_pps_capacity;
		rmnet_shs_wq_cap_list_usr[idx].pps_capacity = cap_node->pps_capacity;
		rmnet_shs_wq_cap_list_usr[idx].bps = cap_node->bps;
		rmnet_shs_wq_cap_list_usr[idx].cpu_num = cap_node->cpu_num;
		idx += 1;
	}

	rm_err("SHS_MEM: cap_dma_ptr = 0x%llx addr = 0x%pK\n",
	       (unsigned long long)virt_to_phys((void *)cap_shared), cap_shared);
	if (!cap_shared) {
		rm_err("%s", "SHS_WRITE: cap_shared is NULL");
		return;
	}
	memcpy((char *) cap_shared->data,
	       (void *) &rmnet_shs_wq_cap_list_usr[0],
	       sizeof(rmnet_shs_wq_cap_list_usr));
}

/* Convert the kernel linked list of gold flows into an array that can be
 * memcpy'd to shared memory.
 * > Add number of flows at the beginning of the shared memory address.
 * > After memcpy is complete, send userspace a message indicating that memcpy
 *   has just completed.
 * > The gold flow list is sorted: heaviest gold flow is first
 *    | num_flows | flow_1 | flow_2 | ... | flow_n | ... |
 *    |  16 bits  | ...                                  |
 */
void rmnet_shs_wq_mem_update_cached_sorted_gold_flows(struct list_head *gold_flows)
{
	struct rmnet_shs_wq_gold_flow_s *gflow_node;
	uint16_t idx = 0;
	int num_gold_flows = 0;

	if (!gold_flows) {
		rm_err("%s", "SHS_SGOLD: Gold Flows List is NULL");
		return;
	}

	rm_err("%s", "SHS_SGOLD: List of sorted gold flows:");
	list_for_each_entry(gflow_node, gold_flows, gflow_list) {
		if (!gflow_node)
			continue;

		if (gflow_node->rx_pps == 0) {
			continue;
		}

		if (idx >= RMNET_SHS_MAX_USRFLOWS) {
			break;
		}

		rm_err("SHS_SGOLD: > flow 0x%x with pps %llu on cpu[%d]",
		       gflow_node->hash, gflow_node->rx_pps,
		       gflow_node->cpu_num);
		num_gold_flows += 1;


		/* Update the cached gold flow list */
		rmnet_shs_wq_gflows_usr[idx].cpu_num = gflow_node->cpu_num;
		rmnet_shs_wq_gflows_usr[idx].hash = gflow_node->hash;
		rmnet_shs_wq_gflows_usr[idx].avg_pps = gflow_node->avg_pps;
		rmnet_shs_wq_gflows_usr[idx].rx_pps = gflow_node->rx_pps;
		idx += 1;
	}

	rm_err("SHS_MEM: gflow_dma_ptr = 0x%llx addr = 0x%pK\n",
	       (unsigned long long)virt_to_phys((void *)gflow_shared),
	       gflow_shared);

	if (!gflow_shared) {
		rm_err("%s", "SHS_WRITE: gflow_shared is NULL");
		return;
	}

	rm_err("SHS_SGOLD: num gold flows = %u\n", idx);

	/* Copy num gold flows into first 2 bytes,
	   then copy in the cached gold flow array */
	memcpy(((char *)gflow_shared->data), &idx, sizeof(idx));
	memcpy(((char *)gflow_shared->data + sizeof(uint16_t)),
	       (void *) &rmnet_shs_wq_gflows_usr[0],
	       sizeof(rmnet_shs_wq_gflows_usr));
}

/* Convert the kernel linked list of slow start tcp flows into an array that can be
 * memcpy'd to shared memory.
 * > Add number of flows at the beginning of the shared memory address.
 * > After memcpy is complete, send userspace a message indicating that memcpy
 *   has just completed.
 * > The ss flow list is sorted: heaviest ss flow is first
 *    | num_flows | flow_1 | flow_2 | ... | flow_n | ... |
 *    |  16 bits  | ...                                  |
 */
void rmnet_shs_wq_mem_update_cached_sorted_ss_flows(struct list_head *ss_flows)
{
	struct rmnet_shs_wq_ss_flow_s *ssflow_node;
	uint16_t idx = 0;
	int num_ss_flows = 0;

	if (!ss_flows) {
		rm_err("%s", "SHS_SLOW: SS Flows List is NULL");
		return;
	}

	rm_err("%s", "SHS_SLOW: List of sorted ss flows:");
	list_for_each_entry(ssflow_node, ss_flows, ssflow_list) {
		if (!ssflow_node)
			continue;


		if (ssflow_node->rx_pps == 0) {
			continue;
		}

		if (idx >= RMNET_SHS_MAX_USRFLOWS) {
			break;
		}

		rm_err("SHS_SLOW: > flow 0x%x with pps %llu on cpu[%d]",
		       ssflow_node->hash, ssflow_node->rx_pps,
		       ssflow_node->cpu_num);
		num_ss_flows += 1;

		/* Update the cached ss flow list */
		rmnet_shs_wq_ssflows_usr[idx].cpu_num = ssflow_node->cpu_num;
		rmnet_shs_wq_ssflows_usr[idx].hash = ssflow_node->hash;
		rmnet_shs_wq_ssflows_usr[idx].avg_pps = ssflow_node->avg_pps;
		rmnet_shs_wq_ssflows_usr[idx].rx_pps = ssflow_node->rx_pps;
		rmnet_shs_wq_ssflows_usr[idx].rx_bps = ssflow_node->rx_bps;
		idx += 1;
	}

	rm_err("SHS_MEM: ssflow_dma_ptr = 0x%llx addr = 0x%pK\n",
	       (unsigned long long)virt_to_phys((void *)ssflow_shared),
	       ssflow_shared);

	if (!ssflow_shared) {
		rm_err("%s", "SHS_WRITE: ssflow_shared is NULL");
		return;
	}

	rm_err("SHS_SLOW: num ss flows = %u\n", idx);

	/* Copy num ss flows into first 2 bytes,
	   then copy in the cached ss flow array */
	memcpy(((char *)ssflow_shared->data), &idx, sizeof(idx));
	memcpy(((char *)ssflow_shared->data + sizeof(uint16_t)),
	       (void *) &rmnet_shs_wq_ssflows_usr[0],
	       sizeof(rmnet_shs_wq_ssflows_usr));
}


/* Extract info required from the rmnet_port array then memcpy to shared mem.
 * > Add number of active netdevices/endpoints at the start.
 * > After memcpy is complete, send userspace a message indicating that memcpy
 *   has just completed.
 * > The netdev is formated like this:
 *    | num_netdevs | data_format | {rmnet_data0,ip_miss,rx_pkts} | ... |
 *    |  16 bits    |   32 bits   |                                     |
 */
void rmnet_shs_wq_mem_update_cached_netdevs(void)
{
	struct rmnet_priv *priv;
	struct rmnet_shs_wq_ep_s *ep = NULL;
	u16 idx = 0;
	u16 count = 0;

	rm_err("SHS_NETDEV: function enter %u\n", idx);
	list_for_each_entry(ep, &rmnet_shs_wq_ep_tbl, ep_list_id) {
		count += 1;
		rm_err("SHS_NETDEV: function enter ep %u\n", count);
		if (!ep)
			continue;

		if (!ep->is_ep_active) {
			rm_err("SHS_NETDEV: ep %u is NOT active\n", count);
			continue;
		}

		rm_err("SHS_NETDEV: ep %u is active and not null\n", count);
		if (idx >= RMNET_SHS_MAX_NETDEVS) {
			break;
		}

		priv = netdev_priv(ep->ep);
		if (!priv) {
			rm_err("SHS_NETDEV: priv for ep %u is null\n", count);
			continue;
		}

		rm_err("SHS_NETDEV: ep %u has name = %s \n", count,
		       ep->ep->name);
		rm_err("SHS_NETDEV: ep %u has mux_id = %u \n", count,
		       priv->mux_id);
		rm_err("SHS_NETDEV: ep %u has ip_miss = %lu \n", count,
		       priv->stats.coal.close.ip_miss);
		rm_err("SHS_NETDEV: ep %u has coal_rx_pkts = %lu \n", count,
		       priv->stats.coal.coal_pkts);
		rm_err("SHS_NETDEV: ep %u has udp_rx_bps = %lu \n", count,
		       ep->udp_rx_bps);
		rm_err("SHS_NETDEV: ep %u has tcp_rx_bps = %lu \n", count,
		       ep->tcp_rx_bps);

		/* Set netdev name and ip mismatch count */
		rmnet_shs_wq_netdev_usr[idx].coal_ip_miss = priv->stats.coal.close.ip_miss;
		rmnet_shs_wq_netdev_usr[idx].hw_evict = priv->stats.coal.close.hw_evict;
		rmnet_shs_wq_netdev_usr[idx].coal_tcp = priv->stats.coal.coal_tcp;
		rmnet_shs_wq_netdev_usr[idx].coal_tcp_bytes = priv->stats.coal.coal_tcp_bytes;
		rmnet_shs_wq_netdev_usr[idx].coal_udp = priv->stats.coal.coal_udp;
		rmnet_shs_wq_netdev_usr[idx].coal_udp_bytes = priv->stats.coal.coal_udp_bytes;
		rmnet_shs_wq_netdev_usr[idx].mux_id = priv->mux_id;
		strlcpy(rmnet_shs_wq_netdev_usr[idx].name,
			ep->ep->name,
			sizeof(rmnet_shs_wq_netdev_usr[idx].name));

		/* Set rx pkt from netdev stats */
		rmnet_shs_wq_netdev_usr[idx].coal_rx_pkts = priv->stats.coal.coal_pkts;
		rmnet_shs_wq_netdev_usr[idx].tcp_rx_bps = ep->tcp_rx_bps;
		rmnet_shs_wq_netdev_usr[idx].udp_rx_bps = ep->udp_rx_bps;
		idx += 1;
	}

	rm_err("SHS_MEM: netdev_shared = 0x%llx addr = 0x%pK\n",
	       (unsigned long long)virt_to_phys((void *)netdev_shared), netdev_shared);
	if (!netdev_shared) {
		rm_err("%s", "SHS_WRITE: netdev_shared is NULL");
		return;
	}

	memcpy(((char *)netdev_shared->data), &idx, sizeof(idx));
	memcpy(((char *)netdev_shared->data + sizeof(uint16_t)),
	       (void *) &rmnet_shs_wq_netdev_usr[0],
	       sizeof(rmnet_shs_wq_netdev_usr));
}

/* Creates the proc folder and files for shs shared memory */
void rmnet_shs_wq_mem_init(void)
{
	shs_proc_dir = proc_mkdir("shs", NULL);

	proc_create(RMNET_SHS_PROC_CAPS, 0644, shs_proc_dir, &rmnet_shs_caps_fops);
	proc_create(RMNET_SHS_PROC_G_FLOWS, 0644, shs_proc_dir, &rmnet_shs_g_flows_fops);
	proc_create(RMNET_SHS_PROC_SS_FLOWS, 0644, shs_proc_dir, &rmnet_shs_ss_flows_fops);
	proc_create(RMNET_SHS_PROC_NETDEV, 0644, shs_proc_dir, &rmnet_shs_netdev_fops);

	rmnet_shs_wq_ep_lock_bh();
	cap_shared = NULL;
	gflow_shared = NULL;
	ssflow_shared = NULL;
	netdev_shared = NULL;
	rmnet_shs_wq_ep_unlock_bh();
}

/* Remove shs files and folders from proc fs */
void rmnet_shs_wq_mem_deinit(void)
{
	remove_proc_entry(RMNET_SHS_PROC_CAPS, shs_proc_dir);
	remove_proc_entry(RMNET_SHS_PROC_G_FLOWS, shs_proc_dir);
	remove_proc_entry(RMNET_SHS_PROC_SS_FLOWS, shs_proc_dir);
	remove_proc_entry(RMNET_SHS_PROC_NETDEV, shs_proc_dir);
	remove_proc_entry(RMNET_SHS_PROC_DIR, NULL);

	rmnet_shs_wq_ep_lock_bh();
	cap_shared = NULL;
	gflow_shared = NULL;
	ssflow_shared = NULL;
	netdev_shared = NULL;
	rmnet_shs_wq_ep_unlock_bh();
}
