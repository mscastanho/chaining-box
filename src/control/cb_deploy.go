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
const bridge_name = "cbox-br"

/* Default entrypoint to guarantee a Docker container keeps alive */
var placeholder_entrypoint = []string{"tail","-f","/dev/null"}
var default_entrypoint = []string{"tail","-f","/dev/null"}

/* Path to Chaining-Box source directory */
var source_dir string

/* Base chaining-box dir */
const target_dir = "/cb"

/* Direct Link global ID */
var dlgid = 0

/* IP address counter */
var ipac = 0

/* Number of VFs already used */
var vfcount = 0

/* Type of dataplane to use */
type NetType string

const (
  OVS NetType = "ovs"
  MACVLAN = "macvlan"
  BRIDGE = "bridge"
  SRIOV = "sriov"
)

var dataplane_type NetType

func usage() {
  var flags_desc string = `
    -c : path to chains configuration file
    -d : path to chaining-box source dir`

    fmt.Printf("Usage: %s -c <path> -d <path>\n\nRequired flags:%s\n", os.Args[0], flags_desc)
}

func runAndLog(filename string, cmdargs ...string) (error) {
  cmd := exec.Command(cmdargs[0], cmdargs[1:]...)

  if filename != "" {
    /* Create file to log the output */
    outfile, err := os.Create("/tmp/" + filename)
    if err != nil {
      return err
    }

    defer outfile.Close()
    cmd.Stdout = outfile
  }

  if err := cmd.Start(); err != nil {
    return err
  }

  return cmd.Wait()
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
      // err := exec.Command("docker", "network", "create", "-d", "bridge",
        // "bridge0").Run()
      // if err != nil {
        // panic("Failed to create Doker network bridge0")
      // }
      // err := exec.Command("brctl", "addif", "docker0", "enp1s0f0").Run()
      // if err != nil {
        // panic("Failed to attach enp1s0f0 to bridge docker0")
      // }
      // err = exec.Command("brctl", "addif", "docker0", "enp1s0f1").Run()
      // if err != nil {
        // panic("Failed to attach enp1s0f1 to bridge docker0")
      // }
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

      if err := ovs_client.VSwitch.AddBridge(bridge_name); err != nil {
        panic(fmt.Sprintf("Failed to create OVS bridge:", err))
      }
    case SRIOV:
      /* Please setup SRIOV VFs before running this. */

      // pf := "enp2s0np0"
      // err := exec.Command("echo", "16", ">",
        // fmt.Sprintf("/sys/class/net/%s/device/sriov_numvfs",pf)).Run()
      // if err != nil {
        // panic(fmt.Sprintf("Failed to create VFs on PF %s",pf))
      // }
    default:
      fmt.Printf("Unknown network type %s\n", dataplane_type);
      os.Exit(1)
  }

  fmt.Printf("Network type %s succesfully configured!\n", dataplane_type)
}

