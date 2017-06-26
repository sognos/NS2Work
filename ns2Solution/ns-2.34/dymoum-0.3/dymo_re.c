/***************************************************************************
 *   Copyright (C) 2005 by Francisco J. Ros                                *
 *   fjrm@dif.um.es                                                        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#ifdef NS_PORT
#include "ns/dymo_um.h"
#else
#include <string.h>


#include "dymo_re.h"
#include "dymo_generic.h"
#include "dymo_socket.h"
#include "icmp_socket.h"
#include "dymo_timeout.h"
#include "rtable.h"
#include "pending_rreq.h"
#include "blacklist.h"


extern int no_path_acc, s_bit;
#endif	/* NS_PORT */
#include<time.h>
RE *NS_CLASS re_create_rreq(struct in_addr target_addr,
		u_int32_t target_seqnum,
		struct in_addr re_node_addr,
		u_int32_t re_node_seqnum,
		u_int8_t prefix, u_int8_t g,
		u_int8_t ttl, u_int8_t thopcnt)
{
	RE *re;
	
	re		= (RE *) dymo_socket_new_element();
	re->m		= 0;
	re->h		= 0;
	re->type	= DYMO_RE_TYPE;
	re->a		= 1;
	re->s		= 0;
	re->i		= 0;
	re->res1	= 0;
	re->res2	= 0;
	re->ttl		= ttl;
	re->len		= RE_BASIC_SIZE + RE_BLOCK_SIZE;
	re->thopcnt	= thopcnt;
	re->target_addr		= (u_int32_t) target_addr.s_addr;
	re->target_seqnum	= htonl(target_seqnum);
	
	re->re_blocks[0].g		= g;
	re->re_blocks[0].prefix		= prefix;
	re->re_blocks[0].res		= 0;
	re->re_blocks[0].re_hopcnt	= 0;
	re->re_blocks[0].re_node_addr	= (u_int32_t) re_node_addr.s_addr;
	re->re_blocks[0].re_node_seqnum	= htonl(re_node_seqnum);
	RREQ_add(target_addr);
	return re;
}



RE *NS_CLASS re_create_rrep(struct in_addr target_addr,
		u_int32_t target_seqnum,
		struct in_addr re_node_addr,
		u_int32_t re_node_seqnum,
		u_int8_t prefix, u_int8_t g,
		u_int8_t ttl, u_int8_t thopcnt)
{
	RE *re;
	
	re		= (RE *) dymo_socket_new_element();
	re->m		= 0;
	re->h		= 0;
	re->type	= DYMO_RE_TYPE;
	re->a		= 0;
	re->s		= s_bit;
	re->i		= 0;
	re->res1	= 0;
	re->res2	= 0;
	re->ttl		= ttl;
	re->len		= RE_BASIC_SIZE + RE_BLOCK_SIZE;
	re->thopcnt	= thopcnt;
	re->target_addr		= (u_int32_t) target_addr.s_addr;
	re->target_seqnum	= htonl(target_seqnum);
	
	re->re_blocks[0].g		= g;
	re->re_blocks[0].prefix		= prefix;
	re->re_blocks[0].res		= 0;
	re->re_blocks[0].re_hopcnt	= 0;
	re->re_blocks[0].re_node_addr	=(u_int32_t) re_node_addr.s_addr;
	re->re_blocks[0].re_node_seqnum	= htonl(re_node_seqnum);
	
	return re;
}

