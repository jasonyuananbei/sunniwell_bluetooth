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
 *  This file contains the GATT client main functions and state machine.
 *
 ******************************************************************************/

#include "bt_target.h"

#if defined(BTA_GATT_INCLUDED) && (BTA_GATT_INCLUDED == TRUE)

#include <string.h>

#include "bta_gattc_int.h"
#include "gki.h"

/*BOARD_HAVE_BLUETOOTH_RTK_VR begin*/
#ifdef BLUETOOTH_RTK_VR
#include "rtk_msbc.h"
#endif
/*BOARD_HAVE_BLUETOOTH_RTK_VR end*/

/*****************************************************************************
** Constants and types
*****************************************************************************/


/* state machine action enumeration list */
enum
{
    BTA_GATTC_OPEN,
    BTA_GATTC_OPEN_FAIL,
    BTA_GATTC_OPEN_ERROR,
    BTA_GATTC_CANCEL_OPEN,
    BTA_GATTC_CANCEL_OPEN_OK,
    BTA_GATTC_CANCEL_OPEN_ERROR,
    BTA_GATTC_CONN,
    BTA_GATTC_START_DISCOVER,
    BTA_GATTC_DISC_CMPL,

    BTA_GATTC_Q_CMD,
    BTA_GATTC_CLOSE,
    BTA_GATTC_CLOSE_FAIL,
    BTA_GATTC_READ,
    BTA_GATTC_WRITE,

    BTA_GATTC_OP_CMPL,
    BTA_GATTC_SEARCH,
    BTA_GATTC_FAIL,
    BTA_GATTC_CONFIRM,
    BTA_GATTC_EXEC,
    BTA_GATTC_READ_MULTI,
    BTA_GATTC_CI_OPEN,
    BTA_GATTC_CI_LOAD,
    BTA_GATTC_CI_SAVE,
    BTA_GATTC_CACHE_OPEN,
    BTA_GATTC_IGNORE_OP_CMPL,
    BTA_GATTC_DISC_CLOSE,
    BTA_GATTC_RESTART_DISCOVER,

    BTA_GATTC_IGNORE
};
/* type for action functions */
typedef void (*tBTA_GATTC_ACTION)(tBTA_GATTC_CLCB *p_clcb, tBTA_GATTC_DATA *p_data);

/* action function list */
const tBTA_GATTC_ACTION bta_gattc_action[] =
{
    bta_gattc_open,
    bta_gattc_open_fail,
    bta_gattc_open_error,
    bta_gattc_cancel_open,
    bta_gattc_cancel_open_ok,
    bta_gattc_cancel_open_error,
    bta_gattc_conn,
    bta_gattc_start_discover,
    bta_gattc_disc_cmpl,

    bta_gattc_q_cmd,
    bta_gattc_close,
    bta_gattc_close_fail,
    bta_gattc_read,
    bta_gattc_write,

    bta_gattc_op_cmpl,
    bta_gattc_search,
    bta_gattc_fail,
    bta_gattc_confirm,
    bta_gattc_execute,
    bta_gattc_read_multi,
    bta_gattc_ci_open,
    bta_gattc_ci_load,
    bta_gattc_ci_save,
    bta_gattc_cache_open,
    bta_gattc_ignore_op_cmpl,
    bta_gattc_disc_close,
    bta_gattc_restart_discover
};


/* state table information */
#define BTA_GATTC_ACTIONS           1       /* number of actions */
#define BTA_GATTC_NEXT_STATE          1       /* position of next state */
#define BTA_GATTC_NUM_COLS            2       /* number of columns in state tables */

