                          Installation Notes
                            Version 2.11.11.0
                              09/01/2017
                 QLogic Linux iSCSI Offload Driver

                          QLogic Corporation
                         26650 Aliso Viejo Pkwy,
			            Aliso Viejo, CA 92656

              Copyright (c) 2014 QLogic Corporation
                           All rights reserved


Table of Contents
=================

  Introduction
  Limitations
  Distros Supported
  Load and Run Necessary iSCSI Software Components
  BNX2I Driver Dependencies
  BNX2I Driver Parameters
  Unloading and Removing Driver
  Driver Messages
  User Application - 'iscsiuio'
  Open-iSCSI User applications
  Bind iSCSI target to QLogic NX2 iSCSI tranport name
  Making connections to iSCSI Targets
  Maximize Offload iSCSI Connections
  Caveats
  

Introduction
============

This file describes the bnx2i Linux drivers for the QLogic's
BCM5706/BCM5708/5709/57710/57711/57711E/57712/578XX
10/100/1000/2500/10G Mbps PCI/PCI-X/PCIE CNIC Network Controller.
The bnx2i driver enables iSCSI offload on QLogic family of
devices.


Distros Supported
=================
	Redhat Enterprise Linux 5.4 and above
	Redhat Enterprise Linux 6.0 and above
	Redhat Enterprise Linux 7.0 and above
	SUSE Linux Enterprise Server 11 SP1 and above
	SUSE Linux Enterprise Server 12 and above

Limitations
===========

The current version of the driver has been tested on 2.6.x kernels starting
from 2.6.32 kernel. The driver may not compile on older kernels except the
distributions listed above.  QLogic QA validation is limited to i386 and
x86_64 architectures, Redhat EL5, EL6, and SUSE 11 SP1 distributions.


Load and Run Necessary iSCSI Software Components
================================================
QLogic iSCSI Offload software suite consists of 3 kernel modules and
a user daemon (iscsiuio).  The user daemon is included in the iscsi-initiator-utils
package in the supported distributions.  Refer to iscsiuio README on how to update
the iscsiuio daemon if necessary.

Required software components can be loaded either manually or
through system services -

1. Unload existing driver if necessary:
Manual:
-------
   # rmmod bnx2i

Please refer to the iscsiuio README for manual termination of the iscsiuio daemon.


2. Load iscsi driver:
Manual:
-------
   # insmod bnx2i.ko
or
   # modprobe bnx2i

Please refer to the iscsiuio README for manual initiation of the iscsiuio daemon.


BNX2I Driver Dependencies
=========================

The driver uses library functions in the scsi_transport_iscsi, bnx2, bnx2x,
cnic and ipv6.ko. It is required to load these library modules either as
loadable module or as kernel built-in component before attempting to load
the driver or unresolved symbol errors will appear. Using modprobe will
resolve the dependencies automatically.


BNX2I Module Parameters
=======================

optional parameters "en_tcp_dack", "error_mask1", and "error_mask2"
can be supplied as a command line arguments to the insmod or modprobe
command for bnx2i.

----------------
event_coal_min :
----------------
Description: "Event Coalescing Minimum Commands", performance tuning parameter
	used to control the minimum rate of interrupt generation by the iscsi
	firmware.

Defaults: 24
Valid Values: 16-32
Note: This parameter is meant for developers to tune the event coalescing for
	performance adjustment and not intended for end users.


----------------
event_coal_div :
----------------
Description: "Event Coalescing Divide Factor", performance tuning parameter
	used to moderate the rate of interrupt generation by the iscsi firmware

Defaults: 2
Valid Values: 1,2,4,8
Note: QLogic did find a single digit improvement in IOPS numbers on 1G chips.
	But QLogic has decided to disable interrupt coalescing for
	5706/5708/5709 as our IOPS numbers are more than double the competition.
	However we believe this parameter makes more sense to 5771x (10G)


------------
en_tcp_dack:
------------
Description: "Enable TCP Delayed ACK", enables/disables TCP delayed ACK
	feature on offloaded iSCSI connections.

Defaults: TCP delayed ACK is ENABLED

e.g.
	# insmod bnx2i.ko en_tcp_dack=0
		or
	# modprobe bnx2i en_tcp_dack=0


-------------
time_stamps :
-------------
Description: "Enable TCP TimeStamps", enables/disables TCP time stamp feature
	on offloaded iSCSI connections.

Defaults: TCP time stamp option is DISABLED

e.g.
	# insmod bnx2i.ko time_stamps=1
		or
	# modprobe bnx2i time_stamps=1


----------------------------
error_mask1 and error_mask2:
----------------------------
Description: "Config FW iSCSI Error Mask #", use to configure
	certain iSCSI protocol violation to treated either as warning
	or fatal error.  All fatal iSCSI protocol violations will result
	in session recovery (ERL 0). These are bit masks.

