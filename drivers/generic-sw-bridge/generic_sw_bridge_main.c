/* Copyright (c) 2017, The Linux Foundation. All rights reserved. */
/*
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/suspend.h>
#include <linux/pm_wakeup.h>
#include <linux/ioctl.h>
#include <linux/wait.h>
#include "generic_sw_bridge.h"
#include "gsb_debugfs.h"
#include <linux/hashtable.h>
#include <linux/hash.h>
#include <linux/timer.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <net/ip.h>
#include <linux/proc_fs.h>

static char gsb_drv_name[] = "gsb";
static struct gsb_ctx *__gc = NULL;
static DECLARE_WAIT_QUEUE_HEAD(wq);

static struct proc_dir_entry* proc_file = NULL;
static struct file_operations proc_file_ops;
int gsb_enable_ipc_low;
#define MAX_PROC_SIZE 10
char tmp_buff[MAX_PROC_SIZE];

static const char gsb_drv_description[] =
	"The Linux Foundation"
	"Generic Software Bridge Driver v0.1";

static void gsb_ipa_send_routine(struct work_struct *work);
static void gsb_ipa_resume_routine(struct work_struct *work);
static void suspend_task(struct work_struct *work);
struct gsb_if_info* get_node_info_from_ht(char *iface);
static void schedule_inactivity_timer(const unsigned int time_in_ms);
static int suspend_all_bridged_interfaces(void);
static DECLARE_DELAYED_WORK(if_suspend_wq, suspend_task);
static struct workqueue_struct *gsb_wq;
extern int (*gsb_nw_stack_recv)(struct sk_buff *skb);



static void release_wake_source(void)
{
	struct gsb_ctx *pgsb_ctx = __gc;
	if (IS_ERR_OR_NULL(pgsb_ctx))
	{
		IPC_ERROR_LOW("NULL GSB Context passed\n");
		return;
	}
	spin_lock_bh(&pgsb_ctx->gsb_wake_lock);
	if (pgsb_ctx->wake_source_ref_count>0) pgsb_ctx->wake_source_ref_count--;
	if (pgsb_ctx->do_we_need_wake_source && !pgsb_ctx->wake_source_ref_count)
	{
		IPC_TRACE_LOW("Scheduling Inactivity timer\n");
		if (!pgsb_ctx->inactivity_timer_scheduled)
		{
			schedule_inactivity_timer(INACTIVITY_TIME);
			pgsb_ctx->inactivity_timer_cnt++;
		}
	}
	else
	{
		IPC_TRACE_LOW("Wake source count %d\n",
				pgsb_ctx->wake_source_ref_count);
	}

	spin_unlock_bh(&pgsb_ctx->gsb_wake_lock);
}

static void acquire_wake_source(void)
{
	struct gsb_ctx *pgsb_ctx = __gc;
	if (IS_ERR_OR_NULL(pgsb_ctx))
	{
		IPC_ERROR_LOW("NULL GSB Context passed\n");
		return;
	}

	spin_lock_bh(&pgsb_ctx->gsb_wake_lock);
	if (!pgsb_ctx->do_we_need_wake_source)
	{
		IPC_TRACE_LOW("Acquiring wake src\n");
		pgsb_ctx->do_we_need_wake_source = true;
		pgsb_ctx->wake_source_ref_count++;
	}
	else
	{
		pgsb_ctx->wake_source_ref_count++;
		IPC_TRACE_LOW("Wake source count %d\n",
				pgsb_ctx->wake_source_ref_count);
	}
	spin_unlock_bh(&pgsb_ctx->gsb_wake_lock);
}

static void acquire_caller(const char *func_name, u32 line_no)
{
	DEBUG_TRACE("%s[%u], calling acquire_wake_source \n", func_name, line_no);
	acquire_wake_source();
}

static void release_caller(const char *func_name, u32 line_no)
{
	DEBUG_TRACE("%s[%u], calling release_wake_source\n", func_name, line_no);
	release_wake_source();
}

#define acquire_wake_source() acquire_caller(__func__, __LINE__)
#define release_wake_source() release_caller(__func__, __LINE__)

static void suspend_task(struct work_struct *work)
{
	DEBUG_TRACE("suspending all interfaces\n");
	if (suspend_all_bridged_interfaces() != 0)
	{
		DEBUG_ERROR("Could not suspend\n");
	}
}

static struct timer_list INACTIVITY_TIMER;

static void dump_packet_util(const struct sk_buff *skb)
{
	char buffer[PACKET_DUMP_BUFFER];
	unsigned int len, printlen;
	int i, buffloc = 0;
	if (skb == NULL || skb->dev == NULL || skb->dev->name == NULL)
	{
		DEBUG_ERROR("Cannot dump this packet\n");
		return;
	}
	DEBUG_TRACE("dumping packet.....\n");
	DUMP_PACKET("[%s] - PKT skb->len=%d skb->head=%pK skb->data=%pK\n",
			skb->dev->name, skb->len, (void *)skb->head, (void *)skb->data);
	DUMP_PACKET("[%s] - PKT skb->tail=%pK skb->end=%pK\n",
			skb->dev->name,  skb_tail_pointer(skb), skb_end_pointer(skb));
	printlen = PACKET_MP_PRINT_LEN;
	if (skb->len > 0) len = skb->len;
	else len = ((unsigned int)(uintptr_t)skb->end) -
			((unsigned int)(uintptr_t)skb->data);

	DUMP_PACKET("[%s]- PKT len: %d, printing first %d bytes\n",
			skb->dev->name, len, printlen);

	memset(buffer, 0, sizeof(buffer));
	for (i = 0; (i < printlen) && (i < len); i++)
	{
		if ((i % 16) == 0)
		{
			DUMP_PACKET("[%s]- PKT %s\n", skb->dev->name, buffer);
			memset(buffer, 0, sizeof(buffer));
			buffloc = 0;
			buffloc += snprintf(&buffer[buffloc],
						sizeof(buffer) - buffloc, "%04X:",
						i);
		}

		buffloc += snprintf(&buffer[buffloc], sizeof(buffer) - buffloc,
					" %02x", skb->data[i]);
	}
	DUMP_PACKET("[%s]- PKT%s\n", skb->dev->name, buffer);
}

static void schedule_inactivity_timer(const unsigned int time_in_ms)
{
	struct gsb_ctx *pgsb_ctx = __gc;
	if (NULL == pgsb_ctx)
	{
		IPC_ERROR_LOW("Context is NULL\n");
		return;
	}

	IPC_TRACE_LOW("fire in 200ms (%ld)\n", jiffies);
	mod_timer(&INACTIVITY_TIMER, jiffies + msecs_to_jiffies(time_in_ms));
	spin_lock_bh(&pgsb_ctx->gsb_lock);
	pgsb_ctx->inactivity_timer_scheduled = true;
	spin_unlock_bh(&pgsb_ctx->gsb_lock);
}

static int suspend_all_bridged_interfaces(void)
{
	struct gsb_if_info *curr;
	struct hlist_node *tmp;
	int bkt;
	struct gsb_ctx *pgsb_ctx = __gc;
	if (NULL == pgsb_ctx)
	{
		DEBUG_ERROR("Context is NULL\n");
		return -EFAULT;
	}

	hash_for_each_safe(pgsb_ctx->cache_htable_list, bkt, tmp, curr, cache_ht_node)
	{
		spin_lock_bh(&pgsb_ctx->gsb_lock);
		if (!curr->is_ipa_bridge_suspended)
		{
			spin_unlock_bh(&pgsb_ctx->gsb_lock);

			if (ipa_bridge_suspend(curr->handle) != 0)
			{
				DEBUG_ERROR("failed to suspend if %s\n", curr->if_name);
				return -EFAULT;
			}
			else
			{
				DEBUG_TRACE("if %s suspended\n", curr->if_name);
				spin_lock_bh(&pgsb_ctx->gsb_lock);
				curr->is_ipa_bridge_suspended = true;
				curr->if_ipa->stats.ipa_suspend_cnt++;
				spin_unlock_bh(&pgsb_ctx->gsb_lock);
			}
		}
		else
		{
			spin_unlock_bh(&pgsb_ctx->gsb_lock);
		}
	}

	// we are good as all if are suspended ...so relax if wake source was acquired
	spin_lock_bh(&pgsb_ctx->gsb_wake_lock);
	pgsb_ctx->do_we_need_wake_source = false;
	if (pgsb_ctx->is_wake_src_acquired)
	{
		DEBUG_TRACE("relaxing wake src\n");
		__pm_relax(&pgsb_ctx->gsb_wake_src);
		pgsb_ctx->is_wake_src_acquired = false;
	}
	else
	{
		DEBUG_TRACE("no wake src acquired to relax\n");
	}
	spin_unlock_bh(&pgsb_ctx->gsb_wake_lock);
	return 0;
}


static void inactivity_timer_cb(unsigned long data)
{
	struct gsb_ctx *pgsb_ctx = __gc;

	if (IS_ERR_OR_NULL(pgsb_ctx))
	{
		DEBUG_ERROR("NULL GSB Context passed\n");
		return;
	}
	DEBUG_TRACE("GSB Inactivity timer expired (%ld)\n", jiffies);
	spin_lock_bh(&pgsb_ctx->gsb_lock);
	if (pgsb_ctx->inactivity_timer_scheduled)
	{
		pgsb_ctx->inactivity_timer_scheduled = false;
		spin_unlock_bh(&pgsb_ctx->gsb_lock);

		DEBUG_TRACE("no data activity for 200 ms..suspending bridged interfaces\n");
		schedule_delayed_work(&if_suspend_wq, 0);
		return;
	}
	spin_unlock_bh(&pgsb_ctx->gsb_lock);
}

static bool is_ipv4_pkt(struct sk_buff *skb)
{
	return ((htons(ETH_P_IP) == skb->protocol));
}

static bool is_ipv6_pkt(struct sk_buff *skb)
{
	return ((htons(ETH_P_IPV6) == skb->protocol));
}

static bool is_non_ip_pkt(struct sk_buff *skb)
{
	return (!is_ipv4_pkt(skb) && !is_ipv6_pkt(skb));
}

static void remove_padding(struct sk_buff *skb, bool ipv4, bool ipv6)
{
	if (ipv4)
	{
		struct iphdr *ip_hdr = NULL;
		ip_hdr = (struct iphdr *)(skb_mac_header(skb) + ETH_HLEN);
		skb_trim(skb, ntohs(ip_hdr->tot_len) + ETH_HLEN);
	}
	else if (ipv6)
	{
		struct ipv6hdr *ip6_hdr = NULL;
		ip6_hdr = (struct ipv6hdr *)(skb_mac_header(skb) + ETH_HLEN);
		skb_trim(skb, ntohs(ip6_hdr->payload_len) + sizeof(struct ipv6hdr) + ETH_HLEN);
	}
}


static void release_pending_packets_exp(struct gsb_if_info *if_info)
{
	struct sk_buff *skb;
	struct if_ipa_ctx *pipa_ctx = if_info->if_ipa;
	int retval = 0;


	spin_lock_bh(&if_info->flow_ctrl_lock);
	while (if_info->pend_queue.qlen)
	{
		skb = __skb_dequeue(&if_info->pend_queue);
		if (IS_ERR_OR_NULL(skb))
		{
			DEBUG_ERROR("null skb\n");
			BUG();
			break;
		}
		//Send Packet to NW stack
		retval = netif_rx_ni(skb);
		if (retval != NET_RX_SUCCESS)
		{
			DEBUG_ERROR("ERROR sending to nw stack %d\n", retval);
			dev_kfree_skb(skb);
			pipa_ctx->stats.exp_if_disconnected_fail++;
		}
		else
		{
			pipa_ctx->stats.exp_if_disconnected++;
		}
	}
	spin_unlock_bh(&if_info->flow_ctrl_lock);
}


static void flush_pending_packets(struct gsb_if_info *if_info)
{
	DEBUG_TRACE("Flush %d Pending UL Packets \n", if_info->pend_queue.qlen);
	skb_queue_purge(&if_info->pend_queue);
}


static  bool is_ip_frag_pkt(struct sk_buff *skb)
{
	struct iphdr *ip_hdr = NULL;
	ip_hdr = (struct iphdr *)(skb_mac_header(skb) + ETH_HLEN);

	/* Return true if: 'More_Frag bit is set' OR
		'Fragmentation Offset is set' */
	if (ip_hdr->frag_off & htons(IP_MF | IP_OFFSET)) return true;
	return false;
}


