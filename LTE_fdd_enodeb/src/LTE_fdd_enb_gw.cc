#line 2 "LTE_fdd_enb_gw.cc" // Make __FILE__ omit the path
/*******************************************************************************

    Copyright 2014-2017, 2021 Ben Wojtowicz

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

    File: LTE_fdd_enb_gw.cc

    Description: Contains all the implementations for the LTE FDD eNodeB
                 IP gateway.

    Revision History
    ----------    -------------    --------------------------------------------
    11/29/2014    Ben Wojtowicz    Created file
    12/16/2014    Ben Wojtowicz    Added ol extension to message queue.
    02/15/2015    Ben Wojtowicz    Moved to new message queue.
    03/11/2015    Ben Wojtowicz    Closing TUN device on stop.
    12/06/2015    Ben Wojtowicz    Changed boost::mutex to pthread_mutex_t and
                                   sem_t.
    02/13/2016    Ben Wojtowicz    Using memcpy instead of a typed cast for
                                   parsing the IP packet header (thanks to
                                   Damian Jarek for finding this).
    07/03/2016    Ben Wojtowicz    Setting processor affinity.
    07/29/2017    Ben Wojtowicz    Moved away from singleton pattern.
    02/14/2021    Ben Wojtowicz    Massive reformat.

*******************************************************************************/

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "LTE_fdd_enb_gw.h"
#include "LTE_fdd_enb_user_mgr.h"
#include <fcntl.h>
#include <arpa/inet.h>
#include <linux/ip.h>
#include <netinet/ip6.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <errno.h>
#include <thread>

/*******************************************************************************
                              DEFINES
*******************************************************************************/


/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/


/*******************************************************************************
                              GLOBAL VARIABLES
*******************************************************************************/


/*******************************************************************************
                              CLASS IMPLEMENTATIONS
*******************************************************************************/

/********************************/
/*    Constructor/Destructor    */
/********************************/
LTE_fdd_enb_gw::LTE_fdd_enb_gw(LTE_fdd_enb_interface *iface, LTE_fdd_enb_user_mgr *um) :
    interface{iface}, user_mgr{um}, started{false}
{
}
LTE_fdd_enb_gw::~LTE_fdd_enb_gw()
{
    stop();
}

/********************/
/*    Start/Stop    */
/********************/
bool LTE_fdd_enb_gw::is_started()
{
    std::lock_guard<std::mutex> lock(start_mutex);

    return started;
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_gw::start(LTE_fdd_enb_msgq *from_pdcp,
                                             LTE_fdd_enb_msgq *to_pdcp,
                                             char             *err_str)
{
    std::lock_guard<std::mutex> lock(start_mutex);
    LTE_fdd_enb_msgq_cb         pdcp_cb(&LTE_fdd_enb_msgq_cb_wrapper<LTE_fdd_enb_gw, &LTE_fdd_enb_gw::handle_pdcp_msg>, this);
    struct ifreq                ifr;
    int32                       sock;
    char                        dev[IFNAMSIZ] = "tun_openlte";
    uint32                      ip_addr;

    if(started)
        return LTE_FDD_ENB_ERROR_ALREADY_STARTED;

    started = true;

    ip_addr = interface->get_ip_addr_start();

    // Construct the TUN device
    tun_fd = open("/dev/net/tun", O_RDWR);
    if(0 > tun_fd)
    {
        err_str = strerror(errno);
        started = false;
        return LTE_FDD_ENB_ERROR_CANT_START;
    }
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    strncpy(ifr.ifr_ifrn.ifrn_name, dev, IFNAMSIZ);
    if(0 > ioctl(tun_fd, TUNSETIFF, &ifr))
    {
        err_str = strerror(errno);
        started = false;
        close(tun_fd);
        return LTE_FDD_ENB_ERROR_CANT_START;
    }

    // Setup the IP address range
    sock                                                   = socket(AF_INET, SOCK_DGRAM, 0);
    ifr.ifr_addr.sa_family                                 = AF_INET;
    ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr = htonl(ip_addr);
    if(0 > ioctl(sock, SIOCSIFADDR, &ifr))
    {
        err_str = strerror(errno);
        started = false;
        close(tun_fd);
        return LTE_FDD_ENB_ERROR_CANT_START;
    }
    ifr.ifr_netmask.sa_family                                 = AF_INET;
    ((struct sockaddr_in *)&ifr.ifr_netmask)->sin_addr.s_addr = inet_addr("255.255.255.0");
    if(0 > ioctl(sock, SIOCSIFNETMASK, &ifr))
    {
        err_str = strerror(errno);
        started = false;
        close(tun_fd);
        return LTE_FDD_ENB_ERROR_CANT_START;
    }

    // Bring up the interface
    if(0 > ioctl(sock, SIOCGIFFLAGS, &ifr))
    {
        err_str = strerror(errno);
        started = false;
        close(tun_fd);
        return LTE_FDD_ENB_ERROR_CANT_START;
    }
    ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
    if(0 > ioctl(sock, SIOCSIFFLAGS, &ifr))
    {
        err_str = strerror(errno);
        started = false;
        close(tun_fd);
        return LTE_FDD_ENB_ERROR_CANT_START;
    }

    // Setup PDCP communication
    msgq_from_pdcp = from_pdcp;
    msgq_to_pdcp   = to_pdcp;
    msgq_from_pdcp->attach_rx(pdcp_cb);

    // Setup a thread to receive packets from the TUN device
    pthread_create(&rx_thread, NULL, &receive_thread, this);
    return LTE_FDD_ENB_ERROR_NONE;
}
void LTE_fdd_enb_gw::stop()
{
    std::lock_guard<std::mutex> lock(start_mutex);
    if(!started)
        return;
    started = false;
    start_mutex.unlock();
    pthread_cancel(rx_thread);
    pthread_join(rx_thread, NULL);
    close(tun_fd);
}

/***********************/
/*    Communication    */
/***********************/
void LTE_fdd_enb_gw::handle_pdcp_msg(LTE_FDD_ENB_MESSAGE_STRUCT &msg)
{
    if(msg.type == LTE_FDD_ENB_MESSAGE_TYPE_GW_DATA_READY)
        return handle_gw_data(&msg.msg.gw_data_ready);

    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                              LTE_FDD_ENB_DEBUG_LEVEL_GW,
                              __FILE__,
                              __LINE__,
                              "Received invalid PDCP message %s",
                              LTE_fdd_enb_message_type_text[msg.type]);
}

