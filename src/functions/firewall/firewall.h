#ifndef FIREWALL_H_
#define FIREWALL_H_

enum fw_action_t {
   FW_ACTION_BLOCK = 1,
   FW_ACTION_PASS,
};

struct fw_action {
  enum fw_action_t what;
};

#endif /* FIREWALL_H_ */