Defaults:  0 means overriding of the error_masks are DISABLED

CAUTION: Do not meddle with 'error_mask' if you are not sure about the
	consequences. These values are to be discussed with the QLogic
	development team on a case by case basis. This is just a mechanism
	to work around iSCSI implementation issues on the target side and
	without proper knowledge of iSCSI protocol details, users are advised
	not to experiment with these parameters.


--------
sq_size:
--------
Description: "Configure SQ size", used to choose send queue size for offloaded
	connections and SQ size determines maximum SCSI commands that can be
	queued. SQ size also has a bearing on the number of connections that can
	be offloaded, as QP size increases, number of connections supported will
	decrease. With default values, 5706/5708 can offload 28 connections.

Defaults: 128
Range: 32 to 128
Note: QLogic validation is limited to power of 2, e.g. 32, 64, 128.


--------
rq_size:
--------
Description: "Configure RQ size", used to choose size of asynchronous buffer
	queue size per offloaded connections and RQ size is not required be
	greater than 16 as it is used to place iSCSI ASYNC/NOP/REJECT messages
	and SCSI sense data.

Defaults: 16
Range: 16 to 32
Note: QLogic validation is limited to power of 2, e.g. 16, 32.


--------------
tcp_buf_size :
--------------
Description: "TCP send/receive buffer size", used to control the size of both
	the transmit and the receive buffer for offload connections.

Defaults: 64
Valid Values: N/A
CAUTION: Users are strongly advised against altering this parameter.


----------------------
last_active_tcp_port :
----------------------
Description: "Last active TCP port", TCP port monitor parameter
	used to indicate the last used TCP port by the iscsi firmware.

Defaults: N/A
Valid Values: N/A


The parameters can also be set in modprobe.conf. See the man page
for more information.


Unloading and Removing Driver
=============================

To unload the driver, disconnect all active iSCSI sessions to targets and run
the following command -

rmmod bnx2i

NOTE: refer to open-iscsi CLI tool, 'iscsiadm' for session teardown instructions.

If the driver was installed using rpm, do the following to remove it:

rpm -e nextreme2 ***

Note *** - this will remove bnx2, bnx2x and cnic modules as well.

If the driver was installed using make install from the tar file, the driver
bnx2i.ko has to be manually deleted from the system. Refer to the section
"Installing Source RPM Package" for the location of the installed driver.


Driver Messages
===============

The following are the most common sample messages that may be logged in the file
/var/log/messages. Use dmesg -n <level> to control the level at which messages
will appear on the console. Most systems are set to level 6 by default. To see
all messages, set the level higher.

BNX2I Driver signon:
-------------------

QLogic iSCSI Driver bnx2i v2.1.2d (May 12, 2010)


Driver completes handshake with iSCSI Offload Enabled CNIC device:
------------------------------------------------------------------

bnx2i [05:00.00]: ISCSI_INIT passed

NOTE: this message is displayed only when user attempts to make an
iSCSI connection.


Driver detects iSCSI Offload is not enabled on the CNIC device:
---------------------------------------------------------------

bnx2i: iSCSI not supported, dev=eth3
bnx2i: LOM is not enabled to offload iSCSI connections, dev=eth0
bnx2i: dev eth0 does not support iscsi


Exceeds maximum allowed iSCSI connection offload limit:
-------------------------------------------------------

bnx2i: alloc_ep: unable to allocate iscsi cid
bnx2i: unable to allocate iSCSI context resources


Network route to target node and transport name binding are 2 different devices:
--------------------------------------------------------------------------------

bnx2i: conn bind, ep=0x... ($ROUTE_HBA) does not belong to hba $USER_CHOSEN_HBA
	where 	ROUTE_HBA --> net device on which connection was offloaded
				based on route information
		USER_CHOSEN_HBA --> HBA to which target node is bound (using
				iscsi transport name)


Target cannot be reached on any of CNIC devices:
------------------------------------------------

bnx2i: check route, can't connect using cnic


Network route is assigned to network interface which is down:
-------------------------------------------------------------

bnx2i: check route, hba not found


SCSI-ML initiated host reset (session recovery):
------------------------------------------------

bnx2i: attempting to reset host, #3


CNIC detects iSCSI protocol violation - FATAL errors:
-----------------------------------------------------

