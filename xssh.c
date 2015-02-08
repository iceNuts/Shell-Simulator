//Li Zeng 06/Feb/2015

#include "xssh.h"

void shell(cmd*);   // operation branch
void shellInterface();  // shell interface
int isInternalCmd(char*);   // check if cmd is interal
int command(char*, cmd*);   // parsing cmd line
void quitHandler(int);  // handle ctrl+c 
void export(char*, char*);  // set global variable
void unexport(char*);   // unset global variable
void set(char*, char*); // set local variable
void unset(char*);  // unset local variable
void changeDir(cmd*);   // change directory
void exitShell(cmd*);   // exit shell with status
void waitProcess(cmd*); // wait for a process
void show(cmd*);    // display some variables in a new line
char* get_local_var(char*); // find key and return its value in memory
void exec_internal_cmdline(cmd*);   // execute internal command
void exec_external_cmdline(cmd*);   // execute external command

// helper
void print_global_mem();   // print shared memory linklist
void print_local_mem();     // print local memory linklist
void print_mem(char*);
void redirection(char**, int, int*, int*);  // parsing cmd line to redirect stdin/stdout to file

// works for export / unexport
static mem_object* global_root;

// works for set / unset
mem_object* local_root;

int fg; // foreground flag
int show_flag;  // -x flag
int debug_level;    // -d level flag
int foreground_ret_value;   // last foreground return value
int last_background_pid;    // last background pid
FILE *inputFilename;    // redirect input file
FILE *outputFilename;   // redirect output file


int main(int argc, char **argv)
{
    printf("Created By Li Zeng™ 433519. 06/Feb/2015.\n");
    // shape initial input
    global_root = NULL;
    local_root = NULL;
    cmd arguments = {argc, argv};
    shell(&arguments);
    return 0;
}

void shell(cmd* arguments) {
    int argCount = arguments -> count;
    char** args = arguments -> vars;

    // every process should handle ctrl+c
    signal(SIGINT, quitHandler);

    // ignore empty line
    if (argCount <= 0) {
        return;
    }

    // exec once -> xssh -x -f
    if (0 == strcmp(args[0], "./xssh")) {
        int i, j;
        show_flag = -1;
        debug_level = 1;
        for (i = 1; i < argCount; i++) {
            if (0 == strcmp(args[i], "-x")) {
                show_flag = 0;
            }
            // execute at once
            else if (0 == strcmp(args[i], "-f")) {
                // clean argument list
                char** execArgs = (char**)malloc(sizeof(char*)*(argCount-i+1));
                for (j = i+1; j < argCount; j++)
                    execArgs[j-(i+1)] = args[j];
                execArgs[j-(i+1)] = NULL;
                // run and return
                pid_t proc = fork();
                if (proc == 0) {
                    local_root = NULL;
                    execvp(execArgs[0], execArgs);
                    exit(0);
                }
                else {
                    int childStatus;
                    wait(&childStatus);
                    foreground_ret_value = WEXITSTATUS(childStatus);
                    return;
                }
            }
            else if (0 == strcmp(args[i], "-d")) {
                debug_level = atoi(args[i+1]);
            }
        }
    }

    // exec a new shell
    if (0 == strcmp(args[0], "./xssh")) {
        pid_t shellPid = fork();
        // new shell
        if (shellPid == 0) {
            fg = 0;
            local_root = NULL;
            shellInterface();
        }
        else if (shellPid < 0) {
            printf("Sorry, it fails to start shell right now.\n");
        }
        // parent 
        else {
            fg = -1;
            int childStatus;
            wait(&childStatus);
            fg = 0;
            foreground_ret_value = WEXITSTATUS(childStatus);
        }
    }
    // internal exec starting at initial
    else if (0 == isInternalCmd(args[0])) {
        exec_internal_cmdline(arguments);
    }
    // external exec
    else {
        exec_external_cmdline(arguments);
    }
}

void shellInterface() {
    printf("Welcome to xssh!\n");
    while (1) {
        char input[300];
        printf(">> ");
        fgets(input, 300, stdin);
        // clean input
        int i = 0;
        for (i = 0; i < strlen(input); i++) {
            if (input[i] == '\n') {
                input[i] = '\0';
                break;
            }
        }
        cmd arguments;
        if (0 == command(input, &arguments))
            shell(&arguments);
    }
}

int command(char* input, cmd* arguments) {
    if (strlen(input) <= 0) {
        return -1;
    }
    int argc = 0;
    char **argv = (char**)malloc(sizeof(char*)*15);
    char* pch;
    pch = strtok(input, " \t");
    while (pch != NULL) {
        if (strlen(pch) > VAR_MAX_LEN) {
            printf("Command error, max command length is %d.\n", VAR_MAX_LEN);
            return -1;
        }
        char initial = pch[0];
        if ('#' == initial)
            break;
        argv[argc] = (char*)malloc(sizeof(char)*(strlen(pch)+1));
        strcpy(argv[argc++], pch);
        pch = strtok(NULL, " \t");
    }
    cmd _arguments = (cmd){
        .count = argc,
        .vars = argv
    };
    *arguments = _arguments;
    return 0;
}