void NS_CLASS re_process(RE *re, struct in_addr ip_src, u_int32_t ifindex) {//packet reception <data or rrep or rreq>

	struct in_addr node_addr;
	
	rtable_entry_t *entry;
	
	int i;
	
	// Assure that there is a block at least
	if (re_numblocks(re) <= 0)
	{
		dlog(LOG_WARNING, 0, __FUNCTION__, "malformed RE received");
		return;
	}
	
	// Check if the message is a RREQ and the previous hop is blacklisted
	if (re->a && blacklist_find(ip_src)){
		dlog(LOG_DEBUG, 0, __FUNCTION__, "ignoring RREQ because previous hop (%s) is blacklisted", ip2str(ip_src.s_addr));
		return;
	}

	/*
	 * Add route to neighbor
	 *
	 * NOTE: this isn't in the spec. Motivation: suppose that path
	 * accumulation is disabled and the S-bit is active. Then, after
	 * receiving a RREP the node sends an ICMP ECHOREPLY message to
	 * the neighbor. If we don't create this route, it'd be needed
	 * a new route discovery. There are other (finer grain) possible
	 * solutions but this seems to be ok.
	 *
	 */
	entry = rtable_find(ip_src);
	
		
	if (re->s){
		if (!entry)
			rtable_insert(
				ip_src,		// dest
				ip_src,		// nxt hop
				ifindex,	// iface
				0,		// seqnum
				0,		// prefix
				1,		// hop count
				0);		// is gw
		icmp_reply_send(ip_src, &DEV_IFINDEX(ifindex));
	}
	
	if(!re->a && re->target_addr == (u_int32_t) DEV_IFINDEX(ifindex).ipaddr.s_addr){/* remove the false response from stupied blackhole */
		if (re->re_blocks[0].re_node_seqnum > this_host.MaxSeq) this_host.MaxSeq =re->re_blocks[0].re_node_seqnum;
		if (re->re_blocks[0].re_node_seqnum < this_host.MinSeq) this_host.MinSeq =re->re_blocks[0].re_node_seqnum;
		this_host.NbRREP++;

		struct in_addr target_addr;
		u_int32_t target_seqnum;

		node_addr.s_addr	= re->re_blocks[0].re_node_addr;
		target_addr.s_addr	=  (u_int32_t) DEV_IFINDEX(ifindex).ipaddr.s_addr;//re->target_addr;envoyer l'adresse originale sans CRC
		target_seqnum		= ntohl(re->target_seqnum);
		printf("%u) Target Node:%u receive a RREP from: %u, max:%u,min:%u,MOY=%u\n",this_host.NbRREP,(u_int32_t) DEV_IFINDEX(ifindex).ipaddr.s_addr,node_addr.s_addr,this_host.MaxSeq,this_host.MinSeq,(this_host.MaxSeq-this_host.MinSeq)/this_host.NbRREP);

		rtable_insertRREP(
			node_addr,		// dest
			ip_src,			// nxt hop
			ifindex,		// iface
			ntohl(re->re_blocks[0].re_node_seqnum),			// seqnum
			re->re_blocks[0].prefix,		// prefix
			re->re_blocks[0].re_hopcnt,	// hop count
			re->re_blocks[0].g);	

		find_BestRREP(node_addr);
	}
	
	// Process blocks
	for (i = 0; i < re_numblocks(re); i++)
	{
		node_addr.s_addr	= re->re_blocks[i].re_node_addr;
		entry			= rtable_find(node_addr);
		if (re_process_block(&re->re_blocks[i], re->a, entry, ip_src, ifindex,i,re->target_addr == (u_int32_t) DEV_IFINDEX(ifindex).ipaddr.s_addr))
		{
			// stale information: drop packet if first block,
			// drop block otherwise
			if (i == 0)
				return;
			else {
				int n = re_numblocks(re) - i - 1;
				
				memmove(&re->re_blocks[i], &re->re_blocks[i+1],	n * sizeof(struct re_block));
				memset(&re->re_blocks[i + n], 0,sizeof(struct re_block));
				
				re->len -= RE_BLOCK_SIZE;
				i--;
			}
		}
	}

//////check if the request destination exist in my set/////////////////////////////////////////////////////////////////////////////////////////////
	struct in_addr target_addrCRC;
	int random;
	if(re->a){//if RREQ Replace @ by CRC(@);
		//target_addrCRC.s_addr=re->target_addr;
		target_addrCRC.s_addr=rc_crc32(0,re->target_addr);/* replace RREQ destination by its CRC */
		entry = rtable_findGetOriginalAddress(target_addrCRC);/* vérifier l'existance de @original */
		
		if(this_host.BLACKHOLE){
			/* generate a random number between 1 & 0 to as the choice of the black hole type*/
			random =(int)clock();
			printf("random value = %u\n",random);
			random = (random %2); 
			//printf("random = %u\n",random);
		}
	}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	

	
	// If this node is the target, the RE must not be retransmitted
	if (((re->target_addr == (u_int32_t) DEV_IFINDEX(ifindex).ipaddr.s_addr) && (!re->a)) ||(target_addrCRC.s_addr == rc_crc32(0,(u_int32_t) DEV_IFINDEX(ifindex).ipaddr.s_addr))){
		// If A-bit is set, a RE is sent back
		if (re->a)
		{
				
			struct in_addr target_addr;
			u_int32_t target_seqnum;
			
			node_addr.s_addr	= re->re_blocks[0].re_node_addr;
			target_addr.s_addr	=  (u_int32_t) DEV_IFINDEX(ifindex).ipaddr.s_addr;//re->target_addr;envoyer l'adresse originale sans CRC
			target_seqnum		= ntohl(re->target_seqnum);
			
			if (!target_seqnum ||
				((int32_t) target_seqnum) - ((int32_t) this_host.seqnum) > 0 ||
				(target_seqnum == this_host.seqnum && re->thopcnt < re->re_blocks[0].re_hopcnt))
				INC_SEQNUM(this_host.seqnum);
			
			RE *rrep = re_create_rrep(
				node_addr,
				ntohl(re->re_blocks[0].re_node_seqnum),
				target_addr,
				this_host.seqnum,
				this_host.prefix,
				this_host.is_gw,
				NET_DIAMETER,
				re->re_blocks[0].re_hopcnt);
			 //printf("Target Node:%u receive a RREQ from: %u To: %u\n",(u_int32_t) DEV_IFINDEX(ifindex).ipaddr.s_addr,node_addr.s_addr,target_addr.s_addr);
			
			re_send_rrep(rrep);
		}else{
			
			
		}
	}
/////////////////////////Stupied Black hole (renvoyer un rrep directement) ///////////////////////
	else if(re->a && this_host.BLACKHOLE && random==0){

		node_addr.s_addr	= re->re_blocks[0].re_node_addr;
		
		RE *rrep = re_create_rrep(
			node_addr,
			ntohl(re->re_blocks[0].re_node_seqnum),
			target_addrCRC,						//repondre sans chercher dans la table de routage
			this_host.seqnum+1000,    			//ajouter un nombre de séquence tres elevé
			this_host.prefix,
			this_host.is_gw,
			NET_DIAMETER,
			1); 								//diminuer le nombre des hops

		//printf("Black hole:%u receive a RREQ from: %u To: %u\n",(u_int32_t) DEV_IFINDEX(ifindex).ipaddr.s_addr,node_addr.s_addr,target_addr.s_addr);

		re_send_rrep(rrep);     
	}
//////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////// Inteligent BlackHole had a Route to destination ////////////////
	else if(re->a && entry && (ntohl(re->target_seqnum) <= entry->rt_seqnum) && (entry->rt_state==RT_VALID)&&this_host.BLACKHOLE && random==1){
		struct in_addr target_addr;
        node_addr.s_addr    = re->re_blocks[0].re_node_addr;
        target_addr.s_addr  = entry->rt_dest_addr.s_addr;
			
		RE *rrep = re_create_rrep(
				node_addr ,
				ntohl(re->re_blocks[0].re_node_seqnum),
				target_addr,
				(entry->rt_seqnum)+1000,
				entry->rt_prefix,
				entry->rt_is_gw,
				NET_DIAMETER,
				1);
			
		if (!no_path_acc){
			int n = re_numblocks(rrep);
			
			INC_SEQNUM(this_host.seqnum);
			re->re_blocks[n].g		= this_host.is_gw;
			re->re_blocks[n].prefix		= this_host.prefix;
			re->re_blocks[n].res		= 0;
			re->re_blocks[n].re_hopcnt	= 0;
			re->re_blocks[n].re_node_seqnum	= htonl(this_host.seqnum);
			re->re_blocks[n].re_node_addr	= (u_int32_t) DEV_IFINDEX(ifindex).ipaddr.s_addr;
			
			re->len += RE_BLOCK_SIZE;
		}
		
		if(entry->rt_hopcnt)
			rrep->re_blocks[0].re_hopcnt    =entry->rt_hopcnt;	
		
		re_send_rrep(rrep);
	
		
		rrep = re_create_rrep(
				target_addr,
				entry->rt_seqnum,
				node_addr ,
				ntohl(re->re_blocks[0].re_node_seqnum),
				re->re_blocks[0].prefix,
				re->re_blocks[0].g,
				NET_DIAMETER,
				re->re_blocks[0].re_hopcnt+entry->rt_hopcnt);
		
		if (!no_path_acc){
			int n = re_numblocks(rrep);
			
			INC_SEQNUM(this_host.seqnum);
			re->re_blocks[n].g		= this_host.is_gw;
			re->re_blocks[n].prefix		= this_host.prefix;
			re->re_blocks[n].res		= 0;
			re->re_blocks[n].re_hopcnt	= 0;
			re->re_blocks[n].re_node_seqnum	= htonl(this_host.seqnum);
			re->re_blocks[n].re_node_addr	= (u_int32_t) DEV_IFINDEX(ifindex).ipaddr.s_addr;
			
			re->len += RE_BLOCK_SIZE;
		}
		
		rrep->re_blocks[0].re_hopcnt    =0;
		
		re_send_rrep(rrep);	
		
		printf("%u receive a RREQ to %u \n",(u_int32_t) DEV_IFINDEX(ifindex).ipaddr.s_addr,re->target_addr);
		
	}
//////////////////////////////////////////////////////////////////////////////////////////////////
	
	// Node have a Route to destination
	else if(re->a && entry && (ntohl(re->target_seqnum) <= entry->rt_seqnum) && (entry->rt_state==RT_VALID)){
		struct in_addr target_addr;
        node_addr.s_addr    = re->re_blocks[0].re_node_addr;
        target_addr.s_addr  = entry->rt_dest_addr.s_addr;
			
		RE *rrep = re_create_rrep(
				node_addr ,
				ntohl(re->re_blocks[0].re_node_seqnum),
				target_addr,
				entry->rt_seqnum,
				entry->rt_prefix,
				entry->rt_is_gw,
				NET_DIAMETER,
				re->re_blocks[0].re_hopcnt+entry->rt_hopcnt);
			
		if (!no_path_acc){
			int n = re_numblocks(rrep);
			
			INC_SEQNUM(this_host.seqnum);
			re->re_blocks[n].g		= this_host.is_gw;
			re->re_blocks[n].prefix		= this_host.prefix;
			re->re_blocks[n].res		= 0;
			re->re_blocks[n].re_hopcnt	= 0;
			re->re_blocks[n].re_node_seqnum	= htonl(this_host.seqnum);
			re->re_blocks[n].re_node_addr	= (u_int32_t) DEV_IFINDEX(ifindex).ipaddr.s_addr;
			
			re->len += RE_BLOCK_SIZE;
		}
		
		if(entry->rt_hopcnt)
			rrep->re_blocks[0].re_hopcnt    =entry->rt_hopcnt;	
		
		re_send_rrep(rrep);
	
		
		rrep = re_create_rrep(
				target_addr,
				entry->rt_seqnum,
				node_addr ,
				ntohl(re->re_blocks[0].re_node_seqnum),
				re->re_blocks[0].prefix,
				re->re_blocks[0].g,
				NET_DIAMETER,
				re->re_blocks[0].re_hopcnt+entry->rt_hopcnt);
		
		if (!no_path_acc){
			int n = re_numblocks(rrep);
			
			INC_SEQNUM(this_host.seqnum);
			re->re_blocks[n].g		= this_host.is_gw;
			re->re_blocks[n].prefix		= this_host.prefix;
			re->re_blocks[n].res		= 0;
			re->re_blocks[n].re_hopcnt	= 0;
			re->re_blocks[n].re_node_seqnum	= htonl(this_host.seqnum);
			re->re_blocks[n].re_node_addr	= (u_int32_t) DEV_IFINDEX(ifindex).ipaddr.s_addr;
			
			re->len += RE_BLOCK_SIZE;
		}
		
		rrep->re_blocks[0].re_hopcnt    =0;
		
		re_send_rrep(rrep);	
		
		//printf("%u receive a RREQ to %u \n",(u_int32_t) DEV_IFINDEX(ifindex).ipaddr.s_addr,re->target_addr);
		
	}
	/// Otherwise the RE is considered to be forwarded
	else if (generic_postprocess((DYMO_element *) re))	{
		if (!no_path_acc)
		{
			int n = re_numblocks(re);
			
			INC_SEQNUM(this_host.seqnum);
			re->re_blocks[n].g		= this_host.is_gw;
			re->re_blocks[n].prefix		= this_host.prefix;
			re->re_blocks[n].res		= 0;
			re->re_blocks[n].re_hopcnt	= 0;
			re->re_blocks[n].re_node_seqnum	= htonl(this_host.seqnum);
			
			re->len += RE_BLOCK_SIZE;
			
			// If this is a RREQ
			if (re->a)
				re_forward_rreq_path_acc(re, n);
			// Else if this is a RREP
			else
			{
				re->re_blocks[n].re_node_addr	= (u_int32_t) DEV_IFINDEX(ifindex).ipaddr.s_addr;
				re_forward_rrep_path_acc(re);
			}
		}
		else
			re_forward(re);
	}
}