/* state table for idle state */
static const UINT8 bta_gattc_st_idle[][BTA_GATTC_NUM_COLS] =
{
/* Event                            Action 1                  Next state */
/* BTA_GATTC_API_OPEN_EVT           */   {BTA_GATTC_OPEN,              BTA_GATTC_W4_CONN_ST},
/* BTA_GATTC_INT_OPEN_FAIL_EVT      */   {BTA_GATTC_IGNORE,            BTA_GATTC_IDLE_ST},
/* BTA_GATTC_API_CANCEL_OPEN_EVT    */   {BTA_GATTC_IGNORE,            BTA_GATTC_IDLE_ST},
/* BTA_GATTC_INT_CANCEL_OPEN_OK_EVT */   {BTA_GATTC_IGNORE,            BTA_GATTC_IDLE_ST},

/* BTA_GATTC_API_READ_EVT           */   {BTA_GATTC_FAIL,              BTA_GATTC_IDLE_ST},
/* BTA_GATTC_API_WRITE_EVT          */   {BTA_GATTC_FAIL,              BTA_GATTC_IDLE_ST},
/* BTA_GATTC_API_EXEC_EVT           */   {BTA_GATTC_FAIL,              BTA_GATTC_IDLE_ST},

/* BTA_GATTC_API_CLOSE_EVT          */   {BTA_GATTC_CLOSE_FAIL,        BTA_GATTC_IDLE_ST},

/* BTA_GATTC_API_SEARCH_EVT         */   {BTA_GATTC_FAIL,              BTA_GATTC_IDLE_ST},
/* BTA_GATTC_API_CONFIRM_EVT        */   {BTA_GATTC_FAIL,              BTA_GATTC_IDLE_ST},
/* BTA_GATTC_API_READ_MULTI_EVT     */   {BTA_GATTC_FAIL,              BTA_GATTC_IDLE_ST},
/* BTA_GATTC_API_REFRESH_EVT        */   {BTA_GATTC_IGNORE,            BTA_GATTC_IDLE_ST},

/* BTA_GATTC_INT_CONN_EVT           */   {BTA_GATTC_CONN,              BTA_GATTC_CONN_ST},
/* BTA_GATTC_INT_DISCOVER_EVT       */   {BTA_GATTC_IGNORE,            BTA_GATTC_IDLE_ST},
/* BTA_GATTC_DISCOVER_CMPL_EVT      */   {BTA_GATTC_IGNORE,            BTA_GATTC_IDLE_ST},
/* BTA_GATTC_OP_CMPL_EVT            */   {BTA_GATTC_IGNORE,            BTA_GATTC_IDLE_ST},
/* BTA_GATTC_INT_DISCONN_EVT       */    {BTA_GATTC_IGNORE,            BTA_GATTC_IDLE_ST},


/* ===> for cache loading, saving   */
/* BTA_GATTC_START_CACHE_EVT        */   {BTA_GATTC_IGNORE,            BTA_GATTC_IDLE_ST},
/* BTA_GATTC_CI_CACHE_OPEN_EVT      */   {BTA_GATTC_IGNORE,            BTA_GATTC_IDLE_ST},
/* BTA_GATTC_CI_CACHE_LOAD_EVT      */   {BTA_GATTC_IGNORE,            BTA_GATTC_IDLE_ST},
/* BTA_GATTC_CI_CACHE_SAVE_EVT      */   {BTA_GATTC_IGNORE,            BTA_GATTC_IDLE_ST}
};

