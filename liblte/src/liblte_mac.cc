/*******************************************************************************

    Copyright 2013-2016, 2021 Ben Wojtowicz

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*******************************************************************************

    File: liblte_mac.cc

    Description: Contains all the implementations for the LTE Medium Access
                 Control Layer library.

    Revision History
    ----------    -------------    --------------------------------------------
    07/21/2013    Ben Wojtowicz    Created file.
    03/26/2014    Ben Wojtowicz    Added DL-SCH/UL-SCH PDU handling.
    05/04/2014    Ben Wojtowicz    Added control element handling.
    06/15/2014    Ben Wojtowicz    Added support for padding LCIDs and breaking
                                   out max and min buffer sizes for BSRs.
    08/03/2014    Ben Wojtowicz    Using the common value_2_bits and
                                   bits_2_value functions.
    11/29/2014    Ben Wojtowicz    Using byte message struct for SDUs.
    02/15/2015    Ben Wojtowicz    Removed FIXMEs for transparent mode and mcs.
    03/11/2015    Ben Wojtowicz    Fixed long BSR CE and added extended power
                                   headroom CE support.
    07/03/2016    Ben Wojtowicz    Fixed extended power headroom CE.
    02/14/2021    Ben Wojtowicz    Massive reformat.

*******************************************************************************/

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "liblte_mac.h"

/*******************************************************************************
                              DEFINES
*******************************************************************************/


/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/


/*******************************************************************************
                              GLOBAL VARIABLES
*******************************************************************************/

uint32 bsr_max_buffer_size[64] = {     0,     10,     12,     14,     17,     19,     22,     26,
                                      31,     36,     42,     49,     57,     67,     78,     91,
                                     107,    125,    146,    171,    200,    234,    274,    321,
                                     376,    440,    515,    603,    706,    826,    967,   1132,
                                    1326,   1552,   1817,   2127,   2490,   2915,   3413,   3995,
                                    4677,   5476,   6411,   7505,   8787,  10287,  12043,  14099,
                                   16507,  19325,  22624,  26487,  31009,  36304,  42502,  49759,
                                   58255,  68201,  79864,  93479, 109439, 128125, 150000, 150000};
uint32 bsr_min_buffer_size[64] = {     0,      0,     10,     12,     14,     17,     19,     22,
                                      26,     31,     36,     42,     49,     57,     67,     78,
                                      91,    107,    125,    146,    171,    200,    234,    274,
                                     321,    376,    440,    515,    603,    706,    826,    967,
                                    1132,   1326,   1552,   1817,   2127,   2490,   2915,   3413,
                                    3995,   4677,   5476,   6411,   7505,   8787,  10287,  12043,
                                   14099,  16507,  19325,  22624,  26487,  31009,  36304,  42502,
                                   49759,  58255,  68201,  79864,  93479, 109439, 128125, 150000};

/*******************************************************************************
                              CONTROL ELEMENT FUNCTIONS
*******************************************************************************/

/*********************************************************************
    MAC CE Name: Truncated Buffer Status Report

    Description: CE containing one LCG ID field and one corresponding
                 Buffer Size field

    Document Reference: 36.321 v10.2.0 Section 6.1.3.1
*********************************************************************/
LIBLTE_ERROR_ENUM liblte_mac_pack_truncated_bsr_ce(LIBLTE_MAC_TRUNCATED_BSR_CE_STRUCT  *truncated_bsr,
                                                   uint8                              **ce_ptr)
{
    if(truncated_bsr == NULL || ce_ptr == NULL)
        return LIBLTE_ERROR_INVALID_INPUTS;

    liblte_value_2_bits(truncated_bsr->lcg_id, ce_ptr, 2);
    uint32 buf_size = 0;
    for(uint32 i=0; i<64; i++)
        if(truncated_bsr->min_buffer_size  > bsr_min_buffer_size[i] &&
           truncated_bsr->max_buffer_size <= bsr_max_buffer_size[i])
            buf_size = i;
    if(buf_size == 0)
    {
        liblte_value_2_bits(63, ce_ptr, 6);
    }else{
        liblte_value_2_bits(buf_size, ce_ptr, 6);
    }

    return LIBLTE_SUCCESS;
}
LIBLTE_ERROR_ENUM liblte_mac_unpack_truncated_bsr_ce(uint8                              **ce_ptr,
                                                     LIBLTE_MAC_TRUNCATED_BSR_CE_STRUCT  *truncated_bsr)
{
    if(ce_ptr == NULL || truncated_bsr == NULL)
        return LIBLTE_ERROR_INVALID_INPUTS;

    truncated_bsr->lcg_id          = liblte_bits_2_value(ce_ptr, 2);
    uint8 buffer_size_idx          = liblte_bits_2_value(ce_ptr, 6);
    truncated_bsr->max_buffer_size = bsr_max_buffer_size[buffer_size_idx];
    truncated_bsr->min_buffer_size = bsr_min_buffer_size[buffer_size_idx];

    return LIBLTE_SUCCESS;
}

/*********************************************************************
    MAC CE Name: Short Buffer Status Report

    Description: CE containing one LCG ID field and one corresponding
                 Buffer Size field

    Document Reference: 36.321 v10.2.0 Section 6.1.3.1
*********************************************************************/
LIBLTE_ERROR_ENUM liblte_mac_pack_short_bsr_ce(LIBLTE_MAC_SHORT_BSR_CE_STRUCT  *short_bsr,
                                               uint8                          **ce_ptr)
{
    return liblte_mac_pack_truncated_bsr_ce(short_bsr, ce_ptr);
}
LIBLTE_ERROR_ENUM liblte_mac_unpack_short_bsr_ce(uint8                          **ce_ptr,
                                                 LIBLTE_MAC_SHORT_BSR_CE_STRUCT  *short_bsr)
{
    return liblte_mac_unpack_truncated_bsr_ce(ce_ptr, short_bsr);
}

