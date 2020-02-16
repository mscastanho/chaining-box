package cbox

import (
  "encoding/json"
  "errors"
  "fmt"
  "net"
)

type CBManager struct {
  connectedAgents map[string]cBClient
}

type cBClient struct {
  conn net.Conn
  addr CBAddress
}

func NewCBManager() *CBManager {
  cbm := new(CBManager)
  cbm.connectedAgents = make(map[string]cBClient)
  return cbm
}

func (cbm *CBManager) HandleConnection(conn net.Conn) {
  var msg CBMsg
  var hello CBMsg_Hello

  d := json.NewDecoder(conn)
  err := d.Decode(&msg)
  if err != nil {
    fmt.Println("Something went wrong!", err)
  }

  obj := ParseCBMsg(msg)
  switch obj.(type){
    case CBMsg_Hello:
      hello,_ = obj.(CBMsg_Hello)
      cbm.connectedAgents[hello.Name] = cBClient{conn, hello.Address}
      fmt.Printf("New connection: %s (MAC: %v) from IP: %v\n",
        hello.Name, hello.Address, conn.RemoteAddr())
  }
}

func (cbm *CBManager) batchInstallRules(rcm map[string]CBRulesConfig) error {

  for node,rules := range rcm {
    conn := cbm.connectedAgents[node].conn
    data := MakeCBMsg_Install(rules.Fwd)
    // fmt.Printf("rules.Fwd:%+v\n Sending: %+v\n", rules.Fwd, data)
    err := json.NewEncoder(conn).Encode(data)
    if err != nil {
      return errors.New("Failed during rule installation! The system may be" +
          "on an inconsistent state!!")
    }
  }

  return nil
}

func (cbm *CBManager) InstallConfiguration(cfg *CBConfig) error {
  var sph uint32
  var fr Fwd_rule
  var pr Proxy_rule
  var flags uint8
  var address_next CBAddress
  var next *CBInstance

  /* Iterate over all nodes in the config to guarantee we have
   * open connections to them. We will only install rules once we
   * make sure we can reach every node on the config, to guarantee
   * we don't endup on a broken state if we find out one of them
   * is missing after we have already performed other installs. */
  for _,node := range cfg.Functions {
    if _,ok := cbm.connectedAgents[node.Tag]; !ok {
      return errors.New(fmt.Sprintf("No connection to node %s", node))
    }
  }

  /* Each node will have a list of rules */
  frules := make(map[string][]Fwd_rule)
  prules := make(map[string][]Proxy_rule)

  for _,chain := range cfg.Chains {
    spi := chain.Id << 8
    for i,node := range chain.Nodes {
      sph = spi | (254-uint32(i))
      curr := cfg.GetNodeByName(node)

      /* Generate forwarding rules */

      if i == len(chain.Nodes) - 1 { /* End of the chain? */
        flags = 1
        next = nil
        address_next = CBAddress{0x00,0x00,0x00,0x00,0x00,0x00}
      } else {
        flags = 0
        next = cfg.GetNodeByName(chain.Nodes[i+1])
        address_next = cbm.connectedAgents[next.Tag].addr
      }

      /* We only need forwarding rules if we *do not* have a direct link
       * to the next SF or this is the end of the chain. */
      if next == nil || (curr.Remote || next.Remote) {
        fr.Key = sph
        fr.Val = Fwd_entry{Flags: flags, Address: address_next}
        frules[node] = append(frules[node], fr)
      }

      /* Generate proxy rules
       *
       * Imagine the following situations:
       * 1) A chain with two local SFs followed by a remote one.
       *    sf1 (local) -> sf2 (local) -> sf3 (remote) -> ...
       * the connection sf1 -> sf2 will be configured with a direct link, and the Dec
       * stage on sf2 will be suppressed. So the NSH table will not have any entries
       * when the Enc stage on sf2 executes.
       *
       * 2)Another possibility is when the last connection before the end of a chain is
       * direct:
       *    sf1 (local) -> sf2 (local) (end)
       * in this case sf2 will also need the information from the NSH data table to
       * know if this is the end of the chain or not.
       *
       * To avoid this problem, we need to populate the NSH data (Proxy table) on sf2
       * with an artificial entry, so the Enc stage can use it when preparing to send
       * the packet to sf3 (case 1) or end the chain (case 2). */
      if i > 0 {
				prev := cfg.GetNodeByName(chain.Nodes[i-1])

        /* If there is a direct link with the previous node in the chain and... */
        if !(prev.Remote) && !(curr.Remote) {
          /* ...is the end of chain (Case 2) || ...is followed by remote (Case 1) */
          if i == len(chain.Nodes) - 1 || cfg.GetNodeByName(chain.Nodes[i+1]).Remote {
            for _,flow := range chain.Flows {
              ip5tuple,err := flow.Parse()
              if err != nil {
                panic(err)
              }
              /* Create proxy rule */
              pr.Key = *ip5tuple
              pr.Val = sph
              prules[node] = append(prules[node], pr)
            }
          }
        }
      }
    }
  }

	fmt.Printf("frules:\n %+v\n\n", frules)
	fmt.Printf("prules:\n %+v\n\n", prules)

  node_cfgs := make(map[string]CBRulesConfig)

  for node, frule := range frules {
		if prule,ok := prules[node]; ok {
			node_cfgs[node] = CBRulesConfig{Fwd: frule, Proxy: prule}
      fmt.Printf("Generated rules for %s:\n\tFwd: %+v\n\tProxy: %+v\n\n", node, frule, prule)
		} else {
			node_cfgs[node] = CBRulesConfig{Fwd: frule}
			fmt.Printf("Generated rules for %s:\n\tFwd: %+v\n\n", node, frule)
		}
  }

  return cbm.batchInstallRules(node_cfgs)
}
