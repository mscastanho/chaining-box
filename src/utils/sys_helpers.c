#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include <net/if.h>
#include <sys/stat.h>
#include <linux/bpf.h>

#include "sys_helpers.h"

int runcmd(char* format, ...) {
    char cmd[CMD_MAX];
    va_list args;
    int ret;
    va_start(args, format);

    snprintf(cmd,CMD_MAX,format,args);

    ret = system(cmd);
#ifdef DEBUG
    if (!WIFEXITED(ret)) {
        fprintf(stderr,
            "ERR(%d): non-zero return value for cmd: %s\n",
            WEXITSTATUS(ret), cmd);
    }
#endif
    va_end(args);

    return ret;
}

bool validate_ifname(const char* input_ifname, char *output_ifname)
{
	size_t len;
	int i;

	len = strlen(input_ifname);
	if (len >= IF_NAMESIZE) {
		return false;
	}
	for (i = 0; i < len; i++) {
		char c = input_ifname[i];

		if (!(isalpha(c) || isdigit(c)))
			return false;
	}
	strncpy(output_ifname, input_ifname, len);
	return true;
}

bool file_exists(char* filename){
    struct stat buf;
    return (stat(filename, &buf) == 0);
}