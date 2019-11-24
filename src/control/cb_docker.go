package main

import (
  // "bytes"
  "context"
  "errors"
  "fmt"
  "io/ioutil"
  "log"
  "os"
  "os/exec"
  "encoding/json"

  /* Interaction with Docker Engine */
  "github.com/docker/docker/api/types"
  "github.com/docker/docker/api/types/container"
  "github.com/docker/docker/api/types/mount"
  "github.com/docker/docker/client"
  "github.com/docker/go-units"

  /* Handling point-to-point links using veth pairs */
  "control/koko"
)

type CBNodeType int

const (
  SF  CBNodeType = iota   /* Service Function */
  CLS                     /* Classifier */
)

type CBAddress [6]byte

type CBInstance struct {
  Tag string
  Type string
  Remote bool
  ContainerId string
  Address CBAddress
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

func (cfg *CBConfig) GetNodeByName(name string) *CBInstance{
  return &cfg.Functions[cfg.name2idx[name]]
}

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

/* --- These should be in sync with src/headers/cb_common.h --- */
// type Fwd_entry struct {
  // flags uint8     `json:flags`
  // address [6]byte `json:address`
// }
//
// type cls_entry struct {
  // sph uint32       `json:sph`
  // next_hop [6]byte `json:next_hop`
// }

/* --- --- */

/* Docker image to be used by containers */
const default_image = "docker.io/mscastanho/chaining-box:cb-node"

/* Default directory containing the eBPF programs*/
const progs_dir = "src/build"

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

func CreateNewContainer(name string) (string, error) {
  ctx := context.Background()

  /* Create instance of client to interact with Docker Engine */
  cli, err := client.NewEnvClient()
	if err != nil {
		fmt.Println("Unable to create docker client")
		panic(err)
	}

  /* Download image from DockerHub if needed */
  _, err = cli.ImagePull(ctx, default_image, types.ImagePullOptions{})
	if err != nil {
    panic(err)
  }

  /* Get definition of memlock ulimit */
  memlock,_ := units.ParseUlimit("memlock=-1")

  /* Create container with all custom configuration needed */
  cont, err := cli.ContainerCreate(
		ctx,
		&container.Config{
      Hostname: name,
			Image: default_image,
      /* TODO: Should be command to run the corresponding SF */
      Entrypoint: []string{"tail","-f","/dev/null"},
		},
		&container.HostConfig {
      /* Containers need to run in privileged mode so they can perform
       * sysadmin operations from inside the container.
       *   Ex: loading BPF programs.*/
      Privileged: true,
      /* Remove container when process finishes */
      AutoRemove: true,
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

func ConfigureStages(container string, ntype CBNodeType, iface string) error {
  var obj string

  switch ntype {
    case SF:
      obj = "sfc_stages_kern.o"
    case CLS:
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

func ParseChainsConfig(cfgfile string) (cfg CBConfig){
  /* TODO: Loading the entire file to memory may fail if the file is too big */
  cfgjson, err := ioutil.ReadFile(cfgfile)
  if err != nil {
    panic(err)
  }

  json.Unmarshal([]byte(cfgjson), &cfg)
  /* TODO: Validate Chain Id is < 2^24 */
  /* TODO: Validate taht chain lengths are < 256 */
  // fmt.Printf("Chains: %v\nFunctions: %v\n", cfg.Chains, cfg.Functions)
  return cfg
}

func GenerateRules(cfg CBConfig) (rules map[string][]Fwd_rule) {
  var sph uint32
  var r Fwd_rule
  var flags uint8
  var address_next CBAddress

  /* Each node will have a list of rules */
  rules = make(map[string][]Fwd_rule)

  for _,chain := range cfg.Chains {
    spi := chain.Id << 24
    for i,node := range chain.Nodes {
      sph = spi | (255-uint32(i))

      /* Is it the end of the chain? */
      if i == len(chain.Nodes) - 1 {
        flags = 1
        address_next = CBAddress{0x00,0x00,0x00,0x00,0x00,0x00}
      } else {
        flags = 0
        address_next = (cfg.GetNodeByName(chain.Nodes[i+1])).Address
      }

      r.Key = sph
      r.Val = Fwd_entry{Flags: flags, Address: address_next}
      rules[node] = append(rules[node], r)
    }
  }

  return rules
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
  cfg.name2idx = make(map[string]int)

  /* Start containers for all functions declared */
  for i := 0 ; i < len(cfg.Functions) ; i++ {
    sf := &cfg.Functions[i]

    /* TODO: We should only start containers for functions that are NOT
     * remote. This is not being done now because the current tests will
     * all take place on the same host, but we also want to exercise the
     * connections WITHOUT direct links (veth pairs), so we use the remote
     * param to force connections between some containers to not use veth.*/
    id, err := CreateNewContainer(sf.Tag)
    if err != nil {
      fmt.Println("Failed to start container!")
      panic(err)
    }

    clist = append(clist,id)
    sf.ContainerId = id
    fmt.Printf("Container %s(%s) started!\n", sf.Tag, id[:6])

    /* Add name -> index mapping to internal map */
    cfg.name2idx[sf.Tag] = i
  }

  fmt.Printf("Chains: %v\nFunctions: %v\n", cfg.Chains, cfg.Functions)
  // fmt.Printf("name2idx: %v\n", cfg.name2idx)

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
      /* TODO: Encapsulate this on a method for CBConfig struct */
      n1 := &cfg.Functions[cfg.name2idx[chain.Nodes[i-1]]]
      n2 := &cfg.Functions[cfg.name2idx[chain.Nodes[i]]]

      fmt.Printf("Node1: %v\nNode2: %v\n\n", n1, n2)
      /* We can only create veth pairs between local nodes */
      if !n1.Remote && !n2.Remote {
        _,_ = CreateDirectLink(n1.ContainerId,n2.ContainerId)
      }
    }
  }

  rules := GenerateRules(cfg)

  fmt.Printf("Rules: %v\n", rules)
  // for node, node_rules := range rules {
    // fmt.Printf("Rules for node %s:\n", node)
    // fmt.Printf(" => Raw: %v\n\n",node_rules)
    // data, err := json.MarshalIndent(CBRulesConfig{Fwd:node_rules},"","  ")
    // if err != nil {
      // fmt.Println("Smth went wrong =(")
    // }
    // fmt.Printf(" => JSON: %s\n\n", string(data))
  // }
}
