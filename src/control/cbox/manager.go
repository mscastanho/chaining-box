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
      fmt.Printf("New connection: %s (MAC: %s)\n",
        hello.Name, hello.Address.String())
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
  var r Fwd_rule
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
  rules := make(map[string][]Fwd_rule)

  for _,chain := range cfg.Chains {
    spi := chain.Id << 24
    for i,node := range chain.Nodes {
      sph = spi | (255-uint32(i))

      /* Is it the end of the chain? */
      if i == len(chain.Nodes) - 1 {
        flags = 1
        address_next = CBAddress{0x00,0x00,0x00,0x00,0x00,0x00}
      } else {
        flags = 0
        next := chain.Nodes[i+1]
        /*TODO: This function should be run from the context of a CBManager, because the Address to be used
          here is the one reported by each node on the initial Hello message */
        address_next = cbm.connectedAgents[next].addr
      }

      r.Key = sph
      r.Val = Fwd_entry{Flags: flags, Address: address_next}
      rules[node] = append(rules[node], r)
    }
  }

  node_cfgs := make(map[string]CBRulesConfig)

  for node, fr := range rules {
    node_cfgs[node] = CBRulesConfig{Fwd: fr}
  }

  return cbm.batchInstallRules(node_cfgs)
}