int NS_CLASS re_process_block(struct re_block *block, u_int8_t is_rreq,rtable_entry_t *entry, struct in_addr ip_src, u_int32_t ifindex,int i,bool amReceiver){
	struct in_addr dest_addr;
	u_int32_t seqnum;
	int rb_state;
	
	if (!block)
		return -1;
	
	dest_addr.s_addr	= block->re_node_addr;
	seqnum			= ntohl(block->re_node_seqnum);
	
	// Increment block hop count
	block->re_hopcnt++;
	
	rb_state = re_info_type(block, entry, is_rreq);
	if (rb_state != RB_FRESH)
	{
		dlog(LOG_DEBUG, 0, __FUNCTION__, "ignoring a %s RE block",
			(rb_state == RB_STALE ? "stale" : (rb_state == RB_LOOP_PRONE ?
			"loop-prone" : (rb_state == RB_INFERIOR ? "inferior" :
			"self-generated"))));
		return -1;
	}
	
	if(!is_rreq && (this_host.NbRREP>0) && (((this_host.MaxSeq-this_host.MinSeq)/this_host.NbRREP)>1)&& i==0 && amReceiver){/* test for the threshold */
		printf("Threshold passed\n");
		this_host.MaxSeq              = 0; 
	}else{

		// Create/update a route towards RENodeAddress
		if (entry){
			rtable_update(
				entry,			// routing table entry
				dest_addr,		// dest
				ip_src,			// nxt hop
				ifindex,		// iface
				seqnum,			// seqnum
				block->prefix,		// prefix
				block->re_hopcnt,	// hop count
				block->g);		// is gw
			//printf("update root from %u to %u seq: %u\n",(u_int32_t) DEV_IFINDEX(ifindex).ipaddr.s_addr,dest_addr.s_addr,seqnum);
		}
		else{
			rtable_insert(
				dest_addr,		// dest
				ip_src,			// nxt hop
				ifindex,		// iface
				seqnum,			// seqnum
				block->prefix,		// prefix
				block->re_hopcnt,	// hop count
				block->g);		// is gw
			//printf("insert root from %u to %u seq: %u\n",(u_int32_t) DEV_IFINDEX(ifindex).ipaddr.s_addr,dest_addr.s_addr,seqnum);
		}
	}
	
	return 0;
}

