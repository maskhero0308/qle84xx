                               README Notes
                        QLogic bnx2 Linux Driver
                             Version 2.2.6a
                                08/11/2020
                        QLogic cnic Linux Driver
                             Version 2.5.21g
                                10/07/2021

               Copyright (c) 2004 - 2014 Broadcom Corporation
	       Copyright (c) 2014 - 2016 QLogic Corporation
	       Copyright (c) 2016 - 2018 Cavium LLC
	       Copyright (c) 2018 - 2021 Marvell Semiconductors Inc
                           All rights reserved

Table of Contents
=================

  Introduction
  Limitations
  bnx2 Driver Dependencies
  bnx2 Driver Settings
  bnx2 Driver Parameters
  bnx2 Driver Defaults
  Unloading and Removing Driver
  Patching Driver into Kernel Tree
  Driver Messages
  

Introduction
============

This file describes the bnx2 and cnic Linux drivers for the QLogic
BCM5706/BCM5708/5709 10/100/1000/2500 Mbps PCI/PCI-X/PCIE
Ethernet Network Controller. The bnx2 driver is the networking driver and
the cnic driver supports additional features required by the bnx2i iSCSI
offload driver. The bnx2i iSCSI driver is packaged separately. 


Limitations
===========

The current version of the driver has been tested on 2.4.x kernels starting
from 2.4.24 and all 2.6.x and 3.x kernels. The driver may not compile on
kernels older than 2.4.24. Testing is concentrated on i386 and x86_64
architectures.  Only limited testing has been done on some other architectures.

Minor changes to some source files and Makefile may be needed on some
kernels.

Additionally, the Makefile will not compile the cnic driver on kernels
older than 2.6.31. iSCSI offload is only supported on 2.6.31 and newer
kernels.  RHEL5.4 and newer has special backported code to support the
cnic driver and these distributions are supported.


bnx2 Driver Dependencies
========================

The driver uses library functions in the crc32 and zlib_inflate libraries.
On most kernels, these libraries are already built into the kernel. In
some cases, it may be necessary to load these library modules before the
driver or unresolved symbol errors will appear. Using modprobe will
resolve the dependencies automatically.

In rare cases where the crc32 and zlib_inflate libraries are not enabled
in the kernel, it will be necessary to compile the kernel again with the
libraries enabled.


bnx2 Driver Settings
====================

The bnx2 driver settings can be queried and changed using ethtool. The
latest ethtool can be downloaded from
ftp://ftp.kernel.org/pub/software/network/ethtool if it is not already
installed. The following are some common examples on how to use ethtool. See
the ethtool man page for more information. ethtool settings do not persist
across reboot or module reload. The ethtool commands can be put in a startup
script such as /etc/rc.local to preserve the settings across a reboot. On
Red Hat distributions, "ethtool -s" parameters can be specified in the
ifcfg-ethx scripts using the ETHTOOL_OPTS keyword. The specified ethtool
parameters will be set during ifup. Example:

/etc/sysconfig/network-scripts/ifcfg-eth0:

ETHTOOL_OPTS="wol g speed 100 duplex half autoneg off"

Some ethtool examples:

1. Show current speed, duplex, and link status:

   ethtool eth0

2. Change speed, duplex, autoneg:

Example: 100Mbps half duplex, no autonegotiation:

   ethtool -s eth0 speed 100 duplex half autoneg off

Example: Autonegotiation with full advertisement:

   ethtool -s eth0 autoneg on

Example: Autonegotiation with 100Mbps full duplex advertisement only:

   ethtool -s eth0 speed 100 duplex full autoneg on

On blade servers using remote PHY modules, the driver can configure the
link speed of the remote copper PHY if it is present or the SerDes link
speed when not using remote copper PHY. Use "port tp" or "port fibre" to
specify the link settings for remote copper PHY and SerDes respectively.

Example: Autonegotiation with 100Mbps half duplex advertisement only on
         the remote copper PHY if it is present:
 
   ethtool -s eth0 port tp speed 100 duplex half autoneg on