int isInternalCmd(char* cmd) {
    char* cmds[9] = {
        "show",
        "set",
        "unset",
        "export",
        "unexport",
        "chdir",
        "exit",
        "wait",
        "mm",
    };
    int i;
    for (i = 0; i < 9; i++) {
        if (0 == strcmp(cmd, cmds[i])) {
            return 0;
        }
    }
    return 1;
}

void quitHandler(int sig) {
    if (0 == fg) {
        printf("\nBye!\n");
        exit(0);
        foreground_ret_value = 0;
    }
}

void export(char* key, char* value) {

    if (strlen(value) > VAR_MAX_LEN || strlen(key) > VAR_MAX_LEN) {
        printf("Export failed, max variable length is %d.\n", VAR_MAX_LEN);
        return;
    }

    int flag = -1;
    mem_object* p = global_root;
    while (p != NULL) {
        if (0 == strcmp(p -> key, key)) {
            // no more new space
            // use old one to be shared
            strcpy(p -> value, value);
            flag = 0;
            break;
        }
        p = p -> next;
    }

    // create new stuff
    if (0 != flag) {
        mem_object* obj = mmap(
            NULL, 
            sizeof(mem_object), 
            PROT_READ | PROT_WRITE, 
            MAP_SHARED | MAP_ANON, 
            -1, 
            0);
        obj -> key = mmap(
            NULL, 
            sizeof(char)*(strlen(key)+1), 
            PROT_READ | PROT_WRITE, 
            MAP_SHARED | MAP_ANON, 
            -1, 
            0);
        obj -> value = mmap(
            NULL, 
            sizeof(char)*(VAR_MAX_LEN), 
            PROT_READ | PROT_WRITE, 
            MAP_SHARED | MAP_ANON, 
            -1, 
            0);
        strcpy(obj -> key, key);
        strcpy(obj -> value, value);
        obj -> next = NULL;

        if (NULL == global_root) {
            global_root = obj;
        }
        else {
            obj -> next = global_root;
            global_root = obj;
        }
    }
}

void unexport(char* key) {
    int flag = -1;
    mem_object* p = global_root;
    mem_object* _p = NULL;
    while (p != NULL) {
        if (0 == strcmp(p -> key, key)) {
            flag = 0;
            if (_p != NULL)
                _p -> next = p -> next;
            break;
        }
        _p = p;
        p = p -> next;
    }
    if (0 == flag) {
        printf("Unexport global var: %s -> %s\n", p->key, p->value);
        if (p == global_root) {
            global_root = global_root -> next;
        }
        munmap(p->key, sizeof(p->key));
        munmap(p->value, sizeof(p->value));
        munmap(p, sizeof(mem_object));
    }
    else {
        printf("No such global variable!\n");
    }
}

void set(char* key, char* value) {

    if (strlen(value) > VAR_MAX_LEN || strlen(key) > VAR_MAX_LEN) {
        printf("Set failed, max variable length is %d.\n", VAR_MAX_LEN);
        return;
    }

    int flag = -1;
    mem_object* p = local_root;
    while (p != NULL) {
        if (0 == strcmp(p -> key, key)) {
            free(p -> value);
            p -> value = NULL;
            p -> value = malloc(sizeof(char)*strlen(value));
            strcpy(p -> value, value);
            flag = 0;
            break;
        }
        p = p -> next;
    }

    // create new stuff
    if (0 != flag) {
        mem_object* obj = (mem_object*)malloc(sizeof(mem_object));
        obj -> key = (char*)malloc(sizeof(char)*(strlen(key)+1));
        obj -> value = (char*)malloc(sizeof(char)*(VAR_MAX_LEN));
        strcpy(obj -> key, key);
        strcpy(obj -> value, value);
        obj -> next = NULL;

        if (NULL == local_root) {
            local_root = obj;
        }
        else {
            obj -> next = local_root;
            local_root = obj;
        }
    }
}

void unset(char* key) {
    int flag = -1;
    mem_object* p = local_root;
    mem_object* _p = NULL;
    while (p != NULL) {
        if (0 == strcmp(p -> key, key)) {
            flag = 0;
            if (_p != NULL)
                _p -> next = p -> next;
            break;
        }
        _p = p;
        p = p -> next;
    }
    if (0 == flag) {
        printf("Unset local var: %s -> %s\n", p->key, p->value);
        free(p -> key);
        free(p -> value);
        free(p);
        p -> key = NULL;
        p -> value = NULL;
        if (p == local_root) {
            p = NULL;
            local_root = local_root -> next;
        }
    }
    else {
        printf("No such local variable!\n");
    }
}

