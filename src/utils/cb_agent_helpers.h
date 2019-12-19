#ifndef CB_AGENT_HELPERS_H_
#define CB_AGENT_HELPERS_H_

#include <stdint.h>
#include "cb_common.h"

int load_ingress_stages(const char* iface, const char* stages_obj);

int load_egress_stages(const char* iface, const char* stages_obj);

int add_fwd_rule(uint32_t key, struct fwd_entry val);

#endif /* CB_AGENT_HELPERS_H_ */
