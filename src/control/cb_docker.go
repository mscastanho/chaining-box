package main

import (
  "context"
  "errors"
  "fmt"
  "io/ioutil"
  "log"
  "net"
  "os"
  "os/exec"
  "time"

  /* Interaction with Docker Engine */
  "github.com/docker/docker/api/types"
  "github.com/docker/docker/api/types/container"
  "github.com/docker/docker/api/types/mount"
  "github.com/docker/docker/client"
  "github.com/docker/go-units"
  "github.com/docker/go-connections/nat"

  /* Handling point-to-point links using veth pairs */
  "control/koko"

  /* ChainingBox definitions */
  "control/cbox"
)

/* Docker image to be used by containers */
const default_image = "docker.io/mscastanho/chaining-box:cb-node"

/* Default directory containing the eBPF programs*/
const progs_dir = "src/build"

/* Port the server use to communicate with clients */
const server_address = ":9000"

/* Base chaining-box dir */
/* TODO: This should be configured through CLI */
var source_dir = "/home/mscastanho/devel/chaining-box"
var target_dir = "/usr/src/chaining-box"

/* Direct Link global ID */
var dlgid = 0

func getDirectLinkNames() (string,string) {
  /* The name of each interface on a veth pair used for direct linking two
   * containers will follow a simple naming scheme: dl<id>[a|b].
   *    Ex: 'dl0a' and 'dl0b'
   *
   * 'dl' stands * for 'direct link', <id> is a global unique id used for direct
   * links and the last character just identifies the end corresponding end of
   * the pair (either 'a' or 'b'). */
  a := fmt.Sprintf("dl%da",dlgid)
  b := fmt.Sprintf("dl%db",dlgid)
  dlgid++
  return a,b
}

func CreateNewContainer(name string, entrypoint []string) (string, error) {
  const port = "9000"
  ctx := context.Background()

  /* Create instance of client to interact with Docker Engine */
  cli, err := client.NewEnvClient()
	if err != nil {
		fmt.Println("Unable to create docker client")
		panic(err)
	}

  /* Download image from DockerHub if needed */
  // _, err = cli.ImagePull(ctx, default_image, types.ImagePullOptions{})
	// if err != nil {
    // panic(err)
  // }

  /* Get definition of memlock ulimit */
  memlock,_ := units.ParseUlimit("memlock=-1")

  /* Define port mapping: 9000 <-> 9000 */
  hostBinding := nat.PortBinding{
    HostIP:   "0.0.0.0",
    HostPort: port,
  }

  containerPort, err := nat.NewPort("tcp", port)
  if err != nil {
    panic(fmt.Sprintf("Unable to use port %s for container %s",
      port, name))
  }

  /* Create container with all custom configuration needed */
  cont, err := cli.ContainerCreate(
		ctx,
		&container.Config{
      Hostname: name,
			Image: default_image,
      /* TODO: Should be command to run the corresponding SF */
      Entrypoint: entrypoint,
		},
		&container.HostConfig {
      /* Containers need to run in privileged mode so they can perform
       * sysadmin operations from inside the container.
       *   Ex: loading BPF programs.*/
      Privileged: true,
      /* Remove container when process finishes */
      // AutoRemove: true,
      /* Map local port 9000 to remote port 9000 */
      PortBindings: nat.PortMap{
        containerPort: []nat.PortBinding{hostBinding},
      },
      /* We need to increase the memlock ulimit for containers so we can
       * create our BPF maps and programs */
      Resources: container.Resources{
        Ulimits: []*units.Ulimit{
          memlock,
        },
      },
      Mounts: []mount.Mount{
        {
          Type: mount.TypeBind,
          /* TODO: Change source to a configurable path. Maybe defined
             by a CLI flag. */
          Source: source_dir,
          Target: target_dir,
        },
      },
    },
    nil, // Networking config
    name)

	if err != nil {
		panic(err)
	}

  /* Start the container created above */
	cli.ContainerStart(context.Background(), cont.ID,
    types.ContainerStartOptions{})

  return cont.ID, nil
}

func CreateDirectLink(container1, container2 string) (string, string){
  var veth1, veth2 koko.VEth
  var err error
  checkErr := func(err error) {
    if err != nil {
      fmt.Fprintf(os.Stderr, "%v", err)
      os.Exit(1)
    }
  }

  veth1.NsName, err = koko.GetDockerContainerNS("", container1)
  checkErr(err)

  veth2.NsName, err = koko.GetDockerContainerNS("", container2)
  checkErr(err)

  veth1.LinkName, veth2.LinkName = getDirectLinkNames()

  err = koko.MakeVeth(veth1,veth2)
  checkErr(err)

  return veth1.LinkName, veth2.LinkName
}