/*******************************/
/*    PDCP Message Handlers    */
/*******************************/
void LTE_fdd_enb_gw::handle_gw_data(LTE_FDD_ENB_GW_DATA_READY_MSG_STRUCT *gw_data)
{
    LIBLTE_BYTE_MSG_STRUCT *msg;

    if(LTE_FDD_ENB_ERROR_NONE != gw_data->rb->get_next_gw_data_msg(&msg))
        return;

    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_GW,
                              __FILE__,
                              __LINE__,
                              msg,
                              "Received GW data message for RNTI=%u and RB=%s",
                              gw_data->user->get_c_rnti(),
                              LTE_fdd_enb_rb_text[gw_data->rb->get_rb_id()]);
    interface->send_ip_pcap_msg(msg->msg, msg->N_bytes);

    if(msg->N_bytes != write(tun_fd, msg->msg, msg->N_bytes))
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_GW,
                                  __FILE__,
                                  __LINE__,
                                  "Write failure");

    // Delete the message
    gw_data->rb->delete_next_gw_data_msg();
}

/********************/
/*    GW Receive    */
/********************/
void* LTE_fdd_enb_gw::receive_thread(void *inputs)
{
    LTE_fdd_enb_gw                             *gw = (LTE_fdd_enb_gw *)inputs;
    LTE_FDD_ENB_PDCP_DATA_SDU_READY_MSG_STRUCT  pdcp_data_sdu;
    LIBLTE_BYTE_MSG_STRUCT                      msg;
    struct iphdr                                ipv4_pkt;
    cpu_set_t                                   af_mask;
    uint32                                      idx      = 0;
    uint32                                      num_cpus = std::thread::hardware_concurrency();
    int32                                       N_bytes;

    // Set affinity to the third to the last core (last is for PHY/Radio, second to last is for MAC)
    CPU_ZERO(&af_mask);
    CPU_SET(num_cpus-3, &af_mask);
    pthread_setaffinity_np(gw->rx_thread, sizeof(af_mask), &af_mask);

    while(gw->is_started())
    {
        N_bytes = read(gw->tun_fd, &msg.msg[idx], LIBLTE_MAX_MSG_SIZE);

        if(N_bytes <= 0)
            break; // Something bad has happened

        msg.N_bytes = idx + N_bytes;
        if(0x60 == (msg.msg[0] & 0xF0))
        {
            // Discard IPv6 packet
            if(msg.N_bytes == ((msg.msg[4]<<8) + msg.msg[5]) + sizeof(ip6_hdr))
            {
                idx = 0;
            }else{
                idx += N_bytes;
            }
            continue;
        }
        memcpy(&ipv4_pkt, msg.msg, sizeof(iphdr));

        // Check if entire packet was received
        if(ntohs(ipv4_pkt.tot_len) != msg.N_bytes)
        {
            idx = N_bytes;
            continue;
        }

        // Find user and rb
        if(LTE_FDD_ENB_ERROR_NONE == gw->user_mgr->find_user(ntohl(ipv4_pkt.daddr), &pdcp_data_sdu.user) &&
           LTE_FDD_ENB_ERROR_NONE == pdcp_data_sdu.user->get_drb(LTE_FDD_ENB_RB_DRB1, &pdcp_data_sdu.rb))
        {
            gw->interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                          LTE_FDD_ENB_DEBUG_LEVEL_GW,
                                          __FILE__,
                                          __LINE__,
                                          &msg,
                                          "Received IP packet for RNTI=%u and RB=%s",
                                          pdcp_data_sdu.user->get_c_rnti(),
                                          LTE_fdd_enb_rb_text[pdcp_data_sdu.rb->get_rb_id()]);
            gw->interface->send_ip_pcap_msg(msg.msg, msg.N_bytes);

            // Send message to PDCP
            pdcp_data_sdu.rb->queue_pdcp_data_sdu(&msg);
            gw->msgq_to_pdcp->send(LTE_FDD_ENB_MESSAGE_TYPE_PDCP_DATA_SDU_READY,
                                   LTE_FDD_ENB_DEST_LAYER_PDCP,
                                   (LTE_FDD_ENB_MESSAGE_UNION *)&pdcp_data_sdu,
                                   sizeof(LTE_FDD_ENB_PDCP_DATA_SDU_READY_MSG_STRUCT));
        }
            
        idx = 0;
    }

    return NULL;
}
