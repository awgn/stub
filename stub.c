/*
 * Copyright (c) 2007 - Nicola Bonelli <bonelli@antifork.org> 
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL").
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* stub.c: a stub network driver

   The purpose of this network driver is to provide a device to point a
   route through and forward packets to a master device.
   For a correct functioning consider to set up convenient
   policy routes.

 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/rtnetlink.h>
#include <linux/if_arp.h>    /* ARPHRD_ETHER */
#include <linux/errno.h>     /* error codes */
#include <linux/inetdevice.h>
#include <linux/ip.h>

#include <net/neighbour.h>

MODULE_LICENSE("Dual BSD/GPL");

extern struct neigh_ops arp_broken_ops;

char *master;			/* device name */
int stubs = 1;
int debug = 0;

struct net_device *dev_master;
struct net_device **dev_stub;

/* Number of stub devices to be set up by this module. */
module_param(stubs, int, 0);
MODULE_PARM_DESC(stubs, "Number of stub pseudo devices");

module_param(master, charp, 0);
MODULE_PARM_DESC(master, "Physical adapter to which attach the stub");

module_param(debug, int, 0);
MODULE_PARM_DESC(debug, "debug messages on traversing sk_buffs");

int stub_xmit(struct sk_buff *skb, struct net_device *dev);
int stub_skb_recv(struct sk_buff *skb, struct net_device *dev, struct packet_type* ptype, struct net_device *orig_dev);
struct net_device_stats *stub_get_stats(struct net_device *dev);

struct packet_type stub_packet_type = {
    .type = __constant_htons(ETH_P_IP),
    .func = stub_skb_recv, 			/* VLAN receive method */
};

/* demux handler: incoming sk_buff from master device are 
 * diverted to the proper stub, according to their destination ip address. 
 */

int stub_skb_recv(struct sk_buff *skb, struct net_device *dev,
                  struct packet_type* ptype, struct net_device *orig_dev)
{
    struct net_device *d;
    struct in_device  *in;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)        
    if (skb->nh.iph == NULL) {
        return 0;
    }
#else
    if (skb_network_header(skb) == NULL) {
        return 0;
    }
#endif
    read_lock(&dev_base_lock);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)        
    for ( d = dev_base; d != NULL; d=d->next) {
#else
    for ( d = first_net_device(&init_net); d != NULL;  d = next_net_device(d) ) {        
#endif
        struct iphdr *iph;

        if ( !netif_running(d) ) {
            continue;
        }
        if ( d->ip_ptr == NULL ) {
            continue;
        }

        in = (struct in_device *)d->ip_ptr;
        if (in->ifa_list == NULL) {
            continue;
        }

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
        iph = (struct iphdr *)skb->nh.iph;
#else
        iph = (struct iphdr *)skb_network_header(skb);
#endif
        if (iph->daddr == in->ifa_list->ifa_address) {
            if (debug)
                printk(KERN_DEBUG "stub_skb_recv: %lu: <- %s (host)\n",jiffies,d->name);

            skb->dev = d; /* sent to skb->dev */
            break;
        }

        if ((iph->daddr & in->ifa_list->ifa_mask) == 
            (in->ifa_list->ifa_address & in->ifa_list->ifa_mask) ) {
            if (debug)
                printk(KERN_DEBUG "stub_skb_recv: %lu: <- %s (net)\n",jiffies,d->name);
            skb->dev = d; /* sent to skb->dev as destination network */
            continue;
        } 
    }

    read_unlock(&dev_base_lock);
    kfree_skb(skb);

    return 0;
}

int stub_xmit(struct sk_buff *skb, struct net_device *dev)
{
    struct net_device_stats *stats = netdev_priv(dev);

    stats->tx_packets++;
    stats->tx_bytes+=skb->len;

    if (debug)
        printk(KERN_DEBUG "%lu:   %s ->\n",jiffies, skb->dev->name);

    // skb->protocol = eth_type_trans(skb,dev);
    // skb->priority = 1;

    skb->dev = dev_master;
    
    dev_queue_xmit(skb); 
    return 0;
}


/* neighbors: this comes form shaper.c (Alan Cox) and is needed for ARP to work
 */

int stub_neigh_setup(struct neighbour *n)
{
    if (n->nud_state == NUD_NONE) {
        n->ops = &arp_broken_ops;
        n->output = n->ops->output;
    }
    return 0;
}


int stub_neigh_setup_dev(struct net_device *dev, struct neigh_parms *p)
{
    if (p->tbl->family == AF_INET) {
        p->neigh_setup = stub_neigh_setup;
        p->ucast_probes = 0;
        p->mcast_probes = 0;
    }
    return 0;
}