func attachExtraInterfaces(cname string) {
  var err0, err1 error
  var subnet_prefix string

  /* Facilitate using containers to debug code and match the IP used in most
     test scripts for source and destination. */
  if cname == "src" || cname == "dst" {
    subnet_prefix = "10.10"
  } else {
    subnet_prefix = "192.168"
  }

  switch dataplane_type {
    case BRIDGE:
      /* TODO: Properly use the SDK for this */
      // err0 = exec.Command("docker", "network", "connect", "bridge0", cname).Run()
      /* For now we will just use the docker0 bridge */
    case MACVLAN:
      /* TODO: Properly use the SDK for this */
      err0 = exec.Command("docker", "network", "connect", "macvlan0", cname).Run()
      err1 = exec.Command("docker", "network", "connect", "macvlan1", cname).Run()
    case OVS:
      /* Add interface attached to OVS bridge */
      ipac += 1
      err0 = exec.Command("ovs-docker", "add-port", bridge_name, "eth1", cname,
              fmt.Sprintf("--ipaddress=%s.0.%d/24", subnet_prefix, ipac)).Run()
      err1 = exec.Command("ovs-docker", "add-port", bridge_name, "eth2", cname,
              fmt.Sprintf("--ipaddress=%s.1.%d/24", subnet_prefix, ipac)).Run()
    case SRIOV:
      /* Add 2 VFs to each container */

      /* This is a simple function to return a VF matching the name scheme used
       * by Netronome SmartNICs (used on the tests) */
      get_vfname := func (i *int) (name string) {
        si := 0
        fi := *i + 2
        name = fmt.Sprintf("enp1s%df%d", si, fi)
        *i += 1
        fmt.Printf("VF being used: %s\n", name)
        return name
      }

      ipac += 1
      err0 = exec.Command("pipework", "--direct-phys", get_vfname(&vfcount), "-i", "eth1",
        cname, fmt.Sprintf("192.168.100.%d/24", ipac)).Run()
      ipac += 1
      err1 = exec.Command("pipework", "--direct-phys", get_vfname(&vfcount), "-i", "eth2",
        cname, fmt.Sprintf("192.168.100.%d/24", ipac)).Run()
  }

  if err0 != nil || err1 != nil {
    fmt.Printf("Failed to configure extra interfaces for %s. NetType: %v err0: %v err1: %v\n",
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
      return "eth1","eth2"
    case SRIOV:
      return "eth1","eth2"
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

  /* Add extra interfaces for the dataplane type being used */
  attachExtraInterfaces(name)

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
      return []string{basedir + "user-redirect", ingress, "10.10.0.1"}
    case "xdp-redirect":
      return []string{basedir + "xdp_redirect_map", ingress, egress}
    case "tc-redirect":
      return []string{"--", basedir + "tc_bench01_redirect", "--ingress", ingress,
                        "--egress", egress, "--srcip", "10.10.0.1"}
    case "tc-redirect-swap":
      return []string{"--", basedir + "tc_bench01_redirect", "--ingress", ingress,
                        "--egress", egress, "--srcip", "10.10.0.1", "--swap"}
    case "flow-monitor":
      return []string{"--", basedir + "flow_monitor", "--ingress", ingress,
                        "--egress", egress, "--srcip", "10.10.0.1"}
    case "vpn":
      return []string{"--", basedir + "vpn", ingress, egress,
                        "10.10.0.1", "10.10.0.2", "10.10.0.1", "10.10.0.2"}
    case "l4lb":
      return []string{"--", basedir + "l4lb", ingress, egress}
    case "firewall":
      return []string{"--", basedir + "firewall", ingress, egress,
                        basedir + "firewall-rules.txt"}
    case "chacha":
      return []string{"--", basedir + "chacha", ingress, egress}
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
    fmt.Printf("ERROR: SF type '%s' unknown, not starting SF '%s'...\n", sf.Type,
            sf.Tag)
    return
  }

  fmt.Printf("Starting %s of type %s on iif %s, eif %v\nEntrypoint: %v\n\n",
          sf.Tag, sf.Type, ingress, egress, entrypoint)


  cmdargs := append([]string{"docker", "exec", "-t", sf.Tag}, entrypoint...)

  /* TODO: Too hacky. Use proper Docker SDK funcs instead. */
  go runAndLog(sf.Tag + ".out", cmdargs...)
}

func main() {
  /* Slice containing IDs of all containers created */
  var clist []string
  var cfgfile string
  var nettype string
  var create_srcdst bool
  var err error
  using_direct_links := false
  ingress_ifaces := make(map[string]string)
  egress_ifaces := make(map[string]string)

  flag.StringVar(&cfgfile, "c", "", "path to the chains configuration file.")
  flag.StringVar(&nettype, "n", "bridge", "type of network to use.")
  flag.BoolVar(&create_srcdst, "t", false, "create src and dest containers for testing.")
  flag.Parse()

  if cfgfile == "" {
    fmt.Println("Please provide the path to the chains config file.")
    usage()
    os.Exit(1)
  }

  /* TODO: Avoid this requirement by installing all needed files to a default
   * location like /usr/local/chaining-box */
  if srcdir, varset := os.LookupEnv("CB_DIR"); !varset {
    fmt.Printf("Env vars: %v\n", os.Environ())
    fmt.Println("Please set CB_DIR env var to the path to chaining-box source code.")
    os.Exit(1)
  } else {
    /* Docker requires paths to be absolute */
    source_dir, err = filepath.Abs(srcdir)
    if err != nil {
      fmt.Printf("Error while turning %s into an absolute path.", srcdir)
      os.Exit(1)
    }
  }

  if nettype != string(OVS) && nettype != MACVLAN && nettype != BRIDGE &&
      nettype != SRIOV {
        panic(fmt.Sprintf("Invalid network type: %s", nettype))
  } else {
    dataplane_type = NetType(nettype)
  }

  cfg := ParseChainsConfig(cfgfile)

  createNetworkInfra()

  /* Create src and dest to test chaining (if needed) */
  if create_srcdst {
    newBareContainer := func (name string) {
      _, err := CreateNewContainer(name, source_dir, default_entrypoint)
      if err != nil {
        panic(fmt.Sprintf("Failed to create source container:", err))
      }
      fmt.Printf("Created container: %s\n", name)
    }

    newBareContainer("src")
    newBareContainer("dst")
  }

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
      fmt.Printf("Chain %d has only %d SFs. Not enough. Skipping...\n",
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
