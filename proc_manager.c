#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include "dictionary_hashtable.c"

int fileCheck(int cmdsize, char *input);
int Restart(char* command, int index);

int main(int argc, char *argv[]) {

    char cmd[50], //stores line of cmdfile
    *filename, //stores cmdfile's name
    *cmdargs[10]; // will be used to break cmd down to prep for execvp
    int cmdNum=0, // tracks current line to be executed
    fdout=1, fderr=2;// file descriptors for PID.out and PID.err respectively

    struct timespec start, finish; //used to record start and finish times of each command
    struct nlist *enter, *collect; //used to record commands into hashtable
    long elapsed = 0; // finish - start

    //validate file input
    if(fileCheck(argc, argv[1]) == 1){
        return 1;
    }
    else
        filename = argv[1];

    //open file
    FILE* file = fopen(filename, "r");

    //scan cmdfile
    while(fgets(cmd, 50, file) != NULL){

        //increment line #
        ++cmdNum;

        //replace final newline with space since fgets() stops at newline
        if(cmd[strlen(cmd)-1] == '\n')
            cmd[strlen(cmd)-1] = ' ';

        clock_gettime(CLOCK_MONOTONIC, &start);

        //fork
        pid_t pid = fork();

        //fork fail case
        if(pid<0) {
            printf("Fork failed");
            return 1;
        }

        //child
        if(pid == 0) {
            //prepare for execvp, extract and store tokens in cmdargs
            int child = getpid(), parent = getppid();
            char *segment = strtok(cmd, " ");
            int index = 0;
            while (segment != NULL) {
                cmdargs[index] = segment;
                index++;
                segment = strtok(NULL, " ");
            }
            cmdargs[index] = NULL;

            //rm confirmation
            if(strcmp(cmdargs[0],"rm") == 0){
                char confirm;
                scanf("You want to remove a file. Press n to cancel now, or press any other key to continue: %c\n", &confirm);
                if(confirm == 'n') {
                    printf("You have successfully canceled\n");
                    return 1;
                }
            }

            //create PID.out and write starting statement
            char out[20];
            sprintf(out, "%d.out", child);
            fdout = open(out, O_RDWR | O_CREAT | O_APPEND, 0777);
            dup2(fdout, fileno(stdout));
            dprintf(fdout,"Started command %d: child %d pid of parent %d\n", cmdNum, child, parent);

            //execute command
            if (execvp(cmdargs[0], cmdargs)== -1) {
                char pidArrforError[10];
                //convert int pid to string
                sprintf(pidArrforError, "%d", child);
                char addMeError[] = ".err";
                // concatenate
                strcat(pidArrforError, addMeError);
                int f_err = open(pidArrforError, O_RDWR | O_CREAT | O_TRUNC,0777);
                dup2(f_err, fileno(stderr));
                perror("Execvp failed\n");
                close(f_err);
                exit(2);
            }
        }

        //parent case
        else {
            //enter child info as a node into hash table
            enter= insert(cmd, pid, cmdNum);
            enter->start = start;

            int status, cpid;
            //while there are still processes
            while ((cpid = wait(&status)) >= 0) {
                if (cpid > 0) {

                    //mark finish
                    clock_gettime(CLOCK_MONOTONIC, &finish);

                    //lookup entry
                    collect = lookup(cpid);
                    collect->finish = finish;
                    elapsed = collect->finish.tv_sec - collect->start.tv_sec;

                    //print finishing statement to PID.out
                    char out[50];
                    sprintf(out, "%d.out", collect->pid);
                    fdout = open(out, O_RDWR | O_CREAT | O_APPEND, 0777);
                    dup2(fdout, fileno(stdout));
                    dprintf(fdout, "Finished child %d of parent %d\nFinished at %ld, runtime duration %ld\n", cpid,
                            getpid(), collect->finish.tv_sec, elapsed);
                    close(fdout);

                    //print exit statement to PID.err
                    char err[50];
                    sprintf(err, "%d.err", collect->pid);
                    fderr = open(err, O_RDWR | O_CREAT | O_APPEND, 0777);
                    dup2(fderr, fileno(stderr));

                    //if child exited normally
                    if (WIFEXITED(status)) {
                        //print exit statement in pid.err
                        fprintf(stderr, "Exited with exit code = %d\n", WEXITSTATUS(status));
                        //restart if child ran for over 2 seconds
                        if(elapsed <=2){//if a child ran less than 2 seconds, dont restart
                            fprintf(stderr, "spawning too fast\n"); //print elapsed <=2 statement to pid.err
                        }
                        else {//restart case
                            Restart(collect->command, collect->index);
                        }
                    } else if (WIFSIGNALED(status)) {// child was killed, so it will not be restarted
                        fprintf(stderr, "Killed with signal %d\n", WEXITSTATUS(status));//print kill statement to pid.err
                    }
                }
            }// end of child wait loop

        }
    }
    return 0;
}

