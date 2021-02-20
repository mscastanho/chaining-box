#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/pkt_cls.h>
#include <linux/bpf.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <stddef.h>

#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>

#include "bpf_elf.h"

#include "cb_helpers.h"
#include "firewall.h"

/* Map with the egress iface to forward packets to. */
MAP(fw_egress, BPF_MAP_TYPE_ARRAY, sizeof(int), sizeof(int), 1, PIN_GLOBAL_NS);

/* Map with rules with targets of interest and instructions on what to do to
   them. */
MAP(rules, BPF_MAP_TYPE_HASH, sizeof(struct ip_5tuple), \
    sizeof(struct fw_action), 100, PIN_GLOBAL_NS);


SEC("firewall")
int fw (struct __sk_buff *ctx)
{
	void *data_end = (void *)(long)ctx->data_end;
	void *data = (void *)(long)ctx->data;
	struct ip_5tuple key = { };
	struct fw_action *action = NULL;
	struct ethhdr *eth;
	struct iphdr *ip;
	int ret;

	if(data + sizeof(struct ethhdr) + sizeof(struct iphdr) > data_end){
	  cb_debug("[FW]: Bounds check failed\n");
	  return TC_ACT_OK;
	}

	/* Check if frame contains IP. Pass along otherwise. */
	eth = data;
	if(eth->h_proto != bpf_htons(ETH_P_IP))
	  return TC_ACT_OK;

	ip = data + sizeof(struct ethhdr);

	ret = get_tuple(ip,data_end,&key);
	if (ret < 0) {
	  cb_debug("[FW]: get_tuple() failed: %d\n", ret);
	  return TC_ACT_OK;
	}

	action = bpf_map_lookup_elem(&rules, &key);
	if (!action)
	  cb_debug("[FW] No special rule for packet. Let it pass.\n");
        else {
	  switch (action->what) {
	    case FW_ACTION_PASS:
	      cb_debug("[FW] Allowing flow!\n");
	      break;
	    case FW_ACTION_BLOCK:
	      cb_debug("[FW] Blocking flow!\n");
	      return TC_ACT_SHOT;
	    default:
	      cb_debug("[FW] Don't know what to do. Blocking flow!\n");
	      return TC_ACT_SHOT;
	  }
	}

	/* Lookup what ifindex to redirect packets to */
	int egress_key = 0;
	int *tx_port = bpf_map_lookup_elem(&fw_egress, &egress_key);
	if (!tx_port) {
	  cb_debug("[FW] No egress port configured. Dropping packet.\n");
	  return TC_ACT_SHOT;
	}

	cb_debug("[FW] Redirecting packet...\n");
	return bpf_redirect(*tx_port, 0);
}

char _license[] SEC("license") = "GPL";
