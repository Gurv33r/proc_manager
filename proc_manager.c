#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

int fileCheck(int cmdsize, char *input);

int main(int argc, char *argv[]) {

    char cmd[50], //stores line of cmdfile
    *filename, //stores cmdfile's name
    *cmdargs[10]; // will be used to break cmd down to prep for execvp
    int cmdNum=0, // tracks current line to be executed
    fdout=1, fderr=2;// file descriptors for PID.out and PID.err respectively

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
            fprintf(stdout,"Started command %d: child %d pid of parent %d\n", cmdNum, child, parent);
            fflush(stdout);

            //execute command
            if (execvp(cmdargs[0], cmdargs)== -1) {
                perror(cmdargs[0]);
                exit(2);
            }
        }
            //parent case
        else{
            int status, cpid;
            //while there are still processes
            while((cpid = wait(&status))>0){
                //print finishing statement to PID.out
                char out[10];
                sprintf(out, "%d.out", cpid);
                fdout = open(out, O_RDWR | O_CREAT | O_APPEND, 0777);
                dup2(fdout, fileno(stdout));
                fprintf(stdout, "Finished child %d of parent %d\n", cpid, getpid());
                fflush(stdout);
                close(fdout);

                //print exit statement to PID.err
                char err[20];
                sprintf(err, "%d.err", cpid);
                fderr = open(err, O_RDWR | O_CREAT | O_APPEND, 0777);
                dup2(fderr, fileno(stderr));
                if(WIFEXITED(status)){
                    fprintf(stderr, "Exited with exit code = %d\n", WEXITSTATUS(status));
                }
                else if(WIFSIGNALED(status)){
                    fprintf(stderr, "Killed with signal %d\n", WEXITSTATUS(status));
                }
            }
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
                printf("Empty file!");
                return 1;
            }
            else
                return 0;
        }
        else{
            printf("File unopenable!");
            return 1;
        }
    }
    else{
        printf("Invalid argument!");
        return 1;
    }
}