int stub_set_address(struct net_device *dev, void *p)
{
    struct sockaddr *sa = p;

    if (!is_valid_ether_addr(sa->sa_data)) 
        return -EADDRNOTAVAIL;

    memcpy(dev->dev_addr, sa->sa_data, ETH_ALEN);
    return 0;
}


int stub_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
    /* stub interface has no ioctl */
    return 0;
}


int stub_open(struct net_device *dev)
{
    netif_start_queue(dev);
    return 0;
}


int stub_close(struct net_device *dev)
{
    netif_stop_queue(dev);
    return 0;
}


int stub_hard_header(struct sk_buff *skb, struct net_device *dev,
                     unsigned short type, const void *daddr, const void *saddr, unsigned len)
{
    int ret;

    if (dev_master->header_ops->create == NULL)
        return 0;

    skb->dev = dev_master;
    ret = skb->dev->header_ops->create(skb, skb->dev, type, daddr, saddr, len);
    skb->dev = dev; 

    return ret;
}


int stub_rebuild_header(struct sk_buff *skb)
{
    struct net_device *dev;
    int ret;

    if (dev_master->header_ops->rebuild == NULL)
        return 0;

    dev = skb->dev;
    skb->dev = dev_master;
    ret = skb->dev->header_ops->rebuild(skb);
    skb->dev = dev;

    return ret;
}


struct net_device_stats *stub_get_stats(struct net_device *dev)
{
    return netdev_priv(dev);
}


void stub_free_one(int index)
{
    unregister_netdev(dev_stub[index]);
    free_netdev(dev_stub[index]);
}


struct header_ops stub_header_ops ____cacheline_aligned = {
    .create         = stub_hard_header,     // 
    .rebuild        = stub_rebuild_header,  // 
};


void __init stub_setup(struct net_device *dev)
{
    ether_setup(dev);

    dev->flags              = dev_master->flags & ~IFF_UP;
    dev->header_ops         = &stub_header_ops;

    // dev->change_mtu         = NULL;
    // dev->hard_header_cache  = NULL;
    // dev->header_cache_update= NULL;
    // dev->hard_header_parse  = NULL;

    dev->set_mac_address    = stub_set_address;

    dev->type               = ARPHRD_ETHER;
    dev->hard_header_len    = ETH_HLEN;
    dev->mtu                = ETH_DATA_LEN;
    dev->addr_len           = ETH_ALEN;
    dev->tx_queue_len       = 1000; /* Ethernet wants good queues */

    dev->hard_start_xmit = stub_xmit;
    dev->set_multicast_list = NULL;

    dev->open = stub_open;
    dev->stop = stub_close;
    dev->do_ioctl = stub_ioctl;

    dev->get_stats = stub_get_stats;
    dev->neigh_setup = stub_neigh_setup_dev;

    memset(dev->broadcast,0xFF, ETH_ALEN);
    random_ether_addr(dev->dev_addr);
}


int __init stub_init_one(int i)
{
    struct net_device *dev;
    int err;

    dev = alloc_netdev(sizeof(struct net_device_stats), "stub%d", stub_setup);

    if (!dev)
        return -ENOMEM;

    if ((err = register_netdev(dev))) {
        free_netdev(dev);
        dev = NULL;
    } else {
        dev_stub[i] = dev; 
    }

    return err;
}


int __init stub_init_module(void)
{ 
    int i, err = 0;
    dev_stub = kmalloc(stubs * sizeof(void *), GFP_KERNEL); 
    if (!dev_stub)
        return -ENOMEM; 

    if (master == NULL) {
        printk(KERN_ALERT "stub: master param required.\n");
        return -EFAULT;
    }

    if( (dev_master = __dev_get_by_name(&init_net, master)) == NULL ){
        printk(KERN_ALERT "stub: %s bad device name.\n",master);
        return -EFAULT;
    }

    for (i = 0; i < stubs && !err; i++)
        err = stub_init_one(i); 

    if (err)
        goto fail;  

    printk("stub: %d stubs registered on device %s.\n",stubs, master);

#ifdef REGISTER_STUB_HANDLER
    stub_packet_type.dev = dev_master;
    dev_add_pack(&stub_packet_type);
#endif
    return 0;

fail:

    i--;
    while(--i >= 0)
        stub_free_one(i);

    return err;
} 


void __exit stub_cleanup_module(void)
{
    int i;
    for (i = 0; i < stubs; i++) 
        stub_free_one(i); 

    dev_remove_pack(&stub_packet_type);
    kfree(dev_stub);	
}

module_init(stub_init_module);
module_exit(stub_cleanup_module);
