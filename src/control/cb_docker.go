package main

import (
  "context"
  "flag"
  "fmt"
  "io/ioutil"
  "net"
  "os"
  "os/exec"
  "path/filepath"

  /* Interaction with Docker Engine */
  "github.com/docker/docker/api/types"
  "github.com/docker/docker/api/types/container"
  "github.com/docker/docker/api/types/mount"
  "github.com/docker/docker/client"
  "github.com/docker/go-units"
  "github.com/docker/go-connections/nat"

  /* Interaction with Open vSwitch */
  "github.com/digitalocean/go-openvswitch/ovs"

  /* Handling point-to-point links using veth pairs */
  "control/koko"

  /* ChainingBox definitions */
  "control/cbox"
)

/* Docker image to be used by containers */
const default_image = "docker.io/mscastanho/chaining-box:cb-node"

/* Default directory containing the eBPF programs*/
const progs_dir = "src/build"

/* Configs to be used on OVS network */
const net_prefix = "10.10.10"
const ip_offset = 10
const ovs_br = "cbox-br"

/* Default entrypoint to guarantee a Docker container keeps alive */
var placeholder_entrypoint = []string{"tail","-f","/dev/null"}
var default_entrypoint = []string{"tail","-f","/dev/null"}

/* Base chaining-box dir */
const target_dir = "/cb"

/* Direct Link global ID */
var dlgid = 0

/* IP address counter */
var ipac = 0

/* Type of dataplane to use */
type NetType int

const (
  OVS NetType = iota
  MACVLAN
  BRIDGE
)

var dataplane_type NetType

func usage() {
  var flags_desc string = `
    -c : path to chains configuration file
    -d : path to chaining-box source dir`

    fmt.Printf("Usage: %s -c <path> -d <path>\n\nRequired flags:%s\n", os.Args[0], flags_desc)
}

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

func createNetworkInfra() {
  switch dataplane_type {
    case BRIDGE:
    case MACVLAN:
      /* TODO: Make physical iface configurable */
      /* TODO: Use SDK for this */
      err := exec.Command("docker", "network", "create", "-d", "macvlan",
        "--subnet=192.168.100.0/24", "--gateway=192.168.100.1",
        "-o", "parent=enp1s0f0", "macvlan0").Run()
      if err != nil {
        panic("Failed to create network macvlan0")
      }

      err = exec.Command("docker", "network", "create", "-d", "macvlan",
        "--subnet=192.168.101.0/24", "--gateway=192.168.101.1",
        "-o", "parent=enp1s0f1", "macvlan1").Run()
      if err != nil {
        panic("Failed to create network macvlan1")
      }
    case OVS:
      /* Create OVS bridge for container communication */
      ovs_client := ovs.New(
        // Prepend "sudo" to all commands.
        ovs.Sudo(),
      )

      if err := ovs_client.VSwitch.AddBridge(ovs_br); err != nil {
        panic(fmt.Sprintf("Failed to create OVS bridge:", err))
      }
    default:
      fmt.Printf("Unknown network type %s\n", dataplane_type);
      os.Exit(1)
  }
}

func attachExtraInterfaces(cname string) {
  var err0, err1 error

  switch dataplane_type {
    case BRIDGE:
    case MACVLAN:
      /* TODO: Properly use the SDK for this */
      err0 = exec.Command("docker", "network", "connect", "macvlan0", cname).Run()
      err1 = exec.Command("docker", "network", "connect", "macvlan1", cname).Run()
    case OVS:
      /* Add interface attached to OVS bridge */
      ipac += 1
      err0 = exec.Command("ovs-docker", "add-port", ovs_br, "eth1", cname,
              fmt.Sprintf("--ipaddress=192.168.100.%d/24", ipac)).Run()
  }

  if err0 != nil || err1 != nil {
    fmt.Printf("Failed to configure extra interfaces for %s. NetType: %v err0: %v err1: %v ",
      cname, dataplane_type, err0, err1)
  }
}

