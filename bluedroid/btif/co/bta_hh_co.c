/******************************************************************************
 *
 *  Copyright (C) 2009-2012 Broadcom Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

//#if (defined(BTA_HH_INCLUDED) && (BTA_HH_INCLUDED == TRUE))

#include <ctype.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <linux/uhid.h>
#include "btif_hh.h"
#include "bta_api.h"
#include "bta_hh_api.h"

#ifdef BLUETOOTH_RTK
#include "btif_dm.h"
#include <sys/ioctl.h>
#include "rtkbt_virtual_hid.h"
#include "rtkbt_ifly_voice.h"
#define DATA_LOCK() pthread_mutex_lock(&resend_data_cb.data_mutex);
#define DATA_UNLOCK() pthread_mutex_unlock(&resend_data_cb.data_mutex);
#define TOTAL_DATA_NUM (10)
typedef struct {
UINT32 num;
pthread_mutex_t data_mutex;
UINT8 * rsd_data[TOTAL_DATA_NUM];
} tRSD_DATA_CB;
static BOOLEAN mutex_init = FALSE;
static tRSD_DATA_CB resend_data_cb;
static UINT8  data_index = 0;
void RTKBT_resend_data(btif_hh_device_t *p_data_dev);
extern int rcu_uhid_fd;
#endif

const char *dev_path = "/dev/uhid";


/*Internal function to perform UHID write and error checking*/
static int uhid_write(int fd, const struct uhid_event *ev)
{
    ssize_t ret;
    ret = write(fd, ev, sizeof(*ev));
    if (ret < 0){
        int rtn = -errno;
        APPL_TRACE_ERROR2("%s: Cannot write to uhid:%s", __FUNCTION__, strerror(errno));
        return rtn;
    } else if (ret != sizeof(*ev)) {
        APPL_TRACE_ERROR3("%s: Wrong size written to uhid: %ld != %lu",
                                                    __FUNCTION__, ret, sizeof(*ev));
        return -EFAULT;
    } else {
        return 0;
    }
}
#ifdef BLUETOOTH_RTK
static int vhid_event(btif_hh_device_t *p_dev)
{
    struct uhid_event ev;
    BT_HDR  *p_buf;
    UINT8 *data ;
    ssize_t ret;
    memset(&ev, 0, sizeof(ev));
    if(!p_dev)
    {
        APPL_TRACE_ERROR1("%s: Device not found",__FUNCTION__)
        return -1;
    }
    ret = read(rcu_uhid_fd, &ev, sizeof(ev));
    if (ret == 0) {
        APPL_TRACE_ERROR2("%s: Read HUP on uhid-cdev %s", __FUNCTION__,
                                                 strerror(errno));
        return -EFAULT;
    } else if (ret < 0) {
        APPL_TRACE_ERROR2("%s:Cannot read uhid-cdev: %s", __FUNCTION__,
                                                strerror(errno));
        return -errno;
    } else if (ret != sizeof(ev)) {
        APPL_TRACE_ERROR3("%s:Invalid size read from uhid-dev: %ld != %lu",
                            __FUNCTION__, ret, sizeof(ev));
        return -EFAULT;
    }

    switch (ev.type) {
    case UHID_START:
        APPL_TRACE_DEBUG0("UHID_START from uhid-dev\n");
        break;
    case UHID_STOP:
        APPL_TRACE_DEBUG0("UHID_STOP from uhid-dev\n");
        break;
    case UHID_OPEN:
        APPL_TRACE_DEBUG0("UHID_OPEN from uhid-dev\n");

#ifdef BLUETOOTH_RTK
        p_dev->uhid_start = TRUE;
#endif
        break;
    case UHID_CLOSE:
        APPL_TRACE_DEBUG0("UHID_CLOSE from uhid-dev\n");

#ifdef BLUETOOTH_RTK
        p_dev->uhid_start = FALSE;
#endif

        break;
    case UHID_OUTPUT:
        APPL_TRACE_DEBUG2("vhid..uHID_OUTPUT: Report type = %d, report_size = %d"
                            ,ev.u.output.rtype, ev.u.output.size);
        //Send SET_REPORT with feature report if the report type in output event is FEATURE
        if(ev.u.output.rtype == UHID_FEATURE_REPORT) {
            btif_hh_setreport(p_dev,BTHH_FEATURE_REPORT,ev.u.output.size,ev.u.output.data);
        } else if(ev.u.output.rtype == UHID_OUTPUT_REPORT) {
           // btif_hh_setreport(p_dev,BTHH_OUTPUT_REPORT,ev.u.output.size,ev.u.output.data);
	   if(ev.u.output.size > 0){
		 if((p_buf = (BT_HDR *) GKI_getbuf((UINT16)(sizeof(BT_HDR) + ev.u.output.size ))) != NULL)
		    {
		        memset(p_buf, 0, sizeof(BT_HDR) + ev.u.output.size);
		        p_buf->len = ev.u.output.size;
			p_buf->layer_specific = BTA_HH_RPTT_OUTPUT;
			data = (UINT8 *)(p_buf + 1);
		        memcpy(data, ev.u.output.data, ev.u.output.size);
		        APPL_TRACE_DEBUG0("BTA_HhSendData to send output data!");
		        BTA_HhSendData(p_dev->dev_handle, NULL, p_buf);
		        GKI_freebuf(p_buf);
		    }
	    } else {
		APPL_TRACE_DEBUG0("not send output data!");
	    }
	} else {
            btif_hh_setreport(p_dev,BTHH_INPUT_REPORT,ev.u.output.size,ev.u.output.data);
        }
	break;
    case UHID_OUTPUT_EV:
        APPL_TRACE_DEBUG0("vHID_OUTPUT_EV from uhid-dev\n");
        break;
    case UHID_FEATURE:
        APPL_TRACE_DEBUG0("UHID_FEATURE from uhid-dev\n");
        break;
    case UHID_FEATURE_ANSWER:
        APPL_TRACE_DEBUG0("UHID_FEATURE_ANSWER from uhid-dev\n");
        break;

    default:
        APPL_TRACE_DEBUG1("Invalid event from uhid-dev: %u\n", ev.type);
    }

    return 0;
}
#endif