//helper function used to validate command line arguments
//returns 0 if there are no issues, else returns 1
int fileCheck(int cmdsize, char *input){
    if(cmdsize == 2){
        FILE *test = fopen(input,"r");
        if(test!=NULL){
            fseek(test, 0, SEEK_END);
            long pos = ftell(test);
            if(pos == 0) {
                printf("Empty file!\n");
                return 1;
            }
            else
                return 0;
        }
        else{
            printf("File unopenable!\n");
            return 1;
        }
    }
    else{
        printf("Invalid argument!\n");
        return 1;
    }
}

//helper function to help with restart feature
//will restart command infinitely until command is killed in a different terminal
int Restart(char* command, int index){

    struct timespec restart,refinish;//record start and finish times of restarted command

    //record start time
    clock_gettime(CLOCK_MONOTONIC, &restart);

    //fork
    pid_t npid = fork();

    //fork fail case
    if (npid < 0) {
        exit(2);
    }
    //new child
    else if (npid == 0) {
        char *cmdargs[10];
        char *segment = strtok(command, " ");
        int index = 0;
        while (segment != NULL) {
            cmdargs[index] = segment;
            index++;
            segment = strtok(NULL, " ");
        }
        cmdargs[index] = NULL;

        //rm confirmation
        if(strcmp(cmdargs[0],"rm") == 0){
            char confirm;
            scanf("You want to remove a file. Press n to cancel now, or press any other key to continue: %c\n", &confirm);
            if(confirm == 'n') {
                printf("You have successfully canceled\n");
                return 1;
            }
        }

        //execute command
        if (execvp(cmdargs[0], cmdargs) == -1) {
            char pidArrforError[10];
            //convert int pid to string
            sprintf(pidArrforError, "%d", npid);
            char addMeError[] = ".err";
            // concatenate
            strcat(pidArrforError, addMeError);
            int f_err = open(pidArrforError, O_RDWR | O_CREAT | O_APPEND,0777);
            dup2(f_err, fileno(stderr));
            perror("Execvp failed\n");
            close(f_err);
            exit(2);
        }
    }
        //new parent
    else {
        char out[20], err[20];

        //enter command again into hashtable
        struct nlist *reenter = insert(command, npid, index);
        reenter->start = restart;

        //create pid.out
        sprintf(out, "%d.out", npid);
        int fdout = open(out, O_RDWR | O_CREAT | O_APPEND, 0777);
        dup2(fdout, fileno(stdout));
        dprintf(fdout, "RESTARTED\nStarted command %d: child %d pid of parent %d\n", reenter->index, reenter->pid, getpid());

        //create pid.err
        sprintf(err, "%d.err", npid);
        int fderr = open(err, O_RDWR | O_CREAT | O_APPEND, 0777);
        dup2(fderr, fileno(stderr));
        fprintf(stderr, "RESTARTING\n");

        //wait for children to finish
        int ncpid, nstatus;
        while ((ncpid = wait(&nstatus)) >= 0) {
            //lookup command
            struct nlist *recollect = lookup(ncpid);

            //mark finish
            clock_gettime(CLOCK_MONOTONIC, &refinish);
            recollect->finish = refinish;

            //compute time elapsed for child
            long relapsed = recollect->finish.tv_sec-recollect->start.tv_sec;

            //print finishing statement to PID.out
            sprintf(out, "%d.out", recollect->pid);
            dprintf(fdout, "Finished child %d of parent %d\nFinished at %ld, runtime duration %ld\n",
                    ncpid, getpid(), recollect->finish.tv_sec, recollect->finish.tv_sec-recollect->start.tv_sec);
            close(fdout);

            //print exit statement to PID.err
            sprintf(err, "%d.err", recollect->pid);
            fderr = open(err, O_RDWR | O_CREAT | O_APPEND, 0777);
            dup2(fderr, fileno(stderr));
            if (WIFEXITED(nstatus)) {
                fprintf(stderr, "Exited with exit code = %d\n", WEXITSTATUS(nstatus));
                if(relapsed > 2){//restart again if child runtime is still more than 2 seconds
                    Restart(command, index);
                }
                //this shouldn't be reached, but i handled it just in case
                else{
                    fprintf(stderr, "spawning too fast\n");
                }
            } else if (WIFSIGNALED(nstatus)) {
                fprintf(stderr, "Killed with signal %d\n", WEXITSTATUS(nstatus));
            }
            close(fderr);
        }
    }
    return 0;
}