/* This function return the names of the default ifaces to use as ingress and egress */
func getDefaultInterfaces() (ingress string, egress string) {
  switch dataplane_type {
    case BRIDGE:
      return "eth0","eth0"
    case MACVLAN:
      return "eth2","eth2"
    case OVS:
      return "eth1","eth1"
    default:
      panic(fmt.Sprintf("Unknown network type %s\n", dataplane_type));
  }

  return "", ""
}

func CreateNewContainer(name string, srcdir string, entrypoint []string) (string, error) {
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
      AutoRemove: true,
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
          Source: srcdir,
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

func ParseChainsConfig(cfgfile string) (cfg *cbox.CBConfig){
  /* TODO: Loading the entire file to memory may fail if the file is too big */
  cfgjson, err := ioutil.ReadFile(cfgfile)
  if err != nil {
    panic(err)
  }

  return cbox.NewCBConfig(cfgjson)
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

func getTypeExecString(sfType, ingress, egress string) []string{
  basedir := target_dir + "/src/build/"
  switch sfType {
    case "user-redirect":
      return []string{basedir + "user-redirect", ingress, "172.17.0.2"}
    case "xdp-redirect":
      return []string{basedir + "xdp_redirect_map", ingress, egress}
    case "tc-redirect":
      return []string{"--", basedir + "tc_bench01_redirect", "--ingress", ingress,
                        "--egress", egress, "--srcip", "172.17.0.2"}
  }

  return nil
}

func startServiceFunction(sf cbox.CBInstance, ingress string, omit_ingress bool,
  egress string, omit_egress bool, server_address string) {

  /* Base entrypoint to start CB agent*/
  entrypoint := []string{
        target_dir + "/src/build/cb_start",
        "--name", sf.Tag}

  /* CB Agent allows us to not configure the stages on a specific direction, to
   * handle the case when we are using veth pairs for direct communication, in
   * which we don't have to performing encap/decap between nodes. To tell this
   * to the agent, we need to pass '-' as the name of an interface to cb_start */
  if omit_ingress {
    entrypoint = append(entrypoint, "--ingress", "-")
  } else {
    entrypoint = append(entrypoint, "--ingress", ingress)
  }

  if omit_egress {
    entrypoint = append(entrypoint, "--egress", "-")
  } else {
    entrypoint = append(entrypoint, "--egress", egress)
  }

  entrypoint = append(entrypoint,
        "--obj", target_dir + "/src/build/sfc_stages_kern.o",
        "--address", server_address)

  /* Extend entrypoint with SF-specific startup cmd*/
  if sfCmd := getTypeExecString(sf.Type, ingress, egress) ; sfCmd != nil {
    entrypoint = append(entrypoint, sfCmd...)
  } else {
    fmt.Printf("ERROR: SF type '%s' unknown, not starting SF '%s'...", sf.Type,
            sf.Tag)
    return
  }

  fmt.Printf("Starting %s of type %s on iif %s, eif %v\nEntrypoint: %v\n\n",
          sf.Tag, sf.Type, ingress, egress, entrypoint)


  args := append([]string{"exec", "-t", sf.Tag}, entrypoint...)

    /* TODO: Too hacky. Use proper Docker SDK funcs instead. */
    cmd := exec.Command("docker", args...)

    /* Create file to log the output */
    outfile, err := os.Create("/tmp/" + sf.Tag + ".out")
    if err != nil {
      panic(err)
    }
    defer outfile.Close()
    cmd.Stdout = outfile

    if err := cmd.Start(); err != nil {
      panic(err)
      return
    }

    go cmd.Wait()
}

func main() {
  /* Slice containing IDs of all containers created */
  var clist []string
  var cfgfile string
  var srcdir string
  using_direct_links := false
  ingress_ifaces := make(map[string]string)
  egress_ifaces := make(map[string]string)

  flag.StringVar(&cfgfile, "c", "", "path to the chains configuration file.")
  flag.StringVar(&srcdir, "d", "", "path to the chaining-box source dir.")
  flag.Parse()

  if cfgfile == "" {
    fmt.Println("Please provide the path to the chains config file.")
    usage()
    os.Exit(1)
  }

  /* TODO: Avoid this requirement by installing all needed files to a default
   * location like /usr/local/chaining-box */
  if srcdir == "" {
    fmt.Println("Please provide the path to chaining-box source code config file.")
    usage()
    os.Exit(1)
  }

  /* Docker requires paths to be absolute */
  source_dir, err := filepath.Abs(srcdir)
  if err != nil {
    fmt.Printf("Error while turning %s into an absolute path.", srcdir)
    os.Exit(1)
  }

  cfg := ParseChainsConfig(cfgfile)

  /* TODO: Make this configurable */
  dataplane_type = MACVLAN

  createNetworkInfra()

  /* Start containers for all functions declared */
  for i := 0 ; i < len(cfg.Functions) ; i++ {
    sf := &cfg.Functions[i]

    /* TODO: We should only start containers for functions that are NOT
     * remote. This is not being done now because the current tests will
     * all take place on the same host, but we also want to exercise the
     * connections WITHOUT direct links (veth pairs), so we use the remote
     * param to force connections between some containers to not use veth.*/
    id, err := CreateNewContainer(sf.Tag, source_dir, placeholder_entrypoint)

    if err != nil {
      fmt.Println("Failed to start container!")
      panic(err)
    }

    attachExtraInterfaces(sf.Tag)

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

      /* We can only create veth pairs between local nodes */
      if !n1.Remote && !n2.Remote {
        if _,ok := ingress_ifaces[n2.Tag]; ok {
          fmt.Println("ERROR: Nodes with direct links should not be",
          "shared between chains.")
          fmt.Printf("\tFailed while creating link %s <-> %s\n", n1.Tag, n2.Tag)
          os.Exit(1)
        }

        if _,ok := egress_ifaces[n1.Tag]; ok {
          fmt.Println("ERROR: Nodes with direct links should not be",
          "shared between chains.")
          fmt.Printf("\tFailed while creating link %s <-> %s\n", n1.Tag, n2.Tag)
          os.Exit(1)
        }

        dl1,dl2 := CreateDirectLink(n1.Id,n2.Id)
        ingress_ifaces[n2.Tag] = dl2
        egress_ifaces[n1.Tag] = dl1
        using_direct_links = true

        fmt.Printf("Direct link created: %s (%s) <=> (%s) %s\n",
          n1.Tag, dl1, dl2, n2.Tag)
      }
    }
  }

  if using_direct_links {
    fmt.Println("WARNING: nodes using direct links should not be shared",
                "between chains. This is not being fully checked. Make sure",
                "this is being respected in the chains config file passed to",
                "this program.")
  }

  /* IP of the docker0 iface so the agents can connect to the manager
   * running locally. */
  server_address := getDockerIP() + ":9000"

  /* Start SF apps on each container. */
  for i := 0 ; i < len(cfg.Functions) ; i++ {
    var ingress, egress string
    var omit_ingress, omit_egress bool

    sf := cfg.Functions[i]
    defi, defe := getDefaultInterfaces()

    /* If ingress_ifaces[sf.Tag] is already set, then it is using
     * a direct link, we should make sure it is used. */
    if val,ok := ingress_ifaces[sf.Tag] ; ok {
      ingress = val
      omit_ingress = true
    } else {
      /* Otherwise, we have to choose the default iface*/
      ingress = defi
      omit_ingress = false

      /* TODO: When using MACVLAN, we need to make sure at least one
       * container receives packets from macvlan0 and sends to
       * macvlan1, so the traffic gets back to the traffic generator. For
       * now we will fix this on sf1. During the tests, all our chains should
       * start on sf1 for this to work.
       *
       * A proper way to fix this would be to allow ingress and egress to be
       * an array of interfaces, and installing the stages on all those interfaces.
       * Then we would only have to make sure that the app running in the container
       * receives and sends to the proper intefaces.
       * >>>>> This is a HACK!!! <<<<< */
       if dataplane_type == MACVLAN && sf.Tag == "sf1" {
        ingress = "eth1"
       }
    }

    if val,ok := egress_ifaces[sf.Tag] ; ok {
      egress = val
      omit_egress = true
    } else {
      egress = defe
      omit_egress = false
    }

    startServiceFunction(sf, ingress, omit_ingress, egress, omit_egress,
      server_address)
  }
}
