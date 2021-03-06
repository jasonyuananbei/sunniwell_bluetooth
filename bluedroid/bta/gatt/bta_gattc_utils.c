/******************************************************************************
 *
 *  Copyright (C) 2003-2012 Broadcom Corporation
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

/******************************************************************************
 *
 *  This file contains the GATT client utility function.
 *
 ******************************************************************************/

#include "bt_target.h"

#if defined(BTA_GATT_INCLUDED) && (BTA_GATT_INCLUDED == TRUE)

#include <string.h>
#include "utl.h"
#include "gki.h"
#include "bta_sys.h"
#include "bta_gattc_int.h"
#include "l2c_api.h"
#include "bd.h"

/*****************************************************************************
**  Constants
*****************************************************************************/


static const UINT8  base_uuid[LEN_UUID_128] = {0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const BD_ADDR dummy_bda = {0,0,0,0,0,0};

/*******************************************************************************
**
** Function         bta_gatt_convert_uuid16_to_uuid128
**
** Description      Convert a 16 bits UUID to be an standard 128 bits one.
**
** Returns          TRUE if two uuid match; FALSE otherwise.
**
*******************************************************************************/
void bta_gatt_convert_uuid16_to_uuid128(UINT8 uuid_128[LEN_UUID_128], UINT16 uuid_16)
{
    UINT8   *p = &uuid_128[LEN_UUID_128 - 4];

    memcpy (uuid_128, base_uuid, LEN_UUID_128);

    UINT16_TO_STREAM(p, uuid_16);
}
/*******************************************************************************
**
** Function         bta_gattc_uuid_compare
**
** Description      Compare two UUID to see if they are the same.
**
** Returns          TRUE if two uuid match; FALSE otherwise.
**
*******************************************************************************/
BOOLEAN bta_gattc_uuid_compare (tBT_UUID *p_src, tBT_UUID *p_tar, BOOLEAN is_precise)
{
    UINT8  su[LEN_UUID_128], tu[LEN_UUID_128];
    UINT8  *ps, *pt;

    /* any of the UUID is unspecified */
    if (p_src == 0 || p_tar == 0)
    {
        if (is_precise)
            return FALSE;
        else
            return TRUE;
    }

    /* If both are 16-bit, we can do a simple compare */
    if (p_src->len == 2 && p_tar->len == 2)
    {
        return p_src->uu.uuid16 == p_tar->uu.uuid16;
    }

    /* One or both of the UUIDs is 128-bit */
    if (p_src->len == LEN_UUID_16)
    {
        /* convert a 16 bits UUID to 128 bits value */
        bta_gatt_convert_uuid16_to_uuid128(su, p_src->uu.uuid16);
        ps = su;
    }
    else
        ps = p_src->uu.uuid128;

    if (p_tar->len == LEN_UUID_16)
    {
        /* convert a 16 bits UUID to 128 bits value */
        bta_gatt_convert_uuid16_to_uuid128(tu, p_tar->uu.uuid16);
        pt = tu;
    }
    else
        pt = p_tar->uu.uuid128;

    return(memcmp(ps, pt, LEN_UUID_128) == 0);
}

/*******************************************************************************
**
** Function         bta_gattc_cl_get_regcb
**
** Description      get registration control block by client interface.
**
** Returns          pointer to the regcb
**
*******************************************************************************/
tBTA_GATTC_RCB * bta_gattc_cl_get_regcb(UINT8 client_if)
{
    UINT8   i = 0;
    tBTA_GATTC_RCB  *p_clrcb = &bta_gattc_cb.cl_rcb[0];

    for (i = 0; i < BTA_GATTC_CL_MAX; i ++, p_clrcb ++)
    {
        if (p_clrcb->in_use &&
            p_clrcb->client_if == client_if)
            return p_clrcb;
    }
    return NULL;
}
/*******************************************************************************
**
** Function         bta_gattc_num_reg_app
**
** Description      find the number of registered application.
**
** Returns          pointer to the regcb
**
*******************************************************************************/
UINT8 bta_gattc_num_reg_app(void)
{
    UINT8   i = 0, j = 0;

    for (i = 0; i < BTA_GATTC_CL_MAX; i ++)
    {
        if (bta_gattc_cb.cl_rcb[i].in_use)
            j ++;
    }
    return j;
}
/*******************************************************************************
**
** Function         bta_gattc_find_clcb_by_cif
**
** Description      get clcb by client interface and remote bd adddress
**
** Returns          pointer to the clcb
**
*******************************************************************************/
tBTA_GATTC_CLCB * bta_gattc_find_clcb_by_cif (UINT8 client_if, BD_ADDR remote_bda)
{
    tBTA_GATTC_CLCB *p_clcb = &bta_gattc_cb.clcb[0];
    UINT8   i;

    for (i = 0; i < BTA_GATTC_CLCB_MAX; i ++, p_clcb ++)
    {
        if (p_clcb->in_use &&
            p_clcb->p_rcb->client_if == client_if &&
            bdcmp(p_clcb->bda, remote_bda) == 0)
            return p_clcb;
    }
    return NULL;
}
/*******************************************************************************
**
** Function         bta_gattc_find_clcb_by_conn_id
**
** Description      get clcb by connection ID
**
** Returns          pointer to the clcb
**
*******************************************************************************/
tBTA_GATTC_CLCB * bta_gattc_find_clcb_by_conn_id (UINT16 conn_id)
{
    tBTA_GATTC_CLCB *p_clcb = &bta_gattc_cb.clcb[0];
    UINT8 i;

    for (i = 0; i < BTA_GATTC_CLCB_MAX; i ++, p_clcb ++)
    {
        if (p_clcb->in_use &&
            p_clcb->bta_conn_id == conn_id)
            return p_clcb;
    }
    return NULL;
}

/*******************************************************************************
**
** Function         bta_gattc_clcb_alloc
**
** Description      allocate CLCB
**
** Returns          pointer to the clcb
**
*******************************************************************************/
tBTA_GATTC_CLCB * bta_gattc_clcb_alloc(tBTA_GATTC_IF client_if, BD_ADDR remote_bda)
{
    UINT8               i_clcb = 0;
    tBTA_GATTC_CLCB     *p_clcb = NULL;

    for (i_clcb = 0; i_clcb < BTA_GATTC_CLCB_MAX; i_clcb++)
    {
        if (!bta_gattc_cb.clcb[i_clcb].in_use)
        {
#if BTA_GATT_DEBUG == TRUE
            APPL_TRACE_DEBUG1("bta_gattc_clcb_alloc: found clcb[%d] available",i_clcb);
#endif
            p_clcb                  = &bta_gattc_cb.clcb[i_clcb];
            p_clcb->in_use          = TRUE;
            p_clcb->status          = BTA_GATT_OK;
            bdcpy(p_clcb->bda, remote_bda);

            p_clcb->p_rcb = bta_gattc_cl_get_regcb(client_if);

            if ((p_clcb->p_srcb = bta_gattc_find_srcb(remote_bda)) == NULL)
                p_clcb->p_srcb      = bta_gattc_srcb_alloc(remote_bda);

            if (p_clcb->p_rcb != NULL && p_clcb->p_srcb != NULL)
            {
                p_clcb->p_srcb->num_clcb ++;
                p_clcb->p_rcb->num_clcb ++;
            }
            else
            {
                /* release this clcb if clcb or srcb allocation failed */
                p_clcb->in_use = FALSE;
                p_clcb = NULL;
            }
            break;
        }
    }
    return p_clcb;
}
/*******************************************************************************
**
** Function         bta_gattc_find_alloc_clcb
**
** Description      find or allocate CLCB if not found.
**
** Returns          pointer to the clcb
**
*******************************************************************************/
tBTA_GATTC_CLCB *bta_gattc_find_alloc_clcb(tBTA_GATTC_IF client_if, BD_ADDR remote_bda)
{
    tBTA_GATTC_CLCB *p_clcb ;

    if ((p_clcb = bta_gattc_find_clcb_by_cif(client_if, remote_bda)) == NULL)
    {
        p_clcb = bta_gattc_clcb_alloc(client_if, remote_bda);
    }
    return p_clcb;
}

/*******************************************************************************
**
** Function         bta_gattc_clcb_dealloc
**
** Description      Deallocte a clcb
**
** Returns          pointer to the clcb
**
*******************************************************************************/
void bta_gattc_clcb_dealloc(tBTA_GATTC_CLCB *p_clcb)
{
    tBTA_GATTC_SERV     *p_srcb = NULL;

    if (p_clcb)
    {
        p_srcb = p_clcb->p_srcb;
        if (p_srcb->num_clcb)
            p_srcb->num_clcb --;

        if (p_clcb->p_rcb->num_clcb)
            p_clcb->p_rcb->num_clcb --;

        /* if the srcb is no longer needed, reset the state */
        if ( p_srcb->num_clcb == 0)
        {
            p_srcb->connected = FALSE;
            p_srcb->state = BTA_GATTC_SERV_IDLE;
        }

        utl_freebuf((void **)&p_clcb->p_q_cmd);

        memset(p_clcb, 0, sizeof(tBTA_GATTC_CLCB));
    }
    else
    {
        APPL_TRACE_ERROR0("bta_gattc_clcb_dealloc p_clcb=NULL");
    }
}

/*******************************************************************************
**
** Function         bta_gattc_find_srcb
**
** Description      find server cache by remote bd address currently in use
**
** Returns          pointer to the server cache.
**
*******************************************************************************/
tBTA_GATTC_SERV * bta_gattc_find_srcb(BD_ADDR bda)
{
    tBTA_GATTC_SERV *p_srcb = &bta_gattc_cb.known_server[0];
    UINT8   i;

    for (i = 0; i < BTA_GATTC_KNOWN_SR_MAX; i ++, p_srcb ++)
    {
        if (p_srcb->in_use && bdcmp(p_srcb->server_bda, bda) == 0)
            return p_srcb;
    }
    return NULL;
}

/*******************************************************************************
**
** Function         bta_gattc_find_srvr_cache
**
** Description      find server cache by remote bd address
**
** Returns          pointer to the server cache.
**
*******************************************************************************/
tBTA_GATTC_SERV * bta_gattc_find_srvr_cache(BD_ADDR bda)
{
    tBTA_GATTC_SERV *p_srcb = &bta_gattc_cb.known_server[0];
    UINT8   i;

    for (i = 0; i < BTA_GATTC_KNOWN_SR_MAX; i ++, p_srcb ++)
    {
        if (bdcmp(p_srcb->server_bda, bda) == 0)
            return p_srcb;
    }
    return NULL;
}
/*******************************************************************************
**
** Function         bta_gattc_find_scb_by_cid
**
** Description      find server control block by connection ID
**
** Returns          pointer to the server cache.
**
*******************************************************************************/
tBTA_GATTC_SERV * bta_gattc_find_scb_by_cid (UINT16 conn_id)
{
    tBTA_GATTC_CLCB *p_clcb = bta_gattc_find_clcb_by_conn_id(conn_id);

    if (p_clcb)
        return p_clcb->p_srcb;
    else
        return NULL;
}
/*******************************************************************************
**
** Function         bta_gattc_srcb_alloc
**
** Description      allocate server cache control block
**
** Returns          pointer to the server cache.
**
*******************************************************************************/
tBTA_GATTC_SERV * bta_gattc_srcb_alloc(BD_ADDR bda)
{
    tBTA_GATTC_SERV *p_tcb = &bta_gattc_cb.known_server[0],
                             *p_recycle = NULL;
    BOOLEAN         found = FALSE;
    UINT8           i;

    for (i = 0; i < BTA_GATTC_KNOWN_SR_MAX; i ++, p_tcb ++)
    {
        if (!p_tcb->in_use)
        {
            found = TRUE;
            break;
        }
        else if (!p_tcb->connected)
        {
            p_recycle = p_tcb;
        }
    }

    /* if not found, try to recycle one known device */
    if (!found && !p_recycle)
        p_tcb = NULL;
    else if (!found && p_recycle)
        p_tcb = p_recycle;

    if (p_tcb != NULL)
    {
        while (p_tcb->cache_buffer.p_first)
            GKI_freebuf (GKI_dequeue (&p_tcb->cache_buffer));

        utl_freebuf((void **)&p_tcb->p_srvc_list);
        memset(p_tcb, 0 , sizeof(tBTA_GATTC_SERV));

        p_tcb->in_use = TRUE;
        bdcpy(p_tcb->server_bda, bda);
    }
    return p_tcb;
}
/*******************************************************************************
**
** Function         bta_gattc_enqueue
**
** Description      enqueue a client request in clcb.
**
** Returns          success or failure.
**
*******************************************************************************/
BOOLEAN bta_gattc_enqueue(tBTA_GATTC_CLCB *p_clcb, tBTA_GATTC_DATA *p_data)
{
    BOOLEAN in_q = FALSE;

    if (p_clcb->p_q_cmd == NULL && p_data)
    {
        UINT16 len;
        switch (p_data->hdr.event)
        {
            case BTA_GATTC_API_SEARCH_EVT:
            {
                if (p_data->api_search.p_srvc_uuid)
                {
                    len = sizeof(tBTA_GATTC_API_SEARCH) + sizeof(tBT_UUID);
                }
                else
                {
                    len = sizeof(tBTA_GATTC_API_SEARCH);
                }
                p_clcb->p_q_cmd = (tBTA_GATTC_DATA *)GKI_getbuf(len);
                if (p_clcb->p_q_cmd == NULL)
                {
                    APPL_TRACE_ERROR0("allocate buffer failed for p_q_cmd");
                    return FALSE;
                }
                memcpy(p_clcb->p_q_cmd, p_data, sizeof(tBTA_GATTC_API_SEARCH));
                if (p_data->api_search.p_srvc_uuid)
                {
                    tBTA_GATTC_API_SEARCH *p_buf;
                    p_buf = &(p_clcb->p_q_cmd->api_search);
                    p_buf->p_srvc_uuid = (tBT_UUID *)(p_buf + 1);
                    memcpy(p_buf->p_srvc_uuid, p_data->api_search.p_srvc_uuid,
                           sizeof(tBT_UUID));
                }
                break;
            }
            case BTA_GATTC_API_READ_EVT:
            {
                if (p_data->api_read.p_descr_type)
                {
                    len = sizeof(tBTA_GATT_ID) + sizeof(tBTA_GATTC_API_READ);
                }
                else
                {
                    len = sizeof(tBTA_GATTC_API_READ);
                }
                p_clcb->p_q_cmd = (tBTA_GATTC_DATA *)GKI_getbuf(len);
                if (p_clcb->p_q_cmd == NULL)
                {
                    APPL_TRACE_ERROR0("allocate buffer failed for p_q_cmd");
                    return FALSE;
                }
                memcpy(p_clcb->p_q_cmd, p_data, sizeof(tBTA_GATTC_API_READ));
                if (p_data->api_read.p_descr_type)
                {
                    tBTA_GATTC_API_READ *p_buf;
                    p_buf = &(p_clcb->p_q_cmd->api_read);
                    p_buf->p_descr_type = (tBTA_GATT_ID *)(p_buf + 1);
                    memcpy(p_buf->p_descr_type, p_data->api_read.p_descr_type,
                           sizeof(tBTA_GATT_ID));
                }
                break;
            }
            case BTA_GATTC_API_WRITE_EVT:
            {
                tBTA_GATTC_API_WRITE  *p_buf;
                len = sizeof(tBTA_GATTC_API_WRITE) + p_data->api_write.len;
                if (p_data->api_write.p_descr_type)
                {
                    len += sizeof(tBTA_GATT_ID);
                }
                p_clcb->p_q_cmd = (tBTA_GATTC_DATA *)GKI_getbuf(len);
                if (p_clcb->p_q_cmd == NULL)
                {
                    APPL_TRACE_ERROR0("allocate buffer failed for p_q_cmd");
                    return FALSE;
                }
                memcpy(p_clcb->p_q_cmd, p_data, sizeof(tBTA_GATTC_API_WRITE));
                p_buf = &(p_clcb->p_q_cmd->api_write);
                if (p_data->api_write.p_descr_type)
                {
                    p_buf->p_descr_type = (tBTA_GATT_ID *)(p_buf + 1);
                    memcpy(p_buf->p_descr_type, p_data->api_write.p_descr_type,
                           sizeof(tBTA_GATT_ID));
                    if (p_buf->len && p_buf->p_value)
                    {
                        p_buf->p_value  = (UINT8 *)(p_buf->p_descr_type + 1);
                        memcpy(p_buf->p_value, p_data->api_write.p_value,
                               p_data->api_write.len);
                    }
                }
                else if (p_buf->len && p_buf->p_value)
                {
                    p_buf->p_value = (UINT8 *)(p_buf + 1);
                    memcpy(p_buf->p_value, p_data->api_write.p_value,
                           p_data->api_write.len);
                }
                break;
            }
            case BTA_GATTC_API_EXEC_EVT:
            {
                len = sizeof(tBTA_GATTC_API_EXEC);
                p_clcb->p_q_cmd = (tBTA_GATTC_DATA *)GKI_getbuf(len);
                if (p_clcb->p_q_cmd == NULL)
                {
                    APPL_TRACE_ERROR0("allocate buffer failed for p_q_cmd");
                    return FALSE;
                }
                memcpy(p_clcb->p_q_cmd, p_data, len);
                break;
            }
            case BTA_GATTC_API_READ_MULTI_EVT:
            {
                len = sizeof(tBTA_GATTC_API_READ_MULTI) +
                      p_data->api_read_multi.num_attr * sizeof(tBTA_GATTC_ATTR_ID);
                p_clcb->p_q_cmd = (tBTA_GATTC_DATA *)GKI_getbuf(len);
                if (p_clcb->p_q_cmd == NULL)
                {
                    APPL_TRACE_ERROR0("allocate buffer failed for p_q_cmd");
                    return FALSE;
                }
                memcpy(p_clcb->p_q_cmd, p_data, sizeof(tBTA_GATTC_API_READ_MULTI));
                if (p_data->api_read_multi.num_attr &&
                    p_data->api_read_multi.p_id_list)
                {
                    tBTA_GATTC_API_READ_MULTI *p_buf;
                    p_buf = &(p_clcb->p_q_cmd->api_read_multi);
                    p_buf->p_id_list = (tBTA_GATTC_ATTR_ID *)(p_buf + 1);
                    memcpy(p_buf->p_id_list, p_data->api_read_multi.p_id_list,
                        p_data->api_read_multi.num_attr * sizeof(tBTA_GATTC_ATTR_ID));
                }
                break;
            }
/*BOARD_HAVE_BLUETOOTH_RTK_RCU_MDT begin*/
#ifdef BLUETOOTH_RTK_VR
            case 0x5555:
            case 0x5556:
                p_clcb->p_q_cmd = (tBTA_GATTC_DATA *)GKI_getbuf(sizeof(tBTA_GATTC_API_CONFIGURE_MTU));
                APPL_TRACE_DEBUG1("cheng_@allocate p_q_cmd = %p",p_clcb->p_q_cmd);

                if (p_data)
                    memcpy(p_clcb->p_q_cmd, p_data, sizeof(tBTA_GATTC_API_CONFIGURE_MTU));
                break;
#endif
/*BOARD_HAVE_BLUETOOTH_RTK_RCU_MDT end*/
            default:
                APPL_TRACE_ERROR1("queue unsupported command %d", p_data->hdr.event);
                return FALSE;
        }

        in_q = TRUE;
    }
    else if (p_clcb->p_q_cmd)
    {
        APPL_TRACE_ERROR0("already has a pending command!!");
        /* skip the callback now. ----- need to send callback ? */
    }
    else
    {
        APPL_TRACE_ERROR0("queue a null command");
    }

    return in_q;
}
/*******************************************************************************
**
** Function         bta_gattc_pack_attr_uuid
**
** Description      pack UUID into a stream.
**
** Returns
**
*******************************************************************************/
void bta_gattc_pack_attr_uuid(tBTA_GATTC_CACHE_ATTR   *p_attr, tBT_UUID *p_uuid)
{
    UINT8 *pp = (UINT8 *)p_attr->p_uuid;

    memset(p_uuid, 0, sizeof(tBT_UUID));

    p_uuid->len = p_attr->uuid_len;

    if (p_attr->uuid_len == LEN_UUID_16)
    {
        STREAM_TO_UINT16(p_uuid->uu.uuid16, pp);
    }
    else
    {
        memcpy(p_uuid->uu.uuid128, pp, LEN_UUID_128);
    }

    return;
}
/*******************************************************************************
**
** Function         bta_gattc_cpygattid
**
** Description      copy two tBTA_GATT_ID value
**
** Returns
**
*******************************************************************************/
void bta_gattc_cpygattid(tBTA_GATT_ID *p_des, tBTA_GATT_ID *p_src)
{
    memset ((void *)p_des, 0, sizeof(tBTA_GATT_ID));

    p_des->inst_id = p_src->inst_id;

    p_des->uuid.len = p_src->uuid.len;

    if (p_des->uuid.len == LEN_UUID_16)
    {
        p_des->uuid.uu.uuid16 = p_src->uuid.uu.uuid16;
    }
    else if (p_des->uuid.len == LEN_UUID_128)
    {
        memcpy(p_des->uuid.uu.uuid128, p_src->uuid.uu.uuid128, LEN_UUID_128);
    }
}
/*******************************************************************************
**
** Function         bta_gattc_gattid_compare
**
** Description      compare two tBTA_GATT_ID type of pointer
**
** Returns
**
*******************************************************************************/
BOOLEAN bta_gattc_gattid_compare(tBTA_GATT_ID *p_src, tBTA_GATT_ID *p_tar)
{
	if (p_src->inst_id == p_tar->inst_id &&
        bta_gattc_uuid_compare (&p_src->uuid, &p_tar->uuid, TRUE ))
        return TRUE;
    else
        return FALSE;

}
/*******************************************************************************
**
** Function         bta_gattc_srvcid_compare
**
** Description      compare two tBTA_GATT_SRVC_ID type of pointer
**
** Returns
**
*******************************************************************************/
BOOLEAN bta_gattc_srvcid_compare(tBTA_GATT_SRVC_ID *p_src, tBTA_GATT_SRVC_ID *p_tar)
{
    if (p_src->is_primary == p_tar->is_primary &&
        bta_gattc_gattid_compare (&p_src->id, &p_tar->id))
        return TRUE;
    else
        return FALSE;
}
/*******************************************************************************
**
** Function         bta_gattc_charid_compare
**
** Description      compare two tBTA_GATTC_CHAR_ID type of pointer
**
** Returns
**
*******************************************************************************/
BOOLEAN bta_gattc_charid_compare(tBTA_GATTC_CHAR_ID *p_src, tBTA_GATTC_CHAR_ID *p_tar)
{
    if (bta_gattc_gattid_compare (&p_src->char_id, &p_tar->char_id) &&
        bta_gattc_srvcid_compare (&p_src->srvc_id, &p_tar->srvc_id))
        return TRUE;
    else
        return FALSE;
}

/*******************************************************************************
**
** Function         bta_gattc_check_notif_registry
**
** Description      check if the service notificaition has been registered.
**
** Returns
**
*******************************************************************************/
BOOLEAN bta_gattc_check_notif_registry(tBTA_GATTC_RCB  *p_clreg, tBTA_GATTC_SERV *p_srcb,
                                       tBTA_GATTC_NOTIFY  *p_notify)
{
    UINT8           i;

    for (i = 0 ; i < BTA_GATTC_NOTIF_REG_MAX; i ++)
    {
        if (p_clreg->notif_reg[i].in_use &&
            bdcmp(p_clreg->notif_reg[i].remote_bda, p_srcb->server_bda) == 0 &&
            bta_gattc_charid_compare (&p_clreg->notif_reg[i].char_id, &p_notify->char_id))
        {
            APPL_TRACE_DEBUG0("Notification registered!");
            return TRUE;
        }
    }
    return FALSE;

}
/*******************************************************************************
**
** Function         bta_gattc_clear_notif_registration
**
** Description      clear up the notification registration information by BD_ADDR.
**
** Returns          None.
**
*******************************************************************************/
void bta_gattc_clear_notif_registration(UINT16 conn_id)
{
    BD_ADDR             remote_bda;
    tBTA_GATTC_IF       gatt_if;
    tBTA_GATTC_RCB      *p_clrcb ;
    UINT8       i;

    if (GATT_GetConnectionInfor(conn_id, &gatt_if, remote_bda))
    {
        if ((p_clrcb = bta_gattc_cl_get_regcb(gatt_if)) != NULL)
        {
            for (i = 0 ; i < BTA_GATTC_NOTIF_REG_MAX; i ++)
            {
                if (p_clrcb->notif_reg[i].in_use && !bdcmp(p_clrcb->notif_reg[i].remote_bda, remote_bda))
                    memset(&p_clrcb->notif_reg[i], 0, sizeof(tBTA_GATTC_NOTIF_REG));
            }
        }
    }
    else
    {
        APPL_TRACE_ERROR0("can not clear indication/notif registration for unknown app");
    }
    return;
}

/*******************************************************************************
**
** Function         bta_gattc_pack_cb_data
**
** Description      pack the data from read response into callback data structure.
**
** Returns
**
*******************************************************************************/
tBTA_GATT_STATUS bta_gattc_pack_read_cb_data(tBTA_GATTC_SERV *p_srcb,
                                             tBT_UUID *p_descr_uuid,
                                             tGATT_VALUE *p_attr,
                                             tBTA_GATT_READ_VAL *p_value)
{
    UINT8                   i = 0, *pp = p_attr->value;
    tBT_UUID                uuid = {LEN_UUID_16, {GATT_UUID_CHAR_AGG_FORMAT}};
    UINT16                  handle;
    tBTA_GATT_STATUS        status = BTA_GATT_OK;

    /* GATT_UUID_CHAR_AGG_FORMAT */
    if (bta_gattc_uuid_compare (&uuid, p_descr_uuid, TRUE))
    {
        while (p_attr->len >= 2 && i < BTA_GATTC_MULTI_MAX)
        {
            STREAM_TO_UINT16(handle, pp);

            if (bta_gattc_handle2id(p_srcb,
                                    handle,
                                    &p_value->aggre_value.pre_format[i].char_id.srvc_id,
                                    &p_value->aggre_value.pre_format[i].char_id.char_id,
                                    &p_value->aggre_value.pre_format[i].descr_id) == FALSE)
            {
                status = BTA_GATT_INTERNAL_ERROR;
                APPL_TRACE_ERROR1("can not map to GATT ID. handle = 0x%04x", handle);
                break;
            }
            i ++;
            p_attr->len -= 2;
        }
        p_value->aggre_value.num_pres_fmt = i;
    }
    else
    {
        /* all others, take as raw format */
        p_value->unformat.len = p_attr->len;
        p_value->unformat.p_value = p_attr->value;
    }
    return status;
}
/*******************************************************************************
**
** Function         bta_gattc_mark_bg_conn
**
** Description      mark background connection status when a bg connection is initiated
**                  or terminated.
**
** Returns          TRUE if success; FALSE otherwise.
**
*******************************************************************************/
BOOLEAN bta_gattc_mark_bg_conn (tBTA_GATTC_IF client_if,  BD_ADDR_PTR remote_bda_ptr,
                                BOOLEAN add, BOOLEAN is_listen)
{
    tBTA_GATTC_BG_TCK   *p_bg_tck = &bta_gattc_cb.bg_track[0];
    UINT8   i = 0;
    tBTA_GATTC_CIF_MASK  *p_cif_mask;

    for (i = 0; i < BTA_GATTC_KNOWN_SR_MAX; i ++, p_bg_tck ++)
    {
        if (p_bg_tck->in_use &&
            ((remote_bda_ptr != NULL && bdcmp(p_bg_tck->remote_bda, remote_bda_ptr) == 0) ||
            (remote_bda_ptr == NULL && bdcmp(p_bg_tck->remote_bda, dummy_bda) == 0)))
        {
             p_cif_mask = is_listen ? &p_bg_tck->cif_adv_mask : &p_bg_tck->cif_mask;

            if (add)
                /* mask on the cif bit */
                *p_cif_mask |= (1 <<(client_if - 1));
            else
            {
                if (client_if != 0)
                    *p_cif_mask &= (~(1 <<(client_if - 1)));
                else
                    *p_cif_mask = 0;
            }
            /* no BG connection for this device, make it available */
            if (p_bg_tck->cif_mask == 0 && p_bg_tck->cif_adv_mask == 0)
            {
                memset(p_bg_tck, 0, sizeof(tBTA_GATTC_BG_TCK));
            }
            return TRUE;
        }
    }
    if (!add)
    {
        APPL_TRACE_ERROR0("Do not find the bg connection mask for the remote device");
        return FALSE;
    }
    else /* adding a new device mask */
    {
        for (i = 0, p_bg_tck = &bta_gattc_cb.bg_track[0];
             i < BTA_GATTC_KNOWN_SR_MAX; i ++, p_bg_tck ++)
        {
            if (!p_bg_tck->in_use)
            {
                p_bg_tck->in_use = TRUE;
                if (remote_bda_ptr)
                    bdcpy(p_bg_tck->remote_bda, remote_bda_ptr);
                else
                    bdcpy(p_bg_tck->remote_bda, dummy_bda);

                p_cif_mask = is_listen ? &p_bg_tck->cif_adv_mask : &p_bg_tck->cif_mask;

                *p_cif_mask = (1 <<(client_if - 1));
                return TRUE;
            }
        }
        APPL_TRACE_ERROR0("no available space to mark the bg connection status");
        return FALSE;
    }
}
/*******************************************************************************
**
** Function         bta_gattc_check_bg_conn
**
** Description      check if this is a background connection background connection.
**
** Returns          TRUE if success; FALSE otherwise.
**
*******************************************************************************/
BOOLEAN bta_gattc_check_bg_conn (tBTA_GATTC_IF client_if,  BD_ADDR remote_bda, UINT8 role)
{
    tBTA_GATTC_BG_TCK   *p_bg_tck = &bta_gattc_cb.bg_track[0];
    UINT8       i = 0;
    BOOLEAN     is_bg_conn = FALSE;

    for (i = 0; i < BTA_GATTC_KNOWN_SR_MAX && !is_bg_conn; i ++, p_bg_tck ++)
    {
        if (p_bg_tck->in_use &&
            (bdcmp(p_bg_tck->remote_bda, remote_bda) == 0 ||
             bdcmp(p_bg_tck->remote_bda, dummy_bda) == 0))
        {
            if (((p_bg_tck->cif_mask &(1 <<(client_if - 1))) != 0) &&
                role == HCI_ROLE_MASTER)
                is_bg_conn = TRUE;

            if (((p_bg_tck->cif_adv_mask &(1 <<(client_if - 1))) != 0) &&
                role == HCI_ROLE_SLAVE)
                is_bg_conn = TRUE;
        }
    }
    return is_bg_conn;
}
/*******************************************************************************
**
** Function         bta_gattc_send_open_cback
**
** Description      send open callback
**
** Returns
**
*******************************************************************************/
void bta_gattc_send_open_cback( tBTA_GATTC_RCB *p_clreg, tBTA_GATT_STATUS status,
                                BD_ADDR remote_bda, UINT16 conn_id)
{
    tBTA_GATTC      cb_data;

    if (p_clreg->p_cback)
    {
        memset(&cb_data, 0, sizeof(tBTA_GATTC));

        cb_data.open.status = status;
        cb_data.open.client_if = p_clreg->client_if;
        cb_data.open.conn_id = conn_id;
        bdcpy(cb_data.open.remote_bda, remote_bda);

        (*p_clreg->p_cback)(BTA_GATTC_OPEN_EVT, &cb_data);
    }
}
/*******************************************************************************
**
** Function         bta_gattc_conn_alloc
**
** Description      allocate connection tracking spot
**
** Returns          pointer to the clcb
**
*******************************************************************************/
tBTA_GATTC_CONN * bta_gattc_conn_alloc(BD_ADDR remote_bda)
{
    UINT8               i_conn = 0;
    tBTA_GATTC_CONN     *p_conn = &bta_gattc_cb.conn_track[0];

    for (i_conn = 0; i_conn < BTA_GATTC_CONN_MAX; i_conn++, p_conn ++)
    {
        if (!p_conn->in_use)
        {
#if BTA_GATT_DEBUG == TRUE
            APPL_TRACE_DEBUG1("bta_gattc_conn_alloc: found conn_track[%d] available",i_conn);
#endif
            p_conn->in_use          = TRUE;
            bdcpy(p_conn->remote_bda, remote_bda);
            return p_conn;
        }
    }
    return NULL;
}

/*******************************************************************************
**
** Function         bta_gattc_conn_find
**
** Description      allocate connection tracking spot
**
** Returns          pointer to the clcb
**
*******************************************************************************/
tBTA_GATTC_CONN * bta_gattc_conn_find(BD_ADDR remote_bda)
{
    UINT8               i_conn = 0;
    tBTA_GATTC_CONN     *p_conn = &bta_gattc_cb.conn_track[0];

    for (i_conn = 0; i_conn < BTA_GATTC_CONN_MAX; i_conn++, p_conn ++)
    {
        if (p_conn->in_use && bdcmp(remote_bda, p_conn->remote_bda) == 0)
        {
#if BTA_GATT_DEBUG == TRUE
            APPL_TRACE_DEBUG1("bta_gattc_conn_find: found conn_track[%d] matched",i_conn);
#endif
            return p_conn;
        }
    }
    return NULL;
}


/*******************************************************************************
**
** Function         bta_gattc_conn_find_alloc
**
** Description      find or allocate connection tracking spot
**
** Returns          pointer to the clcb
**
*******************************************************************************/
tBTA_GATTC_CONN * bta_gattc_conn_find_alloc(BD_ADDR remote_bda)
{
    tBTA_GATTC_CONN     *p_conn = bta_gattc_conn_find (remote_bda);

    if (p_conn == NULL)
    {
        p_conn = bta_gattc_conn_alloc(remote_bda);
    }
    return p_conn;
}

/*******************************************************************************
**
** Function         bta_gattc_conn_dealloc
**
** Description      de-allocate connection tracking spot
**
** Returns          pointer to the clcb
**
*******************************************************************************/
BOOLEAN bta_gattc_conn_dealloc(BD_ADDR remote_bda)
{
    tBTA_GATTC_CONN     *p_conn = bta_gattc_conn_find (remote_bda);

    if (p_conn != NULL)
    {
        p_conn->in_use = FALSE;
        memset(p_conn->remote_bda, 0, BD_ADDR_LEN);
        return TRUE;
    }
    return FALSE;
}

/*******************************************************************************
**
** Function         bta_gattc_find_int_conn_clcb
**
** Description      try to locate a clcb when an internal connecion event arrives.
**
** Returns          pointer to the clcb
**
*******************************************************************************/
tBTA_GATTC_CLCB * bta_gattc_find_int_conn_clcb(tBTA_GATTC_DATA *p_msg)
{
    tBTA_GATTC_CLCB *p_clcb = NULL;

    if (p_msg->int_conn.role == HCI_ROLE_SLAVE)
        bta_gattc_conn_find_alloc(p_msg->int_conn.remote_bda);

    /* try to locate a logic channel */
    if ((p_clcb = bta_gattc_find_clcb_by_cif(p_msg->int_conn.client_if,
                                             p_msg->int_conn.remote_bda)) == NULL)
    {
        /* for a background connection or listening connection */
        if (p_msg->int_conn.role == HCI_ROLE_SLAVE ||
            bta_gattc_check_bg_conn(p_msg->int_conn.client_if,
                                    p_msg->int_conn.remote_bda,
                                    p_msg->int_conn.role))
        {
            /* allocate a new channel */
            p_clcb = bta_gattc_clcb_alloc(p_msg->int_conn.client_if, p_msg->int_conn.remote_bda);
        }
    }
    return p_clcb;
}

/*******************************************************************************
**
** Function         bta_gattc_find_int_disconn_clcb
**
** Description      try to locate a clcb when an internal disconnect callback arrives.
**
** Returns          pointer to the clcb
**
*******************************************************************************/
tBTA_GATTC_CLCB * bta_gattc_find_int_disconn_clcb(tBTA_GATTC_DATA *p_msg)
{
    tBTA_GATTC_CLCB         *p_clcb = NULL;
    tGATT_DISCONN_REASON    reason = p_msg->int_conn.reason;

    bta_gattc_conn_dealloc(p_msg->int_conn.remote_bda);
    /* connection attempt timeout, send connection callback event */
    if (reason == GATT_CONN_CANCEL || reason == GATT_CONN_L2C_FAILURE)
    {
        p_clcb = bta_gattc_find_clcb_by_cif(p_msg->int_conn.client_if,
                                            p_msg->int_conn.remote_bda);
    }
    else if ((p_clcb = bta_gattc_find_clcb_by_conn_id(p_msg->int_conn.hdr.layer_specific)) == NULL)
    {
        APPL_TRACE_DEBUG1("disconnection ID: [%d] not used by BTA",
                           p_msg->int_conn.hdr.layer_specific);
    }
    return p_clcb;
}

#endif /* BTA_GATT_INCLUDED */
