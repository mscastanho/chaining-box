package main

import (
  // "bytes"
  "context"
  "errors"
  "fmt"
  "log"
  "os"
  "os/exec"

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

/* Docker image to be used by containers */
const default_image = "docker.io/mscastanho/chaining-box:cb-node"

/* Default directory containing the eBPF programs*/
const progs_dir = "src/build"

/* Base chaining-box dir */
/* TODO: This should be configured through CLI */
var source_dir = "/home/mscastanho/devel/chaining-box"
var target_dir = "/usr/src/chaining-box"

/* Direct Link global ID */
var dlid = 0


func getDirectLinkNames() (string,string) {
  l0 := fmt.Sprintf("dl%di0",dlid)
  l1 := fmt.Sprintf("dl%di1",dlid)
  dlid++
  return l0,l1
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

func main() {
  /* Slice containing IDs of all containers created */
  var clist []string

  /* TODO: Make the number of containers to be created configurable */
  for i := 0; i < 2; i++ {
    cname := fmt.Sprintf("sfc%d",i)
    id, err := CreateNewContainer(cname)
    if err != nil {
      fmt.Println("Failed to start container!")
      panic(err)
    }

    clist = append(clist,id)
    fmt.Printf("Container %s(%s) started!\n", cname, id[:6])
  }

  i0, i1 := CreateDirectLink(clist[0],clist[1])

  ConfigureStages(clist[0],CLS,i0)
  ConfigureStages(clist[1],SF,i1)
}
