package cbox

// #cgo CFLAGS: -I../../ -I../../libbpf/src/root/usr/include/ -I../../headers
// #cgo LDFLAGS: -L../../build -L../../libbpf/src/ -lcbox -lbpf
// #include "../../utils/cb_agent_helpers.h"
import "C"

import (
  "fmt"
  "encoding/json"
  "errors"
  "io"
  "net"
  "time"
  "unsafe"
)

/* TODO: Make this configurable through the CLI */
const server_address = ":9000"

type CBAgent struct {
  name string
  iface *net.Interface
  conn net.Conn
}

func NewCBAgent(name, ifacename, objpath string) (*CBAgent,error) {
  cba := new(CBAgent)
  cba.name = name

  iface, err := net.InterfaceByName(ifacename)
  if err != nil {
    return nil, err
  }

  cba.iface = iface
  // fmt.Printf("MAC:=> Raw len: %d\n=> Raw: %v\n=> Pretty: %s\n",
    // len(iface.HardwareAddr), iface.HardwareAddr, iface.HardwareAddr.String())
  cba.conn = nil

  err = cba.installStages(objpath)
  if err != nil {
    return nil,err
  }

  return cba,nil
}

func (cba *CBAgent) installStages(obj_path string) error {
  c_iface := C.CString(cba.iface.Name)
  c_path := C.CString(obj_path)

  ret := C.load_stages(c_iface, c_path)
  if ret != 0 {
    return errors.New("Failed to load stages")
  }

  return nil
}

func (cba *CBAgent) ManagerConnect(address string) error {
  var err error
  ntries := 0

  for {
    conn, err := net.Dial("tcp", address)
    if err != nil {
      fmt.Println("Unable to connect to controller. Retrying...")
      ntries++
    } else {
      cba.conn = conn
      break
    }

    if ntries >= 10 {
      return errors.New(fmt.Sprintf(
        "Failed to connect to controller", ntries))
    }

    time.Sleep(time.Second)
  }

	/* Send Hello */
  hello := MakeCBMsg_Hello(cba.name, MakeCBAddress(cba.iface.HardwareAddr))
  err = json.NewEncoder(cba.conn).Encode(hello)
  if err != nil {
    fmt.Println(err)
  }

  return nil
}

func (cba *CBAgent) ManagerListen() error {
  var msg CBMsg
  defer cba.conn.Close()

  if cba.conn == nil {
    return errors.New("No open connection to Chaining-Box manager")
  }

  d := json.NewDecoder(cba.conn)

  for {
    err := d.Decode(&msg)
    if err != nil {
      switch err {
        case io.EOF:
          return errors.New("Agent received EOF")
        default:
          return errors.New("Failed to decode message")
      }
    }

    obj := ParseCBMsg(msg)

    switch obj.(type) {
      case CBMsg_Install:
        mi,_ := obj.(CBMsg_Install)
        go cba.handleInstall(&mi)
      default:
        return errors.New("Unrecognized message format.")
    }
  }
}

func (cba *CBAgent) handleInstall(msg *CBMsg_Install) error {
  fmt.Printf("Received an Install message: %+v\n", msg)

  for i := range msg.Fwd {
    rule := &msg.Fwd[i]
    key := C.uint(rule.Key)
    val := *(*C.struct_fwd_entry)(unsafe.Pointer(&(rule.Val)))

    /* TODO: Don't ignore the error, instead revert (uninstall)
     * all previously just-installed rules */
    _ = C.add_fwd_rule(key, val)
  }

  return nil
}
