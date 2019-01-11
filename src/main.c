#ifndef _GNU_SOURCE
#define _GNU_SOURCE 
#endif

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <sched.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>

#include "lsdir.h"
#include "cmdwrapper.h"

typedef int (*MainFunctionHandler_t)(int, char **);

typedef struct {
    char *name;
    MainFunctionHandler_t handler;
} Entry_t;

Entry_t g_applets[] = {{"cmdwrapper", cmdwrapper_main}, {"lsdir", lsdir_main}, {"nice", nice_main}};

int main(int argc, char **argv)
{
    char *p_name = basename(argv[0]);
    unsigned int i;
    for(i=0; i<sizeof(g_applets) / sizeof(Entry_t); ++i) {
        if (0 == strcmp(g_applets[i].name, p_name)) {
            return g_applets[i].handler(argc, argv);
        }
    }

    if (argc > 1) {
        for(i=0; i<sizeof(g_applets) / sizeof(Entry_t); ++i) {
            if (0 == strcmp(g_applets[i].name, argv[1])) {
                return g_applets[i].handler(argc-1, &(argv[1]));
            }
        }
    }

    printf("E2iBox v%d\n", 1);
    printf("    Unknown applet: %s\n", p_name);
    printf("    Available applets: ");
    for(i=0; i<sizeof(g_applets) / sizeof(Entry_t); ++i) {
        printf("%s ", g_applets[i].name);
    }
    printf("\n");
    return 0;
}