static  ssize_t gsb_read_stats(struct file *file,
				char __user *user_buf, size_t count, loff_t *ppos)
{
	ssize_t ret_cnt = 0;
	unsigned int len = 0, buf_len = 2000;
	char *buf;
	u16 pendq_cnt, ipa_free_desc_cnt;
	u16 max_pkts_allowed, min_pkts_allowed;
	uint64_t schedule_cnt = 0;
	uint64_t idle_cnt = 0;
	uint64_t pending_wde = 0;
	u16 wake_ref_cnt;
	uint64_t inactivity_sceduled = 0;
	uint64_t inactivity_cancelled = 0;
	bool is_suspended;
	char file_name[PATH_MAX];
	struct gsb_ctx *pgsb_ctx = __gc;
	struct gsb_if_info *if_info = NULL;
	char *raw_path = NULL;

	if (IS_ERR_OR_NULL(pgsb_ctx))
	{
		DEBUG_ERROR("NULL GSB Context passed\n");
		return ret_cnt;
	}

	raw_path = dentry_path_raw(file->f_path.dentry, file_name, PATH_MAX);
	DEBUG_TRACE("rawpath  is %s\n", raw_path);
	if (IS_ERR_OR_NULL(raw_path))
	{
		DEBUG_ERROR("NULL path found\n");
		return ret_cnt;
	}


	spin_lock_bh(&pgsb_ctx->gsb_lock);
	if_info = get_node_info_from_ht(raw_path + READ_STATS_OFFSET);
	spin_unlock_bh(&pgsb_ctx->gsb_lock);


	if (if_info == NULL)
	{
		DEBUG_ERROR("Could not find if node for stats\n");
		return ret_cnt;
	}

	if (!if_info->is_debugfs_init)
	{
		DEBUG_ERROR("debugfs not initialized for %s\n", if_info->if_name);
		return ret_cnt;
	}

	buf = kzalloc(MAX_BUFF_LEN, GFP_KERNEL);
	if (!buf) return -ENOMEM;
	pgsb_ctx->mem_alloc_read_stats_buffer++;

	/* stats buffer*/
	len += scnprintf(buf + len, buf_len - len, "\n \n");
	len += scnprintf(buf + len, buf_len - len, "GSB stats for %s if\n",
			if_info->if_name);
	len += scnprintf(buf + len, buf_len - len, "%35s\n\n",
			"==================================================");

	len += scnprintf(buf + len, buf_len - len, "%35s %10llu\n",
			"Total recv from if",
			if_info->if_ipa->stats.total_recv_from_if);
	len += scnprintf(buf + len, buf_len - len, "%35s %10llu\n",
			"sent to ipa",
			if_info->if_ipa->stats.sent_to_ipa);
	len += scnprintf(buf + len, buf_len - len, "%35s %10llu\n",
			"drop, sent to ipa fail",
			if_info->if_ipa->stats.drop_send_to_ipa_fail);
	len += scnprintf(buf + len, buf_len - len, "%35s %10llu\n",
			"exp,ipa not connected",
			if_info->if_ipa->stats.exception_ipa_not_connected);
	len += scnprintf(buf + len, buf_len - len, "%35s %10llu\n",
			"exp,if is suspended",
			if_info->if_ipa->stats.exp_ipa_suspended);
	len += scnprintf(buf + len, buf_len - len, "%35s %10llu\n",
			"exp,insufficient hr",
			 if_info->if_ipa->stats.exp_insufficient_hr);
	len += scnprintf(buf + len, buf_len - len, "%35s %10llu\n",
			"drop, congestion",
			if_info->if_ipa->stats.drop_flow_control_bottleneck);
	len += scnprintf(buf + len, buf_len - len, "%35s %10llu\n",
			"exp, fragmented",
			if_info->if_ipa->stats.exception_fragmented);
	len += scnprintf(buf + len, buf_len - len, "%35s %10llu\n",
			"exp, non ip packet",
			if_info->if_ipa->stats.exception_non_ip_packet);
	len += scnprintf(buf + len, buf_len - len, "%35s %10llu\n",
			"IPA exp to stack fail",
			if_info->if_ipa->stats.exp_packet_from_ipa_fail);
	len += scnprintf(buf + len, buf_len - len, "%35s %10llu\n",
			"exp, if down",
			 if_info->if_ipa->stats.exp_if_disconnected);
	len += scnprintf(buf + len, buf_len - len, "%35s %10llu\n",
			"exp, if down fail",
			 if_info->if_ipa->stats.exp_if_disconnected_fail);
	len += scnprintf(buf + len, buf_len - len, "%35s %10llu\n",
			"TX,sent to if",
			if_info->if_ipa->stats.tx_send_to_if);
	len += scnprintf(buf + len, buf_len - len, "%35s %10llu\n",
			"TX,sent to if fail",
			if_info->if_ipa->stats.tx_send_err);
	len += scnprintf(buf + len, buf_len - len, "%35s %10llu\n",
			"Writedone from IPA",
			if_info->if_ipa->stats.write_done_from_ipa);
	len += scnprintf(buf + len, buf_len - len, "%35s %10llu\n",
			"IPA exp packet",
			if_info->if_ipa->stats.exception_packet_from_ipa);
	len += scnprintf(buf + len, buf_len - len, "%35s %10llu\n",
			"flow control below low wm",
			if_info->if_ipa->stats.ipa_low_watermark_cnt);



	spin_lock_bh(&if_info->flow_ctrl_lock);
	pendq_cnt = if_info->pend_queue.qlen;
	ipa_free_desc_cnt = if_info->ipa_free_desc_cnt;
	max_pkts_allowed = if_info->max_q_len_in_gsb;
	min_pkts_allowed = if_info->low_watermark;
	schedule_cnt = if_info->wq_schedule_cnt;
	idle_cnt = if_info->idle_cnt;
	pending_wde = if_info->if_ipa->ipa_rx_completion;
	spin_unlock_bh(&if_info->flow_ctrl_lock);

	len += scnprintf(buf + len, buf_len - len, "\n \n");

	len += scnprintf(buf + len, buf_len - len, "%35s %10u\n",
			"current Queue len: ", pendq_cnt);
	len += scnprintf(buf + len, buf_len - len, "%35s %10llu\n",
			"Pending wde: ", pending_wde);
	len += scnprintf(buf + len, buf_len - len, "%35s %10u\n",
			"Free ipa desc Count: ", ipa_free_desc_cnt);
	len += scnprintf(buf + len, buf_len - len, "%35s %10u\n",
			"max queue len: ", max_pkts_allowed);
	len += scnprintf(buf + len, buf_len - len, "%35s %10u\n",
			"Low Watermark: ", min_pkts_allowed);
	len += scnprintf(buf + len, buf_len - len, "%35s %10llu\n",
			"send schedule cnt: ", schedule_cnt);



	spin_lock_bh(&pgsb_ctx->gsb_wake_lock);
	is_suspended = if_info->is_ipa_bridge_suspended;
	spin_unlock_bh(&pgsb_ctx->gsb_wake_lock);

	len += scnprintf(buf + len, buf_len - len, "%35s %10u\n",
			"is data src available?",
			!is_suspended);
	len += scnprintf(buf + len, buf_len - len, "\n \n");
	len += scnprintf(buf + len, buf_len - len, "%35s %10u\n",
			"is wake source acquired: ", pgsb_ctx->is_wake_src_acquired);

	if (len > MAX_BUFF_LEN) len = MAX_BUFF_LEN;

	ret_cnt = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);
	pgsb_ctx->mem_alloc_read_stats_buffer--;
	return ret_cnt;
}