func ConfigureStages(container string, ntype cbox.CBNodeType, iface string) error {
  var obj string

  switch ntype {
    case cbox.SF:
      obj = "sfc_stages_kern.o"
    case cbox.CLS:
      obj = "sfc_classifier_kern.o"
    default:
      return  errors.New("Unknown type")
  }

  obj_path := fmt.Sprintf("%s/src/build/%s",target_dir,obj)
  script_path := fmt.Sprintf("%s/test/load-bpf.sh",target_dir)

  /* TODO: Too hacky. Use proper Docker SDK funcs instead. */
  cmd := exec.Command("docker", "exec", "-t", container,
      script_path, obj_path, iface, ntype.String())

  // log.Printf("Configuring stages: %s\n", cmd.String())
  out, _ := cmd.Output()
  log.Printf("%s", out)

  return nil
}

func ParseChainsConfig(cfgfile string) (cfg *cbox.CBConfig){
  /* TODO: Loading the entire file to memory may fail if the file is too big */
  cfgjson, err := ioutil.ReadFile(cfgfile)
  if err != nil {
    panic(err)
  }

  return cbox.NewCBConfig(cfgjson)
}

func serve(cfg *cbox.CBConfig){
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
  err := cbm.InstallConfiguration(cfg)
  if err != nil {
    fmt.Println(err)
  }

  /* Block waiting for the go routine*/
  <- done
}

func getDockerIP() string {
	var ip net.IP

	iface, err := net.InterfaceByName("docker0")
  if err != nil {
    panic("Could not get reference to docker0")
  }

  addrs, err := iface.Addrs()
  if err != nil {
    panic("Could not get addresses for docker0")
  }

	switch a := addrs[0].(type) {
		case *net.IPNet:
			ip = a.IP
		case *net.IPAddr:
			ip = a.IP
	}

  return ip.String()
}

func main() {
  /* Slice containing IDs of all containers created */
  var clist []string

  /* TODO: Create a proper CLI flag to pass this */
  if len(os.Args) != 2 {
    fmt.Println("Please provide the path to the chains config file.")
    os.Exit(1)
  }

  cfgfile := os.Args[1]
  cfg := ParseChainsConfig(cfgfile)

  // serve(cfg)

  /* IP of the docker0 iface so the agents can connect to the manager
   * tunning locally. */
  server_address := getDockerIP() + ":9000"

  /* Start containers for all functions declared */
  for i := 0 ; i < len(cfg.Functions) ; i++ {
    sf := &cfg.Functions[i]

    /* TODO: We should only start containers for functions that are NOT
     * remote. This is not being done now because the current tests will
     * all take place on the same host, but we also want to exercise the
     * connections WITHOUT direct links (veth pairs), so we use the remote
     * param to force connections between some containers to not use veth.*/
    // id, err := CreateNewContainer(sf.Tag, []string{"tail","-f","/dev/null"})
    id, err := CreateNewContainer(sf.Tag,
      []string{target_dir + "/src/build/cb_agent",
      sf.Tag, "eth0", target_dir + "/src/build/sfc_stages_kern.o", server_address})

    if err != nil {
      fmt.Println("Failed to start container!")
      panic(err)
    }

    clist = append(clist,id)
    sf.Id = id
    fmt.Printf("Container %s(%s) started!\n", sf.Tag, id[:6])
  }

  fmt.Printf("Chains: %v\nFunctions: %v\n", cfg.Chains, cfg.Functions)

  /* Now that we have all containers created, we can configure the direct
   * links (veth pairs) where needed. */
  for _,chain := range cfg.Chains {
    clen := len(chain.Nodes)

    /* Check bogus length */
    if clen < 2 {
      fmt.Printf("Chaing %d has only %d SFs. Not enough. Skipping...\n",
          chain.Id, clen)
      continue
    }

    /* Create direct links */
    for i := 1 ; i < clen ; i++ {
      n1 := cfg.GetNodeByName(chain.Nodes[i-1])
      n2 := cfg.GetNodeByName(chain.Nodes[i])

      fmt.Printf("Node1: %v\nNode2: %v\n\n", n1, n2)
      /* We can only create veth pairs between local nodes */
      if !n1.Remote && !n2.Remote {
        _,_ = CreateDirectLink(n1.Id,n2.Id)
      }
    }
  }

  serve(cfg)
}