/* Internal function to parse the events received from UHID driver*/
static int uhid_event(btif_hh_device_t *p_dev)
{
    struct uhid_event ev;
    BT_HDR  *p_buf;
    UINT8 *data ;
    ssize_t ret;
    memset(&ev, 0, sizeof(ev));
    if(!p_dev)
    {
        APPL_TRACE_ERROR1("%s: Device not found",__FUNCTION__)
        return -1;
    }
    ret = read(p_dev->fd, &ev, sizeof(ev));
    if (ret == 0) {
        APPL_TRACE_ERROR2("%s: Read HUP on uhid-cdev %s", __FUNCTION__,
                                                 strerror(errno));
        return -EFAULT;
    } else if (ret < 0) {
        APPL_TRACE_ERROR2("%s:Cannot read uhid-cdev: %s", __FUNCTION__,
                                                strerror(errno));
        return -errno;
    } else if (ret != sizeof(ev)) {
        APPL_TRACE_ERROR3("%s:Invalid size read from uhid-dev: %ld != %lu",
                            __FUNCTION__, ret, sizeof(ev));
        return -EFAULT;
    }

    switch (ev.type) {
    case UHID_START:
        APPL_TRACE_DEBUG0("UHID_START from uhid-dev\n");
        break;
    case UHID_STOP:
        APPL_TRACE_DEBUG0("UHID_STOP from uhid-dev\n");
        break;
    case UHID_OPEN:
        APPL_TRACE_DEBUG0("UHID_OPEN from uhid-dev\n");

#ifdef BLUETOOTH_RTK
        p_dev->uhid_start = TRUE;
#endif
        break;
    case UHID_CLOSE:
        APPL_TRACE_DEBUG0("UHID_CLOSE from uhid-dev\n");

#ifdef BLUETOOTH_RTK
        p_dev->uhid_start = FALSE;
#endif

        break;
    case UHID_OUTPUT:
        APPL_TRACE_DEBUG2("UHID_OUTPUT: Report type = %d, report_size = %d"
                            ,ev.u.output.rtype, ev.u.output.size);
        //Send SET_REPORT with feature report if the report type in output event is FEATURE
        if(ev.u.output.rtype == UHID_FEATURE_REPORT) {
            btif_hh_setreport(p_dev,BTHH_FEATURE_REPORT,ev.u.output.size,ev.u.output.data);
        } else if(ev.u.output.rtype == UHID_OUTPUT_REPORT) {
           // btif_hh_setreport(p_dev,BTHH_OUTPUT_REPORT,ev.u.output.size,ev.u.output.data);
	   if(ev.u.output.size > 0){
		 if((p_buf = (BT_HDR *) GKI_getbuf((UINT16)(sizeof(BT_HDR) + ev.u.output.size ))) != NULL)
		    {
		        memset(p_buf, 0, sizeof(BT_HDR) + ev.u.output.size);
		        p_buf->len = ev.u.output.size;
			p_buf->layer_specific = BTA_HH_RPTT_OUTPUT;
			data = (UINT8 *)(p_buf + 1);
		        memcpy(data, ev.u.output.data, ev.u.output.size);
		        APPL_TRACE_DEBUG0("BTA_HhSendData to send output data!");
		        BTA_HhSendData(p_dev->dev_handle, NULL, p_buf);
		        GKI_freebuf(p_buf);
		    }
	    } else {
		APPL_TRACE_DEBUG0("not send output data!");
	    }
	} else {
            btif_hh_setreport(p_dev,BTHH_INPUT_REPORT,ev.u.output.size,ev.u.output.data);
        }
	break;
    case UHID_OUTPUT_EV:
        APPL_TRACE_DEBUG0("UHID_OUTPUT_EV from uhid-dev\n");
        break;
    case UHID_FEATURE:
        APPL_TRACE_DEBUG0("UHID_FEATURE from uhid-dev\n");
        break;
    case UHID_FEATURE_ANSWER:
        APPL_TRACE_DEBUG0("UHID_FEATURE_ANSWER from uhid-dev\n");
        break;

    default:
        APPL_TRACE_DEBUG1("Invalid event from uhid-dev: %u\n", ev.type);
    }

    return 0;
}

