#ifndef _XSSH_H
#define _XSSH_H

#define VAR_MAX_LEN 20

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <sys/wait.h>

typedef struct
{
    int count;
    char **vars;
} cmd;

typedef struct mem_obj
{
    char* key;
    char* value;
    struct mem_obj* next;
}mem_object;

#endif /* _XSSH_H */
