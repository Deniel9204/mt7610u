/*
 *************************************************************************
 * Ralink Tech Inc.
 * 5F., No.36, Taiyuan St., Jhubei City,
 * Hsinchu County 302,
 * Taiwan, R.O.C.
 *
 * (c) Copyright 2002-2010, Ralink Technology, Inc.
 *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 2 of the License, or     *
 * (at your option) any later version.                                   *
 *                                                                       *
 * This program is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have received a copy of the GNU General Public License     *
 * along with this program; if not, write to the                         *
 * Free Software Foundation, Inc.,                                       *
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 *                                                                       *
 *************************************************************************/


#define RTMP_MODULE_OS

/*#include "rt_config.h" */
#include "rtmp_comm.h"
#include "rt_os_util.h"
#include "rt_os_net.h"

#define RTMP_DRV_NAME  "mt7610u"

/* Following information will be show when you run 'modinfo' */
/* *** If you have a solution for the bug in current version of driver, please mail to me. */
/* Otherwise post to forum in ralinktech's web site(www.ralinktech.com) and let all users help you. *** */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Hans Ulli Kroll <ulli.kroll@googlemail.com>");
MODULE_DESCRIPTION("MT7610U 80211.ac usb driver");
#ifdef CONFIG_STA_SUPPORT
#endif /* CONFIG_STA_SUPPORT */


extern USB_DEVICE_ID rtusb_dev_id[];
extern INT const rtusb_usb_id_len;

static void rt2870_disconnect(
	IN struct usb_device *dev,
	IN void *pAd);

static int rt2870_probe(
	IN struct usb_interface *intf,
	IN struct usb_device *usb_dev,
	IN const USB_DEVICE_ID *dev_id,
	IN void **ppAd);

#ifndef PF_NOFREEZE
#define PF_NOFREEZE  0
#endif


/*extern int rt28xx_close(IN struct net_device *net_dev); */
/*extern int rt28xx_open(struct net_device *net_dev); */

static BOOLEAN USBDevConfigInit(
	IN struct usb_device 	*dev,
	IN struct usb_interface *intf,
	IN void 				*pAd);


void RT28XXVendorSpecificCheck(
	IN struct usb_device 	*dev,
	IN void 				*pAd)
{


	RT_CMD_USB_MORE_FLAG_CONFIG Config = { dev->descriptor.idVendor,
										dev->descriptor.idProduct };
	RTMP_DRIVER_USB_MORE_FLAG_SET(pAd, &Config);
}

/**************************************************************************/
/**************************************************************************/
/*tested for kernel 2.6series */
/**************************************************************************/
/**************************************************************************/

#ifdef CONFIG_PM

static int rt2870_suspend(struct usb_interface *intf, pm_message_t state);
static int rt2870_resume(struct usb_interface *intf);
#endif /* CONFIG_PM */

static int rtusb_probe (struct usb_interface *intf,
						const USB_DEVICE_ID *id);
static void rtusb_disconnect(struct usb_interface *intf);

