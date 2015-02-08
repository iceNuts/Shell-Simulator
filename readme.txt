Name: Li Zeng 433519

Design

    The shell code is based on a few system calls simulating the real shell's behaviour. In xssh.h header, I define two C structs. One is command line model with arguments list and its count, this is the same as main function arguments. The other is for memory model which simulating key-value storage and is a link list as its length is varied. It has O(n) search time, so that all operations for the key-value memory is O(n). It works well for shared memory which is mainly for global operations like "export / unexport", so the parent's global variable could be changed by its child and the result would reflect to parent even if the child exits. It surpass the process-specific memory restriction.

    As for xssh shell itself, I write a shell function to switch each operation to its way, they are:

    1.  empty line like simply enter or ctrl+D
    2.  ctrl+c
    3.  executable command with -f flag
    4.  a new shell with ./xssh -x(optional)
    5.  internal command like show, set, etc.
    6.  external command like php, ls -al, etc.

    Shell interface function is the place to combine operation switch and command line parsing which is handled by command function. To avoid command line is too big to hold, I assume the maximum variable lenght is 20.

    There is a tricky point to handle ctrl+c signal or SIGINT. As this signal is sent to all process in the same process group. To avoid killing other processes, the foreground flag is used, as the flag is process specific, it is easy to identify which one is foreground and which one is on wait.

    To help debug, there is a function called print_mem, which is used to print out all linked list in shared memory.


Assumptions
    
    Besides the requirement, there are some other assumptions for xssh. Search path is based on getenv() system call, however, there is no way to showing alias like "~" or "ll". 

    Shell script with -f flag is treated like external command but could NOT run in background.

    Global variables are based on shared memory, so it could be manipulated by child process:

            >> export x 10
            >> ./xssh 
            >> export x 5
            >> ^C
            ...
            # now x is 5 

FAQ

How long did the lab take?


    About 10-15 hrs including fully test and debug.


Does your lab work as expected?

    
    It almost works for most senarios, but I may miss some cases.


Are there any issues?


    It is fully tested as far as I know under the sp15 requirements. 


How did you test your lab to make sure all functionality was implemented correctly?

    There are multiple way to test the functionalities:

    1. Test one layer shell, internal and external command, redirection etc.
    2. Test nested shells, internal and external command, redirection etc.
    3. Test export / unexport via ctrl+c and sub process.
    4. Ask others to play the shell and help find more bugs.
    
