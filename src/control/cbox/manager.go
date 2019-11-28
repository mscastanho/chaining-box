package cbox

import (
  "encoding/json"
  "errors"
  "fmt"
  "net"
)

type CBManager struct {
  connectedAgents map[string]net.Conn
}

func NewCBManager() *CBManager {
  cbm := new(CBManager)
  cbm.connectedAgents = make(map[string]net.Conn)
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
      cbm.connectedAgents[hello.Name] = conn
      fmt.Printf("New connection: %s (MAC: %s)\n",
        hello.Name, net.HardwareAddr(hello.Address).String())
  }
}

func (cbm *CBManager) BatchInstallRules(rcm map[string]CBRulesConfig) error {
  /* Iterate over all nodes in the config to guarantee we have
   * open connections to them. We will only install rules once we
   * make sure we can reach every node on the config, to guarantee
   * we don't endup on a broken state if we find out one of them
   * is missing after we have already performed other installs. */
  for node := range rcm {
    if _,ok := cbm.connectedAgents[node]; !ok {
      return errors.New(fmt.Sprintf("No connection to node %s", node))
    }
  }

  for node,rules := range rcm {
    conn := cbm.connectedAgents[node]
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