static const struct file_operations debug_fops = {
	.read = gsb_read_stats,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static int gsb_debugfs_init(struct gsb_ctx *gsb_cx)
{
	if (IS_ERR_OR_NULL(gsb_cx))
	{
		DEBUG_ERROR("NULL Context passed\n");
		return -ENOMEM;
	}

	gsb_cx->dbg_dir_root = debugfs_create_dir(gsb_drv_name, NULL);
	if (IS_ERR_OR_NULL(gsb_cx->dbg_dir_root))
	{
		DEBUG_ERROR("Failed to create debugfs dir\n");
		return -ENOMEM;
	}
	return 0;
}

static int create_debugfs_dir_for_if(struct gsb_if_info *if_info)
{
	struct gsb_ctx *pgsb_ctx = __gc;

	if (IS_ERR_OR_NULL(pgsb_ctx))
	{
		DEBUG_ERROR("NULL GSB Context passed\n");
		return -EFAULT;
	}

	if (IS_ERR_OR_NULL(if_info))
	{
		DEBUG_ERROR("NULL if Context passed\n");
		return -EFAULT;
	}

	if_info->dbg_dir_if = debugfs_create_file(if_info->if_name, S_IRUSR, pgsb_ctx->dbg_dir_root,
						  pgsb_ctx, &debug_fops);
	if (IS_ERR_OR_NULL(if_info->dbg_dir_if))
	{
		DEBUG_ERROR("Could not create debugfs dir for if %s\n",
				if_info->if_name);
		return -EFAULT;
	}
	if_info->is_debugfs_init = true;
	return 0;
}

static void remove_debugfs_dir_for_if(struct dentry *entry)
{
	if (IS_ERR_OR_NULL(entry))
	{
		DEBUG_ERROR("NULL entry  passed\n");
		return;
	}

	debugfs_remove(entry);
}

static void gsb_debugfs_exit(struct gsb_ctx *gsb_cx)
{
	if (IS_ERR_OR_NULL(gsb_cx))
	{
		DEBUG_ERROR("NULL Context passed\n");
		return;
	}

	if (!gsb_cx->dbg_dir_root) return;

	debugfs_remove_recursive(gsb_cx->dbg_dir_root);
}


static u32 get_ht_Key(char *str)
{
	u32 num = StringtoAscii(str);
	u32 key  = hash_32(num, NBITS(MAX_SUPPORTED_IF_CONFIG));
	return key;
}

struct gsb_if_info* get_node_info_from_ht(char *iface)
{
	struct gsb_ctx *pgsb_ctx = __gc;
	struct gsb_if_info *curr = NULL;
	u32 key;

	if (NULL == pgsb_ctx)
	{
		IPC_ERROR_LOW("Context is NULL\n");
		return NULL;
	}
	key = get_ht_Key(iface);
	IPC_TRACE_LOW("key %d\n", key);
	hash_for_each_possible(pgsb_ctx->cache_htable_list, curr, cache_ht_node, key)
	{
		IPC_TRACE_LOW("ht iface %s , passed iface %s\n", curr->user_config.if_name,
				iface);
		if (strncmp(curr->user_config.if_name,
				iface,
				IFNAMSIZ) == 0)
		{
			IPC_TRACE_LOW("config found\n");
			return curr;
		}
	}

	IPC_TRACE_LOW("config not found\n");
	return curr;
}


static int remove_entry_from_ht(struct gsb_if_config *info)
{
	u32 key;
	struct gsb_ctx *pgsb_ctx = __gc;
	struct gsb_if_info *curr;

	if (NULL == pgsb_ctx)
	{
		DEBUG_ERROR("Context is NULL\n");
		return -EFAULT;
	}
	key = get_ht_Key(info->if_name);
	hash_for_each_possible(pgsb_ctx->cache_htable_list, curr, cache_ht_node, key)
	{
		DEBUG_TRACE("ht iface %s , passed iface %s\n", curr->if_name,
				info->if_name);
		if (strncmp(curr->if_name,
				info->if_name,
				IFNAMSIZ) == 0)
		{
			hash_del(&curr->cache_ht_node);
			return 0;
		}
	}
	DEBUG_TRACE("config not found\n");
	return -ENODEV;
}

static int cleanup_entries_from_ht(void)
{
	struct gsb_ctx *pgsb_ctx = __gc;
	struct gsb_if_info *curr;
	struct hlist_node *tmp;
	int bkt;

	if (NULL == pgsb_ctx)
	{
		DEBUG_ERROR("Context is NULL\n");
		return -EFAULT;
	}

	hash_for_each_safe(pgsb_ctx->cache_htable_list, bkt, tmp, curr, cache_ht_node)
	{
		DEBUG_TRACE("removing iface %s\n", curr->if_name);
		/* first disconnect this IF from IPA*/
		if (curr->is_connected_to_ipa_bridge &&
			ipa_bridge_disconnect(curr->handle) == 0)
		{
			spin_lock_bh(&pgsb_ctx->gsb_lock);
			curr->is_connected_to_ipa_bridge = false;
			curr->is_ipa_bridge_suspended = true;
			curr->if_ipa->stats.ipa_suspend_cnt++;
			spin_unlock_bh(&pgsb_ctx->gsb_lock);
			flush_pending_packets(curr);
			cancel_work_sync(&curr->ipa_resume_task);
			cancel_work_sync(&curr->ipa_send_task);
			release_wake_source();
			DEBUG_INFO("IPA bridge dis connected for if %s",
					curr->if_name);
		}

		/* delete IF from cache*/
		spin_lock_bh(&pgsb_ctx->gsb_lock);
		hash_del(&curr->cache_ht_node);
		spin_unlock_bh(&pgsb_ctx->gsb_lock);

		remove_debugfs_dir_for_if(curr->dbg_dir_if);

		if (curr->is_ipa_bridge_initialized && ipa_bridge_cleanup(curr->handle) != 0)
		{
			DEBUG_ERROR("issue in cleaning up IPA bridge for if %s\n",
					curr->if_name);
		}

		kfree(curr->if_ipa);
		pgsb_ctx->mem_alloc_if_ipa_context--;
		kfree(curr);

		spin_lock_bh(&pgsb_ctx->gsb_lock);
		pgsb_ctx->mem_alloc_if_node--;
		pgsb_ctx->configured_if_count--;
		spin_unlock_bh(&pgsb_ctx->gsb_lock);
	}

	DEBUG_TRACE("HT cleanup complete\n");
	return 0;
}


static int add_entry_to_ht(struct gsb_if_info *info)
{
	u32 key;
	struct gsb_ctx *pgsb_ctx = __gc;
	struct gsb_if_info *curr;

	if (NULL == pgsb_ctx)
	{
		DEBUG_ERROR("Context is NULL\n");
		return -EFAULT;
	}


	INIT_HLIST_NODE(&info->cache_ht_node);
	key = get_ht_Key(info->if_name);

	DEBUG_TRACE("key %d\n", key);
	hash_for_each_possible(pgsb_ctx->cache_htable_list, curr, cache_ht_node, key)
	{
		if (strncmp(curr->if_name,
				info->if_name,
				IFNAMSIZ) == 0)
			{
				DEBUG_ERROR("config already present\n");
				return -EEXIST;
			}
	}
	DEBUG_TRACE("config not found\n");
	key = get_ht_Key(info->if_name);
	DEBUG_TRACE("key %d\n", key);

	info->is_connected_to_ipa_bridge = false;
	info->is_ipa_bridge_initialized = false;
	hash_add(pgsb_ctx->cache_htable_list, &info->cache_ht_node, key);
	return 0;
}

static void display_cache(void)
{
	struct gsb_if_info *curr;
	struct hlist_node *tmp;
	int bkt;
	struct gsb_ctx *pgsb_ctx = __gc;
	if (NULL == pgsb_ctx)
		{
		DEBUG_ERROR("Context is NULL\n");
		return;
		}

	hash_for_each_safe(pgsb_ctx->cache_htable_list, bkt, tmp, curr, cache_ht_node)
	{
		DEBUG_INFO("gsb iface %s,iface type %d,low wm %d,high wm %d,bw reqd %d\n\n",
				curr->if_name,
				curr->user_config.if_type,
				curr->user_config.if_low_watermark,
				curr->user_config.if_high_watermark,
				curr->user_config.bw_reqd_in_mb);
	}
}

static void gsb_recv_ipa_notification_cb(void *priv, enum ipa_dp_evt_type evt,
					 unsigned long data)
{
	int retval = 0;
	struct gsb_ctx *pgsb_ctx = __gc;
	struct gsb_if_info *if_info = (struct gsb_if_info *)priv;
	struct if_ipa_ctx *pipa_ctx = if_info->if_ipa;
	struct sk_buff *skb = (struct sk_buff *)data;
	u32 qlen = 0;

	if (NULL == pgsb_ctx)
	{
		IPC_ERROR_LOW("Context is NULL\n");
		dev_kfree_skb(skb);
		return;
	}

	if (if_info == NULL)
	{
		IPC_ERROR_LOW("config not available \n");
		dev_kfree_skb(skb);
		return;
	}

	if (!if_info->is_connected_to_ipa_bridge)
	{
		IPC_ERROR_LOW("called before IPA_CONNECT was called with evt %d \n", evt);
		dev_kfree_skb(skb);
		return;
	}

	IPC_TRACE_LOW("EVT Rcvd %d for if %s \n", evt, if_info->if_name);
	switch (evt)
	{
	case IPA_RECEIVE:

		/* Deliver SKB to network adapter */
		//IPA may have returned our skb back to us
		//Lets make sure we tag it so we dont intercept it in stack

		if (skb->cb[0] == '\0')
		{
			strlcpy(skb->cb, "isthisloop", ((sizeof(skb->cb)) / (sizeof(skb->cb[0]))));
			IPC_TRACE_LOW("tagging skb with string %s, skbp= %pK\n", skb->cb, skb);
		}
		else
		{
			IPC_TRACE_LOW("skb is already tagged %s skbp= %pK\n", skb->cb, skb);
		}


		skb->dev = if_info->pdev;
		skb->protocol = eth_type_trans(skb, skb->dev);

		//to do need to optimize this
		retval = netif_rx_ni(skb);
		if (retval != NET_RX_SUCCESS)
		{
			IPC_ERROR_LOW("ERROR sending to nw stack %d\n", retval);
			pipa_ctx->stats.exp_packet_from_ipa_fail++;
		}
		else
		{
			pipa_ctx->stats.exception_packet_from_ipa++;
		}

		break;

	case IPA_WRITE_DONE:
		/* SKB send to IPA, safe to free */
		dev_kfree_skb(skb);

		pgsb_ctx->mem_alloc_skb_free++;
		pipa_ctx->stats.write_done_from_ipa++;


		spin_lock_bh(&if_info->flow_ctrl_lock);
		qlen = if_info->pend_queue.qlen;
		if_info->ipa_free_desc_cnt++;
		pipa_ctx->ipa_rx_completion--;
		spin_unlock_bh(&if_info->flow_ctrl_lock);


		if (!pipa_ctx->ipa_rx_completion && !qlen)
		{
			//seems like this interface is free of activity
			IPC_TRACE_LOW("idle if %s\n", if_info->if_name);
			if_info->idle_cnt++;
			release_wake_source();
		}
		else
		{
			if (if_info->ipa_free_desc_cnt > if_info->low_watermark)
			{
				queue_work(gsb_wq, &if_info->ipa_send_task);
			}
			else
			{
				if_info->if_ipa->stats.ipa_low_watermark_cnt++;
			}
		}
		break;

	default:
		IPC_ERROR_LOW("Invalid %d event from IPA\n", evt);
		break;
	}
}

//to do : we can be 100% sure that we will have wake src
//while we are in this routine. resume will aqcuire or _bind_ will acquire

/*
Scenario 1: Inactivity timer was expired..if was suspended, resume call back will acquire wake source in that case ..no action reqd
Scenario 2: Inactivity timer is been scheduled

*/
static void gsb_recv_dl_dp(void *priv, struct sk_buff *skb)
{
	struct gsb_ctx *pgsb_ctx = __gc;
	bool is_inactivity_timer_cancelled = false;
	struct gsb_if_info *if_info = (struct gsb_if_info *)priv;

	if (NULL == if_info)
	{
		IPC_ERROR_LOW("if info is NULL, freed?\n");
		dev_kfree_skb(skb);
		BUG();
		return;
	}

	if (!if_info->net_dev_state)
	{
		IPC_ERROR_LOW("%s interface does not exist\n",
				if_info->user_config.if_name);
		dev_kfree_skb(skb);
		return;
	}

	if (NULL == pgsb_ctx)
	{
		IPC_ERROR_LOW("Context is NULL\n");
		dev_kfree_skb(skb);
		return;
	}

	if (skb == NULL)
	{
		IPC_ERROR_LOW("skb is NULL\n");
		dev_kfree_skb(skb);
		return;
	}

	if (skb->dev == NULL)
	{
		skb->dev = if_info->pdev;
	}

	// cancel inactivity timer ...if scheduled
	if (pgsb_ctx->inactivity_timer_scheduled)
	{
		acquire_wake_source();
		spin_lock_bh(&pgsb_ctx->gsb_lock);
		pgsb_ctx->inactivity_timer_scheduled = false;
		is_inactivity_timer_cancelled = true;
		pgsb_ctx->inactivity_timer_cancelled_cnt++;
		spin_unlock_bh(&pgsb_ctx->gsb_lock);
	}


	if (dev_queue_xmit(skb) != 0)
	{
		IPC_ERROR_LOW("could not forward the packet\n");
		if_info->if_ipa->stats.tx_send_err++;
		if (is_inactivity_timer_cancelled)
		{
			release_wake_source();
		}
		return;
	}
	else
	{
		IPC_TRACE_LOW("Downlink data was forwarded successfully\n");
	}

	if_info->if_ipa->stats.tx_send_to_if++;

	if (is_inactivity_timer_cancelled)
	{
		release_wake_source();
	}
}


static void gsb_wakeup_cb(void *ptr)
{
	struct gsb_if_info *if_info = (struct gsb_if_info *)ptr;
	if (if_info == NULL)
	{
		DEBUG_ERROR("NULL node acquired\n");
		BUG();
		return;
	}
	queue_work(gsb_wq, &if_info->ipa_resume_task);
}

static int gsb_ipa_set_perf_level(struct gsb_if_info *if_info)
{
	int retval = 0;
	retval = ipa_bridge_set_perf_profile(if_info->handle, if_info->user_config.bw_reqd_in_mb);
	if (retval)
	{
		DEBUG_ERROR("could not set perf requirement with IPA %d\n", retval);
		return retval;
	}
	return retval;
}

static int gsb_bind_if_to_ipa_bridge(struct gsb_if_info *if_info)
{
	struct ipa_bridge_init_params *params_ptr;
	struct ipa_bridge_init_params params;
	struct gsb_ctx *pgsb_ctx = __gc;
	int retval = 0;


	if (NULL == pgsb_ctx)
	{
		DEBUG_ERROR("Context is NULL\n");
		return -EFAULT;
	}

	params_ptr = &params;
	if (!params_ptr)
	{
		return -ENOMEM;
	}

	/* If IPA bridge is already initizliaed, clean up. */
	if (if_info->is_ipa_bridge_initialized)
	{
		if (ipa_bridge_cleanup(if_info->handle) != 0)
		{
			DEBUG_ERROR("issue in cleaning up IPA bridge for if %s\n",
					if_info->if_name);
			return -EFAULT;
		}
		else
		{
			/* Reset the flag. */
			if_info->is_ipa_bridge_initialized = false;
		}
	}

	/* Initialize the IPA bridge driver */
	params_ptr->info.netdev_name = if_info->user_config.if_name;
	params_ptr->info.priv             = if_info;
	params_ptr->info.tx_dp_notify     = gsb_recv_ipa_notification_cb;
	params_ptr->info.send_dl_skb      = (void *)&gsb_recv_dl_dp;
	params_ptr->info.ipa_desc_size    = (if_info->ipa_free_desc_cnt + 1) *
		sizeof(struct sps_iovec);
	//to do: fix mac address
	switch (if_info->pdev->addr_assign_type)
	{
	case NET_ADDR_PERM:
		memcpy(params_ptr->info.device_ethaddr, if_info->pdev->perm_addr, ETH_ALEN);
		DEBUG_INFO("NET_ADDR_PERM assigned \n");
		break;

	case NET_ADDR_RANDOM:
		DEBUG_TRACE("NET_ADDR_RANDOM assigned\n");
		break;
	case NET_ADDR_STOLEN:
		DEBUG_TRACE("NET_ADDR_STOLEN  assigned\n");
		break;
	case NET_ADDR_SET:
		memcpy(params_ptr->info.device_ethaddr, if_info->pdev->dev_addr, ETH_ALEN);
		DEBUG_INFO("NET_ADDR_SET assigned\n");
		break;

	default:
		DEBUG_TRACE("Invalid assign type\n");
		return -EFAULT;
		break;
	}

	params_ptr->wakeup_request = (void *)&gsb_wakeup_cb;

	retval = ipa_bridge_init(params_ptr, &if_info->handle);
	if (retval != 0)
	{
		DEBUG_ERROR("Couldnt initialize ipa bridge Driver\n");
		return -EFAULT;
	}

	spin_lock_bh(&pgsb_ctx->gsb_lock);
	if_info->is_ipa_bridge_initialized = true;
	spin_unlock_bh(&pgsb_ctx->gsb_lock);

	if (gsb_ipa_set_perf_level(if_info) != 0)
	{
		DEBUG_ERROR("Cannot set perf requirement for if %s\n",
				if_info->if_name);
		return -EFAULT;

	}

	spin_lock_init(&if_info->flow_ctrl_lock);
	if_info->flow_ctrl_lock_acquired = false;

	skb_queue_head_init(&if_info->pend_queue);

	retval = ipa_bridge_connect(if_info->handle);
	if (retval != 0)
	{
		DEBUG_ERROR("ipa bridge connect failed for if %s \n",
				if_info->if_name);
		return -EFAULT;
	}

	//protect these flags & ref counters
	spin_lock_bh(&pgsb_ctx->gsb_lock);
	if_info->is_connected_to_ipa_bridge = true;
	if_info->is_ipa_bridge_suspended = false;
	if_info->net_dev_state = true;
	spin_unlock_bh(&pgsb_ctx->gsb_lock);

	INIT_WORK(&if_info->ipa_send_task, gsb_ipa_send_routine);
	INIT_WORK(&if_info->ipa_resume_task, gsb_ipa_resume_routine);

	DEBUG_INFO("GSB<->IPA bind completed for if %s, MAC:  %X:%X:%X:%X:%X:%X \n",
			if_info->if_name, MAC_ADDR_ARRAY(if_info->pdev->perm_addr));

	/*Idea is if there is no activity for this if
	after it was registered, then we should suspend.
	following code will trigger ianctivity timer if this
	is only interface and there is no activity*/
	acquire_wake_source();
	release_wake_source();
	return retval;
	}

static long gsb_ioctl(struct file *filp,
			unsigned int cmd,
			unsigned long arg)
{
	int retval = 0;
	u32 payload_size;
	struct gsb_ctx *pgsb_ctx = NULL;
	struct gsb_if_config *config = NULL;
	struct gsb_if_info *info = NULL;
	struct if_ipa_ctx *pif_ipa = NULL;


	if (__gc != NULL)
	{
		pgsb_ctx = __gc;
	}
	else
	{
		DEBUG_ERROR("Null Context, cannot execute IOCTL\n");
	}

	switch (cmd)
	{
	case GSB_IOC_ADD_IF_CONFIG:
		DEBUG_TRACE("device %s got GSB_IOC_ADD_IF_CONFIG\n",
				gsb_drv_name);
		payload_size = sizeof(struct gsb_if_config);
		config = kzalloc(payload_size, GFP_KERNEL);
		if (!config)
		{
			DEBUG_ERROR("issue allocating ioctl buffer\n");
			return -ENOMEM;
		}
		pgsb_ctx->mem_alloc_ioctl_buffer++;

		if (copy_from_user(config, (struct gsb_if_config *)arg, payload_size))
		{
			retval = -EFAULT;
			break;
		}

		DEBUG_INFO("iface %s,iface type %d,low wm %d,high wm %d,bw reqd %d\n\n",
				config->if_name, config->if_type, config->if_low_watermark,
				config->if_high_watermark, config->bw_reqd_in_mb);
		// add the config to GSB cache
		if (pgsb_ctx->configured_if_count + 1 > MAX_SUPPORTED_IF_CONFIG)
		{
			DEBUG_ERROR("GSB cannot configure any more IF\n");
			retval = -EPERM;
			break;
		}

		info = kzalloc(sizeof(struct gsb_if_info), GFP_KERNEL);
		if (!info)
		{
			retval = -ENOMEM;
			break;
		}
		pgsb_ctx->mem_alloc_if_node++;


		memcpy(&info->user_config, config, sizeof(struct gsb_if_config));
		//init if related variables here
		info->wq_schedule_cnt = 0;
		info->idle_cnt = 0;
		info->is_debugfs_init = false;
		info->ipa_free_desc_cnt = info->user_config.if_high_watermark;
		info->max_q_len_in_gsb = info->user_config.if_high_watermark *
							GSB_FLOW_CNTRL_QUEUE_MULTIPLIER;
		info->low_watermark = info->user_config.if_low_watermark;
		info->is_ipa_bridge_suspended = true;

		//ipa bridge takes address of if_name so there is a chance
		// it can change the memory so make a local copy of the name
		strlcpy(info->if_name, config->if_name, IFNAMSIZ);
		/* Init IPA Context for the interface*/
		pif_ipa = kzalloc(sizeof(struct if_ipa_ctx), GFP_KERNEL);
		if (!pif_ipa)
		{
			DEBUG_ERROR("kzalloc err.\n");
			kfree(info);
			pgsb_ctx->mem_alloc_if_node--;
			retval =  -ENOMEM;
		}
		else
		{
			info->if_ipa = pif_ipa;
			memset(&info->if_ipa->stats, 0, sizeof(struct gsb_ipa_stats));
			pgsb_ctx->mem_alloc_if_ipa_context++;
		}
		create_debugfs_dir_for_if(info);

		spin_lock_bh(&pgsb_ctx->gsb_lock);
		if (!add_entry_to_ht(info))
		{
			DEBUG_INFO("%s added successfully\n", config->if_name);
			pgsb_ctx->configured_if_count++;
		}
		spin_unlock_bh(&pgsb_ctx->gsb_lock);

		break;


	case GSB_IOC_DEL_IF_CONFIG:
		DEBUG_TRACE("device %s got GSB_IOC_DEL_IF_CONFIG\n",
				gsb_drv_name);
		payload_size = sizeof(struct gsb_if_config);
		config = kzalloc(payload_size, GFP_KERNEL);
		if (!config)
		{
			retval = -ENOMEM;
			break;
		}
		pgsb_ctx->mem_alloc_ioctl_buffer++;

		if (copy_from_user(config, (struct gsb_if_config *)arg, payload_size))
		{
			retval = -EFAULT;
			break;
		}

		DEBUG_INFO("del iface %s\n\n", config->if_name);
		if (pgsb_ctx->configured_if_count == 0)
		{
			DEBUG_TRACE("GSB cannot delete as there are no configured IF\n");
			retval =  -EPERM;
		}

		spin_lock_bh(&pgsb_ctx->gsb_lock);
		info = get_node_info_from_ht(config->if_name);
		spin_unlock_bh(&pgsb_ctx->gsb_lock);

		/* first disconnect this IF from IPA*/
		if (info->is_connected_to_ipa_bridge &&
			ipa_bridge_disconnect(info->handle) == 0)
		{
			spin_lock_bh(&pgsb_ctx->gsb_lock);
			info->is_connected_to_ipa_bridge = false;
			info->is_ipa_bridge_suspended = true;
			info->if_ipa->stats.ipa_suspend_cnt++;
			spin_unlock_bh(&pgsb_ctx->gsb_lock);
			flush_pending_packets(info);
			cancel_work_sync(&info->ipa_resume_task);
			cancel_work_sync(&info->ipa_send_task);
			release_wake_source();
			DEBUG_INFO("IPA bridge dis connected for if %s",
					info->if_name);
		}

		/* delete IF from cache*/
		spin_lock_bh(&pgsb_ctx->gsb_lock);
		if (!remove_entry_from_ht(config))
		{
			DEBUG_INFO("%s removed successfully\n", config->if_name);
			pgsb_ctx->configured_if_count--;
		}
		else
		{
			DEBUG_TRACE("some failure..may be if not in cache\n");
		}
		spin_unlock_bh(&pgsb_ctx->gsb_lock);

		if (info != NULL)
		{
			remove_debugfs_dir_for_if(info->dbg_dir_if);
		}
		else
		{
			DEBUG_ERROR("if node is NULL\n");
			break;
		}

		if (ipa_bridge_cleanup(info->handle) != 0)
		{
			DEBUG_ERROR("issue in cleaning up IPA bridge for if %s\n",
					info->if_name);
		}

		/* can free memory now*/
		kfree(info);
		pgsb_ctx->mem_alloc_if_node--;
		kfree(info->if_ipa);
		pgsb_ctx->mem_alloc_if_ipa_context--;
		break;
	default:
		retval = -ENOTTY;
	}
	kfree(config);
	pgsb_ctx->mem_alloc_ioctl_buffer--;
	return retval;
	}


const struct file_operations gsb_ioctl_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = gsb_ioctl,
	};