3. Show flow control settings:

   ethtool -a eth0

4. Change flow control settings:

Example: Turn off flow control

   ethtool -A eth0 autoneg off rx off tx off

Example: Turn flow control autonegotiation on with tx and rx advertisement:

   ethtool -A eth0 autoneg on rx on tx on

   Note that this is only valid if speed is set to autonegotiation.

5. Show offload settings:

   ethtool -k eth0

6. Change offload settings:

Example: Turn off TSO (TCP segmentation offload)

   ethtool -K eth0 tso off

7. Get statistics:

   ethtool -S eth0

8. Perform self-test:

   ethtool -t eth0

   Note that the interface (eth0) must be up to do all tests.

9. Show number of channels (rings):

   ethtool -l eth0

   Note that this is only supported on 5709 devices and available on 3.x
   kernels and 3.x ethtool only.

10. Set number of channels (rings):

   ethtool -L eth0 rx N tx N

   rx and tx channel numbers can be different.  0 means use default.  The
   driver selects the best number of channels based on the number of CPU
   cores.

   Note that this is only supported on 5709 devices and available on 3.x
   kernels and 3.x ethtool only.

11. See ethtool man page for more options.


bnx2 Module Parameters
======================

One optional parameter "disable_msi" can be supplied as a command line
argument to the insmod or modprobe command for bnx2. This parameter is used
to disable Message Signaled Interrupts (MSI) and MSI-X.  The parameter is
only valid on 2.6/3.x kernels that support MSI/MSI-X. On 2.4 kernels, this
parameter cannot be used. By default, the driver will enable MSI or MSI-X if
it is supported by the kernel. MSI-X is only supported on 5709 devices.  The
driver will run an interrupt test during initialization to determine if MSI/
MSI-X is working.  If the test passes, the driver will enable MSI/MSI-X.
Otherwise, it will use legacy INTx mode.

Set the "disable_msi" parameter to 1 as shown below to always disable
MSI/MSI-X on all QLogic bnx2 Gigabit NICs in the system.

insmod bnx2.ko disable_msi=1

or

modprobe bnx2 disable_msi=1

The parameter can also be set in modprobe.conf. See the man page
for more information.


bnx2 Driver Defaults
====================

Speed :                    Autonegotiation with all speeds advertised

Flow control :             Autonegotiation with rx and tx advertised

MTU :                      1500 (range 46 - 9000)

Rx Ring Size :              255 (range 0 - 4080)

Rx Jumbo Ring Size :          0 (range 0 - 16320) automatically adjusted by the
                              driver based on MTU and Rx Ring Size.

Tx Ring Size :              255 (range (MAX_SKB_FRAGS+1) - 255)

                            MAX_SKB_FRAGS varies on different kernels and
                            different architectures. On a 2.6/3.x kernel for
                            x86, MAX_SKB_FRAGS is 18.

Number of RSS channels:    Varies depending on the number of CPUs on 5709.
                           (Range 1 - 8)

Number of TSS channels:    Varies depending on the number of CPUs on 5709.
                           (Range 1 - 8)

Coalesce rx usecs :          18 (range 0 - 1023)

Coalesce rx usecs irq :      18 (range 0 - 1023)
	
Coalesce rx frames :          12 (range 0 - 255)

Coalesce rx frames irq :      2 (range 0 - 255)

Coalesce tx usecs :          80 (range 0 - 1023)

Coalesce tx usecs irq :      18 (range 0 - 1023)

Coalesce tx frames :         20 (range 0 - 255)

Coalesce tx frames irq :     2 (range 0 - 255)

Coalesce stats usecs   : 999936 (aprox. 1 sec.)
                                (range 0 - 16776960 in 256 increments)

MSI/MSI-X :                Enabled (if supported by 2.6/3.x kernel and
                                    interrupt test passes)

TSO :                      Enabled on 2.6/3.x kernels

WoL :                      Initial setting based on NVRAM's setting.


Unloading and Removing Driver
=============================

To unload the driver, use ifconfig to bring down all eth# interfaces opened
by the driver, then do the following:

rmmod bnx2

Note that on 2.6/3.x kernels, it is not necessary to bring down the eth#
interfaces before unloading the driver module.

If the cnic driver is loaded, it must be unloaded first before bnx2 can be
unloaded.

If the driver was installed using rpm, do the following to remove it:

rpm -e bnx2


If the driver was installed using make install from the tar file, the driver
bnx2.o (or bnx2.ko) has to be manually deleted from the system. Refer
to the section "Installing Source RPM Package" for the location of the
installed driver.


Patching Driver into Kernel Tree
================================

A simple script is included in the tar file only to patch the bnx2 driver
into the standard kernel tree.  This may be useful for users who compile
their own kernel and wish to use the latest bnx2 driver from QLogic.
The script will create a patch file to replace the bnx2 driver in the
kernel with the one in the tar file.  It will automatically make some
adjustments to ensure that the driver will compile on most 2.6.x and 3.x
kernels.  The patch will only update the bnx2 driver and not the cnic
driver as the latter has additional dependencies.  The kernel with the
updated bnx2 driver will no longer support the Kconfig CONFIG_CNIC option
for QLogic iSCSI and FCoE.  If QLogic iSCSI or FCoE support is
required, the proper way to update the drivers is to backport all relevant
patches for the QLogic drivers from latest git trees.

To create the patch file to update bnx2 in the kernel, do the following:

From the extracted tar file, cd to bnx2/src and run:

./mk_kernel_patch.sh

If no argument is used for the script, it is assumed that the current running
kernel is the one to create the patch for.  It will try to locate the kernel
source tree under /lib/modules/$(uname -r)/source

To create the patch for a different kernel, provide the kernel as the
argument.  For example:

./mk_kernel_patch.sh 2.6.38

The script will then try to locate the kernel tree under
/lib/modules/2.6.38/source and create the patch.

If the script runs successfully, a standard patch file bnx2-<kernel>.patch
will be created.  Using the same example for kernel 2.6.38 above, the patch
file bnx2-2.6.38.patch will be created.

After that, the kernel tree can be patched in the standard way with the
patch file to update the bnx2 driver or reverse the update.  Refer to
http://www.tux.org/lkml for information on how to patch the kernel.


Driver Messages
===============

The following are the most common sample messages that may be logged in the file
/var/log/messages. Use dmesg -n <level> to control the level at which messages
will appear on the console. Most systems are set to level 6 by default. To see
all messages, set the level higher.

bnx2 Driver signon:
-------------------

QLogic bnx2 Gigabit Ethernet Driver v2.2.5jr (Aug 21, 2014)

CNIC Driver signon:
-------------------

QLogic cnic Driver v2.5.20dr (Aug 21, 2014)

NIC detected:
------------

eth0: Broadcom NetXtreme II BCM5706 1000Base-T (A2) PCI 64-bit 66MHz found at mem f6000000, IRQ 16, node addr 0010180476ae

cnic: Added CNIC device: eth0

MSI enabled successfully:
------------------------

bnx2: eth0: using MSI


Link up and speed indication:
----------------------------

bnx2: eth0 NIC Copper Link is Up, 1000 Mbps full duplex, receive & transmit flow control ON
 

Link down indication:
--------------------

bnx2: eth0 NIC Copper Link is Down


Incompatible cnic, bnx2, bnx2x, bnx2i, bnx2fc drivers requiring netxtreme2
--------------------------------------------------------------------------
package to be re-installed:
--------------------------

cnic: bnx2 not compatible with cnic expecting: 0x12340002 got: 0x12340001

cnic: ulp 1 not compatible with cnic, expecting: 0x57770003 got: 0x57770002


iSCSI/FCoE driver getting stuck, the message sometimes appear during shutdown
-----------------------------------------------------------------------------
and is harmless:
---------------

cnic: eth0: Failed waiting for ULP up call to complete.


Hardware error, reload drivers or reboot system:
-----------------------------------------------

cnic: eth0: KCQ index not resetting to 0.