void changeDir(cmd* arguments) {
    char *oldPath = (char*)malloc(sizeof(char)*100);
    getcwd(oldPath, 100);
    const char* path = arguments -> vars[1];
    chdir(path);
    char *newPath = (char*)malloc(sizeof(char)*100);
    getcwd(newPath, 100);
    printf("%s -> %s\n", oldPath, newPath);
}

void exitShell(cmd* arguments) {
    printf("Bye!\n");
    exit(atoi(arguments -> vars[1]));
    foreground_ret_value = atoi(arguments -> vars[1]);
}

void waitProcess(cmd* arguments) {
    int status;
    pid_t pid = atoi(arguments -> vars[1]);
    if (-1 == pid) {
        waitpid(-1, &status, 0);
        printf("All child processes complete.\n");
    }
    else {
        waitpid(pid, &status, 0);
        printf("Process %d complete.\n", pid);
    }
}

char* get_local_var(char* key) {
    int flag = -1;
    mem_object* p = local_root;
    if (p != NULL)
    {
        while (p != NULL) {
            if (0 == strcmp(p -> key, key)) {
                flag = 0;
                break;
            }
            p = p -> next;
        }
    }
    if (0 == flag) {
        return p -> value;
    }
    else {
        printf("No such local variable!\n");
        return NULL;
    }
}



void show(cmd* arguments) {
    int i;
    int argCount = arguments -> count;
    char** args = arguments -> vars;
    // show replaced cmdline
    if (0 == show_flag) {
        for (i = 0; i < argCount; i++) {
            char initial = args[i][0];
            if (initial == '#') {
                break;
            }
            else if (initial == '$') {
                if (2 == strlen(args[i]))
                {
                    char ending = args[i][1];
                    int magic = -1;
                    if (ending == '$') {
                        magic = 0;
                        printf("%d ", getpid());
                    }
                    else if (ending == '?') {
                        magic = 0;
                        printf("%d ", foreground_ret_value);
                    }
                    else if (ending == '!') {
                        magic = 0;
                        printf("%d ", last_background_pid);
                    }
                    if (0 == magic)
                        continue;
                }
                char* key = (char*)malloc(sizeof(char)*strlen(args[i]));
                memcpy(key, &args[i][1], strlen(args[i])-1);
                char* value = get_local_var(key);
                if (value != NULL)
                    printf("%s ", value);
            }
            else {
                printf("%s ", args[i]);
            }
        }
        int j;
        for (j = i; i < argCount; j++)
            printf("%s ", args[j]);
        printf("\n");
    }
    // print result
    for (i = 1; i < argCount; i++) {
        char initial = args[i][0];
        // comment skip all remaining
        if (initial == '#') 
            break;
        // var substitution or magic var
        else if (initial == '$') {
            // magic var
            if (2 == strlen(args[i]))
            {
                char ending = args[i][1];
                int magic = -1;
                if (ending == '$') {
                    magic = 0;
                    printf("%d ", getpid());
                }
                else if (ending == '?') {
                    magic = 0;
                    printf("%d ", foreground_ret_value);
                }
                else if (ending == '!') {
                    magic = 0;
                    printf("%d ", last_background_pid);
                }
                if (0 == magic)
                    continue;
            }
            char* key = (char*)malloc(sizeof(char)*strlen(args[i]));
            memcpy(key, &args[i][1], strlen(args[i])-1);
            char* value = get_local_var(key);
            if (value != NULL)
                printf("%s ", value);
        }
        // normal print
        else {
            printf("%s ", args[i]);
        }
    }
    printf("\n");
}

void exec_internal_cmdline(cmd* arguments) {
    int argCount = arguments -> count;
    char** args = arguments -> vars;

    if (argCount < 2) {
        printf("Less input variable?\n");
        return;
    }

    char* action = args[0];
    if (0 == strcmp("show", action))
        show(arguments);
    else if (0 == strcmp("set", action) && argCount == 3)
        set(args[1], args[2]);
    else if (0 == strcmp("unset", action) && argCount == 2)
        unset(args[1]);
    else if (0 == strcmp("export", action) && argCount == 3)
        export(args[1], args[2]);
    else if (0 == strcmp("unexport", action) && argCount == 2)
        unexport(args[1]);
    else if (0 == strcmp("chdir", action) && argCount == 2)
        changeDir(arguments);
    else if (0 == strcmp("exit", action) && argCount == 2)
        exitShell(arguments);
    else if (0 == strcmp("wait", action) && argCount == 2)
        waitProcess(arguments);
    else if (0 == strcmp("mm", action) && argCount == 2)
        print_mem(args[1]);
    else 
        printf("Internal commands format issue.\n");
}

