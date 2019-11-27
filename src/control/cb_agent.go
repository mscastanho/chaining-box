package main

import (
  "control/cbox"
  "fmt"
  "os"
)

/* TODO: Make this configurable through the CLI */
const server_address = ":9000"

func main(){
  name := os.Args[1]
  iface := os.Args[2]
  // objpath := C.CString(os.Args[2])
//
  // ret := C.load_stages(iface, objpath)
  // if ret != 0 {
    // panic("Something went wrong when loading the stages!")
  // }
//
  // ret = C.get_map_fds()
  // if ret != 0 {
    // panic("Could not get map fds!")
  // }

  cba, err := cbox.NewCBAgent(name, iface)
  if err != nil {
    fmt.Println(err)
    os.Exit(1)
  }

  err = cba.ManagerConnect(server_address)
  if err != nil {
    fmt.Println(err)
    os.Exit(1)
  }

  err = cba.ManagerListen()
  if err != nil {
    fmt.Println(err)
    os.Exit(1)
  }
}