/*******************************************************************************
**
** Function create_thread
**
** Description creat a select loop
**
** Returns pthread_t
**
*******************************************************************************/
static inline pthread_t create_thread(void *(*start_routine)(void *), void * arg){
    APPL_TRACE_DEBUG0("create_thread: entered");
    pthread_attr_t thread_attr;

    pthread_attr_init(&thread_attr);
    pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE);
    pthread_t thread_id = -1;
    if ( pthread_create(&thread_id, &thread_attr, start_routine, arg)!=0 )
    {
        APPL_TRACE_ERROR1("pthread_create : %s", strerror(errno));
        return -1;
    }
    APPL_TRACE_DEBUG0("create_thread: thread created successfully");
    return thread_id;
}

/*******************************************************************************
**
** Function btif_hh_poll_event_thread
**
** Description the polling thread which polls for event from UHID driver
**
** Returns void
**
*******************************************************************************/
static void *btif_hh_poll_event_thread(void *arg)
{

    btif_hh_device_t *p_dev = arg;
    APPL_TRACE_DEBUG2("%s: Thread created fd = %d", __FUNCTION__, p_dev->fd);
#ifdef BLUETOOTH_RTK
    struct pollfd pfds[2];
    int ret;
    if(rcu_uhid_fd > 0)
    {
       APPL_TRACE_DEBUG1("vhid...btif_hh_poll_event_thread: POLLIN fd = %d",rcu_uhid_fd);
       pfds[0].fd = rcu_uhid_fd;
       pfds[0].events = POLLIN;
    }
    pfds[1].fd = p_dev->fd;
    pfds[1].events = POLLIN;
#endif
    while(p_dev->hh_keep_polling){
        ret = poll(pfds, 2, 500);
        if (ret < 0) {
            APPL_TRACE_ERROR2("%s: Cannot poll for fds: %s\n", __FUNCTION__, strerror(errno));
            break;
        }
        if ((pfds[0].revents & POLLIN)) {
            APPL_TRACE_DEBUG0("vhid...btif_hh_poll_event_thread: POLLIN");
            ret = vhid_event(p_dev);
            if (ret){
                break;
            }
        }
#ifdef BLUETOOTH_RTK
	if ((pfds[1].revents & POLLIN)) {
            APPL_TRACE_DEBUG0("btif_hh_poll_event_thread: POLLIN");
            ret = uhid_event(p_dev);
            if (ret){
                break;
            }
        }
#endif
    }

    p_dev->hh_poll_thread_id = -1;
    return 0;
}

static inline void btif_hh_close_poll_thread(btif_hh_device_t *p_dev)
{
    APPL_TRACE_DEBUG1("%s", __FUNCTION__);
    p_dev->hh_keep_polling = 0;
    if(p_dev->hh_poll_thread_id > 0)
#ifdef BLUETOOTH_RTK
        pthread_detach(p_dev->hh_poll_thread_id);
        return;
#endif
        pthread_join(p_dev->hh_poll_thread_id,NULL);

    return;
}