/*********************************************************************
    MAC CE Name: Long Buffer Status Report

    Description: CE containing four Buffer Size fields, corresponding
                 to LCG IDs #0 through #3

    Document Reference: 36.321 v10.2.0 Section 6.1.3.1
*********************************************************************/
LIBLTE_ERROR_ENUM liblte_mac_pack_long_bsr_ce(LIBLTE_MAC_LONG_BSR_CE_STRUCT  *long_bsr,
                                              uint8                         **ce_ptr)
{
    if(long_bsr == NULL || ce_ptr == NULL)
        return LIBLTE_ERROR_INVALID_INPUTS;

    uint32 buf_size[4] = {0, 0, 0, 0};

    for(uint32 i=0; i<64; i++)
    {
        if(long_bsr->min_buffer_size_0  > bsr_min_buffer_size[i] &&
           long_bsr->max_buffer_size_0 <= bsr_max_buffer_size[i])
            buf_size[0] = i;
        if(long_bsr->min_buffer_size_1  > bsr_min_buffer_size[i] &&
           long_bsr->max_buffer_size_1 <= bsr_max_buffer_size[i])
            buf_size[1] = i;
        if(long_bsr->min_buffer_size_2  > bsr_min_buffer_size[i] &&
           long_bsr->max_buffer_size_2 <= bsr_max_buffer_size[i])
            buf_size[2] = i;
        if(long_bsr->min_buffer_size_3  > bsr_min_buffer_size[i] &&
           long_bsr->max_buffer_size_3 <= bsr_max_buffer_size[i])
            buf_size[3] = i;
    }
    for(uint32 i=0; i<4; i++)
    {
        if(buf_size[i] == 0)
        {
            liblte_value_2_bits(63, ce_ptr, 6);
        }else{
            liblte_value_2_bits(buf_size[i], ce_ptr, 6);
        }
    }

    return LIBLTE_SUCCESS;
}
LIBLTE_ERROR_ENUM liblte_mac_unpack_long_bsr_ce(uint8                         **ce_ptr,
                                                LIBLTE_MAC_LONG_BSR_CE_STRUCT  *long_bsr)
{
    if(ce_ptr == NULL || long_bsr == NULL)
        return LIBLTE_ERROR_INVALID_INPUTS;

    uint32 buffer_size_0        = liblte_bits_2_value(ce_ptr, 6);
    long_bsr->max_buffer_size_0 = bsr_max_buffer_size[buffer_size_0];
    long_bsr->min_buffer_size_0 = bsr_min_buffer_size[buffer_size_0];
    uint32 buffer_size_1        = liblte_bits_2_value(ce_ptr, 6);
    long_bsr->max_buffer_size_1 = bsr_max_buffer_size[buffer_size_1];
    long_bsr->min_buffer_size_1 = bsr_min_buffer_size[buffer_size_1];
    uint32 buffer_size_2        = liblte_bits_2_value(ce_ptr, 6);
    long_bsr->max_buffer_size_2 = bsr_max_buffer_size[buffer_size_2];
    long_bsr->min_buffer_size_2 = bsr_min_buffer_size[buffer_size_2];
    uint32 buffer_size_3        = liblte_bits_2_value(ce_ptr, 6);
    long_bsr->max_buffer_size_3 = bsr_max_buffer_size[buffer_size_3];
    long_bsr->min_buffer_size_3 = bsr_min_buffer_size[buffer_size_3];

    return LIBLTE_SUCCESS;
}

/*********************************************************************
    MAC CE Name: C-RNTI

    Description: CE containing a C-RNTI

    Document Reference: 36.321 v10.2.0 Section 6.1.3.2
*********************************************************************/
LIBLTE_ERROR_ENUM liblte_mac_pack_c_rnti_ce(LIBLTE_MAC_C_RNTI_CE_STRUCT  *c_rnti,
                                            uint8                       **ce_ptr)
{
    if(c_rnti == NULL || ce_ptr == NULL)
        return LIBLTE_ERROR_INVALID_INPUTS;

    liblte_value_2_bits(c_rnti->c_rnti, ce_ptr, 16);

    return LIBLTE_SUCCESS;
}
LIBLTE_ERROR_ENUM liblte_mac_unpack_c_rnti_ce(uint8                       **ce_ptr,
                                              LIBLTE_MAC_C_RNTI_CE_STRUCT  *c_rnti)
{
    if(ce_ptr == NULL || c_rnti == NULL)
        return LIBLTE_ERROR_INVALID_INPUTS;

    c_rnti->c_rnti = liblte_bits_2_value(ce_ptr, 16);

    return LIBLTE_SUCCESS;
}

/*********************************************************************
    MAC CE Name: DRX Command

    Description: CE containing nothing

    Document Reference: 36.321 v10.2.0 Section 6.1.3.3
*********************************************************************/

/*********************************************************************
    MAC CE Name: UE Contention Resolution Identity

    Description: CE containing the contention resolution identity for
                 a UE

    Document Reference: 36.321 v10.2.0 Section 6.1.3.4
*********************************************************************/
LIBLTE_ERROR_ENUM liblte_mac_pack_ue_contention_resolution_id_ce(LIBLTE_MAC_UE_CONTENTION_RESOLUTION_ID_CE_STRUCT  *ue_con_res_id,
                                                                 uint8                                            **ce_ptr)
{
    if(ue_con_res_id == NULL || ce_ptr == NULL)
        return LIBLTE_ERROR_INVALID_INPUTS;

    liblte_value_2_bits((uint32)(ue_con_res_id->id >> 32), ce_ptr, 16);
    liblte_value_2_bits((uint32)(ue_con_res_id->id),       ce_ptr, 32);

    return LIBLTE_SUCCESS;
}
LIBLTE_ERROR_ENUM liblte_mac_unpack_ue_contention_resolution_id_ce(uint8                                            **ce_ptr,
                                                                   LIBLTE_MAC_UE_CONTENTION_RESOLUTION_ID_CE_STRUCT  *ue_con_res_id)
{
    if(ce_ptr == NULL || ue_con_res_id == NULL)
        return LIBLTE_ERROR_INVALID_INPUTS;

    ue_con_res_id->id = (((uint64)liblte_bits_2_value(ce_ptr, 16) << 32) |
                         (uint64)liblte_bits_2_value(ce_ptr, 32));

    return LIBLTE_SUCCESS;
}