static int gsb_ioctl_init(void)
{
	int ret;
	struct device *dev;

	ret = alloc_chrdev_region(&device, 0, dev_num, gsb_drv_name);
	if (ret)
	{
		DEBUG_ERROR("device_alloc err\n");
		goto dev_alloc_err;
	}

	gsb_class = class_create(THIS_MODULE, gsb_drv_name);
	if (IS_ERR(gsb_class))
	{
		DEBUG_ERROR("class_create err\n");
		goto class_err;
	}

	dev = device_create(gsb_class, NULL, device,
			    __gc, gsb_drv_name);
	if (IS_ERR(dev))
	{
		DEBUG_ERROR("device_create err\n");
		goto device_err;
	}

	cdev_init(&gsb_ioctl_cdev, &gsb_ioctl_fops);
	ret = cdev_add(&gsb_ioctl_cdev, device, dev_num);
	if (ret)
	{
		DEBUG_ERROR("cdev_add err\n");
		goto cdev_add_err;
	}

	DEBUG_TRACE("ioctl init OK!!\n");
	return 0;


cdev_add_err:
	device_destroy(gsb_class, device);
device_err:
	class_destroy(gsb_class);
class_err:
	unregister_chrdev_region(device, dev_num);
dev_alloc_err:
	return -ENODEV;
}