/* state table for wait for open state */
static const UINT8 bta_gattc_st_w4_conn[][BTA_GATTC_NUM_COLS] =
{
/* Event                            Action 1                             Next state */
/* BTA_GATTC_API_OPEN_EVT           */   {BTA_GATTC_OPEN,              BTA_GATTC_W4_CONN_ST},
/* BTA_GATTC_INT_OPEN_FAIL_EVT      */   {BTA_GATTC_OPEN_FAIL,         BTA_GATTC_IDLE_ST},
/* BTA_GATTC_API_CANCEL_OPEN_EVT    */   {BTA_GATTC_CANCEL_OPEN,       BTA_GATTC_W4_CONN_ST},
/* BTA_GATTC_INT_CANCEL_OPEN_OK_EVT */   {BTA_GATTC_CANCEL_OPEN_OK,    BTA_GATTC_IDLE_ST},

/* BTA_GATTC_API_READ_EVT           */   {BTA_GATTC_FAIL,               BTA_GATTC_W4_CONN_ST},
/* BTA_GATTC_API_WRITE_EVT          */   {BTA_GATTC_FAIL,               BTA_GATTC_W4_CONN_ST},
/* BTA_GATTC_API_EXEC_EVT           */   {BTA_GATTC_FAIL,               BTA_GATTC_W4_CONN_ST},

/* BTA_GATTC_API_CLOSE_EVT          */   {BTA_GATTC_CANCEL_OPEN,         BTA_GATTC_W4_CONN_ST},

/* BTA_GATTC_API_SEARCH_EVT         */   {BTA_GATTC_FAIL,               BTA_GATTC_W4_CONN_ST},
/* BTA_GATTC_API_CONFIRM_EVT        */   {BTA_GATTC_FAIL,               BTA_GATTC_W4_CONN_ST},
/* BTA_GATTC_API_READ_MULTI_EVT     */   {BTA_GATTC_FAIL,               BTA_GATTC_W4_CONN_ST},
/* BTA_GATTC_API_REFRESH_EVT        */   {BTA_GATTC_IGNORE,             BTA_GATTC_W4_CONN_ST},

/* BTA_GATTC_INT_CONN_EVT           */   {BTA_GATTC_CONN,               BTA_GATTC_CONN_ST},
/* BTA_GATTC_INT_DISCOVER_EVT       */   {BTA_GATTC_IGNORE,             BTA_GATTC_W4_CONN_ST},
/* BTA_GATTC_DISCOVER_CMPL_EVT       */  {BTA_GATTC_IGNORE,             BTA_GATTC_W4_CONN_ST},
/* BTA_GATTC_OP_CMPL_EVT            */   {BTA_GATTC_IGNORE,             BTA_GATTC_W4_CONN_ST},
/* BTA_GATTC_INT_DISCONN_EVT      */     {BTA_GATTC_OPEN_FAIL,          BTA_GATTC_IDLE_ST},

/* ===> for cache loading, saving   */
/* BTA_GATTC_START_CACHE_EVT        */   {BTA_GATTC_IGNORE,             BTA_GATTC_W4_CONN_ST},
/* BTA_GATTC_CI_CACHE_OPEN_EVT      */   {BTA_GATTC_IGNORE,             BTA_GATTC_W4_CONN_ST},
/* BTA_GATTC_CI_CACHE_LOAD_EVT      */   {BTA_GATTC_IGNORE,             BTA_GATTC_W4_CONN_ST},
/* BTA_GATTC_CI_CACHE_SAVE_EVT      */   {BTA_GATTC_IGNORE,             BTA_GATTC_W4_CONN_ST}
};

/* state table for open state */
static const UINT8 bta_gattc_st_connected[][BTA_GATTC_NUM_COLS] =
{
/* Event                            Action 1                            Next state */
/* BTA_GATTC_API_OPEN_EVT           */   {BTA_GATTC_OPEN_ERROR,         BTA_GATTC_CONN_ST},
/* BTA_GATTC_INT_OPEN_FAIL_EVT      */   {BTA_GATTC_IGNORE,             BTA_GATTC_CONN_ST},
/* BTA_GATTC_API_CANCEL_OPEN_EVT    */   {BTA_GATTC_CANCEL_OPEN_ERROR, BTA_GATTC_CONN_ST},
/* BTA_GATTC_INT_CANCEL_OPEN_OK_EVT */   {BTA_GATTC_IGNORE,            BTA_GATTC_CONN_ST},

/* BTA_GATTC_API_READ_EVT           */   {BTA_GATTC_READ,               BTA_GATTC_CONN_ST},
/* BTA_GATTC_API_WRITE_EVT          */   {BTA_GATTC_WRITE,              BTA_GATTC_CONN_ST},
/* BTA_GATTC_API_EXEC_EVT           */   {BTA_GATTC_EXEC,               BTA_GATTC_CONN_ST},

/* BTA_GATTC_API_CLOSE_EVT          */   {BTA_GATTC_CLOSE,              BTA_GATTC_IDLE_ST},

/* BTA_GATTC_API_SEARCH_EVT         */   {BTA_GATTC_SEARCH,             BTA_GATTC_CONN_ST},
/* BTA_GATTC_API_CONFIRM_EVT        */   {BTA_GATTC_CONFIRM,            BTA_GATTC_CONN_ST},
/* BTA_GATTC_API_READ_MULTI_EVT     */   {BTA_GATTC_READ_MULTI,         BTA_GATTC_CONN_ST},
/* BTA_GATTC_API_REFRESH_EVT        */   {BTA_GATTC_IGNORE,             BTA_GATTC_CONN_ST},

/* BTA_GATTC_INT_CONN_EVT           */   {BTA_GATTC_IGNORE,             BTA_GATTC_CONN_ST},
/* BTA_GATTC_INT_DISCOVER_EVT       */   {BTA_GATTC_START_DISCOVER,     BTA_GATTC_DISCOVER_ST},
/* BTA_GATTC_DISCOVER_CMPL_EVT       */  {BTA_GATTC_IGNORE,             BTA_GATTC_CONN_ST},
/* BTA_GATTC_OP_CMPL_EVT            */   {BTA_GATTC_OP_CMPL,            BTA_GATTC_CONN_ST},

/* BTA_GATTC_INT_DISCONN_EVT        */   {BTA_GATTC_CLOSE,              BTA_GATTC_IDLE_ST},

/* ===> for cache loading, saving   */
/* BTA_GATTC_START_CACHE_EVT        */   {BTA_GATTC_CACHE_OPEN,         BTA_GATTC_DISCOVER_ST},
/* BTA_GATTC_CI_CACHE_OPEN_EVT      */   {BTA_GATTC_IGNORE,             BTA_GATTC_CONN_ST},
/* BTA_GATTC_CI_CACHE_LOAD_EVT      */   {BTA_GATTC_IGNORE,             BTA_GATTC_CONN_ST},
/* BTA_GATTC_CI_CACHE_SAVE_EVT      */   {BTA_GATTC_IGNORE,             BTA_GATTC_CONN_ST}
};