void NS_CLASS __re_send(RE *re){
	struct in_addr dest_addr;
	int i;
	
	// If it is a RREQ
	if (re->a)
	{
		dest_addr.s_addr = DYMO_BROADCAST;
	
		// Queue the new RE
		re = (RE *) dymo_socket_queue((DYMO_element *) re);
	
		// Send RE over all enabled interfaces
		for (i = 0; i < DYMO_MAX_NR_INTERFACES; i++)
			if (DEV_NR(i).enabled)
				dymo_socket_send(dest_addr, &DEV_NR(i));
	}
	// Else if RREP
	else
	{
		dest_addr.s_addr = re->target_addr;
		rtable_entry_t *entry = rtable_find(dest_addr);
		if (entry && entry->rt_state == RT_VALID)
		{
			dest_addr.s_addr = entry->rt_nxthop_addr.s_addr;
			
			// Queue the new RE
			re = (RE *) dymo_socket_queue((DYMO_element *) re);
			
			// Send RE over appropiate interface
			if (DEV_IFINDEX(entry->rt_ifindex).enabled)
				dymo_socket_send(dest_addr, &DEV_IFINDEX(entry->rt_ifindex));
			
			// Add next hop to the blacklist until we receive a
			// unicast message from it
			if (re->s)
			{
				blacklist_t *blacklist= blacklist_add(dest_addr);
				
				timer_init(&blacklist->timer, &NS_CLASS blacklist_timeout,blacklist);
				timer_set_timeout(&blacklist->timer, BLACKLIST_TIMEOUT);
				timer_add(&blacklist->timer);
			}
		}
	}
}