void exec_external_cmdline(cmd* arguments) {
    int argCount = arguments -> count;
    char** args = arguments -> vars;

    char* action = args[0];
    int background_flag = (0 == strcmp(args[argCount-1], "&"))? 0 : -1;

    // file not found -> deep search
    char* newAction = NULL;
    char* path = (char*)malloc(sizeof(char)*strlen(getenv("PATH")));
    strcpy(path,getenv("PATH"));
    if (access(action, F_OK) == -1 && NULL != path) {
        printf("Searching %s in PATH ...", action);
        char* prefix = strtok(path, ":");
        int found_flag = -1;
        while (prefix != NULL) {
            free(newAction);
            newAction = NULL;
            newAction = (char*)malloc(sizeof(char)*(strlen(prefix)+strlen(action)+2));
            strcpy(newAction, prefix);
            strcat(newAction, "/");
            strcat(newAction, action);
            if (access(newAction, F_OK) != -1 && access(newAction, X_OK) != -1) {
                free(action);
                action = NULL;
                action = newAction;
                found_flag = 0;
                printf(" -> %s\n", action);
                printf("--------------------------------------------------------------------------\n");
                break;
            }
            prefix = strtok(NULL, ":");
        }
        if (0 != found_flag) {
            printf(" not found \n");
            return;
        }
    }

    if (access(action, F_OK) != -1 && access(action, X_OK) != -1) {
        // clear argument list
        char** execArgs = malloc(sizeof(char*)*(argCount+1));
        int i;
        int redirect_flag = -1;
        for (i = 0; i < argCount; i++) {
            if (0 == strcmp(args[i], "&"))
                break;
            else if (0 == strcmp(args[i], "<") || 0 == strcmp(args[i], ">")) {
                redirect_flag = 0;
                continue;
            }
            if (0 == redirect_flag)
                continue;
            execArgs[i] = args[i];
        }
        execArgs[i] = NULL;

        int link[2];
        int fileIn = 0;
        int fileOut = 0;
        if (0 != background_flag) {
            pipe(link);
            redirection(args, argCount, &fileIn, &fileOut);
        }

        pid_t proc = fork();
        // run program
        if (proc == 0) {
            local_root = NULL;
            if (0 != background_flag) {

                if (fileOut) {
                    fflush(stdout);
                    dup2(fileno(outputFilename), STDOUT_FILENO);
                    fclose(outputFilename);
                }

                if (fileIn) {
                    fflush(stdin);
                    dup2(fileno(inputFilename), STDIN_FILENO);
                    fclose(inputFilename);
                }

                if (!fileOut) {
                    // capture output to tty
                    close(link[0]); // no need to read
                    dup2(link[1], 1);   // redirect stdout
                    dup2(link[1], 2);   // redirect stderr
                }
            }
            // detached
            else {
                setpgid(0, 0);
                setsid();
            }
            execvp(action, execArgs);
            exit(0);
        }
        else if (proc < 0) {
            printf("Sorry, this file failed to execute right now.\n");
        }
        // shell 
        else {
            if (0 != background_flag) {
                // capture output
                char buffer[1];
                close(link[1]); // parent doesn't write
                while(read(link[0], buffer, sizeof(buffer)) > 0) {
                    write(1, buffer, 1);
                }
                close(link[0]);
                // wait
                printf("\nPress ctrl+d to stop sub task.\n");
                int childStatus;
                wait(&childStatus);
                // clear up
                free(newAction);
                newAction = NULL;
                free(execArgs);
                execArgs = NULL;
            }
            else {
                last_background_pid = proc;
            }
        }
    }
    // not executable
    else {
        printf("This file is not executable, not enough permission?\n");
    }
}

// helper

void print_mem(char* flag) {
    if (0 == strcmp("0", flag)) {
        print_local_mem();
    }
    else if (0 == strcmp("1", flag)) {
        print_global_mem();
    }
}

void print_global_mem() {
    mem_object* p = global_root;
    while (p != NULL) {
        printf("%s %s\n", p -> key, p -> value);
        p = p -> next;
    }
}

void print_local_mem() {
    mem_object* p = local_root;
    while (p != NULL) {
        printf("%s %s\n", p -> key, p -> value);
        p = p -> next;
    }
}

void redirection(char **cmdString,int numCmd, int *inputFile, int *outputFile) {
    int i;
    for (i = 0; i < numCmd; i++) {
        if (0 == strcmp(cmdString[i],"<")) {
            *inputFile = 1;
            inputFilename = fopen(cmdString[i+1],"r");
        } else if (0 == strcmp(cmdString[i],">")) {
            *outputFile = 1;
            outputFilename = fopen(cmdString[i+1],"wb");
        }
    }
}
