static void gsb_ioctl_deinit(void)
{
	cdev_del(&gsb_ioctl_cdev);
	device_destroy(gsb_class, device);
	class_destroy(gsb_class);
	unregister_chrdev_region(device, dev_num);
}

static void gsb_ipa_send_routine(struct work_struct *work)
{
	struct gsb_if_info *if_info = container_of(work,
							struct gsb_if_info, ipa_send_task);
	int ret = 0;
	struct if_ipa_ctx *pipa_ctx = if_info->if_ipa;
	struct gsb_ctx *pgsb_ctx = __gc;
	struct sk_buff *skb;

	if (NULL == pgsb_ctx)
	{
		DEBUG_ERROR("Context is NULL\n");
		return;
	}

	if (IS_ERR_OR_NULL(if_info))
	{
		DEBUG_ERROR("if info is NULL\n");
		return;
	}

	if (IS_ERR_OR_NULL(pipa_ctx))
	{
		DEBUG_ERROR("ipa stats structure is NULL\n");
		return;
	}

	spin_lock_bh(&if_info->flow_ctrl_lock);


	if (if_info->ipa_free_desc_cnt < if_info->low_watermark)
	{
		pipa_ctx->stats.ipa_low_watermark_cnt++;
		spin_unlock_bh(&if_info->flow_ctrl_lock);
		return;
	}


	while (if_info->pend_queue.qlen && if_info->ipa_free_desc_cnt)
	{
		skb = __skb_dequeue(&if_info->pend_queue);
		if (IS_ERR_OR_NULL(skb))
		{
			DEBUG_ERROR("null skb\n");
			break;
		}
		//Send Packet to IPA bridge Driver
		ret = ipa_bridge_tx_dp(if_info->handle, skb, NULL);
		if (ret)
		{
			DEBUG_ERROR("ret %d\n", ret);
			dev_kfree_skb(skb);
			pgsb_ctx->mem_alloc_skb_free++;
			pipa_ctx->stats.drop_send_to_ipa_fail++;
		}
		else
		{
			if_info->wq_schedule_cnt++;
			pipa_ctx->stats.sent_to_ipa++;
			if_info->ipa_free_desc_cnt--;
			pipa_ctx->ipa_rx_completion++;
		}
		/*Giving some time to IPA to send WD events back to GSB*/
		spin_unlock_bh(&if_info->flow_ctrl_lock);
		spin_lock_bh(&if_info->flow_ctrl_lock);
	}
	spin_unlock_bh(&if_info->flow_ctrl_lock);
}

