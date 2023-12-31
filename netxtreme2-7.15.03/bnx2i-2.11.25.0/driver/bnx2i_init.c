/* bnx2i.c: QLogic iSCSI driver.
 *
 * Copyright (c) 2006-2014 Broadcom Corporation
 * Copyright (c) 2014 QLogic Corporation
 * Copyright (c) 2007, 2008 Red Hat, Inc.  All rights reserved.
 * Copyright (c) 2007, 2008 Mike Christie
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * Written by: Anil Veerabhadrappa (anilgv@broadcom.com)
 * Maintained by: Eddie Wai (eddie.wai@broadcom.com)
 */

#include "bnx2i.h"
#include "bnx2i_debugfs.h"

static struct list_head adapter_list = LIST_HEAD_INIT(adapter_list);
static u32 adapter_count;

#define DRV_MODULE_NAME		"bnx2i"
#define DRV_MODULE_VERSION	"2.11.25.0"
#define DRV_MODULE_RELDATE	"May 05, 2020"

static char version[] __devinitdata =
		"QLogic iSCSI Driver " DRV_MODULE_NAME \
		" v" DRV_MODULE_VERSION " (" DRV_MODULE_RELDATE ")\n";

MODULE_AUTHOR("Anil Veerabhadrappa <anilgv@broadcom.com> and "
	      "Eddie Wai <eddie.wai@broadcom.com>");