/* state table for discover state */
static const UINT8 bta_gattc_st_discover[][BTA_GATTC_NUM_COLS] =
{
/* Event                            Action 1                            Next state */
/* BTA_GATTC_API_OPEN_EVT           */   {BTA_GATTC_OPEN_ERROR,         BTA_GATTC_DISCOVER_ST},
/* BTA_GATTC_INT_OPEN_FAIL_EVT      */   {BTA_GATTC_IGNORE,             BTA_GATTC_DISCOVER_ST},
/* BTA_GATTC_API_CANCEL_OPEN_EVT    */   {BTA_GATTC_CANCEL_OPEN_ERROR,  BTA_GATTC_DISCOVER_ST},
/* BTA_GATTC_INT_CANCEL_OPEN_OK_EVT */   {BTA_GATTC_FAIL,               BTA_GATTC_DISCOVER_ST},

/* BTA_GATTC_API_READ_EVT           */   {BTA_GATTC_Q_CMD,              BTA_GATTC_DISCOVER_ST},
/* BTA_GATTC_API_WRITE_EVT          */   {BTA_GATTC_Q_CMD,              BTA_GATTC_DISCOVER_ST},
/* BTA_GATTC_API_EXEC_EVT           */   {BTA_GATTC_Q_CMD,              BTA_GATTC_DISCOVER_ST},

/* BTA_GATTC_API_CLOSE_EVT          */   {BTA_GATTC_DISC_CLOSE,         BTA_GATTC_DISCOVER_ST},

/* BTA_GATTC_API_SEARCH_EVT         */   {BTA_GATTC_Q_CMD,              BTA_GATTC_DISCOVER_ST},
/* BTA_GATTC_API_CONFIRM_EVT        */   {BTA_GATTC_CONFIRM,            BTA_GATTC_DISCOVER_ST},
/* BTA_GATTC_API_READ_MULTI_EVT     */   {BTA_GATTC_Q_CMD,              BTA_GATTC_DISCOVER_ST},
/* BTA_GATTC_API_REFRESH_EVT        */   {BTA_GATTC_IGNORE,             BTA_GATTC_DISCOVER_ST},

/* BTA_GATTC_INT_CONN_EVT           */   {BTA_GATTC_CONN,               BTA_GATTC_DISCOVER_ST},
/* BTA_GATTC_INT_DISCOVER_EVT       */   {BTA_GATTC_RESTART_DISCOVER,   BTA_GATTC_DISCOVER_ST},
/* BTA_GATTC_DISCOVER_CMPL_EVT      */   {BTA_GATTC_DISC_CMPL,          BTA_GATTC_CONN_ST},
/* BTA_GATTC_OP_CMPL_EVT            */   {BTA_GATTC_IGNORE_OP_CMPL,     BTA_GATTC_DISCOVER_ST},
/* BTA_GATTC_INT_DISCONN_EVT        */   {BTA_GATTC_CLOSE,              BTA_GATTC_IDLE_ST},

/* ===> for cache loading, saving       */
/* BTA_GATTC_START_CACHE_EVT        */   {BTA_GATTC_IGNORE,             BTA_GATTC_DISCOVER_ST},
/* BTA_GATTC_CI_CACHE_OPEN_EVT      */   {BTA_GATTC_CI_OPEN,            BTA_GATTC_DISCOVER_ST},
/* BTA_GATTC_CI_CACHE_LOAD_EVT      */   {BTA_GATTC_CI_LOAD,            BTA_GATTC_DISCOVER_ST},
/* BTA_GATTC_CI_CACHE_SAVE_EVT      */   {BTA_GATTC_CI_SAVE,            BTA_GATTC_DISCOVER_ST}
};

