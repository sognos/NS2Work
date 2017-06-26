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
#include "dymo_timeout.h"
#include "dymo_re.h"
#include "dymo_netlink.h"
#include "pending_rreq.h"
#include "rtable.h"
#include "blacklist.h"
#include "dymo_nb.h"
#include "debug.h"


extern int reissue_rreq;
#endif	/* NS_PORT */

void NS_CLASS route_valid_timeout(void *arg)
{
	rtable_entry_t *entry = (rtable_entry_t *) arg;
	
	if (!entry)
	{
		dlog(LOG_WARNING, 0, __FUNCTION__,
			"NULL routing table entry, ignoring timeout");
		return;
	}
	
	rtable_invalidate(entry);
}

void NS_CLASS route_del_timeout(void *arg)
{
	rtable_entry_t *entry = (rtable_entry_t *) arg;
	
	if (!entry)
	{
		dlog(LOG_WARNING, 0, __FUNCTION__,
			"NULL routing table entry, ignoring timeout");
		return;
	}
	
	//if (entry->rt_state == RT_INVALID) // I think this isn't needed
		rtable_delete(entry);
}

void NS_CLASS blacklist_timeout(void *arg)
{
	blacklist_t *entry = (blacklist_t *) arg;
	
	if (!entry)
	{
		dlog(LOG_WARNING, 0, __FUNCTION__,
			"NULL blacklist entry, ignoring timeout");
		return;
	}
	
	blacklist_remove(entry);
}

void NS_CLASS route_discovery_timeout(void *arg)
{
	pending_rreq_t *entry = (pending_rreq_t *) arg;
	
	
	//initialisé le nbre de RREP received 
	this_host.NbRREP		= 0;   
	this_host.MaxSeq              = 0;      /* ini Max Sq nb received */
	this_host.MinSeq              = 65535;  /* ini Min Sq Nb Received */

	RREQ_remove();                          // Effacer les Padding REREQs//
	rtable_destroyRREP();                   //Effacer les RREP sauvgarder
	
	if (!entry)
	{
		dlog(LOG_WARNING, 0, __FUNCTION__,"NULL pending route discovery list entry, ignoring timeout");		
		return;
	}
	
	if (reissue_rreq)
	{
		if (entry->tries < RREQ_TRIES)
        {
            rtable_entry_t *rte;

            entry->tries++;
            timer_set_timeout(&entry->timer,RREQ_WAIT_TIME << entry->tries);
            timer_add(&entry->timer);

            rte = rtable_find(entry->dest_addr);
            if (rte)
                re_send_rreq(entry->dest_addr, entry->seqnum,rte->rt_hopcnt);
            else
                re_send_rreq(entry->dest_addr, entry->seqnum,0);

            return;
        }
	}

	#ifdef NS_PORT
		packet_queue_set_verdict(entry->dest_addr, PQ_DROP);
	#else
		netlink_no_route_found(entry->dest_addr);
	#endif	/* NS_PORT */

	pending_rreq_remove(entry);
	
	
}

void NS_CLASS nb_timeout(void *arg)
{
	nb_t *nb = (nb_t *) arg;
	
	if (!nb)
	{
		dlog(LOG_WARNING, 0, __FUNCTION__,
			"NULL nblist entry, ignoring timeout");
		return;
	}
	
	// A link break has been detected: Expire all routes utilizing the
	// broken link
	rtable_expire_timeout_all(nb->nb_addr, nb->ifindex);
	nb_remove(nb);
}