void NS_CLASS re_send_rrep(RE *rrep){
	dlog(LOG_DEBUG, 0, __FUNCTION__, "sending RREP to %s",ip2str(rrep->target_addr));
	__re_send(rrep);
}

void NS_CLASS re_send_rreq(struct in_addr dest_addr, u_int32_t seqnum,	u_int8_t thopcnt){
	int i;
	RE *rreq;
	struct in_addr bcast_addr;
	
	dlog(LOG_DEBUG, 0, __FUNCTION__, "sending RREQ to find %s",
		ip2str(dest_addr.s_addr));
	
	bcast_addr.s_addr = DYMO_BROADCAST;
	
	INC_SEQNUM(this_host.seqnum);
	for (i = 0; i < DYMO_MAX_NR_INTERFACES; i++)
	{
		if (DEV_NR(i).enabled)
		{
			rreq = re_create_rreq(dest_addr, seqnum,
				DEV_NR(i).ipaddr, this_host.seqnum,
				this_host.prefix, this_host.is_gw,
				NET_DIAMETER, thopcnt);
			
			dymo_socket_queue((DYMO_element *) rreq);
			dymo_socket_send(bcast_addr, &DEV_NR(i));
		}
	}
}

void NS_CLASS re_forward(RE *re){
	if (re->a)
		dlog(LOG_DEBUG, 0, __FUNCTION__, "forwarding RREQ to find %s",
			ip2str(re->target_addr));
	else
		dlog(LOG_DEBUG, 0, __FUNCTION__, "forwarding RREP to %s",
			ip2str(re->target_addr));
	__re_send(re);
}