/* type for state table */
typedef const UINT8 (*tBTA_GATTC_ST_TBL)[BTA_GATTC_NUM_COLS];

/* state table */
const tBTA_GATTC_ST_TBL bta_gattc_st_tbl[] =
{
    bta_gattc_st_idle,
    bta_gattc_st_w4_conn,
    bta_gattc_st_connected,
    bta_gattc_st_discover
};

/*****************************************************************************
** Global data
*****************************************************************************/

/* GATTC control block */
#if BTA_DYNAMIC_MEMORY == FALSE
tBTA_GATTC_CB  bta_gattc_cb;
#endif

#if BTA_GATT_DEBUG == TRUE
static char *gattc_evt_code(tBTA_GATTC_INT_EVT evt_code);
static char *gattc_state_code(tBTA_GATTC_STATE state_code);
#endif

/*******************************************************************************
**
** Function         bta_gattc_sm_execute
**
** Description      State machine event handling function for GATTC
**
**
** Returns          void
**
*******************************************************************************/
void bta_gattc_sm_execute(tBTA_GATTC_CLCB *p_clcb, UINT16 event, tBTA_GATTC_DATA *p_data)
{
    tBTA_GATTC_ST_TBL     state_table;
    UINT8               action;
    int                 i;
#if BTA_GATT_DEBUG == TRUE
    tBTA_GATTC_STATE in_state = p_clcb->state;
    UINT16         in_event = event;
    APPL_TRACE_DEBUG4("bta_gattc_sm_execute: State 0x%02x [%s], Event 0x%x[%s]", in_state,
                      gattc_state_code(in_state),
                      in_event,
                      gattc_evt_code(in_event));
#endif


    /* look up the state table for the current state */
    state_table = bta_gattc_st_tbl[p_clcb->state];

    event &= 0x00FF;

    /* set next state */
    p_clcb->state = state_table[event][BTA_GATTC_NEXT_STATE];

    /* execute action functions */
    for (i = 0; i < BTA_GATTC_ACTIONS; i++)
    {
        if ((action = state_table[event][i]) != BTA_GATTC_IGNORE)
        {
            (*bta_gattc_action[action])(p_clcb, p_data);
/*BOARD_HAVE_BLUETOOTH_RTK_RCU_MDT begin*/
#ifdef BLUETOOTH_RTK_VR
            int change_count = 0;
            if((in_event == BTA_GATTC_INT_CONN_EVT) && (p_clcb->state == BTA_GATTC_CONN_ST) && ((in_state == BTA_GATTC_IDLE_ST)|| (in_state == BTA_GATTC_W4_CONN_ST)))
            {
                tBTA_GATTC_DATA *voice_mtu;
                if((voice_mtu = (tBTA_GATTC_DATA *) GKI_getbuf((UINT16) sizeof(tBTA_GATTC_DATA))) != NULL)
                {
                    APPL_TRACE_DEBUG0("cheng_@bta_gattc_sm_execute begin to change MTU first!");
                    memset(voice_mtu, 0, sizeof(tBTA_GATTC_DATA));
                    voice_mtu->api_configure_mtu.configureMTU_value = RTKBT_MTU_SIZE;
                    voice_mtu->api_configure_mtu.hdr.event =0x5556;
                    bta_gattc_RTKBT_VR_configure_mtu(p_clcb,voice_mtu);
                    GKI_freebuf(voice_mtu);
                }
                else{
                    APPL_TRACE_DEBUG0("cheng_@bta_gattc_sm_execute to get buffer fail!");
                }
            }
#endif
/*BOARD_HAVE_BLUETOOTH_RTK_RCU_MDT end*/
        }
        else
        {
            break;
        }
    }

#if BTA_GATT_DEBUG == TRUE
    if (in_state != p_clcb->state)
    {
        APPL_TRACE_DEBUG3("GATTC State Change: [%s] -> [%s] after Event [%s]",
                          gattc_state_code(in_state),
                          gattc_state_code(p_clcb->state),
                          gattc_evt_code(in_event));
    }
#endif
}

