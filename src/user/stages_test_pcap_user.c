#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <pcap.h>
#include <string.h>
#include <linux/if_ether.h>
#include <errno.h>
#include <limits.h>

#include <bpf/bpf.h>
#include <bpf/libbpf.h>

#include "load_helpers.h"
#include "sys_helpers.h"
#include "cb_common.h"
#include "nsh.h"

#define MAXMTU 1500

/* Flags */
#define SHOW_BEFORE 0x1
#define SHOW_AFTER	0x2

struct args {
		int progfd;
		unsigned char *pktbuf;
		size_t buflen;
		int count;
		int flags;
		pcap_dumper_t *dumper;
};

/* Map handles */
int nsh_data = -1;
int src_mac = -1;
int fwd_table = -1;

void hex_dump(char* desc, void* addr, int len){
		int i;
		unsigned char buff[17];
		unsigned char *pc = (unsigned char*)addr;

		// Output description if given.
		if (desc != NULL)
				printf ("%s:\n", desc);

		if (len == 0) {
				printf("  ZERO LENGTH\n");
				return;
		}
		if (len < 0) {
				printf("  NEGATIVE LENGTH: %i\n",len);
				return;
		}

		// Process every byte in the data.
		for (i = 0; i < len; i++) {
				// Multiple of 16 means new line (with line offset).

				if ((i % 16) == 0) {
						// Just don't print ASCII for the zeroth line.
						if (i != 0)
								printf ("  %s\n", buff);

						// Output the offset.
						printf ("  %04x ", i);
				}

				// Now the hex code for the specific character.
				printf (" %02x", pc[i]);

				// And store a printable ASCII character for later.
				if ((pc[i] < 0x20) || (pc[i] > 0x7e))
						buff[i % 16] = '.';
				else
						buff[i % 16] = pc[i];
				buff[(i % 16) + 1] = '\0';
		}

		// Pad out last line if not exactly 16 characters.
		while ((i % 16) != 0) {
				printf ("   ");
				i++;
		}

		// And print the final ASCII bit.
		printf ("  %s\n", buff);
}

void try_message(){
		printf("Try 'ebpflow-test -h' for more information.\n");
}

void usage(){
		printf(
				"Usage: ebpflow-test [FLAGS] -f <pcap-file> <ebpf-file.o>\n"
				"Tool to test eBPF code on eBPFlow Switch emulator.\n"
				"\n"
				"Options:\n"
				"   -f PCAP-FILE             Input pcap file (required)\n"
				"   -r RULES-FILE            Rules to be added to maps before running the code\n"
				"   -b                       Show packet before running code\n"
				"   -a                       Show packet after running code\n"
				"   -o FILE                  Output modified packets to file\n"
				"   -h                       Print this help message\n"
		);
}

// int insert_map_rules(struct ebpflow_fw *fw, char *inrules){
	// FILE *fin;
	// char buf[1024];
	// char map_name[64];
	// uint64_t key = 0, val = 0, mask = 0;
//
	// fin = fopen(inrules,"r");
	// if(fin == NULL){
		// return -1;
	// }
//
	// int c = 0;
	// while(fgets(buf,sizeof(buf),fin)){
		// if(buf[0] == '#' || buf[0] == '\n') // Skip comment and empty lines
			// continue;
//
		// sscanf(buf,"%s 0x%lx 0x%lx 0x%lx",map_name,&key,&mask,&val);
//
		// ebpflow_soft_map_insert(fw,map_name,key,val);
	// }
// }

void run_code(unsigned char *args, const struct pcap_pkthdr *meta, const unsigned char *packet){
		struct args *myargs = (struct args *) args;
		int progfd = myargs->progfd;
		unsigned char *buf = myargs->pktbuf;
		size_t buflen = myargs->buflen;
		int flags = myargs->flags;
		struct timeval t;
		uint32_t retval;
		uint32_t duration;
		size_t totlen =  meta->caplen;
		void *pkt_init;

		myargs->count++;
		printf("[Packet #%d] ",myargs->count);

		if(totlen > buflen){
				printf("Packet too big: %lu. Skipping...\n",totlen);
				return;
		}

		// We have to copy the packet because metadata + pkt should
		// be contiguous in memory
		// pkt_init = buf+sizeof(struct metadatahdr);
		// memcpy(pkt_init,packet,meta->caplen);

		if(flags & SHOW_BEFORE)
			hex_dump("\nPacket before",pkt_init,meta->caplen);

    printf("Testing program:\n");
    int err = bpf_prog_test_run(progfd, 1, &buf, totlen,
        NULL, NULL, &retval, &duration);
    printf("Result: retval=%d duration=%d nsecs\n", retval, duration);

		if(flags & SHOW_AFTER)
			hex_dump("\nPacket after",pkt_init,meta->caplen);

		if(myargs->dumper){
			pcap_dump((unsigned char *)myargs->dumper,meta,pkt_init);
		}
}