void bta_hh_co_destroy(int fd)
{
    struct uhid_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = UHID_DESTROY;
    uhid_write(fd, &ev);
    close(fd);
}

int bta_hh_co_write(int fd, UINT8* rpt, UINT16 len)
{
    APPL_TRACE_DEBUG0("bta_hh_co_data: UHID write");
    struct uhid_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = UHID_INPUT;
    ev.u.input.size = len;
    if(len > sizeof(ev.u.input.data)){
        APPL_TRACE_WARNING1("%s:report size greater than allowed size",__FUNCTION__);
        return -1;
    }
    memcpy(ev.u.input.data, rpt, len);
    return uhid_write(fd, &ev);

}


/*******************************************************************************
**
** Function      bta_hh_co_open
**
** Description   When connection is opened, this call-out function is executed
**               by HH to do platform specific initialization.
**
** Returns       void.
*******************************************************************************/
void bta_hh_co_open(UINT8 dev_handle, UINT8 sub_class, tBTA_HH_ATTR_MASK attr_mask,
                    UINT8 app_id)
{
    UINT32 i;
    btif_hh_device_t *p_dev = NULL;

    if (dev_handle == BTA_HH_INVALID_HANDLE) {
        APPL_TRACE_WARNING2("%s: Oops, dev_handle (%d) is invalid...", __FUNCTION__, dev_handle);
        return;
    }

    for (i = 0; i < BTIF_HH_MAX_HID; i++) {
        p_dev = &btif_hh_cb.devices[i];
        if (p_dev->dev_status != BTHH_CONN_STATE_UNKNOWN && p_dev->dev_handle == dev_handle) {
            // We found a device with the same handle. Must be a device reconnected.
            APPL_TRACE_WARNING2("%s: Found an existing device with the same handle "
                                                                "dev_status = %d",__FUNCTION__,
                                                                p_dev->dev_status);
            APPL_TRACE_WARNING6("%s:     bd_addr = [%02X:%02X:%02X:%02X:%02X:]", __FUNCTION__,
                 p_dev->bd_addr.address[0], p_dev->bd_addr.address[1], p_dev->bd_addr.address[2],
                 p_dev->bd_addr.address[3], p_dev->bd_addr.address[4]);
                 APPL_TRACE_WARNING4("%s:     attr_mask = 0x%04x, sub_class = 0x%02x, app_id = %d",
                                  __FUNCTION__, p_dev->attr_mask, p_dev->sub_class, p_dev->app_id);

            if(p_dev->fd<0) {
                p_dev->fd = open(dev_path, O_RDWR | O_CLOEXEC);
                if (p_dev->fd < 0){
                    APPL_TRACE_ERROR2("%s: Error: failed to open uhid, err:%s",
                                                                    __FUNCTION__,strerror(errno));
                }else
                    APPL_TRACE_DEBUG2("%s: uhid fd = %d", __FUNCTION__, p_dev->fd);
            }
            p_dev->hh_keep_polling = 1;
            p_dev->hh_poll_thread_id = create_thread(btif_hh_poll_event_thread, p_dev);
#ifdef BLUETOOTH_RTK_VR
            RTKBT_Iflytek_NotifyRcuStatus(1, &(p_dev->bd_addr));
#endif
            break;
        }
        p_dev = NULL;
    }

    if (p_dev == NULL) {
        // Did not find a device reconnection case. Find an empty slot now.
        for (i = 0; i < BTIF_HH_MAX_HID; i++) {
            if (btif_hh_cb.devices[i].dev_status == BTHH_CONN_STATE_UNKNOWN) {
                p_dev = &btif_hh_cb.devices[i];
                p_dev->dev_handle = dev_handle;
                p_dev->attr_mask  = attr_mask;
                p_dev->sub_class  = sub_class;
                p_dev->app_id     = app_id;
                p_dev->local_vup  = FALSE;
#ifdef BLUETOOTH_RTK
                p_dev->uhid_start = FALSE;
#endif

                btif_hh_cb.device_num++;
                // This is a new device,open the uhid driver now.
                p_dev->fd = open(dev_path, O_RDWR | O_CLOEXEC);
                if (p_dev->fd < 0){
                    APPL_TRACE_ERROR2("%s: Error: failed to open uhid, err:%s",
                                                                    __FUNCTION__,strerror(errno));
                }else{
                    APPL_TRACE_DEBUG2("%s: uhid fd = %d", __FUNCTION__, p_dev->fd);
                    p_dev->hh_keep_polling = 1;
                    p_dev->hh_poll_thread_id = create_thread(btif_hh_poll_event_thread, p_dev);
#ifdef BLUETOOTH_RTK_VR
                    RTKBT_Iflytek_NotifyRcuStatus(1, &(p_dev->bd_addr));
#endif
                }


                break;
            }
        }
    }

    if (p_dev == NULL) {
        APPL_TRACE_ERROR1("%s: Error: too many HID devices are connected", __FUNCTION__);
        return;
    }

    p_dev->dev_status = BTHH_CONN_STATE_CONNECTED;
    APPL_TRACE_DEBUG2("%s: Return device status %d", __FUNCTION__, p_dev->dev_status);
}