bnx2i: iscsi_error - wrong StatSN rcvd
bnx2i: iscsi_error - hdr digest err
bnx2i: iscsi_error - data digest err
bnx2i: iscsi_error - wrong opcode rcvd
bnx2i: iscsi_error - AHS len > 0 rcvd
bnx2i: iscsi_error - invalid ITT rcvd
bnx2i: iscsi_error - wrong StatSN rcvd
bnx2i: iscsi_error - wrong DataSN rcvd
bnx2i: iscsi_error - pend R2T violation
bnx2i: iscsi_error - ERL0, UO
bnx2i: iscsi_error - ERL0, U1
bnx2i: iscsi_error - ERL0, U2
bnx2i: iscsi_error - ERL0, U3
bnx2i: iscsi_error - ERL0, U4
bnx2i: iscsi_error - ERL0, U5
bnx2i: iscsi_error - ERL0, U
bnx2i: iscsi_error - invalid resi len
bnx2i: iscsi_error - MRDSL violation
bnx2i: iscsi_error - F-bit not set
bnx2i: iscsi_error - invalid TTT
bnx2i: iscsi_error - invalid DataSN
bnx2i: iscsi_error - burst len violation
bnx2i: iscsi_error - buf offset violation
bnx2i: iscsi_error - invalid LUN field
bnx2i: iscsi_error - invalid R2TSN field
bnx2i: iscsi_error - invalid cmd len1
bnx2i: iscsi_error - invalid cmd len2
bnx2i: iscsi_error - pend r2t exceeds MaxOutstandingR2T value
bnx2i: iscsi_error - TTT is rsvd
bnx2i: iscsi_error - MBL violation
bnx2i: iscsi_error - data seg len != 0
bnx2i: iscsi_error - reject pdu len error
bnx2i: iscsi_error - async pdu len error
bnx2i: iscsi_error - nopin pdu len error
bnx2i: iscsi_error - pend r2t in cleanup
bnx2i: iscsi_error - IP fragments rcvd
bnx2i: iscsi_error - IP options error
bnx2i: iscsi_error - urgent flag error


CNIC detects iSCSI protocol violation - non-FATAL, warning:
-----------------------------------------------------------

bnx2i: iscsi_warning - invalid TTT
bnx2i: iscsi_warning - invalid DataSN
bnx2i: iscsi_warning - invalid LUN field

NOTE: driver by default is configured to consider certain violation to be
treated as warning and not as errors.


Driver puts a session through recovery:
---------------------------------------

conn_err - hostno 3 conn 03fbcd00, iscsi_cid 2 cid a1800


REJECT iSCSI PDU recieved from the target:
------------------------------------------

bnx2i - printing rejected PDU contents
[0]: 1 ffffffa1 0 0 0 0 20 0
[8]: 0 7 0 0 0 0 0 0
[10]: 0 0 40 24 0 0 ffffff80 0
[18]: 0 0 3 ffffff88 0 0 3 4b
[20]: 2a 0 0 2 ffffffc8 14 0 0
[28]: 40 0 0 0 0 0 0 0


Open-iSCSI daemon handing over session to driver:
-------------------------------------------------
bnx2i: conn update - MBL 0x800 FBL 0x800MRDSL_I 0x800 MRDSL_T 0x2000 


User Application - 'iscsiuio':
=================================

The 'iscsiuio' application needs to be installed before iSCSI offload
connections can be made.  If using a device that is not supported by drivers
supplied with the operating system, then you may need to update 'iscsiuio'.
Please refer to the 'iscsiuio' README for details.  Run 'iscsiuio' daemon
before attempting to create iSCSI connections.  Driver won't be able to
establish connections to iSCSI target without daemon's assistance.

	# iscsiuio


Open-iSCSI User applications:
=============================

Install and run open-iscsi programs, 'iscsid' & 'iscsiadm' from the
open-iscsi package, refer to "Packaging" section for more
details. 

1. Start the daemon
	# iscsid

Note that in the case where the open-iscsi package needs to be re-installed,
the previously installed version of iscsiuio will be overwritten.  If the
previous installed version of iscsiuio is so desired, a re-installation
of it is required.


Making connections to iSCSI Targets:
====================================

Please refer to open-iscsi documentation for a comprehensive list of 'iscsiadm'
commands. Here is a sample list of commands to discover targets and
create iscsi connections to target.


Add static entry:
-----------------

	# iscsiadm -m node -p <ipaddr[:port],TPGT> -T <iqn.targetname> \
		-I <iface_file_name> -o new 


iSCSI target discovery using 'SendTargets':
-------------------------------------------

	# iscsiadm -m discovery -t sendtargets -p <ipaddr[:port]>
		-I <iface_file_name>


Login to target using 'iscsiadm' command:
-----------------------------------------

	# iscsiadm -mode node -p <ipaddr[:port]> -T <iqn.targetname> \
		-I <iface_file_name> --login


List all drives active in the system:
-------------------------------------
	# fdisk -l


Bind iSCSI target to QLogic NX2 iSCSI tranport name:
====================================================

