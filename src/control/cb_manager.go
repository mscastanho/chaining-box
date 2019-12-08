package main

import (
  "fmt"
  "io/ioutil"
  "net"
  "os"
  "time"
  "control/cbox"
)

/* Port the server use to communicate with clients */
const server_address = ":9000"

func main() {

  if len(os.Args) != 2 {
    fmt.Println("Please provide the path to the chains config file.")
    os.Exit(1)
  }

  cfgfile := os.Args[1]
  cfgjson, err := ioutil.ReadFile(cfgfile)
  if err != nil {
    panic(err)
  }

  /* Get configuration */
  cfg := cbox.NewCBConfig(cfgjson)

  /* Serve nodes */
  cbm := cbox.NewCBManager()

  sock, _ := net.Listen("tcp", server_address)
  done := make(chan bool)

  go func(done chan bool) {
    for {
      conn, _ := sock.Accept()
      defer conn.Close()
      go cbm.HandleConnection(conn)
    }
    done <- true
  }(done)

  fmt.Println("Listening for connections...")
  time.Sleep(3*time.Second)

  fmt.Println("Installing rules...")
  err = cbm.InstallConfiguration(cfg)
  if err != nil {
    fmt.Println(err)
  }

  /* Block waiting for the go routine*/
  <- done
}