static BOOLEAN USBDevConfigInit(
	IN struct usb_device 	*dev,
	IN struct usb_interface *intf,
	IN void 				*pAd)
{
	struct usb_host_interface *iface_desc;
	ULONG BulkOutIdx;
	ULONG BulkInIdx;
	UINT32 i;
	RT_CMD_USB_DEV_CONFIG Config, *pConfig = &Config;

	/* get the active interface descriptor */
	iface_desc = intf->cur_altsetting;

	/* get # of enpoints  */
	pConfig->NumberOfPipes = iface_desc->desc.bNumEndpoints;
	DBGPRINT(RT_DEBUG_TRACE, ("NumEndpoints=%d\n", iface_desc->desc.bNumEndpoints));

	/* Configure Pipes */
	BulkOutIdx = 0;
	BulkInIdx = 0;

	for (i = 0; i < pConfig->NumberOfPipes; i++)
	{
		if ((iface_desc->endpoint[i].desc.bmAttributes == USB_ENDPOINT_XFER_BULK) &&
			((iface_desc->endpoint[i].desc.bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN))
		{
			if (BulkInIdx < 2)
			{
				pConfig->BulkInEpAddr[BulkInIdx++] = iface_desc->endpoint[i].desc.bEndpointAddress;
				pConfig->BulkInMaxPacketSize = le2cpu16(iface_desc->endpoint[i].desc.wMaxPacketSize);
				DBGPRINT_RAW(RT_DEBUG_TRACE, ("BULK IN MaxPacketSize = %d\n", pConfig->BulkInMaxPacketSize));
				DBGPRINT_RAW(RT_DEBUG_TRACE, ("EP address = 0x%2x\n", iface_desc->endpoint[i].desc.bEndpointAddress));
			}
			else
			{
				DBGPRINT(RT_DEBUG_ERROR, ("Bulk IN endpoint nums large than 2\n"));
			}
		}
		else if ((iface_desc->endpoint[i].desc.bmAttributes == USB_ENDPOINT_XFER_BULK) &&
				((iface_desc->endpoint[i].desc.bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_OUT))
		{
			if (BulkOutIdx < 6)
			{
				/* there are 6 bulk out EP. EP6 highest priority. */
				/* EP1-4 is EDCA.  EP5 is HCCA. */
				pConfig->BulkOutEpAddr[BulkOutIdx++] = iface_desc->endpoint[i].desc.bEndpointAddress;
				pConfig->BulkOutMaxPacketSize = le2cpu16(iface_desc->endpoint[i].desc.wMaxPacketSize);

				DBGPRINT_RAW(RT_DEBUG_TRACE, ("BULK OUT MaxPacketSize = %d\n", pConfig->BulkOutMaxPacketSize));
				DBGPRINT_RAW(RT_DEBUG_TRACE, ("EP address = 0x%2x  \n", iface_desc->endpoint[i].desc.bEndpointAddress));
			}
			else
			{
				DBGPRINT(RT_DEBUG_ERROR, ("Bulk Out endpoint nums large than 6\n"));
			}
		}
	}

	if (!(pConfig->BulkInEpAddr && pConfig->BulkOutEpAddr[0]))
	{
		printk("%s: Could not find both bulk-in and bulk-out endpoints\n", __FUNCTION__);
		return FALSE;
	}

	pConfig->pConfig = &dev->config->desc;
	usb_set_intfdata(intf, pAd);
	RTMP_DRIVER_USB_CONFIG_INIT(pAd, pConfig);
	RT28XXVendorSpecificCheck(dev, pAd);

	return TRUE;

}



static int rtusb_probe (struct usb_interface *intf,
						const USB_DEVICE_ID *id)
{
	struct rtmp_adapter  *pAd;
	struct usb_device *dev;
	int rv;

	dev = interface_to_usbdev(intf);
	dev = usb_get_dev(dev);

	rv = rt2870_probe(intf, dev, id, &pAd);
	if (rv != 0)
	{
		usb_put_dev(dev);
	}
#ifdef IFUP_IN_PROBE
	else
	{
		if (VIRTUAL_IF_UP(pAd) != 0)
		{
			struct rtmp_adapter  = usb_get_intfdata(intf);
			usb_set_intfdata(intf, NULL);
			rt2870_disconnect(dev, pAd);
			rv = -ENOMEM;
		}
	}
#endif /* IFUP_IN_PROBE */
	return rv;
}


static void rtusb_disconnect(struct usb_interface *intf)
{
	struct usb_device   *dev = interface_to_usbdev(intf);
	void 			*pAd;


	pAd = usb_get_intfdata(intf);
#ifdef IFUP_IN_PROBE
	VIRTUAL_IF_DOWN(pAd);
#endif /* IFUP_IN_PROBE */
	usb_set_intfdata(intf, NULL);

	rt2870_disconnect(dev, pAd);

#ifdef CONFIG_PM
#ifdef USB_SUPPORT_SELECTIVE_SUSPEND
	printk("rtusb_disconnect usb_autopm_put_interface \n");
	usb_autopm_put_interface(intf);
	printk(" ^^rt2870_disconnect ====> pm_usage_cnt %d \n", atomic_read(&intf->pm_usage_cnt));
#endif /* USB_SUPPORT_SELECTIVE_SUSPEND */
#endif /* CONFIG_PM */

}


struct usb_driver rtusb_driver = {
	.name = RTMP_DRV_NAME,
	.probe = rtusb_probe,
	.disconnect = rtusb_disconnect,
	.id_table = rtusb_dev_id,

#ifdef CONFIG_PM
#ifdef USB_SUPPORT_SELECTIVE_SUSPEND
	.supports_autosuspend = 1,
#endif /* USB_SUPPORT_SELECTIVE_SUSPEND */
	.supports_autosuspend = 1,
	suspend:	rt2870_suspend,
	resume:		rt2870_resume,
#endif /* CONFIG_PM */
	};

#ifdef CONFIG_PM

void RT2870RejectPendingPackets(
	IN	void *pAd)
{
	/* clear PS packets */
	/* clear TxSw packets */
}

static int rt2870_suspend(
	struct usb_interface *intf,
	pm_message_t state)
{
	struct net_device *net_dev;
	void *pAd = usb_get_intfdata(intf);

	DBGPRINT(RT_DEBUG_TRACE, ("===> rt2870_suspend()\n"));

#ifdef USB_SUPPORT_SELECTIVE_SUSPEND
	UCHAR Flag;
	DBGPRINT(RT_DEBUG_ERROR, ("autosuspend===> rt2870_suspend()\n"));
#ifdef WOW_SUPPORT
	RTMP_DRIVER_ADAPTER_RT28XX_USB_WOW_STATUS(pAd, &Flag);
	if (Flag == TRUE)
		RTMP_DRIVER_ADAPTER_RT28XX_USB_WOW_ENABLE(pAd);
	else
#endif /* WOW_SUPPORT */
	{
#ifdef CONFIG_STA_SUPPORT
		RTMP_DRIVER_ADAPTER_END_DISSASSOCIATE(pAd);
#endif
		RTMP_DRIVER_ADAPTER_IDLE_RADIO_OFF_TEST(pAd, &Flag);

		if(!Flag)
		{
			RTMP_DRIVER_ADAPTER_RT28XX_USB_ASICRADIO_OFF(pAd);
		}
	}
	RTMP_DRIVER_ADAPTER_SUSPEND_SET(pAd);
	return 0;
#endif /* USB_SUPPORT_SELECTIVE_SUSPEND */

#ifdef CONFIG_STA_SUPPORT
	RTMP_DRIVER_ADAPTER_END_DISSASSOCIATE(pAd);
#endif

	RTMP_DRIVER_ADAPTER_RT28XX_USB_ASICRADIO_OFF(pAd);
	RTMP_DRIVER_ADAPTER_SUSPEND_SET(pAd);

/*	net_dev = pAd->net_dev; */
	//RTMP_DRIVER_NET_DEV_GET(pAd, &net_dev);
	//netif_device_detach(net_dev);

	//RTMP_DRIVER_USB_SUSPEND(pAd, netif_running(net_dev));
	DBGPRINT(RT_DEBUG_TRACE, ("<=== rt2870_suspend()\n"));
	return 0;
}

static int rt2870_resume(
	struct usb_interface *intf)
{
	struct net_device *net_dev;
	void *pAd = usb_get_intfdata(intf);

#ifdef USB_SUPPORT_SELECTIVE_SUSPEND
	INT 		pm_usage_cnt;
	UCHAR Flag;

	pm_usage_cnt = atomic_read(&intf->pm_usage_cnt);

	if(pm_usage_cnt  <= 0)
		usb_autopm_get_interface(intf);

	DBGPRINT(RT_DEBUG_ERROR, ("autosuspend===> rt2870_resume()\n"));

	/*RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_SUSPEND); */
	RTMP_DRIVER_ADAPTER_SUSPEND_CLEAR(pAd);

#ifdef WOW_SUPPORT
	RTMP_DRIVER_ADAPTER_RT28XX_USB_WOW_STATUS(pAd, &Flag);
	if (Flag == TRUE)
		RTMP_DRIVER_ADAPTER_RT28XX_USB_WOW_DISABLE(pAd);
	else
#endif /* WOW_SUPPORT */
	RTMP_DRIVER_ADAPTER_RT28XX_USB_ASICRADIO_ON(pAd);

	DBGPRINT(RT_DEBUG_ERROR, ("autosuspend<===  rt2870_resume()\n"));

	return 0;
#endif /* USB_SUPPORT_SELECTIVE_SUSPEND */
	DBGPRINT(RT_DEBUG_TRACE, ("===> rt2870_resume()\n"));
	mdelay(1000);

	RTMP_DRIVER_ADAPTER_SUSPEND_CLEAR(pAd);
	RTMP_DRIVER_ADAPTER_RT28XX_USB_ASICRADIO_ON(pAd);

/*	pAd->PM_FlgSuspend = 0; */
	//RTMP_DRIVER_USB_RESUME(pAd);

/*	net_dev = pAd->net_dev; */
	//RTMP_DRIVER_NET_DEV_GET(pAd, &net_dev);
	//netif_device_attach(net_dev);
	//netif_start_queue(net_dev);
	//netif_carrier_on(net_dev);
	//netif_wake_queue(net_dev);

	DBGPRINT(RT_DEBUG_TRACE, ("<=== rt2870_resume()\n"));
	return 0;
}
#endif /* CONFIG_PM */

module_usb_driver(rtusb_driver);

/*---------------------------------------------------------------------	*/
/* function declarations												*/
/*---------------------------------------------------------------------	*/


/*
========================================================================
Routine Description:
    Release allocated resources.

Arguments:
    *dev				Point to the PCI or USB device
	pAd					driver control block pointer

Return Value:
    None

Note:
========================================================================
*/
static void rt2870_disconnect(struct usb_device *dev, void *pAd)
{
	struct net_device *net_dev;


	DBGPRINT(RT_DEBUG_ERROR, ("rtusb_disconnect: unregister usbnet usb-%s-%s\n",
				dev->bus->bus_name, dev->devpath));
	if (!pAd)
	{
		usb_put_dev(dev);

		printk("rtusb_disconnect: pAd == NULL!\n");
		return;
	}
/*	RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST); */
	RTMP_DRIVER_NIC_NOT_EXIST_SET(pAd);

	/* for debug, wait to show some messages to /proc system */
	udelay(1);


	RTMP_DRIVER_NET_DEV_GET(pAd, &net_dev);

	RtmpPhyNetDevExit(pAd, net_dev);

	/* FIXME: Shall we need following delay and flush the schedule?? */
	udelay(1);
	flush_scheduled_work();
	udelay(1);

#ifdef RT_CFG80211_SUPPORT
	RTMP_DRIVER_80211_UNREGISTER(pAd, net_dev);
#endif /* RT_CFG80211_SUPPORT */

	/* free the root net_device */
//	RtmpOSNetDevFree(net_dev);

	RtmpRaDevCtrlExit(pAd);

	/* free the root net_device */
	RtmpOSNetDevFree(net_dev);

	/* release a use of the usb device structure */
	usb_put_dev(dev);
	udelay(1);

	DBGPRINT(RT_DEBUG_ERROR, (" RTUSB disconnect successfully\n"));
}


static int rt2870_probe(
	IN struct usb_interface *intf,
	IN struct usb_device *usb_dev,
	IN const USB_DEVICE_ID *dev_id,
	IN void **ppAd)
{
	struct  net_device		*net_dev = NULL;
	void       				*pAd = (void *) NULL;
	INT                 	status, rv;
	void *				handle;
	RTMP_OS_NETDEV_OP_HOOK	netDevHook;
	ULONG					OpMode;
#ifdef CONFIG_PM
#ifdef USB_SUPPORT_SELECTIVE_SUSPEND
/*	INT 		pm_usage_cnt; */
	INT		 res =1 ;
#endif /* USB_SUPPORT_SELECTIVE_SUSPEND */
#endif /* CONFIG_PM */



	DBGPRINT(RT_DEBUG_TRACE, ("===>rt2870_probe()!\n"));

#ifdef CONFIG_PM
#ifdef USB_SUPPORT_SELECTIVE_SUSPEND

        res = usb_autopm_get_interface(intf);
	if (res)
	{
			DBGPRINT(RT_DEBUG_ERROR, ("rt2870_probe autopm_resume fail ------\n"));
		     return -EIO;
	}

	atomic_set(&intf->pm_usage_cnt, 1);
	 printk(" rt2870_probe ====> pm_usage_cnt %d \n", atomic_read(&intf->pm_usage_cnt));


#endif /* USB_SUPPORT_SELECTIVE_SUSPEND */
#endif /* CONFIG_PM */



/*RtmpDevInit============================================= */
	/* Allocate struct rtmp_adapteradapter structure */
/*	handle = kmalloc(sizeof(struct os_cookie), GFP_KERNEL); */
	os_alloc_mem(NULL, (UCHAR **)&handle, sizeof(struct os_cookie));
	if (handle == NULL)
	{
		printk("rt2870_probe(): Allocate memory for os handle failed!\n");
		return -ENOMEM;
	}
	memset(handle, 0, sizeof(struct os_cookie));

	((struct os_cookie *)handle)->pUsb_Dev = usb_dev;

#ifdef CONFIG_PM
#ifdef USB_SUPPORT_SELECTIVE_SUSPEND
	((struct os_cookie *)handle)->intf = intf;
#endif /* USB_SUPPORT_SELECTIVE_SUSPEND */
#endif /* CONFIG_PM */



	/* set/get operators to/from DRIVER module */

	rv = RTMPAllocAdapterBlock(handle, &pAd);
	if (rv != NDIS_STATUS_SUCCESS)
	{
/*		kfree(handle); */
		kfree(handle);
		goto err_out;
	}

/*USBDevInit============================================== */
	if (USBDevConfigInit(usb_dev, intf, pAd) == FALSE)
		goto err_out_free_radev;

	RtmpRaDevCtrlInit(pAd, RTMP_DEV_INF_USB);

/*NetDevInit============================================== */
	net_dev = RtmpPhyNetDevInit(pAd, &netDevHook);
	if (net_dev == NULL)
		goto err_out_free_radev;

	/* Here are the net_device structure with usb specific parameters. */
#ifdef NATIVE_WPA_SUPPLICANT_SUPPORT
	/* for supporting Network Manager.
	  * Set the sysfs physical device reference for the network logical device if set prior to registration will
	  * cause a symlink during initialization.
	 */
	SET_NETDEV_DEV(net_dev, &(usb_dev->dev));
#endif /* NATIVE_WPA_SUPPLICANT_SUPPORT */

#ifdef CONFIG_STA_SUPPORT
/*    pAd->StaCfg.OriDevType = net_dev->type; */
	RTMP_DRIVER_STA_DEV_TYPE_SET(pAd, net_dev->type);
#endif /* CONFIG_STA_SUPPORT */

/*All done, it's time to register the net device to linux kernel. */
	/* Register this device */
#ifdef RT_CFG80211_SUPPORT
{
/*	pAd->pCfgDev = &(usb_dev->dev); */
/*	pAd->CFG80211_Register = CFG80211_Register; */
/*	RTMP_DRIVER_CFG80211_INIT(pAd, usb_dev); */

	/*
		In 2.6.32, cfg80211 register must be before register_netdevice();
		We can not put the register in rt28xx_open();
		Or you will suffer NULL pointer in list_add of
		cfg80211_netdev_notifier_call().
	*/
	CFG80211_Register(pAd, &(usb_dev->dev), net_dev);
}
#endif /* RT_CFG80211_SUPPORT */

	RTMP_DRIVER_OP_MODE_GET(pAd, &OpMode);
	status = RtmpOSNetDevAttach(OpMode, net_dev, &netDevHook);
	if (status != 0)
		goto err_out_free_netdev;

/*#ifdef KTHREAD_SUPPORT */

	*ppAd = pAd;

#ifdef EXT_BUILD_CHANNEL_LIST
	RTMP_DRIVER_SET_PRECONFIG_VALUE(pAd);
#endif /* EXT_BUILD_CHANNEL_LIST */

	DBGPRINT(RT_DEBUG_TRACE, ("<===rt2870_probe()!\n"));

	return 0;

	/* --------------------------- ERROR HANDLE --------------------------- */
err_out_free_netdev:
	RtmpOSNetDevFree(net_dev);

err_out_free_radev:
	RTMPFreeAdapter(pAd);

err_out:
	*ppAd = NULL;

	return -1;

}