By default, the open-iscsi daemon will discover targets using software
initiatior (transport name = 'tcp') if the iface_file_name parameter is not
used. Users who wish to offload iSCSI connection onto CNIC device need to
explicitly re-create the node with the correct iface_file_name specified:
This can be easily done using 'iscsiadm' CLI utility as follows,

	# iscsiadm -m node -T iqn.2004-06.com.broadcom:tg1 \
		-p 192.168.1.100 -I iface_file_name -o new

where the iface file includes the following information: 

	iface.net_ifacename = ethX
	iface.iscsi_ifacename = <name of this iface file> 
	iface.hwaddress = XX:XX:XX:XX:XX:XX <make sure this is lower case>
	iface.ipaddress = XX.XX.XX.XX
	iface.transport_name = bnx2i

Note that the iface.hwaddress must be in the lower case format.

If the user wishes to switch back to use software initiator for whatever reason,
the node must be recreated:
	# iscsiadm -m node -T iqn.2004-06.com.broadcom:tg1 \
		-p 192.168.1.100 -o new

where the iface file specifies TCP transport:

	iface.net_ifacename = ethX
	iface.iscsi_ifacename = <name of this iface file> 
	iface.transport_name = tcp

Please refer to the open-iscsi README for further details.


Maximize iSCSI Offload Connections:
===================================
With default driver parameter set which includes 128 outstanding commands,
bnx2i can offload the following number of connections:
	5706/5708	- 28
	5709		- 43
	5771x		- 128
This is no hard limit, just a simple on chip resource allocation math.
bnx2i will be able to offload > 28 connections on 1G devices by reducing
the shared queue size which in turn limits the maximum outstanding tasks
on a connection. Refer to section "BNX2I Module Parameters" for further
details on sq_size and rq_size. Driver logs the following message to syslog
when maximum allowed connection offload limit is reached -
 "bnx2i: unable to allocate iSCSI context resources"


Caveats:
========

1. iSCSI support on CNIC devices:
---------------------------------

Not all QLogic devices support iSCSI offload, please contact
your server manufacturer on instructions to enable iSCSI offload.


2) iSCSI Session won't recover after hot remove and hotplug:
------------------------------------------------------------

Successive device registration with the iSCSI transport layer will result in
the creation of different 64-bit HBA handles.  Additionally, when the device is
removed from a pre-established session on one HBA, the network route could
possibly resolve the new connection request to a different HBA.  This is not an
acceptable configuration.  QLogic advises administrators to logoff all iSCSI
sessions before removing the HBA from the server.


3. iSCSI Session recovery due to network device operation:
----------------------------------------------------------

The following network device operations will result in iSCSI connection
teardown.  In most cases, the connections will recover automatically depending
on the network configuration:
  a) Ethernet interface reset
	# service network restart
	# ifdown eth#; ifup eth#

  b) Change IP address
	# ifconfig eth0 192.168.1.20

  c) MTU change
	# ifconfig eth0 mtu 1000 up

  d) ethtool selftest
	# ethtool -t eth0


4. MPIO using open-iscsi needs pro-active iSCSI nopout's enabled:
-----------------------------------------------------------------
For MPIO to work properly, iSCSI nopout should be enabled on each iSCSI
sessions.  Please refer to the open-iscsi documentation on how to set the
'noop_out_interval' and the 'noop_out_timeout' values.  Default values can
differ between releases and is advisable to set them to 10 sec and 15 secs
respectively.


5. iSCSI boot using iSCSI Offload with VLAN issue:
--------------------------------------------------
For VLAN configurations, the user has to set the hardware net device identifier
instead of the VLAN net device identifier.  For example:
For VLAN ethX.y net device, use ethX in the net device identification.

Please refer to the iscsiuio README for more details on VLAN operation.


6. iSCSI Sessions can no longer be established after some strenuous reset test:
-------------------------------------------------------------------------------
Please always use the latest open-iscsi util for the supported OS.


7. iSCSI Sessions can be established but no LUNs are presented:
-------------------------------------------------------------------------------
Please make sure the LUNs are unmounted before disconnecting the corresponding
iSCSI connections.  Otherwise, the LUNs will no longer be presented after
subsequent connections.  If encountered, please unmount the stale LUN first
before attempting to reconnect.

8. Smart SAN Support.
---------------------
QLogic iSCSI offload driver would maintain the statistics that need to reported
via Smart SAN RDP protocol. RDP information will be made available through
debugfs interface. New node per host/hba basis will be created by driver.

# cat /sys/kernel/debug/bnx2i/<hostX>/ssan_rdp

Statistics Format: <Attrbute type>: <length>: <Value>

Modules Param *bnx2i_ssan_feature* to enable or disable Smart-SAN feature.
Default is enabled.