/*********************************************************************
    MAC CE Name: Timing Advance Command

    Description: CE containing a timing advance command for a UE

    Document Reference: 36.321 v10.2.0 Section 6.1.3.5
*********************************************************************/
LIBLTE_ERROR_ENUM liblte_mac_pack_ta_command_ce(LIBLTE_MAC_TA_COMMAND_CE_STRUCT  *ta_command,
                                                uint8                           **ce_ptr)
{
    if(ta_command == NULL || ce_ptr == NULL)
        return LIBLTE_ERROR_INVALID_INPUTS;

    liblte_value_2_bits(0,              ce_ptr, 2); // R
    liblte_value_2_bits(ta_command->ta, ce_ptr, 6);

    return LIBLTE_SUCCESS;
}
LIBLTE_ERROR_ENUM liblte_mac_unpack_ta_command_ce(uint8                           **ce_ptr,
                                                  LIBLTE_MAC_TA_COMMAND_CE_STRUCT  *ta_command)
{
    if(ce_ptr == NULL || ta_command == NULL)
        return LIBLTE_ERROR_INVALID_INPUTS;

    liblte_bits_2_value(ce_ptr, 2); // R
    ta_command->ta = liblte_bits_2_value(ce_ptr, 6);

    return LIBLTE_SUCCESS;
}

/*********************************************************************
    MAC CE Name: Power Headroom

    Description: CE containing the power headroom of the UE

    Document Reference: 36.321 v10.2.0 Section 6.1.3.6
*********************************************************************/
LIBLTE_ERROR_ENUM liblte_mac_pack_power_headroom_ce(LIBLTE_MAC_POWER_HEADROOM_CE_STRUCT  *power_headroom,
                                                    uint8                               **ce_ptr)
{
    if(power_headroom == NULL || ce_ptr == NULL)
        return LIBLTE_ERROR_INVALID_INPUTS;

    liblte_value_2_bits(0,                  ce_ptr, 2); // R
    liblte_value_2_bits(power_headroom->ph, ce_ptr, 6);

    return LIBLTE_SUCCESS;
}
LIBLTE_ERROR_ENUM liblte_mac_unpack_power_headroom_ce(uint8                               **ce_ptr,
                                                      LIBLTE_MAC_POWER_HEADROOM_CE_STRUCT  *power_headroom)
{
    if(ce_ptr == NULL || power_headroom == NULL)
        return LIBLTE_ERROR_INVALID_INPUTS;

    liblte_bits_2_value(ce_ptr, 2); // R
    power_headroom->ph = liblte_bits_2_value(ce_ptr, 6);

    return LIBLTE_SUCCESS;
}

/*********************************************************************
    MAC CE Name: Extended Power Headroom

    Description: CE containing the power headroom of the UE

    Document Reference: 36.321 v10.2.0 Section 6.1.3.6a
*********************************************************************/
void pack_eph_cell(LIBLTE_MAC_EPH_CELL_STRUCT  &eph_cell,
                   uint8                      **ce_ptr)
{
    liblte_value_2_bits(eph_cell.p,  ce_ptr, 1);
    liblte_value_2_bits(eph_cell.v,  ce_ptr, 1);
    liblte_value_2_bits(eph_cell.ph, ce_ptr, 6);
    if(!eph_cell.v)
    {
        liblte_value_2_bits(0,               ce_ptr, 2); // R
        liblte_value_2_bits(eph_cell.p_cmax, ce_ptr, 6);
    }
}
LIBLTE_ERROR_ENUM liblte_mac_pack_ext_power_headroom_ce(LIBLTE_MAC_EXT_POWER_HEADROOM_CE_STRUCT  *ext_power_headroom,
                                                        uint8                                   **ce_ptr)
{
    if(ext_power_headroom == NULL || ce_ptr == NULL)
        return LIBLTE_ERROR_INVALID_INPUTS;

    // SCell Presence Indicators
    for(uint32 i=0; i<7; i++)
        liblte_value_2_bits(ext_power_headroom->scell_present[6-i], ce_ptr, 1);
    liblte_value_2_bits(0, ce_ptr, 1); // R

    // PCell Type 2
    if(ext_power_headroom->pcell_type_2_present)
        pack_eph_cell(ext_power_headroom->pcell_type_2, ce_ptr);

    // PCell Type 1
    pack_eph_cell(ext_power_headroom->pcell_type_1, ce_ptr);

    // SCell
    for(uint32 i=0; i<7; i++)
        if(ext_power_headroom->scell_present[i])
            pack_eph_cell(ext_power_headroom->scell[i], ce_ptr);

    return LIBLTE_SUCCESS;
}
void unpack_eph_cell(uint8                      **ce_ptr,
                     LIBLTE_MAC_EPH_CELL_STRUCT  &eph_cell)
{
    eph_cell.p  = liblte_bits_2_value(ce_ptr, 1);
    eph_cell.v  = liblte_bits_2_value(ce_ptr, 1);
    eph_cell.ph = liblte_bits_2_value(ce_ptr, 6);
    if(!eph_cell.v)
    {
        liblte_bits_2_value(ce_ptr, 2); // R
        eph_cell.p_cmax = liblte_bits_2_value(ce_ptr, 6);
    }
}
LIBLTE_ERROR_ENUM liblte_mac_unpack_ext_power_headroom_ce(uint8                                   **ce_ptr,
                                                          bool                                      simultaneous_pucch_pusch,
                                                          LIBLTE_MAC_EXT_POWER_HEADROOM_CE_STRUCT  *ext_power_headroom)
{
    if(ce_ptr == NULL || ext_power_headroom == NULL)
        return LIBLTE_ERROR_INVALID_INPUTS;

    // SCell Presence Indicators
    for(uint32 i=0; i<7; i++)
        ext_power_headroom->scell_present[6-i] = liblte_bits_2_value(ce_ptr, 1);
    liblte_bits_2_value(ce_ptr, 1); // R

    // PCell Type 2
    ext_power_headroom->pcell_type_2_present = false;
    if(simultaneous_pucch_pusch)
    {
        ext_power_headroom->pcell_type_2_present = true;
        unpack_eph_cell(ce_ptr, ext_power_headroom->pcell_type_2);
    }

    // PCell Type 1
    unpack_eph_cell(ce_ptr, ext_power_headroom->pcell_type_1);

    // SCell
    for(uint32 i=0; i<7; i++)
        if(ext_power_headroom->scell_present[i])
            unpack_eph_cell(ce_ptr, ext_power_headroom->scell[i]);

    return LIBLTE_SUCCESS;
}