/*******************************************************************************
**
** Function         bta_gattc_hdl_event
**
** Description      GATT client main event handling function.
**
**
** Returns          void
**
*******************************************************************************/
BOOLEAN bta_gattc_hdl_event(BT_HDR *p_msg)
{
    tBTA_GATTC_CB *p_cb = &bta_gattc_cb;
    tBTA_GATTC_CLCB *p_clcb = NULL;
    tBTA_GATTC_RCB      *p_clreg;
#if BTA_GATT_DEBUG == TRUE
    APPL_TRACE_DEBUG1("bta_gattc_hdl_event: Event [%s]", gattc_evt_code(p_msg->event));
#endif
    switch (p_msg->event)
    {
        case BTA_GATTC_API_DISABLE_EVT:
            bta_gattc_disable(p_cb);
            break;

        case BTA_GATTC_API_REG_EVT:
            bta_gattc_register(p_cb, (tBTA_GATTC_DATA *) p_msg);
            break;

        case BTA_GATTC_INT_START_IF_EVT:
            bta_gattc_start_if(p_cb, (tBTA_GATTC_DATA *) p_msg);
            break;

        case BTA_GATTC_API_DEREG_EVT:
            p_clreg = bta_gattc_cl_get_regcb(((tBTA_GATTC_DATA *)p_msg)->api_dereg.client_if);
            bta_gattc_deregister(p_cb, p_clreg);
            break;

        case BTA_GATTC_API_OPEN_EVT:
            bta_gattc_process_api_open(p_cb, (tBTA_GATTC_DATA *) p_msg);
            break;

        case BTA_GATTC_API_CANCEL_OPEN_EVT:
            bta_gattc_process_api_open_cancel(p_cb, (tBTA_GATTC_DATA *) p_msg);
            break;

        case BTA_GATTC_API_REFRESH_EVT:
            bta_gattc_process_api_refresh(p_cb, (tBTA_GATTC_DATA *) p_msg);
            break;

#if BLE_INCLUDED == TRUE
        case BTA_GATTC_API_LISTEN_EVT:
            bta_gattc_listen(p_cb, (tBTA_GATTC_DATA *) p_msg);
            break;
#endif

        case BTA_GATTC_ENC_CMPL_EVT:
            bta_gattc_process_enc_cmpl(p_cb, (tBTA_GATTC_DATA *) p_msg);
            break;

        default:
            if (p_msg->event == BTA_GATTC_INT_CONN_EVT)
                p_clcb = bta_gattc_find_int_conn_clcb((tBTA_GATTC_DATA *) p_msg);
            else if (p_msg->event == BTA_GATTC_INT_DISCONN_EVT)
                p_clcb = bta_gattc_find_int_disconn_clcb((tBTA_GATTC_DATA *) p_msg);
            else
                p_clcb = bta_gattc_find_clcb_by_conn_id(p_msg->layer_specific);

            if (p_clcb != NULL)
            {
                bta_gattc_sm_execute(p_clcb, p_msg->event, (tBTA_GATTC_DATA *) p_msg);
            }
            else
            {
                APPL_TRACE_DEBUG1("Ignore unknown conn ID: %d", p_msg->layer_specific);
            }

            break;
    }


    return(TRUE);
}


/*****************************************************************************
**  Debug Functions
*****************************************************************************/
#if BTA_GATT_DEBUG == TRUE