MODULE_DESCRIPTION("QLogic BCM5706/5708/5709/57710/57711/57712"
		   "/57800/57810/57840 iSCSI Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_MODULE_VERSION);
#ifdef THUNK_INLINE
MODULE_INFO(retpoline, "Y");
#endif

static DEFINE_MUTEX(bnx2i_dev_lock);

unsigned int event_coal_min = 24;
module_param(event_coal_min, int, 0664);
MODULE_PARM_DESC(event_coal_min, "Event Coalescing Minimum Commands");

unsigned int event_coal_div = 2;
module_param(event_coal_div, int, 0664);
MODULE_PARM_DESC(event_coal_div, "Event Coalescing Divide Factor");

unsigned int en_tcp_dack = 1;
module_param(en_tcp_dack, int, 0664);
MODULE_PARM_DESC(en_tcp_dack, "Enable TCP Delayed ACK");

unsigned int time_stamps = 0x00;
module_param(time_stamps, int, 0664);
MODULE_PARM_DESC(time_stamps, "Enable TCP TimeStamps");

unsigned int error_mask1 = 0x00;
module_param(error_mask1, uint, 0664);
MODULE_PARM_DESC(error_mask1, "Config FW iSCSI Error Mask #1");

unsigned int error_mask2 = 0x00;
module_param(error_mask2, uint, 0664);
MODULE_PARM_DESC(error_mask2, "Config FW iSCSI Error Mask #2");

unsigned int sq_size;
module_param(sq_size, int, 0664);
MODULE_PARM_DESC(sq_size, "Configure SQ size");

unsigned int rq_size = BNX2I_RQ_WQES_DEFAULT;
module_param(rq_size, int, 0664);
MODULE_PARM_DESC(rq_size, "Configure RQ size");

unsigned int tcp_buf_size = 64;
module_param(tcp_buf_size, int, 0664);
MODULE_PARM_DESC(tcp_buf_size, "TCP send/receive buffer size");
 
unsigned int last_active_tcp_port = 0x00;
module_param(last_active_tcp_port, int, 0664);
MODULE_PARM_DESC(last_active_tcp_port, "Display last tcp src port info");

unsigned int debug_logging = 0x0;
module_param(debug_logging, int, 0664);
MODULE_PARM_DESC(debug_logging, "Print debug informational messages");

unsigned int bnx2i_ssan_feature = 0x1;
module_param(bnx2i_ssan_feature, int, 0664);
MODULE_PARM_DESC(bnx2i_ssan_feature,
		 " Smart SAN feature enable/disable (1/0)");

u64 iscsi_error_mask = 0x00;

DEFINE_PER_CPU(struct bnx2i_percpu_s, bnx2i_percpu);

int bnx2i_create_sysfs_attr(struct bnx2i_hba *hba);
void bnx2i_remove_sysfs_attr(struct bnx2i_hba *hba);

#ifndef _DEFINE_USE_CPU_HP_
static int bnx2i_cpu_callback(struct notifier_block *nfb,
			      unsigned long action, void *hcpu);
/* notification function for CPU hotplug events */
static struct notifier_block bnx2i_cpu_notifier = {
	.notifier_call = bnx2i_cpu_callback,
};
#endif

/**
 * bnx2i_identify_device - identifies QLogic device type
 * @hba: 		Adapter structure pointer
 * @cnic:		Corresponding cnic device
 *
 * This function identifies the NX2 device type and sets appropriate
 *	queue mailbox register access method, 5709 requires driver to
 *	access MBOX regs using *bin* mode
 */
void bnx2i_identify_device(struct bnx2i_hba *hba, struct cnic_dev *dev)
{
	hba->cnic_dev_type = 0;
	if (test_bit(CNIC_F_BNX2_CLASS, &dev->flags)) {
		if ((hba->pci_did == PCI_DEVICE_ID_NX2_5706) ||
		    (hba->pci_did == PCI_DEVICE_ID_NX2_5706S))
			set_bit(BNX2I_NX2_DEV_5706, &hba->cnic_dev_type);
		else if ((hba->pci_did == PCI_DEVICE_ID_NX2_5708) ||
		    (hba->pci_did == PCI_DEVICE_ID_NX2_5708S))
			set_bit(BNX2I_NX2_DEV_5708, &hba->cnic_dev_type);
		else if ((hba->pci_did == PCI_DEVICE_ID_NX2_5709) ||
		    (hba->pci_did == PCI_DEVICE_ID_NX2_5709S)) {
			set_bit(BNX2I_NX2_DEV_5709, &hba->cnic_dev_type);
			hba->mail_queue_access = BNX2I_MQ_BIN_MODE;
		}
	} else if (test_bit(CNIC_F_BNX2X_CLASS, &dev->flags))
		set_bit(BNX2I_NX2_DEV_57710, &hba->cnic_dev_type);
	else
		printk(KERN_ALERT "bnx2i: unknown device, 0x%x\n",
				  hba->pci_did);
}


/**
 * get_adapter_list_head - returns head of adapter list
 */
struct bnx2i_hba *get_adapter_list_head(void)
{
	struct bnx2i_hba *hba = NULL;
	struct bnx2i_hba *tmp_hba;

	if (!adapter_count)
		goto hba_not_found;

	mutex_lock(&bnx2i_dev_lock);
	list_for_each_entry(tmp_hba, &adapter_list, link) {
		if (tmp_hba->cnic && tmp_hba->cnic->cm_select_dev) {
			hba = tmp_hba;
			break;
		}
	}
	mutex_unlock(&bnx2i_dev_lock);
hba_not_found:
	return hba;
}


/**
 * bnx2i_find_hba_for_cnic - maps cnic device instance to bnx2i adapter instance
 * @cnic:	pointer to cnic device instance
 *
 */
struct bnx2i_hba *bnx2i_find_hba_for_cnic(struct cnic_dev *cnic)
{
	struct bnx2i_hba *hba, *temp;

	mutex_lock(&bnx2i_dev_lock);
	list_for_each_entry_safe(hba, temp, &adapter_list, link) {
		if (hba->cnic == cnic) {
			mutex_unlock(&bnx2i_dev_lock);
			return hba;
		}
	}
	mutex_unlock(&bnx2i_dev_lock);
	return NULL;
}


/**
 * bnx2i_start - cnic callback to initialize & start adapter instance
 * @handle:	transparent handle pointing to adapter structure
 *
 * This function maps adapter structure to pcidev structure and initiates
 *	firmware handshake to enable/initialize on chip iscsi components
 * 	This bnx2i - cnic interface api callback is issued after following
 *	2 conditions are met -
 *	  a) underlying network interface is up (marked by event 'NETDEV_UP'
 *		from netdev
 *	  b) bnx2i adapter instance is registered
 */
void bnx2i_start(void *handle)
{
#define BNX2I_INIT_POLL_TIME	(1000 / HZ)
	struct bnx2i_hba *hba = handle;
	int i = HZ;

	/* On some bnx2x devices, it is possible that iSCSI is no
	 * longer supported after firmware is downloaded.  In that
	 * case, the iscsi_init_msg will return failure.
	 */

	bnx2i_send_fw_iscsi_init_msg(hba);
	while (!test_bit(ADAPTER_STATE_UP, &hba->adapter_state) &&
	       !test_bit(ADAPTER_STATE_INIT_FAILED, &hba->adapter_state) && i--)
		msleep(BNX2I_INIT_POLL_TIME);
}


/**
 * bnx2i_chip_cleanup - local routine to handle chip cleanup
 * @hba:	Adapter instance to register
 *
 * Driver checks if adapter still has any active connections before
 *	executing the cleanup process
 */
static void bnx2i_chip_cleanup(struct bnx2i_hba *hba)
{
	struct bnx2i_endpoint *bnx2i_ep;
	struct list_head *pos, *tmp;

	if (hba->ofld_conns_active) {
		/* Stage to force the disconnection
		 * This is the case where the daemon is either slow or
		 * not present
		 */
		printk(KERN_ALERT "bnx2i: (%s) chip cleanup for %d active "
			"connections\n", hba->netdev->name,
			hba->ofld_conns_active);
		mutex_lock(&hba->net_dev_lock);
		list_for_each_safe(pos, tmp, &hba->ep_active_list) {
			bnx2i_ep = list_entry(pos, struct bnx2i_endpoint, link);
			/* Clean up the chip only */
			bnx2i_hw_ep_disconnect(bnx2i_ep);
			bnx2i_ep->cm_sk = NULL;
		}
		mutex_unlock(&hba->net_dev_lock);
	}
}


/**
 * bnx2i_stop - cnic callback to shutdown adapter instance
 * @handle:	transparent handle pointing to adapter structure
 *
 * driver checks if adapter is already in shutdown mode, if not start
 *	the shutdown process
 */
void bnx2i_stop(void *handle)
{
	struct bnx2i_hba *hba = handle;
	int conns_active;
	int wait_delay = 1 * HZ;

	/* check if cleanup happened in GOING_DOWN context */
	if (!test_and_set_bit(ADAPTER_STATE_GOING_DOWN,
			      &hba->adapter_state)) {
		iscsi_host_for_each_session(hba->shost,
					    bnx2i_drop_session);
		wait_delay = hba->hba_shutdown_tmo;
	}
	/* Wait for inflight offload connection tasks to complete before
	 * proceeding. Forcefully terminate all connection recovery in
	 * progress at the earliest, either in bind(), send_pdu(LOGIN),
	 * or conn_start()
	 */
	wait_event_interruptible_timeout(hba->eh_wait,
					 (list_empty(&hba->ep_ofld_list) &&
					 list_empty(&hba->ep_destroy_list)),
					 2 * HZ);
	/* Wait for all endpoints to be torn down, Chip will be reset once
	 *  control returns to network driver. So it is required to cleanup and
	 * release all connection resources before returning from this routine.
	 */
	while (hba->ofld_conns_active) {
		conns_active = hba->ofld_conns_active;
		wait_event_interruptible_timeout(hba->eh_wait,
				(hba->ofld_conns_active != conns_active),
				wait_delay);
		if (hba->ofld_conns_active == conns_active)
			break;
	}
	bnx2i_chip_cleanup(hba);

	/* This flag should be cleared last so that ep_disconnect() gracefully
	 * cleans up connection context
	 */
	clear_bit(ADAPTER_STATE_GOING_DOWN, &hba->adapter_state);
	clear_bit(ADAPTER_STATE_UP, &hba->adapter_state);
}


/**
 * bnx2i_init_one - initialize an adapter instance and allocate memory resources
 * @hba:	bnx2i adapter instance
 * @cnic:	cnic device handle
 *
 * Global resource lock is held during critical sections below. This routine is
 *	called from either cnic_register_driver() or device hot plug context and
 *	and does majority of device specific initialization
 */
static int bnx2i_init_one(struct bnx2i_hba *hba, struct cnic_dev *cnic)
{
	int rc;

	mutex_lock(&bnx2i_dev_lock);
	if (!cnic->max_iscsi_conn) {
		printk(KERN_ALERT "bnx2i: dev %s does not support "
			"iSCSI\n", hba->netdev->name);
		rc = -EOPNOTSUPP;
		goto out;
	}

	hba->cnic = cnic;
	rc = cnic->register_device(cnic, CNIC_ULP_ISCSI, hba);
	if (!rc) {
		hba->age++;
		set_bit(BNX2I_CNIC_REGISTERED, &hba->reg_with_cnic);
		list_add_tail(&hba->link, &adapter_list);
		adapter_count++;
	} else if (rc == -EBUSY) 	/* duplicate registration */
		printk(KERN_ALERT "bnx2i, duplicate registration"
				  "hba=%p, cnic=%p\n", hba, cnic);
	else if (rc == -EAGAIN)
		printk(KERN_ERR "bnx2i, driver not registered\n");
	else if (rc == -EINVAL)
		printk(KERN_ERR "bnx2i, invalid type %d\n", CNIC_ULP_ISCSI);
	else
		printk(KERN_ERR "bnx2i dev reg, unknown error, %d\n", rc);

	set_bit(CNIC_F_ISCSI_OOO_ENABLE, &cnic->flags);
out:
	mutex_unlock(&bnx2i_dev_lock);

	return rc;
}


/**
 * bnx2i_ulp_init - initialize an adapter instance
 * @dev:	cnic device handle
 *
 * Called from cnic_register_driver() context to initialize all enumerated
 *	cnic devices. This routine allocate adapter structure and other
 *	device specific resources.
 */
void bnx2i_ulp_init(struct cnic_dev *dev)
{
	struct bnx2i_hba *hba;

	if (dev->version != CNIC_DEV_VER) {
		printk(KERN_WARNING "bnx2i init: cnic not compatible, "
				"expecting: 0x%x got: 0x%x\n",
				CNIC_DEV_VER, dev->version);
		return;
	}

	/* Allocate a HBA structure for this device */
	hba = bnx2i_alloc_hba(dev);
	if (!hba) {
		printk(KERN_ERR "bnx2i init: (%s) hba initialization failed\n",
			dev->netdev->name);
		return;
	}

	/* Get PCI related information and update hba struct members */
	clear_bit(BNX2I_CNIC_REGISTERED, &hba->reg_with_cnic);
	if (bnx2i_init_one(hba, dev)) {
		printk(KERN_ERR "bnx2i init: (%s) hba %p init failed\n",
			dev->netdev->name, hba);
		bnx2i_free_hba(hba);
		return;
	}

	bnx2i_create_sysfs_attr(hba);
}


/**
 * bnx2i_ulp_exit - shuts down adapter instance and frees all resources
 * @dev:	cnic device handle
 *
 */
void bnx2i_ulp_exit(struct cnic_dev *dev)
{
	struct bnx2i_hba *hba;

	hba = bnx2i_find_hba_for_cnic(dev);
	if (!hba) {
		printk(KERN_INFO "bnx2i_ulp_exit: hba not "
				 "found, dev 0x%p\n", dev);
		return;
	}
	mutex_lock(&bnx2i_dev_lock);
	list_del_init(&hba->link);
	adapter_count--;

	if (test_bit(BNX2I_CNIC_REGISTERED, &hba->reg_with_cnic)) {
		hba->cnic->unregister_device(hba->cnic, CNIC_ULP_ISCSI);
		clear_bit(BNX2I_CNIC_REGISTERED, &hba->reg_with_cnic);
	}
	mutex_unlock(&bnx2i_dev_lock);

	bnx2i_remove_sysfs_attr(hba);
	bnx2i_free_hba(hba);
}

/**
 * bnx2i_get_boot_session - Get boot session data for ocbb.
 * @hba:        adapter instance pointer
 */
static void bnx2i_get_boot_session(struct bnx2i_hba *hba)
{
	struct iscsi_cls_session *cls_sess;
	struct bnx2i_endpoint *bnx2i_ep;
	struct bnx2i_conn *bnx2i_conn;
	struct iscsi_stats_info *stats;
	struct iscsi_conn *conn;
	struct cnic_sock *csk;
	int i;

	stats = (struct iscsi_stats_info *)hba->cnic->stats_addr;
	if (!stats)
		return;

	for (i = 0; i < hba->max_active_conns; i++) { 
		bnx2i_conn = bnx2i_get_conn_from_id(hba, i);
		if (!bnx2i_conn)
			continue;

		cls_sess = iscsi_conn_to_session(bnx2i_conn->cls_conn);
		if (!iscsi_is_session_online(cls_sess))
			continue;

		conn = bnx2i_conn->cls_conn->dd_data;
		bnx2i_ep = bnx2i_conn->ep;
		csk = bnx2i_ep->cm_sk;
		if (!csk)
			continue;

		memset(&stats->boot_target_ip[0], 0,
		       sizeof(stats->boot_target_ip));
		memset(&stats->boot_init_ip[0], 0,
		       sizeof(stats->boot_init_ip));

		if (!test_bit(SK_F_IPV6, &csk->flags)) {
			/* If IP address is IPv4, Set bytes 10 and 11 in
			 * IP address field to 0xFF,
			 * Copy IP address to bytes 12-15
			 */
			memset(&stats->boot_target_ip[10], 0xff, 2*sizeof(char));
			memset(&stats->boot_init_ip[10], 0xff, 2*sizeof(char));
			memcpy(&stats->boot_init_ip[12],&csk->src_ip[0],
			       sizeof(struct in_addr));
			memcpy(&stats->boot_target_ip[12],&csk->dst_ip[0],
			       sizeof(struct in_addr));
		} else {
			memcpy(&stats->boot_init_ip[0],&csk->src_ip[0],
			       sizeof(struct in6_addr));
			memcpy(&stats->boot_target_ip[0],&csk->dst_ip[0],
			       sizeof(struct in6_addr));
		}
		break;
	}
}

/**
 * bnx2i_get_stats - Retrieve various statistic from iSCSI offload
 * @handle:		bnx2i_hba
 *
 * function callback exported via bnx2i - cnic driver interface to
 *	retrieve various iSCSI offload related statistics.
 */
int bnx2i_get_stats(void *handle)
{
	struct bnx2i_hba *hba = handle;
	struct iscsi_stats_info *stats;

	if (!hba)
		return -EINVAL;

	stats = (struct iscsi_stats_info *)hba->cnic->stats_addr;

	if (!stats)
		return -ENOMEM;

	strlcpy(stats->version, DRV_MODULE_VERSION, sizeof(stats->version));
	memcpy(stats->mac_add1 + 2, hba->cnic->mac_addr, ETH_ALEN);

	stats->max_frame_size = hba->netdev->mtu;
	stats->txq_size = hba->max_sqes;
	stats->rxq_size = hba->max_cqes;

	/* Loop through all ep to get the cqe_left average */
	stats->txq_avg_depth = 0;
	stats->rxq_avg_depth = 0;

	GET_STATS_64(hba, stats, rx_pdus);
	GET_STATS_64(hba, stats, rx_bytes);

	GET_STATS_64(hba, stats, tx_pdus);
	GET_STATS_64(hba, stats, tx_bytes);

	bnx2i_get_boot_session(handle);

	return 0;
}

#ifndef _DEFINE_USE_CPU_HP_
/**
 * bnx2i_percpu_thread_create - Create a receive thread for an
 *				online CPU
 *
 * @cpu:	cpu index for the online cpu
 */
static void bnx2i_percpu_thread_create(unsigned int cpu)
{
	struct bnx2i_percpu_s *p;
	struct task_struct *thread;

	p = &per_cpu(bnx2i_percpu, cpu);

	thread = kthread_create_on_node(bnx2i_percpu_io_thread, (void *)p,
					cpu_to_node(cpu),
					"bnx2i_thread/%d", cpu);
	/* bind thread to the cpu */
	if (likely(!IS_ERR(thread))) {
		kthread_bind(thread, cpu);
		p->iothread = thread;
		wake_up_process(thread);
	}
}


static void bnx2i_percpu_thread_destroy(unsigned int cpu)
{
	struct bnx2i_percpu_s *p;
	struct task_struct *thread;
	struct bnx2i_work *work, *tmp;

	/* Prevent any new work from being queued for this CPU */
	p = &per_cpu(bnx2i_percpu, cpu);
	spin_lock_bh(&p->p_work_lock);
	thread = p->iothread;
	p->iothread = NULL;

	/* Free all work in the list */
	list_for_each_entry_safe(work, tmp, &p->work_list, list) {
		list_del_init(&work->list);
		bnx2i_process_scsi_cmd_resp(work->session,
					    work->bnx2i_conn, &work->cqe);
		kfree(work);
	}

	spin_unlock_bh(&p->p_work_lock);
	if (thread)
		kthread_stop(thread);
}
#endif

#ifdef _DEFINE_USE_CPU_HP_
/**
 * bnx2i_cpu_online - Create a receive thread for an online CPU
 *
 * @cpu:   cpu index for the online cpu
 */
static int bnx2i_cpu_online(unsigned int cpu)
{
	struct bnx2i_percpu_s *p;
	struct task_struct *thread;

	p = &per_cpu(bnx2i_percpu, cpu);

	thread = kthread_create_on_node(bnx2i_percpu_io_thread, (void *)p,
					cpu_to_node(cpu),
					"bnx2i_thread/%d", cpu);
	if (IS_ERR(thread))
		return PTR_ERR(thread);

	/* bind thread to the cpu */
	kthread_bind(thread, cpu);
	p->iothread = thread;
	wake_up_process(thread);
	return 0;
}

static int bnx2i_cpu_offline(unsigned int cpu)
{
	struct bnx2i_percpu_s *p;
	struct task_struct *thread;
	struct bnx2i_work *work, *tmp;

	/* Prevent any new work from being queued for this CPU */
	p = &per_cpu(bnx2i_percpu, cpu);
	spin_lock_bh(&p->p_work_lock);
	thread = p->iothread;
	p->iothread = NULL;

	/* Free all work in the list */
	list_for_each_entry_safe(work, tmp, &p->work_list, list) {
		list_del_init(&work->list);
		bnx2i_process_scsi_cmd_resp(work->session,
					    work->bnx2i_conn, &work->cqe);
		kfree(work);
	}

	spin_unlock_bh(&p->p_work_lock);
	if (thread)
		kthread_stop(thread);
	return 0;
}

static enum cpuhp_state bnx2i_online_state;

#else
/**
 * bnx2i_cpu_callback - Handler for CPU hotplug events
 *
 * @nfb:	The callback data block
 * @action:	The event triggering the callback
 * @hcpu:	The index of the CPU that the event is for
 *
 * This creates or destroys per-CPU data for iSCSI
 *
 * Returns NOTIFY_OK always.
 */
static int bnx2i_cpu_callback(struct notifier_block *nfb,
			      unsigned long action, void *hcpu)
{
	unsigned cpu = (unsigned long)hcpu;

	switch (action) {
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		printk(KERN_INFO "bnx2i: CPU %x online: Create Rx thread\n",
			cpu);
		bnx2i_percpu_thread_create(cpu);
		break;
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		printk(KERN_INFO "CPU %x offline: Remove Rx thread\n", cpu);
		bnx2i_percpu_thread_destroy(cpu);
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}
#endif

/**
 * bnx2i_mod_init - module init entry point
 *
 * initialize any driver wide global data structures such as endpoint pool,
 *	tcp port manager/queue, sysfs. finally driver will register itself
 *	with the cnic module
 */
static int __init bnx2i_mod_init(void)
{
	int err;
	unsigned cpu = 0;
	struct bnx2i_percpu_s *p;

	printk(KERN_INFO "%s", version);

	if (sq_size && !is_power_of_2(sq_size))
		sq_size = roundup_pow_of_two(sq_size);

	mutex_init(&bnx2i_dev_lock);

	bnx2i_scsi_xport_template =
			iscsi_register_transport(&bnx2i_iscsi_transport);
	if (!bnx2i_scsi_xport_template) {
		printk(KERN_ERR "Could not register bnx2i transport.\n");
		err = -ENOMEM;
		goto out;
	}

	bnx2i_dbg_init(BNX2_ISCSI_DRIVER_NAME);
	err = cnic_register_driver2(CNIC_ULP_ISCSI, &bnx2i_cnic_cb);
	if (err) {
		printk(KERN_ERR "Could not register bnx2i cnic driver.\n");
		goto unreg_xport;
	}

	/* Create percpu kernel threads to handle iSCSI I/O completions */
	for_each_possible_cpu(cpu) {
		p = &per_cpu(bnx2i_percpu, cpu);
		INIT_LIST_HEAD(&p->work_list);
		spin_lock_init(&p->p_work_lock);
		p->iothread = NULL;
	}

#ifdef _DEFINE_USE_CPU_HP_
	err = cpuhp_setup_state(CPUHP_AP_ONLINE_DYN, "scsi/bnx2i:online",
				bnx2i_cpu_online, bnx2i_cpu_offline);
	if (err < 0)
		goto unreg_driver;
	bnx2i_online_state = err;
#else
	for_each_online_cpu(cpu)
		bnx2i_percpu_thread_create(cpu);

	/* Initialize per CPU interrupt thread */
	register_hotcpu_notifier(&bnx2i_cpu_notifier);
#endif

	return 0;

#ifdef _DEFINE_USE_CPU_HP_
unreg_driver:
	cnic_unregister_driver2(CNIC_ULP_ISCSI);
#endif

unreg_xport:
	iscsi_unregister_transport(&bnx2i_iscsi_transport);
out:
	bnx2i_dbg_exit();
	return err;
}


/**
 * bnx2i_mod_exit - module cleanup/exit entry point
 *
 * Global resource lock and host adapter lock is held during critical sections
 *	in this function. Driver will browse through the adapter list, cleans-up
 *	each instance, unregisters iscsi transport name and finally driver will
 *	unregister itself with the cnic module
 */
static void __exit bnx2i_mod_exit(void)
{
	struct bnx2i_hba *hba;

#ifndef _DEFINE_USE_CPU_HP_
	unsigned cpu = 0;
#endif

	mutex_lock(&bnx2i_dev_lock);
	while (!list_empty(&adapter_list)) {
		hba = list_entry(adapter_list.next, struct bnx2i_hba, link);
		list_del(&hba->link);
		adapter_count--;

		if (test_bit(BNX2I_CNIC_REGISTERED, &hba->reg_with_cnic)) {
			bnx2i_chip_cleanup(hba);
			hba->cnic->unregister_device(hba->cnic, CNIC_ULP_ISCSI);
			clear_bit(BNX2I_CNIC_REGISTERED, &hba->reg_with_cnic);
		}

		bnx2i_free_hba(hba);
	}
	mutex_unlock(&bnx2i_dev_lock);

#ifdef _DEFINE_USE_CPU_HP_
	cpuhp_remove_state(bnx2i_online_state);
#else
	unregister_hotcpu_notifier(&bnx2i_cpu_notifier);

	for_each_online_cpu(cpu)
		bnx2i_percpu_thread_destroy(cpu);

#endif

	bnx2i_dbg_exit();
	iscsi_unregister_transport(&bnx2i_iscsi_transport);
	cnic_unregister_driver2(CNIC_ULP_ISCSI);
}

module_init(bnx2i_mod_init);
module_exit(bnx2i_mod_exit);