void NS_CLASS re_forward_rreq_path_acc(RE *rreq, int blindex){
	int i;
	struct in_addr bcast_addr;
	
	dlog(LOG_DEBUG, 0, __FUNCTION__, "forwarding RREQ to find %s",
		ip2str(rreq->target_addr));
	
	bcast_addr.s_addr = DYMO_BROADCAST;
	
	for (i = 0; i < DYMO_MAX_NR_INTERFACES; i++)
	{
		if (DEV_NR(i).enabled)
		{
			rreq->re_blocks[blindex].re_node_addr =
				(u_int32_t) DEV_NR(i).ipaddr.s_addr;
			
			dymo_socket_queue((DYMO_element *) rreq);
			dymo_socket_send(bcast_addr, &DEV_NR(i));
		}
	}
}

void NS_CLASS re_forward_rrep_path_acc(RE *rrep){
	dlog(LOG_DEBUG, 0, __FUNCTION__, "forwarding RREP to %s",
		ip2str(rrep->target_addr));
	__re_send(rrep);
}

void NS_CLASS route_discovery(struct in_addr dest_addr){
	u_int32_t	seqnum;
	u_int8_t	thopcnt;
	
	// If we are already doing a route discovery for dest_addr,
	// then simply return
	if (pending_rreq_find(dest_addr))
		return;
	
	// Get info from routing table (if there exists an entry)
	rtable_entry_t *rt_entry = rtable_find(dest_addr);
	if (rt_entry)
	{
		seqnum	= rt_entry->rt_seqnum;
		thopcnt	= rt_entry->rt_hopcnt;
	}
	else
	{
		seqnum	= 0;
		thopcnt	= 0;
	}
	
	// Send a RREQ
	re_send_rreq(dest_addr, seqnum, thopcnt);
	
	// Record information for destination and set a timer
	pending_rreq_t *pend_rreq = pending_rreq_add(dest_addr, seqnum);
	timer_init(&pend_rreq->timer, &NS_CLASS route_discovery_timeout,pend_rreq);
	timer_set_timeout(&pend_rreq->timer, RREQ_WAIT_TIME);
	timer_add(&pend_rreq->timer);
}