/*******************************************************************************
**
** Function         gattc_evt_code
**
** Description
**
** Returns          void
**
*******************************************************************************/
static char *gattc_evt_code(tBTA_GATTC_INT_EVT evt_code)
{
    switch (evt_code)
    {
        case BTA_GATTC_API_OPEN_EVT:
            return "BTA_GATTC_API_OPEN_EVT";
        case BTA_GATTC_INT_OPEN_FAIL_EVT:
            return "BTA_GATTC_INT_OPEN_FAIL_EVT";
        case BTA_GATTC_API_CANCEL_OPEN_EVT:
            return "BTA_GATTC_API_CANCEL_OPEN_EVT";
        case BTA_GATTC_INT_CANCEL_OPEN_OK_EVT:
            return "BTA_GATTC_INT_CANCEL_OPEN_OK_EVT";
        case BTA_GATTC_API_READ_EVT:
            return "BTA_GATTC_API_READ_EVT";
        case BTA_GATTC_API_WRITE_EVT:
            return "BTA_GATTC_API_WRITE_EVT";
        case BTA_GATTC_API_EXEC_EVT:
            return "BTA_GATTC_API_EXEC_EVT";
        case BTA_GATTC_API_CLOSE_EVT:
            return "BTA_GATTC_API_CLOSE_EVT";
        case BTA_GATTC_API_SEARCH_EVT:
            return "BTA_GATTC_API_SEARCH_EVT";
        case BTA_GATTC_API_CONFIRM_EVT:
            return "BTA_GATTC_API_CONFIRM_EVT";
        case BTA_GATTC_API_READ_MULTI_EVT:
            return "BTA_GATTC_API_READ_MULTI_EVT";
        case BTA_GATTC_INT_CONN_EVT:
            return "BTA_GATTC_INT_CONN_EVT";
        case BTA_GATTC_INT_DISCOVER_EVT:
            return "BTA_GATTC_INT_DISCOVER_EVT";
        case BTA_GATTC_DISCOVER_CMPL_EVT:
            return "BTA_GATTC_DISCOVER_CMPL_EVT";
        case BTA_GATTC_OP_CMPL_EVT:
            return "BTA_GATTC_OP_CMPL_EVT";
        case BTA_GATTC_INT_DISCONN_EVT:
            return "BTA_GATTC_INT_DISCONN_EVT";
        case BTA_GATTC_START_CACHE_EVT:
            return "BTA_GATTC_START_CACHE_EVT";
        case BTA_GATTC_CI_CACHE_OPEN_EVT:
            return "BTA_GATTC_CI_CACHE_OPEN_EVT";
        case BTA_GATTC_CI_CACHE_LOAD_EVT:
            return "BTA_GATTC_CI_CACHE_LOAD_EVT";
        case BTA_GATTC_CI_CACHE_SAVE_EVT:
            return "BTA_GATTC_CI_CACHE_SAVE_EVT";
        case BTA_GATTC_INT_START_IF_EVT:
            return "BTA_GATTC_INT_START_IF_EVT";
        case BTA_GATTC_API_REG_EVT:
            return "BTA_GATTC_API_REG_EVT";
        case BTA_GATTC_API_DEREG_EVT:
            return "BTA_GATTC_API_DEREG_EVT";
        case BTA_GATTC_API_REFRESH_EVT:
            return "BTA_GATTC_API_REFRESH_EVT";
        case BTA_GATTC_API_LISTEN_EVT:
            return "BTA_GATTC_API_LISTEN_EVT";
        case BTA_GATTC_API_DISABLE_EVT:
            return "BTA_GATTC_API_DISABLE_EVT";
        default:
            return "unknown GATTC event code";
    }
}

/*******************************************************************************
**
** Function         gattc_state_code
**
** Description
**
** Returns          void
**
*******************************************************************************/
static char *gattc_state_code(tBTA_GATTC_STATE state_code)
{
    switch (state_code)
    {
        case BTA_GATTC_IDLE_ST:
            return "GATTC_IDLE_ST";
        case BTA_GATTC_W4_CONN_ST:
            return "GATTC_W4_CONN_ST";
        case BTA_GATTC_CONN_ST:
            return "GATTC_CONN_ST";
        case BTA_GATTC_DISCOVER_ST:
            return "GATTC_DISCOVER_ST";
        default:
            return "unknown GATTC state code";
    }
}

#endif  /* Debug Functions */
#endif /* BTA_GATT_INCLUDED */