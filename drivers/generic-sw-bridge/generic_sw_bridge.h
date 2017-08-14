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

#ifndef _GENERIC_SW_BRIDGE_H_
#define _GENERIC_SW_BRIDGE_H_


#include <linux/cdev.h>
#include <linux/ipa_odu_bridge.h>




#define MAX_SUPPORTED_IF_CONFIG 1
#define DRV_VERSION "v1.0"
/*
 * MAX buffer length for stats display.
 */
#define MAX_BUFF_LEN 2000
#define INACTIVITY_TIME 200

#define PACKET_DUMP_BUFFER 200
#define PACKET_MP_PRINT_LEN 100
#define READ_STATS_OFFSET 5
#define TAG_LENGTH 48
#define MAX_PACKETS_TO_SEND 50

#define GSB_ACCEPT 1
#define GSB_DROP 2
#define GSB_FORWARD 3

#define GSB_FLOW_CNTRL_QUEUE_MULTIPLIER 4

#define MAC_ADDR_ARRAY(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]

/**
 *  * struct gsb_ipa_stats - GSB - IPA_Bridge/IPA Stats
 *  */
struct gsb_ipa_stats
{
	/* RX Side (UPLINK)*/
	uint64_t total_recv_from_if;
	uint64_t sent_to_ipa;
	uint64_t exception_ipa_not_connected;
	uint64_t exception_non_ip_packet;
	uint64_t exception_fragmented;
	uint64_t drop_flow_control_bottleneck;
	uint64_t ipa_suspend_cnt;
	uint64_t exp_ipa_suspended;
	uint64_t exp_insufficient_hr;
	uint64_t exception_packet_from_ipa;
	uint64_t exp_packet_from_ipa_fail;
	uint64_t drop_send_to_ipa_fail;
	uint64_t exp_if_disconnected;
	uint64_t exp_if_disconnected_fail;

	/* TX Side(DOWNLINK)*/
	uint64_t tx_send_to_if;
	uint64_t tx_send_err;
	uint64_t write_done_from_ipa;

	/* Flow Control Stats */
	uint64_t ipa_low_watermark_cnt;
};

enum if_device_type
{
	WLAN_TYPE = 1,
	ETH_TYPE,
};
struct gsb_if_config
{
	char if_name[IFNAMSIZ];
	u32 bw_reqd_in_mb;
	u16 if_high_watermark;
	u16 if_low_watermark;
	enum if_device_type if_type;
};

/**
 * struct if_ipa_ctx - Interface IPA Context
 *  @stats: GSB - IPA brigde stats
 *  @ipa_rx_completion: Keeps track of pending IPA WRITE DONE Evts
 **/
struct if_ipa_ctx
{
	struct gsb_ipa_stats stats;
	uint64_t ipa_rx_completion;
};


/*
 *  struct gsb_if_info - Each if configured in
 *  GSB will have its own context
 **/
struct gsb_if_info
{
	u32 handle;
	struct dentry *dbg_dir_if;
	struct net_device *pdev;
	struct gsb_if_config  user_config;
	char if_name[IFNAMSIZ];
	bool net_dev_state;
	struct hlist_node cache_ht_node;
	bool is_connected_to_ipa_bridge;
	bool is_wq_scheduled;
	bool is_ipa_bridge_suspended;
	bool is_debugfs_init;
	bool is_ipa_bridge_initialized;

	struct if_ipa_ctx *if_ipa;

	u16 max_q_len_in_gsb;
	u16 low_watermark;
	u16 pendq_cnt;
	u16 freeq_cnt;
	u16 ipa_free_desc_cnt;
	uint64_t wq_schedule_cnt;
	uint64_t idle_cnt;
	spinlock_t flow_ctrl_lock;
	bool flow_ctrl_lock_acquired;

	struct sk_buff_head pend_queue;

	struct list_head pend_queue_head;
	struct list_head free_queue_head;
	struct work_struct ipa_send_task;
	struct work_struct ipa_resume_task;

};

/*
 *  struct gsb_ctx - GSB global Context
 **/
struct gsb_ctx
{
	struct dentry *dbg_dir_root;
	spinlock_t gsb_lock;
	bool gsb_lock_acquired;

	/*
	 * Callback notifiers.
	 */
	struct notifier_block gsb_dev_notifier;
	struct notifier_block gsb_pm_notifier;

	spinlock_t gsb_wake_lock;
	bool gsb_wake_lock_acquired;
	u16 wake_source_ref_count;
	bool do_we_need_wake_source;
	struct wakeup_source gsb_wake_src;
	bool is_wake_src_acquired;

	bool inactivity_timer_scheduled;

	u32 mem_alloc_if_node;
	u32 mem_alloc_read_stats_buffer;
	u32 mem_alloc_if_ipa_context;
	u32 mem_alloc_ioctl_buffer;
	uint64_t mem_alloc_skb_free;

	bool is_ipa_ready;
	u32 gsb_state_mask;
	u16 configured_if_count;
	uint64_t inactivity_timer_cnt;
	uint64_t inactivity_timer_cancelled_cnt;
	struct hlist_head cache_htable_list[MAX_SUPPORTED_IF_CONFIG];
};

u8 NBITS(u32 n)
{
	u8 ret = 0;
	while (n >>= 1) ret++;
	return ret;
}

u32 StringtoAscii(char *str)
{
	int ret = -1;
	int i = 0;
	int len = strlen(str);
	for (i = 0; i < len; i++)
	{
		ret = ret + str[i];
	}

	return ret;
}



// definitions required for IOCTL
static unsigned int dev_num = 1;
static struct cdev gsb_ioctl_cdev;
static struct class *gsb_class;
static dev_t device;
#define GSB_IOC_MAGIC 0xED

/*
 * Ioctls supported by bridge driver
 */
#define GSB_IOC_ADD_IF_CONFIG _IOWR(GSB_IOC_MAGIC, \
	0, \
	struct gsb_if_config *)
#define GSB_IOC_DEL_IF_CONFIG _IOWR(GSB_IOC_MAGIC, \
	1, \
	struct gsb_if_config *)




#endif