/*********************************************************************
    MAC CE Name: MCH Scheduling Information

    Description: CE containing MTCH stops

    Document Reference: 36.321 v10.2.0 Section 6.1.3.7
*********************************************************************/
LIBLTE_ERROR_ENUM liblte_mac_pack_mch_scheduling_information_ce(LIBLTE_MAC_MCH_SCHEDULING_INFORMATION_CE_STRUCT  *mch_sched_info,
                                                                uint8                                           **ce_ptr)
{
    if(mch_sched_info == NULL || ce_ptr == NULL)
        return LIBLTE_ERROR_INVALID_INPUTS;

    if(mch_sched_info->N_items > LIBLTE_MAC_MCH_SCHEDULING_INFORMATION_MAX_N_ITEMS)
        return LIBLTE_ERROR_INVALID_INPUTS;

    for(uint32 i=0; i<mch_sched_info->N_items; i++)
    {
        liblte_value_2_bits(mch_sched_info->lcid[i],     ce_ptr,  5);
        liblte_value_2_bits(mch_sched_info->stop_mch[i], ce_ptr, 11);
    }

    return LIBLTE_SUCCESS;
}
LIBLTE_ERROR_ENUM liblte_mac_unpack_mch_scheduling_information_ce(uint8                                           **ce_ptr,
                                                                  LIBLTE_MAC_MCH_SCHEDULING_INFORMATION_CE_STRUCT  *mch_sched_info)
{
    if(ce_ptr == NULL || mch_sched_info == NULL)
        return LIBLTE_ERROR_INVALID_INPUTS;

    if(mch_sched_info->N_items > LIBLTE_MAC_MCH_SCHEDULING_INFORMATION_MAX_N_ITEMS)
        return LIBLTE_ERROR_INVALID_INPUTS;

    for(uint32 i=0; i<mch_sched_info->N_items; i++)
    {
        mch_sched_info->lcid[i]     = liblte_bits_2_value(ce_ptr,  5);
        mch_sched_info->stop_mch[i] = liblte_bits_2_value(ce_ptr, 11);
    }

    return LIBLTE_SUCCESS;
}

/*********************************************************************
    MAC CE Name: Activation Deactivation

    Description: CE containing activation/deactivation of SCells

    Document Reference: 36.321 v10.2.0 Section 6.1.3.8
*********************************************************************/
LIBLTE_ERROR_ENUM liblte_mac_pack_activation_deactivation_ce(LIBLTE_MAC_ACTIVATION_DEACTIVATION_CE_STRUCT  *act_deact,
                                                             uint8                                        **ce_ptr)
{
    if(act_deact == NULL || ce_ptr == NULL)
        return LIBLTE_ERROR_INVALID_INPUTS;

    liblte_value_2_bits(act_deact->c7, ce_ptr, 1);
    liblte_value_2_bits(act_deact->c6, ce_ptr, 1);
    liblte_value_2_bits(act_deact->c5, ce_ptr, 1);
    liblte_value_2_bits(act_deact->c4, ce_ptr, 1);
    liblte_value_2_bits(act_deact->c3, ce_ptr, 1);
    liblte_value_2_bits(act_deact->c2, ce_ptr, 1);
    liblte_value_2_bits(act_deact->c1, ce_ptr, 1);
    liblte_value_2_bits(0,             ce_ptr, 1); // R

    return LIBLTE_SUCCESS;
}
LIBLTE_ERROR_ENUM liblte_mac_unpack_activation_deactivation_ce(uint8                                        **ce_ptr,
                                                               LIBLTE_MAC_ACTIVATION_DEACTIVATION_CE_STRUCT  *act_deact)
{
    if(ce_ptr == NULL || act_deact == NULL)
        return LIBLTE_ERROR_INVALID_INPUTS;

    act_deact->c7 = liblte_bits_2_value(ce_ptr, 1);
    act_deact->c6 = liblte_bits_2_value(ce_ptr, 1);
    act_deact->c5 = liblte_bits_2_value(ce_ptr, 1);
    act_deact->c4 = liblte_bits_2_value(ce_ptr, 1);
    act_deact->c3 = liblte_bits_2_value(ce_ptr, 1);
    act_deact->c2 = liblte_bits_2_value(ce_ptr, 1);
    act_deact->c1 = liblte_bits_2_value(ce_ptr, 1);
    liblte_bits_2_value(ce_ptr, 1); // R

    return LIBLTE_SUCCESS;
}

/*******************************************************************************
                              PDU FUNCTIONS
*******************************************************************************/