static void gsb_ipa_resume_routine(struct work_struct *work)
	{
	struct gsb_ctx *pgsb_ctx = __gc;
	struct gsb_if_info *if_info = NULL;

	if (IS_ERR_OR_NULL(pgsb_ctx))
	{
		DEBUG_ERROR("NULL Context\n");
		return;
	}

	if_info = container_of(work,struct gsb_if_info,
				ipa_resume_task);

	if (IS_ERR_OR_NULL(if_info))
	{
		DEBUG_ERROR("issue in getting correct node\n");
		return;
	}

	spin_lock_bh(&pgsb_ctx->gsb_lock);
	if (if_info->is_ipa_bridge_suspended)
	{
		spin_unlock_bh(&pgsb_ctx->gsb_lock);
		if (ipa_bridge_resume(if_info->handle) != 0)
		{
			DEBUG_ERROR("issue in resuming IPA for if %s\n",
					if_info->if_name);
			return;
		}
		else
		{
			DEBUG_TRACE("if %s resumed\n", if_info->if_name);
			spin_lock_bh(&pgsb_ctx->gsb_lock);
			if_info->is_ipa_bridge_suspended = false;
			//cancel  inactivity timer if scheduled
			if (pgsb_ctx->inactivity_timer_scheduled)
			{
				pgsb_ctx->inactivity_timer_scheduled = false;
				pgsb_ctx->inactivity_timer_cancelled_cnt++;
			}
			spin_unlock_bh(&pgsb_ctx->gsb_lock);
			acquire_wake_source();
			release_wake_source();
			return;
		}
	}
	spin_unlock_bh(&pgsb_ctx->gsb_lock);
	}


