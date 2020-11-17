/* Peer event handling, typically ICMP messages.
 *
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/net.h>
#include <linux/skbuff.h>
#include <linux/errqueue.h>
#include <linux/udp.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/icmp.h>
#include <net/sock.h>
#include <net/af_rxrpc.h>
#include <net/ip.h>
#include "ar-internal.h"

static void rxrpc_store_error(struct rxrpc_peer *, struct sock_exterr_skb *);

/*
 * Find the peer associated with an ICMP packet.
 */
static struct rxrpc_peer *rxrpc_lookup_peer_icmp_rcu(struct rxrpc_local *local,
						     const struct sk_buff *skb)
{
	struct sock_exterr_skb *serr = SKB_EXT_ERR(skb);
	struct sockaddr_rxrpc srx;

	_enter("");

	memset(&srx, 0, sizeof(srx));
	srx.transport_type = local->srx.transport_type;
	srx.transport.family = local->srx.transport.family;

	/* Can we see an ICMP4 packet on an ICMP6 listening socket?  and vice
	 * versa?
	 */
	switch (srx.transport.family) {
	case AF_INET:
		srx.transport.sin.sin_port = serr->port;
		srx.transport_len = sizeof(struct sockaddr_in);
		switch (serr->ee.ee_origin) {
		case SO_EE_ORIGIN_ICMP:
			_net("Rx ICMP");
			memcpy(&srx.transport.sin.sin_addr,
			       skb_network_header(skb) + serr->addr_offset,
			       sizeof(struct in_addr));
			break;
		case SO_EE_ORIGIN_ICMP6:
			_net("Rx ICMP6 on v4 sock");
			memcpy(&srx.transport.sin.sin_addr,
			       skb_network_header(skb) + serr->addr_offset + 12,
			       sizeof(struct in_addr));
			break;
		default:
			memcpy(&srx.transport.sin.sin_addr, &ip_hdr(skb)->saddr,
			       sizeof(struct in_addr));
			break;
		}
		break;

	default:
		BUG();
	}

	return rxrpc_lookup_peer_rcu(local, &srx);
}

/*
 * Handle an MTU/fragmentation problem.
 */
static void rxrpc_adjust_mtu(struct rxrpc_peer *peer, struct sock_exterr_skb *serr)
{
	u32 mtu = serr->ee.ee_info;

	_net("Rx ICMP Fragmentation Needed (%d)", mtu);

	/* wind down the local interface MTU */
	if (mtu > 0 && peer->if_mtu == 65535 && mtu < peer->if_mtu) {
		peer->if_mtu = mtu;
		_net("I/F MTU %u", mtu);
	}

	if (mtu == 0) {
		/* they didn't give us a size, estimate one */
		mtu = peer->if_mtu;
		if (mtu > 1500) {
			mtu >>= 1;
			if (mtu < 1500)
				mtu = 1500;
		} else {
			mtu -= 100;
			if (mtu < peer->hdrsize)
				mtu = peer->hdrsize + 4;
		}
	}

	if (mtu < peer->mtu) {
		spin_lock_bh(&peer->lock);
		peer->mtu = mtu;
		peer->maxdata = peer->mtu - peer->hdrsize;
		spin_unlock_bh(&peer->lock);
		_net("Net MTU %u (maxdata %u)",
		     peer->mtu, peer->maxdata);
	}
}

/*
 * Handle an error received on the local endpoint.
 */