/*********************************************************************
    PDU Name: DL-SCH and UL-SCH MAC PDU

    Description: PDU containing a MAC header, zero or more MAC SDUs,
                 and zero or more MAC control elements

    Document Reference: 36.321 v10.2.0 Section 6.1.2
*********************************************************************/
void pack_length(uint32   length,
                 uint8  **msg_ptr)
{
    if(length < 128)
    {
        liblte_value_2_bits(0,      msg_ptr, 1); // F
        liblte_value_2_bits(length, msg_ptr, 7);
    }else{
        liblte_value_2_bits(1,      msg_ptr, 1); // F
        liblte_value_2_bits(length, msg_ptr, 15);
    }
}
LIBLTE_ERROR_ENUM liblte_mac_pack_mac_pdu(LIBLTE_MAC_PDU_STRUCT *pdu,
                                          LIBLTE_BIT_MSG_STRUCT *msg)
{
    if(pdu == NULL || msg == NULL)
        return LIBLTE_ERROR_INVALID_INPUTS;

    if(pdu->N_subheaders > LIBLTE_MAC_MAX_MAC_PDU_N_SUBHEADERS)
        return LIBLTE_ERROR_INVALID_INPUTS;

    uint8 *msg_ptr = msg->msg;

    // Pack the subheaders
    for(uint32 i=0; i<pdu->N_subheaders; i++)
    {
        liblte_value_2_bits(0, &msg_ptr, 2); // R
        if(i != pdu->N_subheaders-1)
        {
            liblte_value_2_bits(1, &msg_ptr, 1); // E
        }else{
            liblte_value_2_bits(0, &msg_ptr, 1); // E
        }
        liblte_value_2_bits(pdu->subheader[i].lcid, &msg_ptr, 5);
        if(i == (pdu->N_subheaders-1))
            continue;
        if(LIBLTE_MAC_CHAN_TYPE_DLSCH == pdu->chan_type)
        {
            if(!(LIBLTE_MAC_DLSCH_ACTIVATION_DEACTIVATION_LCID     == pdu->subheader[i].lcid ||
                 LIBLTE_MAC_DLSCH_UE_CONTENTION_RESOLUTION_ID_LCID == pdu->subheader[i].lcid ||
                 LIBLTE_MAC_DLSCH_TA_COMMAND_LCID                  == pdu->subheader[i].lcid ||
                 LIBLTE_MAC_DLSCH_DRX_COMMAND_LCID                 == pdu->subheader[i].lcid ||
                 LIBLTE_MAC_DLSCH_PADDING_LCID                     == pdu->subheader[i].lcid))
                pack_length(pdu->subheader[i].payload.sdu.N_bytes, &msg_ptr);
        }else if(LIBLTE_MAC_CHAN_TYPE_ULSCH == pdu->chan_type){
            if(LIBLTE_MAC_ULSCH_EXT_POWER_HEADROOM_REPORT_LCID == pdu->subheader[i].lcid)
            {
                // SCell presence byte is always present
                uint32 length = 1;
                if(pdu->subheader[i].payload.ext_power_headroom.pcell_type_2_present)
                {
                    length++;
                    if(!pdu->subheader[i].payload.ext_power_headroom.pcell_type_2.v)
                        length++;
                }
                length++;
                if(!pdu->subheader[i].payload.ext_power_headroom.pcell_type_1.v)
                    length++;
                for(uint32 j=0; j<7; j++)
                {
                    if(pdu->subheader[i].payload.ext_power_headroom.scell_present[j])
                    {
                        length++;
                        if(!pdu->subheader[i].payload.ext_power_headroom.scell[j].v)
                            length++;
                    }
                }
                pack_length(length, &msg_ptr);
            }else if(!(LIBLTE_MAC_ULSCH_POWER_HEADROOM_REPORT_LCID == pdu->subheader[i].lcid ||
                       LIBLTE_MAC_ULSCH_C_RNTI_LCID                == pdu->subheader[i].lcid ||
                       LIBLTE_MAC_ULSCH_TRUNCATED_BSR_LCID         == pdu->subheader[i].lcid ||
                       LIBLTE_MAC_ULSCH_SHORT_BSR_LCID             == pdu->subheader[i].lcid ||
                       LIBLTE_MAC_ULSCH_LONG_BSR_LCID              == pdu->subheader[i].lcid ||
                       LIBLTE_MAC_ULSCH_PADDING_LCID               == pdu->subheader[i].lcid)){
                pack_length(pdu->subheader[i].payload.sdu.N_bytes, &msg_ptr);
            }
        }else{ // LIBLTE_MAC_CHAN_TYPE_MCH == mac_pdu->chan_type
            if(LIBLTE_MAC_MCH_SCHEDULING_INFORMATION_LCID == pdu->subheader[i].lcid)
            {
                pack_length(pdu->subheader[i].payload.mch_sched_info.N_items*2, &msg_ptr);
            }else{
                pack_length(pdu->subheader[i].payload.sdu.N_bytes, &msg_ptr);
            }
        }
    }

    // Pack the control elements and SDUs
    for(uint32 i=0; i<pdu->N_subheaders; i++)
    {
        if(LIBLTE_MAC_CHAN_TYPE_DLSCH == pdu->chan_type)
        {
            if(LIBLTE_MAC_DLSCH_ACTIVATION_DEACTIVATION_LCID == pdu->subheader[i].lcid)
            {
                liblte_mac_pack_activation_deactivation_ce(&pdu->subheader[i].payload.act_deact, &msg_ptr);
            }else if(LIBLTE_MAC_DLSCH_UE_CONTENTION_RESOLUTION_ID_LCID == pdu->subheader[i].lcid){
                liblte_mac_pack_ue_contention_resolution_id_ce(&pdu->subheader[i].payload.ue_con_res_id, &msg_ptr);
            }else if(LIBLTE_MAC_DLSCH_TA_COMMAND_LCID == pdu->subheader[i].lcid){
                liblte_mac_pack_ta_command_ce(&pdu->subheader[i].payload.ta_command, &msg_ptr);
            }else if(LIBLTE_MAC_DLSCH_DRX_COMMAND_LCID == pdu->subheader[i].lcid){
                // No content for DRX Command CE
            }else if(LIBLTE_MAC_DLSCH_PADDING_LCID == pdu->subheader[i].lcid){
                // No content for PADDING CE
            }else{ // SDU
                for(uint32 j=0; j<pdu->subheader[i].payload.sdu.N_bytes; j++)
                    liblte_value_2_bits(pdu->subheader[i].payload.sdu.msg[j], &msg_ptr, 8);
            }
        }else if(LIBLTE_MAC_CHAN_TYPE_ULSCH == pdu->chan_type){
            if(LIBLTE_MAC_ULSCH_EXT_POWER_HEADROOM_REPORT_LCID == pdu->subheader[i].lcid)
            {
                liblte_mac_pack_ext_power_headroom_ce(&pdu->subheader[i].payload.ext_power_headroom, &msg_ptr);
            }else if(LIBLTE_MAC_ULSCH_POWER_HEADROOM_REPORT_LCID == pdu->subheader[i].lcid){
                liblte_mac_pack_power_headroom_ce(&pdu->subheader[i].payload.power_headroom, &msg_ptr);
            }else if(LIBLTE_MAC_ULSCH_C_RNTI_LCID == pdu->subheader[i].lcid){
                liblte_mac_pack_c_rnti_ce(&pdu->subheader[i].payload.c_rnti, &msg_ptr);
            }else if(LIBLTE_MAC_ULSCH_TRUNCATED_BSR_LCID == pdu->subheader[i].lcid){
                liblte_mac_pack_truncated_bsr_ce(&pdu->subheader[i].payload.truncated_bsr, &msg_ptr);
            }else if(LIBLTE_MAC_ULSCH_SHORT_BSR_LCID == pdu->subheader[i].lcid){
                liblte_mac_pack_short_bsr_ce(&pdu->subheader[i].payload.short_bsr, &msg_ptr);
            }else if(LIBLTE_MAC_ULSCH_LONG_BSR_LCID == pdu->subheader[i].lcid){
                liblte_mac_pack_long_bsr_ce(&pdu->subheader[i].payload.long_bsr, &msg_ptr);
            }else{ // SDU
                for(uint32 j=0; j<pdu->subheader[i].payload.sdu.N_bytes; j++)
                    liblte_value_2_bits(pdu->subheader[i].payload.sdu.msg[j], &msg_ptr, 8);
            }
        }else{ // LIBLTE_MAC_CHAN_TYPE_MCH == mac_pdu->chan_type
            if(LIBLTE_MAC_MCH_SCHEDULING_INFORMATION_LCID == pdu->subheader[i].lcid)
            {
                liblte_mac_pack_mch_scheduling_information_ce(&pdu->subheader[i].payload.mch_sched_info, &msg_ptr);
            }else{ // SDU
                for(uint32 j=0; j<pdu->subheader[i].payload.sdu.N_bytes; j++)
                    liblte_value_2_bits(pdu->subheader[i].payload.sdu.msg[j], &msg_ptr, 8);
            }
        }
    }

    msg->N_bits = msg_ptr - msg->msg;

    return LIBLTE_SUCCESS;
}
uint32 unpack_length(uint8 **msg_ptr,
                     uint8   e_bit)
{
    if(!e_bit)
        return 0;
    if(liblte_bits_2_value(msg_ptr, 1)) // F
        return liblte_bits_2_value(msg_ptr, 15);
    return liblte_bits_2_value(msg_ptr, 7);
}
LIBLTE_ERROR_ENUM liblte_mac_unpack_mac_pdu(LIBLTE_BIT_MSG_STRUCT *msg,
                                            bool                   simultaneous_pucch_pusch,
                                            LIBLTE_MAC_PDU_STRUCT *pdu)
{
    if(msg == NULL || pdu == NULL)
        return LIBLTE_ERROR_INVALID_INPUTS;

    uint8 *msg_ptr = msg->msg;
    uint8  e_bit   = 1;

    // Unpack the subheaders
    pdu->N_subheaders = 0;
    while(e_bit)
    {
        liblte_bits_2_value(&msg_ptr, 2); // R
        e_bit = liblte_bits_2_value(&msg_ptr, 1);
        pdu->subheader[pdu->N_subheaders].lcid = liblte_bits_2_value(&msg_ptr, 5);

        if(LIBLTE_MAC_CHAN_TYPE_DLSCH == pdu->chan_type)
        {
            if(!(LIBLTE_MAC_DLSCH_ACTIVATION_DEACTIVATION_LCID     == pdu->subheader[pdu->N_subheaders].lcid ||
                 LIBLTE_MAC_DLSCH_UE_CONTENTION_RESOLUTION_ID_LCID == pdu->subheader[pdu->N_subheaders].lcid ||
                 LIBLTE_MAC_DLSCH_TA_COMMAND_LCID                  == pdu->subheader[pdu->N_subheaders].lcid ||
                 LIBLTE_MAC_DLSCH_DRX_COMMAND_LCID                 == pdu->subheader[pdu->N_subheaders].lcid ||
                 LIBLTE_MAC_DLSCH_PADDING_LCID                     == pdu->subheader[pdu->N_subheaders].lcid))
                pdu->subheader[pdu->N_subheaders].payload.sdu.N_bytes = unpack_length(&msg_ptr,
                                                                                      e_bit);
        }else if(LIBLTE_MAC_CHAN_TYPE_ULSCH == pdu->chan_type){
            if(LIBLTE_MAC_ULSCH_EXT_POWER_HEADROOM_REPORT_LCID == pdu->subheader[pdu->N_subheaders].lcid)
            {
                // Skip over length fields
                unpack_length(&msg_ptr, e_bit);
            }else if(!(LIBLTE_MAC_ULSCH_POWER_HEADROOM_REPORT_LCID == pdu->subheader[pdu->N_subheaders].lcid ||
                       LIBLTE_MAC_ULSCH_C_RNTI_LCID                == pdu->subheader[pdu->N_subheaders].lcid ||
                       LIBLTE_MAC_ULSCH_TRUNCATED_BSR_LCID         == pdu->subheader[pdu->N_subheaders].lcid ||
                       LIBLTE_MAC_ULSCH_SHORT_BSR_LCID             == pdu->subheader[pdu->N_subheaders].lcid ||
                       LIBLTE_MAC_ULSCH_LONG_BSR_LCID              == pdu->subheader[pdu->N_subheaders].lcid ||
                       LIBLTE_MAC_ULSCH_PADDING_LCID               == pdu->subheader[pdu->N_subheaders].lcid)){
                pdu->subheader[pdu->N_subheaders].payload.sdu.N_bytes = unpack_length(&msg_ptr,
                                                                                      e_bit);
            }
        }else{ // LIBLTE_MAC_CHAN_TYPE_MCH == mac_pdu->subheader[i].lcid
            if(LIBLTE_MAC_MCH_SCHEDULING_INFORMATION_LCID == pdu->subheader[pdu->N_subheaders].lcid)
            {
                pdu->subheader[pdu->N_subheaders].payload.mch_sched_info.N_items = unpack_length(&msg_ptr,
                                                                                                 e_bit) / 2;
            }else{
                pdu->subheader[pdu->N_subheaders].payload.sdu.N_bytes = unpack_length(&msg_ptr,
                                                                                      e_bit);
            }
        }
        pdu->N_subheaders++;
    }

    // Unpack the control elements and SDUs
    for(uint32 i=0; i<pdu->N_subheaders; i++)
    {
        if(LIBLTE_MAC_CHAN_TYPE_DLSCH == pdu->chan_type)
        {
            if(LIBLTE_MAC_DLSCH_ACTIVATION_DEACTIVATION_LCID == pdu->subheader[i].lcid)
            {
                liblte_mac_unpack_activation_deactivation_ce(&msg_ptr, &pdu->subheader[i].payload.act_deact);
            }else if(LIBLTE_MAC_DLSCH_UE_CONTENTION_RESOLUTION_ID_LCID == pdu->subheader[i].lcid){
                liblte_mac_unpack_ue_contention_resolution_id_ce(&msg_ptr, &pdu->subheader[i].payload.ue_con_res_id);
            }else if(LIBLTE_MAC_DLSCH_TA_COMMAND_LCID == pdu->subheader[i].lcid){
                liblte_mac_unpack_ta_command_ce(&msg_ptr, &pdu->subheader[i].payload.ta_command);
            }else if(LIBLTE_MAC_DLSCH_DRX_COMMAND_LCID == pdu->subheader[i].lcid){
                // No content for DRX Command CE
            }else if(LIBLTE_MAC_DLSCH_PADDING_LCID == pdu->subheader[i].lcid){
                // No content for PADDING CE
            }else{ // SDU
                if(pdu->subheader[i].payload.sdu.N_bytes == 0)
                    pdu->subheader[i].payload.sdu.N_bytes = (msg->N_bits - (msg_ptr - msg->msg))/8;
                for(uint32 j=0; j<pdu->subheader[i].payload.sdu.N_bytes; j++)
                    pdu->subheader[i].payload.sdu.msg[j] = liblte_bits_2_value(&msg_ptr, 8);
            }
        }else if(LIBLTE_MAC_CHAN_TYPE_ULSCH == pdu->chan_type){
            if(LIBLTE_MAC_ULSCH_EXT_POWER_HEADROOM_REPORT_LCID == pdu->subheader[i].lcid)
            {
                liblte_mac_unpack_ext_power_headroom_ce(&msg_ptr, simultaneous_pucch_pusch, &pdu->subheader[i].payload.ext_power_headroom);
            }else if(LIBLTE_MAC_ULSCH_POWER_HEADROOM_REPORT_LCID == pdu->subheader[i].lcid){
                liblte_mac_unpack_power_headroom_ce(&msg_ptr, &pdu->subheader[i].payload.power_headroom);
            }else if(LIBLTE_MAC_ULSCH_C_RNTI_LCID == pdu->subheader[i].lcid){
                liblte_mac_unpack_c_rnti_ce(&msg_ptr, &pdu->subheader[i].payload.c_rnti);
            }else if(LIBLTE_MAC_ULSCH_TRUNCATED_BSR_LCID == pdu->subheader[i].lcid){
                liblte_mac_unpack_truncated_bsr_ce(&msg_ptr, &pdu->subheader[i].payload.truncated_bsr);
            }else if(LIBLTE_MAC_ULSCH_SHORT_BSR_LCID == pdu->subheader[i].lcid){
                liblte_mac_unpack_short_bsr_ce(&msg_ptr, &pdu->subheader[i].payload.short_bsr);
            }else if(LIBLTE_MAC_ULSCH_LONG_BSR_LCID == pdu->subheader[i].lcid){
                liblte_mac_unpack_long_bsr_ce(&msg_ptr, &pdu->subheader[i].payload.long_bsr);
            }else if(LIBLTE_MAC_ULSCH_PADDING_LCID == pdu->subheader[i].lcid){
                // No content for PADDING CE
            }else{ // SDU
                if(pdu->subheader[i].payload.sdu.N_bytes == 0)
                    pdu->subheader[i].payload.sdu.N_bytes = (msg->N_bits - (msg_ptr - msg->msg))/8;
                for(uint32 j=0; j<pdu->subheader[i].payload.sdu.N_bytes; j++)
                    pdu->subheader[i].payload.sdu.msg[j] = liblte_bits_2_value(&msg_ptr, 8);
            }
        }else{ // LIBLTE_MAC_CHAN_TYPE_MCH == mac_pdu->chan_type
            if(LIBLTE_MAC_MCH_SCHEDULING_INFORMATION_LCID == pdu->subheader[i].lcid)
            {
                if(pdu->subheader[i].payload.mch_sched_info.N_items == 0)
                    pdu->subheader[i].payload.mch_sched_info.N_items = ((msg->N_bits - (msg_ptr - msg->msg)) / 8) / 2;
                liblte_mac_unpack_mch_scheduling_information_ce(&msg_ptr, &pdu->subheader[i].payload.mch_sched_info);
            }else if(LIBLTE_MAC_MCH_PADDING_LCID == pdu->subheader[i].lcid){
                // No content for PADDING CE
            }else{ // SDU
                if(pdu->subheader[i].payload.sdu.N_bytes == 0)
                    pdu->subheader[i].payload.sdu.N_bytes = (msg->N_bits - (msg_ptr - msg->msg))/8;
                for(uint32 j=0; j<pdu->subheader[i].payload.sdu.N_bytes; j++)
                    pdu->subheader[i].payload.sdu.msg[j] = liblte_bits_2_value(&msg_ptr, 8);
            }
        }
    }

    return LIBLTE_SUCCESS;
}

