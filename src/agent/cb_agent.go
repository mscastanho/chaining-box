package main

// #cgo CFLAGS: -I../ -I../libbpf/src/root/usr/include/
// #cgo LDFLAGS: -L../build -L../libbpf/src/ -lcbox -lbpf
// #include "../utils/cb_agent_helpers.h"
import "C"

import (
  "fmt"
  "os"
)

func main(){
  if len(os.Args) != 3 {
    fmt.Println("Make sure you're using this prog correctly!!")
    os.Exit(1)
  }

  iface := C.CString(os.Args[1])
  objpath := C.CString(os.Args[2])

  ret := C.load_stages(iface, objpath)
  if ret != 0 {
    panic("Something went wrong when loading the stages!")
  }

  ret = C.get_map_fds()
  if ret != 0 {
    panic("Could not get map fds!")
  }

  fmt.Println("All is well!!!!")
}