void rxrpc_error_report(struct sock *sk)
{
	struct sock_exterr_skb *serr;
	struct rxrpc_local *local = sk->sk_user_data;
	struct rxrpc_peer *peer;
	struct sk_buff *skb;

	_enter("%p{%d}", sk, local->debug_id);

	skb = sock_dequeue_err_skb(sk);
	if (!skb) {
		_leave("UDP socket errqueue empty");
		return;
	}
	serr = SKB_EXT_ERR(skb);
	if (!skb->len && serr->ee.ee_origin == SO_EE_ORIGIN_TIMESTAMPING) {
		_leave("UDP empty message");
		kfree_skb(skb);
		return;
	}

	rxrpc_new_skb(skb);

	rcu_read_lock();
	peer = rxrpc_lookup_peer_icmp_rcu(local, skb);
	if (peer && !rxrpc_get_peer_maybe(peer))
		peer = NULL;
	if (!peer) {
		rcu_read_unlock();
		rxrpc_free_skb(skb);
		_leave(" [no peer]");
		return;
	}

	if ((serr->ee.ee_origin == SO_EE_ORIGIN_ICMP &&
	     serr->ee.ee_type == ICMP_DEST_UNREACH &&
	     serr->ee.ee_code == ICMP_FRAG_NEEDED)) {
		rxrpc_adjust_mtu(peer, serr);
		rcu_read_unlock();
		rxrpc_free_skb(skb);
		rxrpc_put_peer(peer);
		_leave(" [MTU update]");
		return;
	}

	rxrpc_store_error(peer, serr);
	rcu_read_unlock();
	rxrpc_free_skb(skb);

	/* The ref we obtained is passed off to the work item */
	rxrpc_queue_work(&peer->error_distributor);
	_leave("");
}

/*
 * Map an error report to error codes on the peer record.
 */
static void rxrpc_store_error(struct rxrpc_peer *peer,
			      struct sock_exterr_skb *serr)
{
	struct sock_extended_err *ee;
	int err;

	_enter("");

	ee = &serr->ee;

	_net("Rx Error o=%d t=%d c=%d e=%d",
	     ee->ee_origin, ee->ee_type, ee->ee_code, ee->ee_errno);

	err = ee->ee_errno;

	switch (ee->ee_origin) {
	case SO_EE_ORIGIN_ICMP:
		switch (ee->ee_type) {
		case ICMP_DEST_UNREACH:
			switch (ee->ee_code) {
			case ICMP_NET_UNREACH:
				_net("Rx Received ICMP Network Unreachable");
				break;
			case ICMP_HOST_UNREACH:
				_net("Rx Received ICMP Host Unreachable");
				break;
			case ICMP_PORT_UNREACH:
				_net("Rx Received ICMP Port Unreachable");
				break;
			case ICMP_NET_UNKNOWN:
				_net("Rx Received ICMP Unknown Network");
				break;
			case ICMP_HOST_UNKNOWN:
				_net("Rx Received ICMP Unknown Host");
				break;
			default:
				_net("Rx Received ICMP DestUnreach code=%u",
				     ee->ee_code);
				break;
			}
			break;

		case ICMP_TIME_EXCEEDED:
			_net("Rx Received ICMP TTL Exceeded");
			break;

		default:
			_proto("Rx Received ICMP error { type=%u code=%u }",
			       ee->ee_type, ee->ee_code);
			break;
		}
		break;

	case SO_EE_ORIGIN_NONE:
	case SO_EE_ORIGIN_LOCAL:
		_proto("Rx Received local error { error=%d }", err);
		err += RXRPC_LOCAL_ERROR_OFFSET;
		break;

	case SO_EE_ORIGIN_ICMP6:
	default:
		_proto("Rx Received error report { orig=%u }", ee->ee_origin);
		break;
	}

	peer->error_report = err;
}

/*
 * Distribute an error that occurred on a peer
 */
void rxrpc_peer_error_distributor(struct work_struct *work)
{
	struct rxrpc_peer *peer =
		container_of(work, struct rxrpc_peer, error_distributor);
	struct rxrpc_call *call;
	int error_report;

	_enter("");

	error_report = READ_ONCE(peer->error_report);

	_debug("ISSUE ERROR %d", error_report);

	spin_lock_bh(&peer->lock);

	while (!hlist_empty(&peer->error_targets)) {
		call = hlist_entry(peer->error_targets.first,
				   struct rxrpc_call, error_link);
		hlist_del_init(&call->error_link);

		write_lock(&call->state_lock);
		if (call->state != RXRPC_CALL_COMPLETE &&
		    call->state < RXRPC_CALL_NETWORK_ERROR) {
			call->error_report = error_report;
			call->state = RXRPC_CALL_NETWORK_ERROR;
			set_bit(RXRPC_CALL_EV_RCVD_ERROR, &call->events);
			rxrpc_queue_call(call);
		}
		write_unlock(&call->state_lock);
	}

	spin_unlock_bh(&peer->lock);

	rxrpc_put_peer(peer);
	_leave("");
}