static int gsb_pm_handler(struct notifier_block *nb,
			  unsigned long pm_evt, void *unused)
{
	struct gsb_ctx *pgsb_ctx = __gc;

	if (IS_ERR_OR_NULL(pgsb_ctx))
	{
		DEBUG_ERROR("NULL Context\n");
		return NOTIFY_DONE;
	}

	switch (pm_evt)
	{
	case PM_SUSPEND_PREPARE:
	case PM_HIBERNATION_PREPARE:
	case PM_RESTORE_PREPARE:
		if (pgsb_ctx->do_we_need_wake_source && !pgsb_ctx->is_wake_src_acquired)
		{
			spin_lock_bh(&pgsb_ctx->gsb_wake_lock);
			__pm_stay_awake(&pgsb_ctx->gsb_wake_src);
			DEBUG_INFO("Wake src acquired..will not suspend! if cnt %d, ref cnt %d\n",
					pgsb_ctx->configured_if_count,
					pgsb_ctx->wake_source_ref_count);
			pgsb_ctx->is_wake_src_acquired = true;
			spin_unlock_bh(&pgsb_ctx->gsb_wake_lock);
		}
		else
		{
			DEBUG_INFO("GSB in idle mode .. suspending\n");
		}
		break;
	case PM_POST_SUSPEND:
	case PM_POST_HIBERNATION:
	case PM_POST_RESTORE:
		DEBUG_TRACE("Resuming\n");
		spin_lock_bh(&pgsb_ctx->gsb_wake_lock);
		if (pgsb_ctx->is_wake_src_acquired)
		{
			__pm_relax(&pgsb_ctx->gsb_wake_src);
			pgsb_ctx->is_wake_src_acquired = false;
		}
		spin_unlock_bh(&pgsb_ctx->gsb_wake_lock);
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}


static int gsb_device_event(struct notifier_block *this, unsigned long event, void *ptr)
{
	struct net_device *dev =  netdev_notifier_info_to_dev(ptr);
	struct gsb_ctx *pgsb_ctx = __gc;
	struct gsb_if_info *if_info = NULL;


	if (IS_ERR_OR_NULL(pgsb_ctx))
	{
		DEBUG_ERROR("NULL Context\n");
		return NOTIFY_DONE;
	}
	//obtain node if present in hash.
	if (dev->name != NULL)
	{
		spin_lock_bh(&pgsb_ctx->gsb_lock);
		if_info = get_node_info_from_ht(dev->name);
		spin_unlock_bh(&pgsb_ctx->gsb_lock);

		if (if_info == NULL)
		{
			DEBUG_TRACE("iface %s not registered with gsb\n", dev->name);
			return NOTIFY_DONE;
		}
	}

	DEBUG_INFO("event %d received for iface %s\n", event, dev->name);

	switch (event)
	{
	case NETDEV_DOWN:
		if (if_info->is_connected_to_ipa_bridge)
		{
			// lets send all pending packets to Network stack
			release_pending_packets_exp(if_info);

			// no need for inactivity timer as netdev going down
			if (ipa_bridge_disconnect(if_info->handle) == 0)
			{
				spin_lock_bh(&pgsb_ctx->gsb_lock);
				if_info->is_connected_to_ipa_bridge = false;
				if_info->is_ipa_bridge_suspended = true;
				if_info->if_ipa->stats.ipa_suspend_cnt++;
				if_info->net_dev_state = false;
				spin_unlock_bh(&pgsb_ctx->gsb_lock);
				release_wake_source();
				DEBUG_INFO("IPA bridge dis connected on NETDEV_DOWN for if %s\n",
						if_info->if_name);
			}
			else
			{
				DEBUG_ERROR("ERROR in IPA_BRIDGE_DISCONNECT for if %s\n",
						if_info->if_name);
			}
		}
		DEBUG_TRACE("Net device %s went down\n", dev->name);
		break;
	case NETDEV_REGISTER:
		DEBUG_TRACE("Net dev  %s is registered\n", dev->name);
		break;
	case NETDEV_PRE_UP:
		DEBUG_TRACE("Net %s about to come UP\n", dev->name);

		//IPA should be ready most of the time but we still have
		//this code to be sure.
		if (!pgsb_ctx->is_ipa_ready)
		{
			DEBUG_TRACE("waiting for IPA to be ready\n");
			wait_event_interruptible(wq, (pgsb_ctx->is_ipa_ready != 0));
			if (pgsb_ctx->is_ipa_ready)
			{
				DEBUG_TRACE("IPA is ready now\n");
			}
		}
		//is this netdev in cache?
		//if it is, is it already registered to IPA bridge?
		//if not we need to bind it before it comes up.
		//to do we should not be in atomic context to bind to IPA .IPA needs
		//preemption to do what it need to do.
		if (!in_atomic() && if_info != NULL)
		{
			if_info->pdev = dev;
			if (gsb_bind_if_to_ipa_bridge(if_info) != 0)
			{
				DEBUG_ERROR("failed to bind if %s with IPA bridge\n",
					if_info->if_name);
			}
		}
		else
		{
			DEBUG_TRACE("IPA bridge already initialized for if %s\n",
				if_info->if_name);
		}
		break;

	case NETDEV_UP:
		DEBUG_TRACE("Net dev  %s is up\n", dev->name);
		if (!if_info->is_connected_to_ipa_bridge &&
			if_info->is_ipa_bridge_initialized)
		{
			if (ipa_bridge_connect(if_info->handle) == 0)
			{
				spin_lock_bh(&pgsb_ctx->gsb_lock);
				if_info->is_connected_to_ipa_bridge = true;
				if_info->is_ipa_bridge_suspended = false;
				if_info->net_dev_state = true;
				spin_unlock_bh(&pgsb_ctx->gsb_lock);

				acquire_wake_source();
				DEBUG_INFO("IPA bridge connected on NETDEV_UP for if %s",
						if_info->if_name);

			}
			else
			{
				DEBUG_ERROR("ERROR in IPA_BRIDGE_CONNECT for if %s",
						if_info->if_name);
			}
		}

		break;
	case NETDEV_CHANGE:
		DEBUG_TRACE("Net dev  %s changed\n", dev->name);
		break;
	case NETDEV_CHANGEADDR:
		DEBUG_INFO("Net dev addr  %s changed\n", dev->name);
		DEBUG_ERROR("This operation is not supported in GSB\n");
		WARN_ON(1);
		break;
	case NETDEV_GOING_DOWN:
		DEBUG_TRACE("Net device %s about to go down\n", dev->name);
		break;
	case NETDEV_CHANGENAME:
		DEBUG_TRACE("Net device %s changed name\n", dev->name);
		break;
	}
	return NOTIFY_DONE;
}






/*Do we need this for first version. From discussion, it is assumed that IPA
  is ready but for ODU to bind with IF following steps need to be performed
  Stop IF --> ipa_bridge_init --> Start IF
  Above steps are done from connection manager. if IPA is not ready and IF comes up
  then we will not be able to bind the IF from conn manager to IPA bridge*/
static void gsb_ipa_ready_cb(void *context)
{
	struct gsb_ctx *pgsb_ctx = (struct gsb_ctx *)context;
	DEBUG_TRACE("--- IPA is ready --- \n");
	if (pgsb_ctx->is_ipa_ready)
	{
		DEBUG_TRACE("False event received\n");
		return;
	}

	pgsb_ctx->is_ipa_ready = true;
	wake_up_interruptible(&wq);
}

/*
when packet received here...
scenario 1. inactivity timer scheduled ( wake source avaialble  , if not suspended)
scenario 2  ianctivity timer  expired (no wake source, if suspended)
scenario 3. iactivity timer not scheduled...all good in this case( wake source available)
*/

static int gsb_intercept_packet_in_nw_stack(struct sk_buff *skb)
{
	struct gsb_ctx *pgsb_ctx = __gc;
	struct gsb_if_info *if_info = NULL;
	bool is_pkt_ipv4 = false;
	bool is_pkt_ipv6 = false;
	bool is_inactivity_timer_cancelled = false;
	int ret = 0;
	u32 pkts_to_send, qlen = 0;
	unsigned char *ptr = NULL;
	struct sk_buff *pend_skb;


	if (NULL == pgsb_ctx)
	{
		IPC_ERROR_LOW("Context is NULL\n");
		return -EFAULT;
	}


	spin_lock_bh(&pgsb_ctx->gsb_lock);
	if (!IS_ERR_OR_NULL(skb))
	{
		if_info = get_node_info_from_ht(skb->dev->name);
	}
	else
	{
		IPC_ERROR_LOW("NULL skb passed\n");
		BUG();
	}
	spin_unlock_bh(&pgsb_ctx->gsb_lock);

	if (if_info == NULL)
	{
		IPC_TRACE_LOW("device %s not registered to GSB\n", skb->dev->name);
		return 0;
	}
	else
	{
		IPC_TRACE_LOW("recvd packet from %s if\n", if_info->if_name);
		if_info->if_ipa->stats.total_recv_from_if++;
	}

	if (strncmp(skb->cb, "isthisloop", TAG_LENGTH) == 0)
	{
		IPC_TRACE_LOW("tagged skb with string %s found, skbp= %pK\n", skb->cb, skb);
		return 0;
	}


	is_pkt_ipv4 = is_ipv4_pkt(skb);
	is_pkt_ipv6 = is_ipv6_pkt(skb);

	/* if if not connected to IPA, ther eis nothing we could do here*/
	if (unlikely(!if_info->is_connected_to_ipa_bridge))
	{
		IPC_TRACE_LOW("if %s not connected to IPA bridge yet\n",
				if_info->if_name);
		if_info->if_ipa->stats.exception_ipa_not_connected++;
		return 0;
	}
	/* If Packet is an IP Packet and non-fragmented then Send it to
		IPA Bridge Driver*/
	if (is_non_ip_pkt(skb) ||
		((is_pkt_ipv4) && is_ip_frag_pkt(skb)) ||
		(!if_info->is_connected_to_ipa_bridge))
	{
		/* Send packet to network stack */
		if (is_non_ip_pkt(skb))
			{
			if_info->if_ipa->stats.exception_non_ip_packet++;
			}
		else if (is_ip_frag_pkt(skb))
			{
			if_info->if_ipa->stats.exception_fragmented++;
			}
		return 0;
	}


	// check if if is suspended
	spin_lock_bh(&pgsb_ctx->gsb_lock);
	if (if_info->is_ipa_bridge_suspended)
	{
		//To do : need to see if we can do anything for
		//if IPA bridge to resume here itself.
		//at present let this packet go to nw stack
		queue_work(gsb_wq, &if_info->ipa_resume_task);
		IPC_TRACE_LOW("waiting for if %s to resume\n",
				if_info->if_name);
		if_info->if_ipa->stats.exp_ipa_suspended++;
		spin_unlock_bh(&pgsb_ctx->gsb_lock);
		return 0;
	}
	// cancel inactivity timer ...if scheduled
	if (pgsb_ctx->inactivity_timer_scheduled)
	{
		pgsb_ctx->inactivity_timer_scheduled = false;
		is_inactivity_timer_cancelled = true;
		pgsb_ctx->inactivity_timer_cancelled_cnt++;
	}
	spin_unlock_bh(&pgsb_ctx->gsb_lock);


	spin_lock_bh(&if_info->flow_ctrl_lock);
	qlen = if_info->pend_queue.qlen;
	if (!if_info->ipa_free_desc_cnt &&
		(qlen > (if_info->max_q_len_in_gsb)))
	{
		dev_kfree_skb(skb);
		pgsb_ctx->mem_alloc_skb_free++;
		if_info->if_ipa->stats.drop_flow_control_bottleneck++;
		/*claim packet by setting ret = non zero*/
		ret = GSB_ACCEPT;
		goto unlock_and_schedule;
	}
	//fix skb for IPA
	if (skb_headroom(skb) > ETH_HLEN)
	{
		skb_reset_mac_header(skb);
		ptr = (unsigned char *)skb->data;
		ptr = ptr - ETH_HLEN;
		skb->data = (unsigned char *)ptr;
		skb->len += ETH_HLEN;
	}
	else
	{
		if_info->if_ipa->stats.exp_insufficient_hr++;
		return 0;
	}


	/* Remove extra padding if the rcv_pkt_len == 64 */
	//to do only do for eth, dont do it for wlan
	if (skb->len == ETH_ZLEN && if_info->user_config.if_type == ETH_TYPE)
	{
		IPC_TRACE_LOW("remove_padding\n");
		remove_padding(skb, is_pkt_ipv4, is_pkt_ipv6);
	}

	/* Packet ready for processing now */
	__skb_queue_tail(&if_info->pend_queue, skb);
	IPC_TRACE_LOW("Packet skbp= %pK enqueued,qlen %d,  free desc %d\n",
			skb, if_info->pend_queue.qlen, if_info->ipa_free_desc_cnt);
	/*claim packet by setting ret = non zero*/
	ret = GSB_ACCEPT;

	if (if_info->ipa_free_desc_cnt)
	{
		pkts_to_send = if_info->ipa_free_desc_cnt;
		pkts_to_send = (pkts_to_send > MAX_PACKETS_TO_SEND) ?
			MAX_PACKETS_TO_SEND : pkts_to_send;
		while (if_info->pend_queue.qlen && pkts_to_send)
		{
			pend_skb = __skb_dequeue(&if_info->pend_queue);
			if (unlikely(IS_ERR_OR_NULL(pend_skb)))
			{
				IPC_ERROR_LOW("null skb\n");
				BUG();
			}
			/* Send Packet to IPA bridge Driver */
			ret = ipa_bridge_tx_dp(if_info->handle, pend_skb, NULL);
			if (ret)
			{
				IPC_ERROR_LOW("ret %d, free pkt %pK\n", ret, pend_skb);
				dev_kfree_skb(pend_skb);
				pgsb_ctx->mem_alloc_skb_free++;
				if_info->if_ipa->stats.drop_send_to_ipa_fail++;
				ret = GSB_DROP;
				goto unlock_and_schedule;
			}
			else
			{
				if_info->ipa_free_desc_cnt--;
				if_info->if_ipa->stats.sent_to_ipa++;
				if_info->if_ipa->ipa_rx_completion++;
				ret = GSB_FORWARD;
			}
			pkts_to_send--;
		}
	}

unlock_and_schedule:
	spin_unlock_bh(&if_info->flow_ctrl_lock);
	if (ret == GSB_FORWARD && is_inactivity_timer_cancelled)
	{
		/*this wake source will be released after IPA WDE*/
		acquire_wake_source();
	}
	return ret;
}

static ssize_t gsb_proc_read_cb(struct file *filp,char *buf,size_t count,loff_t *offp )
{
	if (*offp != 0)
		return 0;

	return snprintf(buf, PAGE_SIZE, "%d\n", gsb_enable_ipc_low);
}

static ssize_t gsb_proc_write_cb(struct file *file,const char *buf,size_t count,loff_t *data )

{
	int tmp = 0;

	if(count > MAX_PROC_SIZE)
		count = MAX_PROC_SIZE;
	if(copy_from_user(tmp_buff, buf, count))
		return -EFAULT;

	if (sscanf(tmp_buff, "%du", &tmp) < 0)
		pr_err("sscanf failed\n");
	else {
		if (tmp) {
			if (!ipc_gsb_log_ctxt_low) {
					ipc_gsb_log_ctxt_low = ipc_log_context_create(
						IPCLOG_STATE_PAGES, "gsb_low", 0);
			}
			if (!ipc_gsb_log_ctxt_low) {
				pr_err("failed to create ipc gsb low context\n");
				return -EFAULT;
			}
		} else {
			if (ipc_gsb_log_ctxt_low)
				ipc_log_context_destroy(ipc_gsb_log_ctxt_low);
				ipc_gsb_log_ctxt_low = NULL;
		}
	}
	gsb_enable_ipc_low = tmp;
	return count;
}


static int __init gsb_init_module(void)
{
	int retval = -1;
	struct gsb_ctx *pgsb_ctx = NULL;
	DEBUG_INFO("gsb enter %s\n", DRV_VERSION);


	if (__gc)
	{
		DEBUG_ERROR("GSB context already initialized\n");
		return -EEXIST;
	}

	ipc_gsb_log_ctxt = ipc_log_context_create(IPCLOG_STATE_PAGES,
							"gsb", 0);
	if (!ipc_gsb_log_ctxt)
		pr_err("error creating logging context for GSB\n");
	else
		pr_info("IPC logging has been enabled for GSB\n");

	//define proc file and operations
	memset(&proc_file_ops, 0, sizeof(struct file_operations));
	proc_file_ops.owner = THIS_MODULE;
	proc_file_ops.read =  gsb_proc_read_cb;
	proc_file_ops.write = gsb_proc_write_cb;
	if((proc_file = proc_create("gsb_enable_ipc_low", 0, NULL,
		&proc_file_ops)) == NULL) {
		pr_err(" error creating proc entry!\n");
		return -EINVAL;
	}

	pgsb_ctx = kzalloc(sizeof(struct gsb_ctx), GFP_KERNEL);
	if (pgsb_ctx == NULL)
	{
		DEBUG_ERROR("Memory allocation failed\n");
		return -ENOMEM;
	}
	else
	{
		__gc = pgsb_ctx;
	}

	gsb_wq  = create_singlethread_workqueue("gsb");
	if (!gsb_wq)
	{
		DEBUG_ERROR("Unable to create gsb workqueue\n");
		return -ENOMEM;
	}

	retval = gsb_debugfs_init(pgsb_ctx);
	if (retval != 0)
	{
		DEBUG_ERROR("debugfs init failed\n");
		goto debug_fs_init_failure;
	}

	spin_lock_init(&pgsb_ctx->gsb_lock);
	pgsb_ctx->gsb_lock_acquired = false;


	wakeup_source_init(&pgsb_ctx->gsb_wake_src, "gsb_wake_source");
	pgsb_ctx->do_we_need_wake_source = false;
	pgsb_ctx->is_wake_src_acquired = false;
	pgsb_ctx->mem_alloc_if_ipa_context = 0;
	pgsb_ctx->mem_alloc_if_node = 0;
	pgsb_ctx->mem_alloc_ioctl_buffer = 0;
	pgsb_ctx->mem_alloc_read_stats_buffer = 0;
	pgsb_ctx->mem_alloc_skb_free = 0;



	spin_lock_init(&pgsb_ctx->gsb_wake_lock);
	pgsb_ctx->gsb_wake_lock_acquired = false;



	pgsb_ctx->gsb_dev_notifier.notifier_call = gsb_device_event;
	retval = register_netdevice_notifier(&pgsb_ctx->gsb_dev_notifier);
	if (retval != 0)
	{
		DEBUG_ERROR("registering netdev notification failed\n");
		goto failed_netdev_notification;
	}

	pgsb_ctx->gsb_pm_notifier.notifier_call = gsb_pm_handler;
	retval = register_pm_notifier(&pgsb_ctx->gsb_pm_notifier);
	if (retval != 0)
	{
		DEBUG_ERROR("registering system power notification failed\n");
		goto failed_power_notification;
	}

	retval = gsb_ioctl_init();
	if (retval != 0)
	{
		DEBUG_ERROR("Initializing IOCTL failed\n");
		goto failed_ioctl_initialization;
	}



	//create hash table
	hash_init(pgsb_ctx->cache_htable_list);
	pgsb_ctx->gsb_state_mask = 0;
	pgsb_ctx->is_ipa_ready = false;
	pgsb_ctx->configured_if_count = 0;
	pgsb_ctx->wake_source_ref_count = 0;
	pgsb_ctx->inactivity_timer_cnt = 0;
	pgsb_ctx->inactivity_timer_cancelled_cnt = 0;


	retval = ipa_register_ipa_ready_cb(gsb_ipa_ready_cb, (void *)pgsb_ctx);
	if (retval < 0)
	{
		if (retval == -EEXIST)
		{
			DEBUG_TRACE("-- IPA is Ready retval %d \n",
					retval);
			pgsb_ctx->is_ipa_ready = true;
			//not an error so change return code.
			retval = 0;
		}
		else
		{
			DEBUG_TRACE(" -- IPA is Not Ready retval %d \n",
					retval);
			//not an error so change return code.
			retval = 0;
		}
	}

	setup_timer(&INACTIVITY_TIMER, inactivity_timer_cb, 0);


	/*
	* Hook the receive path in the network stack.
	*/
	BUG_ON(gsb_nw_stack_recv != NULL);
	RCU_INIT_POINTER(gsb_nw_stack_recv, gsb_intercept_packet_in_nw_stack);
	DEBUG_INFO("%s\n", gsb_drv_description);

	return retval;


failed_ioctl_initialization:
	unregister_pm_notifier(&pgsb_ctx->gsb_pm_notifier);
failed_power_notification:
	unregister_netdevice_notifier(&pgsb_ctx->gsb_dev_notifier);
failed_netdev_notification:
	gsb_debugfs_exit(pgsb_ctx);
debug_fs_init_failure:
	destroy_workqueue(gsb_wq);
	kfree(pgsb_ctx);
	DEBUG_ERROR("Failed to load %s\n", gsb_drv_name);
	return retval;
}

static void __exit gsb_exit_module(void)
{
	int ret = 0;
	struct gsb_ctx *pgsb_ctx = __gc;

	if (NULL == pgsb_ctx)
	{
		DEBUG_ERROR("Context is NULL\n");
		return -EFAULT;
	}

	unregister_netdevice_notifier(&pgsb_ctx->gsb_dev_notifier);
	unregister_pm_notifier(&pgsb_ctx->gsb_pm_notifier);

	/*lets delete the if  from cache so no more packets are
		processed from stack*/
	cancel_delayed_work_sync(&if_suspend_wq);
	cleanup_entries_from_ht();
	destroy_workqueue(gsb_wq);

	/*
	 * Unregister our receive callback so we dont intercept at all
	 */
	RCU_INIT_POINTER(gsb_nw_stack_recv, NULL);
	/* delete inactivity timer*/

	ret = del_timer(&INACTIVITY_TIMER);
	if (ret) DEBUG_TRACE("timer still in use\n");


	if (pgsb_ctx->is_wake_src_acquired)
	{
		__pm_relax(&pgsb_ctx->gsb_wake_src);
	}
	wakeup_source_trash(&pgsb_ctx->gsb_wake_src);

	gsb_debugfs_exit(pgsb_ctx);
	gsb_ioctl_deinit();

	kfree(pgsb_ctx);

	proc_remove(proc_file);

	if (ipc_gsb_log_ctxt != NULL)
		ipc_log_context_destroy(ipc_gsb_log_ctxt);

	if (ipc_gsb_log_ctxt_low != NULL)
		ipc_log_context_destroy(ipc_gsb_log_ctxt_low);

	pr_info("exiting GSB %s\n", DRV_VERSION);
}

module_init(gsb_init_module);
module_exit(gsb_exit_module);

MODULE_AUTHOR("The Linux Foundation ");
MODULE_DESCRIPTION("Generic Software Bridge");
MODULE_LICENSE("GPL v2");
