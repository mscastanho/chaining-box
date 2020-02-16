package cbox

import (
	"encoding/binary"
  "encoding/json"
	"errors"
  "fmt"
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

type CBChain struct {
  Id uint32
  Nodes []string
  Flows []Ip_5tuplestr
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

func (cbc *CBConfig) GetNodeByName(name string) *CBInstance{
  if idx,ok := cbc.name2idx[name]; ok {
    return &cbc.Functions[idx]
  }

  return nil
}


func (cbc *CBConfig) parseChainsConfig(cfgbytes []byte) {
  json.Unmarshal([]byte(cfgbytes), &cbc)
  /* TODO: Validate Chain Id is < 2^24 */
  /* TODO: Validate taht chain lengths are < 256 */
  fmt.Printf("Chains: %v\nFunctions: %v\n", cbc.Chains, cbc.Functions)
}

type Fwd_entry struct {
  Flags uint8       `json:"flags"`
  Address CBAddress `json:"address"`
}

type Fwd_rule struct {
  Key uint32    `json:"key"`
  Val Fwd_entry `json:"val"`
}

type Ip_5tuplestr struct {
  Ipsrc string  `json:"ipsrc"`
  Ipdst string  `json:"ipdst"`
  Sport uint16  `json:"sport"`
  Dport uint16  `json:"dport"`
  Proto uint8   `json:"proto"`
}

func ip2int(ip net.IP) uint32 {
	if len(ip) == 16 {
		return binary.BigEndian.Uint32(ip[12:16])
	}
	return binary.BigEndian.Uint32(ip)
}

func int2ip(nn uint32) net.IP {
	ip := make(net.IP, 4)
	binary.BigEndian.PutUint32(ip, nn)
	return ip
}

func (tstr *Ip_5tuplestr) Parse() (*Ip_5tuple,error) {
  ip5t := new(Ip_5tuple)

  ipsrc := net.ParseIP(tstr.Ipsrc)
  if ipsrc == nil {
    return nil,errors.New("Failed to parse source IP")
  }
	ip5t.Ipsrc = ip2int(ipsrc)

  ipdst := net.ParseIP(tstr.Ipdst)
  if ipdst == nil {
    return nil,errors.New("Failed to parse destination IP")
  }
	ip5t.Ipdst = ip2int(ipdst)

  return ip5t,nil

}

type Ip_5tuple struct {
  Ipsrc uint32 `json:"ipsrc"`
  Ipdst uint32 `json:"ipdst"`
  Sport uint16 `json:"sport"`
  Dport uint16 `json:"dport"`
  Proto uint8  `json:"proto"`
}


type Proxy_rule struct {
  Key Ip_5tuple  `json:"key"`
  Val uint32		 `json:"val"`
}

type Nsh_header struct {
  Basic_info uint16 `json:"basic_info"`
  Md_type    uint8  `json:"md_type"`
  Next_proto uint8  `json:"next_proto"`
  Serv_path  uint8  `json:"serv_path"`
}

// var default_nsh := Nsh_header {
	// Basic_info: 0x,
	// Md_type: 0x2,
	// Next_proto: 0xaa,
	// Serv_path: 0,
// }

type CBRulesConfig struct {
  Fwd []Fwd_rule `json:"fwd"`
  Proxy []Proxy_rule `json:"proxy"`
}