//////////////////////////////////////////CRC 32 Calc////////////////////////////////////////
u_int32_t NS_CLASS rc_crc32(u_int32_t crc, const u_int32_t val){
    char aux[32];
    sprintf(aux,"%u", val);
    size_t len= strlen(aux);
    static u_int32_t table[256];
    static int have_table = 0;
    u_int32_t rem;
    u_int8_t octet;
    int i, j;
    const char *p, *q;

    /* This check is not thread safe; there is no mutex. */
    if (have_table == 0) {
            /* Calculate CRC table. */
            for (i = 0; i < 256; i++) {
                    rem = i;  /* remainder from polynomial division */
                    for (j = 0; j < 8; j++) {
                            if (rem & 1) {
                                    rem >>= 1;
                                    rem ^= 0xedb88320;
                            } else
                                    rem >>= 1;
                    }
                    table[i] = rem;
            }
            have_table = 1;
    }

    crc = ~crc;
    q = aux + len;
    for (p = aux; p < q; p++) {
            octet = *p;  /* Cast to unsigned octet. */
            crc = (crc >> 8) ^ table[(crc & 0xff) ^ octet];
    }
    return ~crc;
}
/////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////Get The @ of the CRC/////////////////////////////////////
rtable_entry_t *NS_CLASS rtable_findGetOriginalAddress(struct in_addr dest_addr){
	dlist_head_t *pos;
	
	dlist_for_each(pos, &rtable.l)
	{
		rtable_entry_t *e = (rtable_entry_t *) pos;
		
		if (rc_crc32(0,e->rt_dest_addr.s_addr) == dest_addr.s_addr){
			return e;
		}
	}
	return NULL;
}
/////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////Insert received RREPs/////////////////////////////////////
rtable_entry_t *NS_CLASS rtable_insertRREP(struct in_addr dest_addr,	struct in_addr nxthop_addr,	u_int32_t ifindex,u_int32_t seqnum,	u_int8_t prefix,u_int8_t hopcnt,u_int8_t is_gw){
	rtable_entry_t *entry;
	
	// Create the new entry
	if ((entry = (rtable_entry_t *) malloc(sizeof(rtable_entry_t)))	== NULL)
	{
		exit(EXIT_FAILURE);
	}
	memset(entry, 0, sizeof(rtable_entry_t));
	
	entry->rt_ifindex	= ifindex;
	entry->rt_seqnum	= seqnum;
	entry->rt_prefix	= prefix;
	entry->rt_hopcnt	= hopcnt;
	entry->rt_is_gw		= is_gw;
	entry->rt_is_used	= 0;
	entry->rt_state		= RT_VALID;
	entry->rt_dest_addr.s_addr	= dest_addr.s_addr;
	entry->rt_nxthop_addr.s_addr	= nxthop_addr.s_addr;
	
	
	// Add the entry to the routing table
	dlist_add(&entry->l, &receivedRREP.l);

	return entry;
}
/////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////Destroy the RREP store table///////////////////////////////
void NS_CLASS rtable_destroyRREP(){
	
	dlist_head_t *pos, *tmp;
	
	dlist_for_each_safe(pos, tmp, &receivedRREP.l)
	{
		rtable_entry_t *e = (rtable_entry_t *) pos;
		if (!e)
			return;
		dlist_del(&e->l);
		free(e);
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////Get the Best Route From RREP Table//////////////////////////
rtable_entry_t *NS_CLASS find_BestRREP(struct in_addr dest_addr){
	dlist_head_t *pos;
	
	dlist_for_each(pos, &rtable.l)
	{
		rtable_entry_t *e = (rtable_entry_t *) pos;
		/*if (e->rt_dest_addr.s_addr == dest_addr.s_addr)
			return e;*/
		printf("RREP TABLE: %u\n",e->rt_dest_addr.s_addr);
	}
	return NULL;
}
/////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////Insert sent RREQs/////////////////////////////////////////
rtable_entry_t *NS_CLASS RREQ_add(struct in_addr dest_addr){
	rtable_entry_t *entry;
	
	// Create the new entry
	if ((entry = (rtable_entry_t *) malloc(sizeof(rtable_entry_t)))	== NULL)
	{
		exit(EXIT_FAILURE);
	}
	memset(entry, 0, sizeof(rtable_entry_t));
	
	entry->rt_dest_addr.s_addr	= dest_addr.s_addr;
	
	
	// Add the entry to the routing table
	dlist_add(&entry->l, &sentRREQ.l);

	return entry;
}
/////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////Destroy the RREQ store table///////////////////////////////
void NS_CLASS RREQ_remove(){
	
	dlist_head_t *pos, *tmp;
	
	dlist_for_each_safe(pos, tmp, &sentRREQ.l)
	{
		rtable_entry_t *e = (rtable_entry_t *) pos;
		if (!e)
			return;
		dlist_del(&e->l);
		free(e);
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////

///////////////CHECK A RREP IS VALID OR NOT (is there any RREQ sent to received reply)///////
rtable_entry_t *NS_CLASS RREQ_find(struct in_addr dest_addr){
	dlist_head_t *pos;
	
	dlist_for_each(pos, &sentRREQ.l)
	{
		rtable_entry_t *e = (rtable_entry_t *) pos;
		if (e->rt_dest_addr.s_addr == dest_addr.s_addr)
			return e;
		//printf("RREP TABLE: %u\n",e->rt_dest_addr.s_addr);
	}
	return NULL;
}
///////////////////////////////////////////////////////////////////////////////////////////