int main(int argc, char** argv){
  int opt;
  char *filepath = NULL;
  char *inpcap = NULL;
  char *outpcap = NULL;
  char *inrules = NULL;
  char error_buffer[PCAP_ERRBUF_SIZE];
  void *code;
  uint32_t code_len;
  unsigned char *buf;
  size_t buflen = 0;
  int ret;
  int flags = 0;
  pcap_dumper_t *dumper = NULL;
	struct bpf_object *obj;

  // Flags
  int dry_run = 0;

  while( (opt = getopt(argc,argv,"f:r:abo:p:h")) != -1){
      switch(opt){
          case 'f':
              inpcap = optarg;
              break;
          case 'r':
              inrules = optarg;
              break;
          case 'a':
              flags |= SHOW_AFTER;
              break;
          case 'b':
              flags |= SHOW_BEFORE;
              break;
          case 'o':
              outpcap = optarg;
              break;
          case 'h':
              usage();
              return 0;
          default:
              try_message();
              exit(1);
              break;
      }
  }

  if(inpcap == NULL){
    printf("Expected pcap file.\n");
    try_message();
    return 1;
  }

  if(argc == optind){
    printf("Expected .o file to load.\n");
    try_message();
    return 1;
  }

  // Filename should always be the last argument
  filepath = argv[optind];

  /* Load program */
  obj = bpf_object__open(filepath);

  struct bpf_program *p;
  int cnt = 0;
  bpf_object__for_each_program(p,obj){
    const char* name = bpf_program__title(p,false);
    enum bpf_prog_type pt;
    enum bpf_attach_type att;

    if(libbpf_prog_type_by_name(name,&pt,&att)){
      printf("ERR: Could not infer type for prog \"%s\"\n", name);
      bpf_object__close(obj);
      return -1;
    }

    // Set prog type (inferred from sec name)
    bpf_program__set_type(p, pt);
  }

  bpf_object__load(obj);

  struct bpf_map *m;
  m = bpf_object__find_map_by_name(obj,"nsh_data");
  if(!m) printf("ERR: Could not find map \"nsh_data\"\n");
  else nsh_data = bpf_map__fd(m);

  m = bpf_object__find_map_by_name(obj,"src_mac");
  if(!m) printf("ERR: Could not find map \"src_mac\"\n");
  else src_mac = bpf_map__fd(m);

  m = bpf_object__find_map_by_name(obj,"fwd_table");
  if(!m) printf("ERR: Could not find map \"fwd_table\"\n");
  else fwd_table = bpf_map__fd(m);

  if(nsh_data == -1 || src_mac == -1 || fwd_table == -1)
    printf("ERR: Could not get handle for maps\n");
  else
    printf("nsh_data = %d\nsrc_mac = %d\nfwd_table = %d\n",
        nsh_data, src_mac, fwd_table);

	// Attach Dec stage to corresponding iface
	struct bpf_program *dec = bpf_object__find_program_by_title(
                obj, "xdp/decap");

	struct bpf_program *enc = bpf_object__find_program_by_title(
                obj, "action/encap");

	struct bpf_program *fwd = bpf_object__find_program_by_title(
                obj, "action/forward");

  if(!dec){
    ret = -1;
    goto CLEANUP;
  }

  int dec_fd = bpf_program__fd(dec);
  int enc_fd = bpf_program__fd(enc);
  int fwd_fd = bpf_program__fd(fwd);

  if(dec_fd == -1){
    ret = -1;
    goto CLEANUP;
  }

  printf("dec_fd = %d // enc_fd = %d // fwd_fd = %d\n",
      dec_fd, enc_fd, fwd_fd);
    
  /* Load rules to maps */

  // Buffer to hold packet
  buflen = MAXMTU;
  buf = (unsigned char*) calloc(buflen,1);

  pcap_t *handle = pcap_open_offline(inpcap, error_buffer);

  if(outpcap != NULL){
    dumper = pcap_dump_open(handle,outpcap);
  }

  struct args myargs = {enc_fd,buf,buflen,0,flags,dumper};

  // -1 will cause to process the entire file
  pcap_loop(handle, -1, run_code, (unsigned char*)&myargs);

  printf("\n");

  /* Call map_inspector to dump maps? */
  
  if(dumper != NULL){
    pcap_dump_close(dumper);
  }

  CLEANUP:
    bpf_object__close(obj);

    return ret;
}