/*******************************************************************************
**
** Function      bta_hh_co_close
**
** Description   When connection is closed, this call-out function is executed
**               by HH to do platform specific finalization.
**
** Parameters    dev_handle  - device handle
**                  app_id      - application id
**
** Returns          void.
*******************************************************************************/
void bta_hh_co_close(UINT8 dev_handle, UINT8 app_id)
{
    UINT32 i;
    btif_hh_device_t *p_dev = NULL;

    APPL_TRACE_WARNING3("%s: dev_handle = %d, app_id = %d", __FUNCTION__, dev_handle, app_id);
    if (dev_handle == BTA_HH_INVALID_HANDLE) {
        APPL_TRACE_WARNING2("%s: Oops, dev_handle (%d) is invalid...", __FUNCTION__, dev_handle);
        return;
    }

    for (i = 0; i < BTIF_HH_MAX_HID; i++) {
        p_dev = &btif_hh_cb.devices[i];
        if (p_dev->dev_status != BTHH_CONN_STATE_UNKNOWN && p_dev->dev_handle == dev_handle) {
            APPL_TRACE_WARNING3("%s: Found an existing device with the same handle "
                                                        "dev_status = %d, dev_handle =%d"
                                                        ,__FUNCTION__,p_dev->dev_status
                                                        ,p_dev->dev_handle);
            btif_hh_close_poll_thread(p_dev);
#ifdef BLUETOOTH_RTK_VR
            RTKBT_Iflytek_NotifyRcuStatus(0, &(p_dev->bd_addr));
            p_dev->uhid_start = FALSE;
#endif
            break;
        }
     }

}