/*********************************************************************
    PDU Name: Transparent

    Description: PDU containing a MAC SDU

    Document Reference: 36.321 v10.2.0 Section 6.1.4
*********************************************************************/

/*********************************************************************
    PDU Name: Random Access Response

    Description: PDU containing a MAC header and zero or more MAC
                 Random Access Responses

    Document Reference: 36.321 v10.2.0 Section 6.1.5

    Notes: Currently only supports 1 RAR per PDU
*********************************************************************/
LIBLTE_ERROR_ENUM liblte_mac_pack_random_access_response_pdu(LIBLTE_MAC_RAR_STRUCT *rar,
                                                             LIBLTE_BIT_MSG_STRUCT *pdu)
{
    if(rar == NULL || pdu == NULL)
        return LIBLTE_ERROR_INVALID_INPUTS;

    uint8 *pdu_ptr = pdu->msg;

    if(LIBLTE_MAC_RAR_HEADER_TYPE_BI == rar->hdr_type)
    {
        // Pack Header
        liblte_value_2_bits(0,             &pdu_ptr, 1); // E
        liblte_value_2_bits(rar->hdr_type, &pdu_ptr, 1);
        liblte_value_2_bits(0,             &pdu_ptr, 2); // R
        liblte_value_2_bits(rar->BI,       &pdu_ptr, 4);

        pdu->N_bits = pdu_ptr - pdu->msg;

        return LIBLTE_SUCCESS;
    }
    if(LIBLTE_MAC_RAR_HEADER_TYPE_RAPID == rar->hdr_type)
    {
        // Pack Header
        liblte_value_2_bits(0,             &pdu_ptr, 1); // E
        liblte_value_2_bits(rar->hdr_type, &pdu_ptr, 1);
        liblte_value_2_bits(rar->RAPID,    &pdu_ptr, 6);

        // Pack RAR
        liblte_value_2_bits(0,                   &pdu_ptr, 1); // R
        liblte_value_2_bits(rar->timing_adv_cmd, &pdu_ptr, 11);
        liblte_value_2_bits(rar->hopping_flag,   &pdu_ptr, 1);
        liblte_value_2_bits(rar->rba,            &pdu_ptr, 10);
        liblte_value_2_bits(rar->mcs,            &pdu_ptr, 4);
        liblte_value_2_bits(rar->tpc_command,    &pdu_ptr, 3);
        liblte_value_2_bits(rar->ul_delay,       &pdu_ptr, 1);
        liblte_value_2_bits(rar->csi_req,        &pdu_ptr, 1);
        liblte_value_2_bits(rar->temp_c_rnti,    &pdu_ptr, 16);

        pdu->N_bits = pdu_ptr - pdu->msg;

        return LIBLTE_SUCCESS;
    }

    return LIBLTE_ERROR_INVALID_INPUTS;
}
LIBLTE_ERROR_ENUM liblte_mac_unpack_random_access_response_pdu(LIBLTE_BIT_MSG_STRUCT *pdu,
                                                               LIBLTE_MAC_RAR_STRUCT *rar)
{
    if(pdu == NULL || rar == NULL)
        return LIBLTE_ERROR_INVALID_INPUTS;

    uint8 *pdu_ptr = pdu->msg;

    liblte_bits_2_value(&pdu_ptr, 1); // E
    rar->hdr_type = (LIBLTE_MAC_RAR_HEADER_TYPE_ENUM)liblte_bits_2_value(&pdu_ptr, 1);
    if(LIBLTE_MAC_RAR_HEADER_TYPE_BI == rar->hdr_type)
    {
        liblte_bits_2_value(&pdu_ptr, 2); // R
        rar->BI = liblte_bits_2_value(&pdu_ptr, 4);

        return LIBLTE_SUCCESS;
    }
    if(LIBLTE_MAC_RAR_HEADER_TYPE_RAPID == rar->hdr_type)
    {
        // Unpack header
        rar->RAPID = liblte_bits_2_value(&pdu_ptr, 6);

        // Unpack RAR
        liblte_bits_2_value(&pdu_ptr, 1); // R
        rar->timing_adv_cmd = liblte_bits_2_value(&pdu_ptr, 11);
        rar->hopping_flag   = (LIBLTE_MAC_RAR_HOPPING_ENUM)liblte_bits_2_value(&pdu_ptr, 1);
        rar->rba            = liblte_bits_2_value(&pdu_ptr, 10);
        rar->mcs            = liblte_bits_2_value(&pdu_ptr, 4);
        rar->tpc_command    = (LIBLTE_MAC_RAR_TPC_COMMAND_ENUM)liblte_bits_2_value(&pdu_ptr, 3);
        rar->ul_delay       = (LIBLTE_MAC_RAR_UL_DELAY_ENUM)liblte_bits_2_value(&pdu_ptr, 1);
        rar->csi_req        = (LIBLTE_MAC_RAR_CSI_REQ_ENUM)liblte_bits_2_value(&pdu_ptr, 1);
        rar->temp_c_rnti    = liblte_bits_2_value(&pdu_ptr, 16);

        return LIBLTE_SUCCESS;
    }

    return LIBLTE_ERROR_INVALID_INPUTS;
}
