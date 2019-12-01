package cbox

import (
  "encoding/json"
  // "fmt"
  "net"
)

type CBNodeType int

const (
  SF  CBNodeType = iota   /* Service Function */
  CLS                     /* Classifier */
)

func (ntype CBNodeType) String() string {

  names := [...]string{
    "sf",
    "cls",
  }

  if ntype < SF || ntype > CLS {
    return "none"
  }

  return names[ntype]
}

type CBAddress [6]byte

func MakeCBAddress (addr []byte) CBAddress {
  var cba [6]byte
  copy(cba[:], addr)
  return CBAddress(cba)
}

func (addr CBAddress) String() string {
  a := [6]byte(addr)
  return net.HardwareAddr(a[:]).String()
}

type CBInstance struct {
  Tag string
  Type string
  Remote bool
  Id string
  Address CBAddress
}

func (cfg *CBConfig) GetNodeByName(name string) *CBInstance{
  return &cfg.Functions[cfg.name2idx[name]]
}

type CBChain struct {
  Id uint32
  Nodes []string
}

type CBConfig struct {
  Chains []CBChain
  Functions []CBInstance
  name2idx map[string]int
}

func NewCBConfig(cfgbytes []byte) *CBConfig{
  cfg := new(CBConfig)
  cfg.parseChainsConfig(cfgbytes)
  cfg.name2idx = make(map[string]int)

  /* Initialize name2idx mapping */
  for i,f := range cfg.Functions {
    cfg.name2idx[f.Tag] = i
  }

  return cfg
}

func (cbc *CBConfig) parseChainsConfig(cfgbytes []byte) {
  json.Unmarshal([]byte(cfgbytes), &cbc)
  /* TODO: Validate Chain Id is < 2^24 */
  /* TODO: Validate taht chain lengths are < 256 */
  // fmt.Printf("Chains: %v\nFunctions: %v\n", cfg.Chains, cfg.Functions)
}

type Fwd_entry struct {
  Flags uint8       `json:"flags"`
  Address CBAddress `json:"address"`
}

type Fwd_rule struct {
  Key uint32    `json:"key"`
  Val Fwd_entry `json:"val"`
}

type CBRulesConfig struct {
  Fwd []Fwd_rule `json:"fwd"`
}