/*******************************************************************************
**
** Function         bta_hh_co_data
**
** Description      This function is executed by BTA when HID host receive a data
**                  report.
**
** Parameters       dev_handle  - device handle
**                  *p_rpt      - pointer to the report data
**                  len         - length of report data
**                  mode        - Hid host Protocol Mode
**                  sub_clas    - Device Subclass
**                  app_id      - application id
**
** Returns          void
*******************************************************************************/
void bta_hh_co_data(UINT8 dev_handle, UINT8 *p_rpt, UINT16 len, tBTA_HH_PROTO_MODE mode,
                    UINT8 sub_class, UINT8 ctry_code, BD_ADDR peer_addr, UINT8 app_id)
{
    btif_hh_device_t *p_dev;
#ifdef BLUETOOTH_RTK
    UINT8           *p_buf;
    int j = 0;
#endif

    APPL_TRACE_DEBUG6("%s: dev_handle = %d, subclass = 0x%02X, mode = %d, "
         "ctry_code = %d, app_id = %d",
         __FUNCTION__, dev_handle, sub_class, mode, ctry_code, app_id);

    p_dev = btif_hh_find_connected_dev_by_handle(dev_handle);
    if (p_dev == NULL) {
        APPL_TRACE_WARNING2("%s: Error: unknown HID device handle %d", __FUNCTION__, dev_handle);
#ifdef BLUETOOTH_RTK
        goto resend_data;
#else
        return;
#endif
    }
    // Send the HID report to the kernel.
    if (p_dev->fd >= 0) {
        bta_hh_co_write(p_dev->fd, p_rpt, len);
    }else {
        APPL_TRACE_WARNING3("%s: Error: fd = %d, len = %d", __FUNCTION__, p_dev->fd, len);
    }

#ifdef BLUETOOTH_RTK
            if(p_dev->uhid_start != FALSE)
            {
                APPL_TRACE_WARNING2("%s: p_dev->uhid_start = %d", __FUNCTION__, p_dev->uhid_start);
                return;
            }
resend_data:
    APPL_TRACE_ERROR0("before mutex_init");
    if(mutex_init == FALSE)
    {
        pthread_mutexattr_t attr = PTHREAD_MUTEX_NORMAL;
        pthread_mutex_init(&resend_data_cb.data_mutex, &attr);
        mutex_init = TRUE;
    }
    APPL_TRACE_ERROR1("after mutex_init = %d", mutex_init);
    DATA_LOCK();
    if ((p_buf = (UINT8 *)GKI_getbuf((UINT16)(len + 3))) == NULL)
    {
        APPL_TRACE_ERROR0("No resources to send report data");
        DATA_UNLOCK();
        return;
    }
    p_buf[0] = dev_handle;
    memcpy(&p_buf[3], p_rpt, len);
    memcpy(&p_buf[1], &len , 2);

    if(resend_data_cb.num == TOTAL_DATA_NUM){
        GKI_freebuf(resend_data_cb.rsd_data[data_index]);
        resend_data_cb.rsd_data[data_index] = p_buf;
        data_index = (data_index + 1) % TOTAL_DATA_NUM;
    }
    else{
        for(j = 0; j < TOTAL_DATA_NUM; j++)
        {
            if(resend_data_cb.rsd_data[(data_index + j)%TOTAL_DATA_NUM] == NULL)
            {
                resend_data_cb.rsd_data[(data_index + j)%TOTAL_DATA_NUM] = p_buf;
                APPL_TRACE_WARNING3("%s position is %d, data len is %d", __FUNCTION__,data_index + j,len);
                break;
            }
        }
        resend_data_cb.num++;
    }
    DATA_UNLOCK();

    APPL_TRACE_WARNING3("%s resend_data end, num = %d,data_index = %d", __FUNCTION__,resend_data_cb.num,data_index);

#endif
}


/*******************************************************************************
**
** Function         bta_hh_co_send_hid_info
**
** Description      This function is called in btif_hh.c to process DSCP received.
**
** Parameters       dev_handle  - device handle
**                  dscp_len    - report descriptor length
**                  *p_dscp     - report descriptor
**
** Returns          void
*******************************************************************************/
void bta_hh_co_send_hid_info(btif_hh_device_t *p_dev, char *dev_name, UINT16 vendor_id,
                             UINT16 product_id, UINT16 version, UINT8 ctry_code,
                             int dscp_len, UINT8 *p_dscp)
{
    int result;
    struct uhid_event ev;

    if (p_dev->fd < 0) {
        APPL_TRACE_WARNING3("%s: Error: fd = %d, dscp_len = %d", __FUNCTION__, p_dev->fd, dscp_len);
        return;
    }

    APPL_TRACE_WARNING4("%s: fd = %d, name = [%s], dscp_len = %d", __FUNCTION__,
                                                                    p_dev->fd, dev_name, dscp_len);
    APPL_TRACE_WARNING5("%s: vendor_id = 0x%04x, product_id = 0x%04x, version= 0x%04x,"
                                                                    "ctry_code=0x%02x",__FUNCTION__,
                                                                    vendor_id, product_id,
                                                                    version, ctry_code);

//Create and send hid descriptor to kernel
    memset(&ev, 0, sizeof(ev));
    ev.type = UHID_CREATE;
    strncpy((char*)ev.u.create.name, dev_name, sizeof(ev.u.create.name) - 1);
    ev.u.create.rd_size = dscp_len;
    ev.u.create.rd_data = p_dscp;
    ev.u.create.bus = BUS_BLUETOOTH;
    ev.u.create.vendor = vendor_id;
    ev.u.create.product = product_id;
    ev.u.create.version = version;
    ev.u.create.country = ctry_code;
    result = uhid_write(p_dev->fd, &ev);

    APPL_TRACE_WARNING4("%s: fd = %d, dscp_len = %d, result = %d", __FUNCTION__,
                                                                    p_dev->fd, dscp_len, result);

    if (result) {
        APPL_TRACE_WARNING2("%s: Error: failed to send DSCP, result = %d", __FUNCTION__, result);

        /* The HID report descriptor is corrupted. Close the driver. */
        close(p_dev->fd);
        p_dev->fd = -1;
    }
#ifdef BLUETOOTH_RTK
    else{
        int i = 0;
        while((p_dev->uhid_start != TRUE) && (i < 100)){
            i++;
            usleep(1000);
        }
        if(p_dev->uhid_start != TRUE)
            return;
        RTKBT_resend_data(p_dev);
    }
#endif
}

