package cbox

import "encoding/json"

/* Types */
type CBMsgType int

const (
  None    CBMsgType = iota
  Hello
  Install
)

type CBMsg struct{
  Type CBMsgType        `json:"type"`
  Data json.RawMessage  `json:"body"`
}

type CBMsg_Hello struct {
  Name string           `json:"name"`
  Address CBAddress     `json:"address"`
}

type CBMsg_Install struct {
  CBRulesConfig
}

/* Constructors */
func MakeCBMsg_Hello(name string, addr CBAddress) (msg CBMsg){
  m,_ := json.Marshal(CBMsg_Hello{Name: name, Address: addr})
  return CBMsg{Hello, m}
}

func MakeCBMsg_Install(fwd []Fwd_rule, pxy []Proxy_rule) (msg CBMsg){
  m,_ := json.Marshal(CBMsg_Install{CBRulesConfig{Fwd: fwd, Proxy: pxy}})
  return CBMsg{Install, m}
}

/* Helpers */
func ParseCBMsg(msg CBMsg) interface{} {
  switch msg.Type {
    case Hello:
      var mh CBMsg_Hello
      json.Unmarshal(msg.Data, &mh)
      return mh
    case Install:
      var mi CBMsg_Install
      json.Unmarshal(msg.Data, &mi)
      return mi
  }

  return nil
}
