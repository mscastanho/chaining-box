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

      /* Is it the end of the chain? */
      if i == len(chain.Nodes) - 1 {
        flags = 1
        address_next = CBAddress{0x00,0x00,0x00,0x00,0x00,0x00}
      } else {
        flags = 0
        next := chain.Nodes[i+1]
        address_next = cbm.connectedAgents[next].addr
      }

      fr.Key = sph
      fr.Val = Fwd_entry{Flags: flags, Address: address_next}
      frules[node] = append(frules[node], fr)

      /* Imagine this situation: a chain with two local SFs followed by a remote one.
       *    sf1 (local) -> sf2 (local) -> sf3 (remote)
       * the connection sf1 -> sf2 will be configured with a direct link, and the Dec
       * stage on sf2 will be suppressed. So the NSH table will not have any entries
       * when the Enc stage on sf2 executes. 
       *
       * To avoid this problem, we need to populate the NSH data (Proxy table) on sf2
       * with an artificial entry, so the Enc stage can use it when preparing to send
       * the packet to sf3. */
      if i > 0 && i < len(chain.Nodes) - 1 {
				prev := cfg.GetNodeByName(chain.Nodes[i-1])
				curr := cfg.GetNodeByName(chain.Nodes[i])
				next := cfg.GetNodeByName(chain.Nodes[i+1])

				if !(prev.Remote) && !(curr.Remote) && next.Remote {
					for _,flow := range chain.Flows {
						ip5tuple,err := flow.Parse()
						if err != nil {
							panic(err)
						}
						pr.Key = *ip5tuple
						pr.Val = sph
						prules[node] = append(prules[node], pr)
					}
				}
        /* Create proxy rule */
      }
    }
  }

  node_cfgs := make(map[string]CBRulesConfig)

  for node, frule := range frules {
		if prule,ok := prules[node]; ok {
			node_cfgs[node] = CBRulesConfig{Fwd: frule, Proxy: prule}
		} else {
			node_cfgs[node] = CBRulesConfig{Fwd: frule}
		}
  }

  return cbm.batchInstallRules(node_cfgs)
}