#ifdef BLUETOOTH_RTK
void RTKBT_resend_data(btif_hh_device_t *p_data_dev)
{
    int num = 0;
    int i = 0;
    int j = 0;
    int k = 0;
    UINT8 dev_handle;
    UINT16 len;
    btif_hh_device_t *p_dev;
    APPL_TRACE_WARNING3("%s: start num = %d,mutex_init = %d", __FUNCTION__,num,mutex_init);
    if(mutex_init == FALSE)
    {
        pthread_mutexattr_t attr = PTHREAD_MUTEX_NORMAL;
        pthread_mutex_init(&resend_data_cb.data_mutex, &attr);
        mutex_init = TRUE;
    }
    DATA_LOCK();
    num = resend_data_cb.num;
    APPL_TRACE_WARNING3("%s: start num = %d,data_index = %d", __FUNCTION__,num,data_index);
    k = data_index;
    if(resend_data_cb.num == 0){
        DATA_UNLOCK();
        return;
    }
    usleep(150000);
    for(i = 0; i < TOTAL_DATA_NUM; i++){
        j = (k + i) % TOTAL_DATA_NUM;
        if(resend_data_cb.rsd_data[j] != NULL) {
            dev_handle = resend_data_cb.rsd_data[j][0];
            memcpy(&len, &resend_data_cb.rsd_data[j][1], 2);
            p_dev = btif_hh_find_connected_dev_by_handle(dev_handle);
            APPL_TRACE_WARNING2("%s: resend data, dev_handle = %d", __FUNCTION__,dev_handle);
            if(p_data_dev->dev_handle == dev_handle) {
                APPL_TRACE_WARNING1("%s: begin to resend data", __FUNCTION__);
                if (p_dev->fd >= 0) {
                    bta_hh_co_write(p_dev->fd, &resend_data_cb.rsd_data[j][3], len);
                    usleep(10000);
                }
                GKI_freebuf(resend_data_cb.rsd_data[j]);
                resend_data_cb.rsd_data[j] = NULL;
                resend_data_cb.num--;
            }
            else if(data_index == k)
                data_index = j;
        }
    }
    APPL_TRACE_WARNING3("%s: end num = %d,data_index = %d", __FUNCTION__,resend_data_cb.num,data_index);
    DATA_UNLOCK();
}

void RTKBT_clear_db()
{
    int i = 0;
    int j = 0;
    UINT8 dev_handle;
    btif_hh_device_t *p_dev;
    if(mutex_init == FALSE)
    {
        pthread_mutexattr_t attr = PTHREAD_MUTEX_NORMAL;
        pthread_mutex_init(&resend_data_cb.data_mutex, &attr);
        mutex_init = TRUE;
    }

    DATA_LOCK();
    if(resend_data_cb.num == 0){
        DATA_UNLOCK();
        return;
    }

    int k = data_index;
    for(i = 0; i < TOTAL_DATA_NUM; i++){
        j = (k + i) % TOTAL_DATA_NUM;
        if(resend_data_cb.rsd_data[j] != NULL) {
            dev_handle = resend_data_cb.rsd_data[j][0];
            if((p_dev = btif_hh_find_connected_dev_by_handle(dev_handle)) == NULL)
            {
                GKI_freebuf(resend_data_cb.rsd_data[j]);
                resend_data_cb.rsd_data[j] = NULL;
                resend_data_cb.num--;
            }
            else
            {
                if((p_dev->dev_status != BTHH_CONN_STATE_CONNECTED) || (p_dev->dev_status != BTHH_CONN_STATE_CONNECTING))
                {
                    GKI_freebuf(resend_data_cb.rsd_data[j]);
                    resend_data_cb.rsd_data[j] = NULL;
                    resend_data_cb.num--;
                }
                else if(resend_data_cb.rsd_data[data_index] == NULL)
                    data_index = j;
            }
        }
    }
    if(resend_data_cb.num == 0)
    {
        data_index = 0;
    }
    DATA_UNLOCK();
    APPL_TRACE_WARNING3("%s: end num = %d,data_index = %d", __FUNCTION__,resend_data_cb.num,data_index);
}
#endif
