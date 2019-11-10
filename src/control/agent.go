package main

import (
  "context"
  "fmt"

  "github.com/docker/docker/api/types"
  "github.com/docker/docker/api/types/container"
  "github.com/docker/docker/api/types/mount"
  "github.com/docker/docker/client"
  "github.com/docker/go-units"
)

/* Docker image to be used by containers */
const default_image = "docker.io/mscastanho/chaining-box:cb-node"

/* Default directory containing the eBPF programs*/
const progs_dir = "../src/build"

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
			Image: default_image,
      /* TODO: Should be command to run the corresponding SF */
      Entrypoint: []string{"tail","-f","/dev/null"},
		},
		&container.HostConfig {
      /* Containers need to run in privileged mode so they can perform
       * sysadmin operations from inside the container.
       *   Ex: loading BPF programs.*/
      Privileged: true,

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
          Source: "/home/mscastanho/devel/chaining-box",
          Target: "/usr/src/chaining-box",
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
    fmt.Printf("Container %s(%s) started!\n", cname, id[:4])
  }

}
