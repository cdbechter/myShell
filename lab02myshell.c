#include "lab02myshell.h"

/* The main function -- This is where the enviroment is set, we set the batch file to NULL,
    check if the batch file is in arg[1], if so, run the batch, and if not, move to begin 
    the shell loop */
int main(int argc, char **argv) {
    shSetEnv("SHELL");
    char *batchfile = NULL;
    if(argv[1]) {
        batchfile = argv[1];
        shBatch(argv[1]);
    }
    shLoop();
    return EXIT_SUCCESS;
}

/* This is the function to open, parse, and execute all the function in the batch file
    using I/O redirections, Inputs, and using dup/dup2 to copy, and copy and replace
    stdin, stdout, and stderr, then get each line, all the arguments, and run each argument,
    then free the points, close the batch. */
void shBatch(char *batchfile) {
    char temp[128] = "";
    char **argument = NULL;

    /* for I/O restoring */
    int inCp  = dup(STDIN_FILENO);
    int outCp = dup(STDOUT_FILENO);
    int errCp = dup(STDERR_FILENO);
    
    /* open the file */
    FILE *batchCmd = fopen(batchfile, "r");
    FILE *in = NULL;
    FILE *out = NULL;

    if(batchCmd == NULL) {
        write(STDERR_FILENO, error_message, strlen(error_message));
    } else {
        fgets(temp, 128 * sizeof(char), batchCmd);
        while(!feof(batchCmd)) {
            rmNewLn(temp);
            argument = parseLine(temp);
            /* for redirect if found in the batch file */
            in = redirectIn(argument);
            out = redirectOut(argument);
            if(in) {
                dup2(fileno(in), STDIN_FILENO);
            }
            if(out) {
                dup2(fileno(out), STDOUT_FILENO);
                dup2(fileno(out), STDERR_FILENO);
            }
            /* Here we execute the argument, reset stdIN/OUT/ERR, free the pointer,
                erase temp, and get the next line */
            shExecute(argument);
            resetIO(in, out, inCp, outCp, errCp);
            free(argument);
            strcpy(temp, "");
            fgets(temp, sizeof(temp), batchCmd);
        }
        strcpy(temp, "");
        fclose(batchCmd);
    }
    exit(EXIT_SUCCESS);
}


/* The main shell Loop that will keep the program running until the shell is exited.
    This is essentially the same as the shBatch which is also a loop. This will take in 
    and input from the user, using a prompt. Then parse the argument, find if it is a 
    redirection, use the redirect functions, then execute the argument. */
void shLoop() {
    char *input;
    char **argument;

    /* for I/O restoring */
    int inCp  = dup(STDIN_FILENO);
    int outCp = dup(STDOUT_FILENO);
    int errCp = dup(STDERR_FILENO);

    /* make sure FILE is empty before shell strt */
    FILE *in = NULL;
    FILE *out = NULL;

    while(1) {
        shPrompt();
        input = readLine();
        /* parse the argument to split the different arguments */
        argument = parseLine(input);
        /* for redirect if found in the batch file */        
        in = redirectIn(argument);
        out = redirectOut(argument);
        if(in)
            dup2(fileno(in), STDIN_FILENO);
        if(out) {
            dup2(fileno(out), STDOUT_FILENO);
            dup2(fileno(out), STDERR_FILENO);
        }
        /* Here we execute the argument, reset stdIN/OUT/ERR, free the pointers */
        shExecute(argument);
        resetIO(in, out, inCp, outCp, errCp);
        free(argument);
        free(input);
    }
}

/*********************** Input Functions ***********************/

/* Simple read line function used in CIS2107 */
char *readLine(void) {
    char *line = NULL;
    size_t buff = 0;
    getline(&line, &buff, stdin);
    return line;
}

/* Common Parse function that makes sure there is a line, erases new lines and 
    spaces findiing the arguments, and sending each argument back. Got this function
    in my last class, CIS2107 */
char **parseLine(char *input) {
    char **line = malloc(BUFF_SIZE *sizeof(char *));
    char *temp;
    int index = 0;

    if(line == NULL) {
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(EXIT_FAILURE);
    }
    temp = strtok(input, DELIM);
    while(temp != NULL) {
        line[index] = temp;
        index++;
        temp = strtok(NULL, DELIM);
    }
    line[index] = NULL;
    return line;
}

/* Simple remove newline function that replaces it with NULL learned in CIS2107 */
void rmNewLn(char *temp) {
    int i = 0;
    while(temp[i] != '\0') {
        if(temp[i] == '\n') {
            temp[i] = '\0';
        }
        i++;
    }
}

/*********************** Execute Function ***********************/

/* Execute function that is the heart of the program. This function first test 
   wether quit/exit or cd are executed. If not, move along to finding one of the 
   builtin functions ands well as possible piping, background testing */
int shExecute(char **argument) {
    if(strcmp(argument[0], "quit") == 0 || strcmp(argument[0], "exit") == 0) {
        shQuit(argument);
    } else if(strcmp(argument[0], "cd") == 0) {
        shCd(argument);
    } else {
        pid_t pid;
        /* this find the builtin arguments */
        void (*func)(char **) = NULL;
        func = builtIn(argument);
        int pb;
        /* this finds if piping should start */
        if((pb = shFindPipe(argument)) != 0) {
            shPipe(func, argument, pb);
        } else if((pid = fork()) == 0) {
            /* First we pipe built in arguments, if not, 
                move to external commands */
            if(func) {
                (*func)(argument);
                exit(EXIT_SUCCESS);
            } else if(execvp(argument[0], argument) == -1) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                exit(EXIT_FAILURE);
            }
        } else if(pid < 0) {
            write(STDERR_FILENO, error_message, strlen(error_message));
        } else {
            /* for background processing */
            if(!bg(argument)){
                waitpid(pid, NULL, 0);
            }
        }
        return 0;
    }
    return 0;
}

/*********************** Piping ***********************/

/* Piping function, this is simply to find out if piping is used in the argument and 
    also, i was getting a seg fault with parsing, changing the pipe to NULL fixed 
    that issue but it still finds the pipe. This also returns the argument after the pipe */
int shFindPipe(char **argument) {
    int i = 0;
    while(argument[i]) {
        if(strcmp(argument[i], "|") == 0) {
            argument[i] = NULL;
            i++;
            return i;
        }
        i++;
    }
    return 0;
}

/* Piping function that gets called when there is a pipe found in the execution of the user
    inputted argument. */
void shPipe(void func(char **), char **argument, int pb) {
    int pid, pid2;
    char **argument2 = argument + pb;
    int fd[2];
    pipe(fd);

    void (*func2)(char **) = NULL;
    func2 = builtIn(argument2);

    /* begins the forking and creating of the child 
        as well as the stdin and out then closes both 
        read and write */
    if((pid2 = fork()) == 0) {
        dup2(fd[READ], STDIN_FILENO);
        if((pid = fork()) == 0) {
            dup2(fd[WRITE], STDOUT_FILENO);
            close(fd[READ]);
            close(fd[WRITE]);

            /* if statement to either do the builtin argument
                then exit or exec if its not a builtin */
            if(func) {
                (*func)(argument);
                exit(EXIT_SUCCESS);
            } else {
                execvp(argument[0], argument);
            }
        }
        close(fd[WRITE]);
        close(fd[READ]);
        waitpid(pid, NULL, 0);
        /* this will do the second argument, same as above */
        if(func2) {
            (*func2)(argument2);
            exit(EXIT_SUCCESS);
        } else {
            execvp(argument2[0], argument2);
        }
    }
    close(fd[WRITE]);
    close(fd[READ]);
    waitpid(pid2, NULL, 0);
}

/*********************** Prompt/Background/Reset ***********************/

/* Background function that cycles the entire argument to find '&' or not */
int bg(char **argument) {
    int i = 0;
    while(argument[i]) {
        i++;
    }
    if(strcmp(argument[i-1], "&") == 0) {
        return 1;
    } else {
        return 0;
    }
}

/* Simple prompt function, displace the shells prompt */
void shPrompt() {
    char this_dir[1024];
    if(getcwd(this_dir, sizeof(this_dir))) {
        printf("%s/myshell>", this_dir);
    } else {
        write(STDERR_FILENO, error_message, strlen(error_message));
    }
}

/* Shell pointer a friend helped me with redirection problems, its basically
    another for of delimiter that replaces the next argument */
void shPointer(char **argument, int direction) {
    int i = direction;
    while(argument[i+1]) {
        argument[i] = argument[i+1];
        i++;
    }
}

/* Resets all the I/O descriptors back to defaults */
void resetIO(FILE *in, FILE *out, int inCp, int outCp, int errCp) {
    dup2(inCp, STDIN_FILENO);
    if(in)
        fclose(in);
    if(out)
        fclose(out);
    dup2(outCp, STDOUT_FILENO);
    dup2(errCp, STDERR_FILENO);
}

/*********************** Built-In Functions ***********************/

/* This is a function pointer for each of the built in function commands, as explained 
    by the TA, this is one of the best ways to approach this problem of calling the 
    correct function. The idea is still hard to swallow but plenty of information
    on stack and geeksforgeeks helped me 
    orginally we set to null incase nothing is entered */
void (*builtIn(char **argument))(char **argument) {
    void (*func)(char **) = NULL; 
    if(strcmp(argument[0], "clr") == 0 || strcmp(argument[0], "clear") == 0) {
        return &shClr;
    } else if(strcmp(argument[0], "dir") == 0) {
        return &shDir;
    } else if(strcmp(argument[0], "environ") == 0) {
        return &shShowEnviron;
    } else if(strcmp(argument[0], "echo") == 0) {
        return &shEcho;
    } else if(strcmp(argument[0], "help") == 0) {
        return &shHelp;
    } else if(strcmp(argument[0], "pause") == 0) {
        return &shPause;
    }
    return NULL;
}

/* Change Directory function that we went over in class and slides */
void shCd(char **argument) {
    if(!argument[1] || strcmp(argument[1], " ") == 0) {
        chdir(getenv("HOME")); 
    } else {
        chdir(argument[1]);
    }
}

/* Simple clear argument, cant use system("clear") so it was suggested to me to use the
    ANSI encoder to clear the screen */
void shClr(char **argument) {
    printf("\033[2J\033[H");
    }

/* This is the directory function, given to us in class by the professor with an updated 
    error message just in case no directory is found. Also included to skip period 
    and double period. */
void shDir(char **argument) {
    char *target = argument[1];
    DIR *d;
    struct dirent *dir;
    char path[256];

    if((d = opendir(target)) == NULL) {
        fprintf(stderr, "OpenDIR %s  %s\n", path, strerror(errno));
        return;
    }
    if(d != NULL) {
        while((dir = readdir(d))) {
            if(strcmp(".", dir->d_name) == 0 || strcmp("..", dir->d_name) == 0)
                continue;
            printf("%s\n", dir->d_name);
        }
        closedir(d);
    }
}

/* Echo function that prints everything after args[0] to the screen */
void shEcho(char **argument) {
    int i = 1;
    while(argument[i]) {
        printf("%s ", argument[i]);
        i++;
    }
    printf("\n");
}

/* Help function that first finds the the current working directory, puts 
    /readme at the end of the path, fork and execute the readme, allowing
    more arguments with path, then kill the child or wait. */
void shHelp(char **argument) {
    char current[512] = "";
    pid_t pid;

    /* This gets parent directory and places /readme at the end of it
        then forks, uses more to continue the readme, then when the 
        readme ends and cant execute anymore, kills the child. */
    strcat(current, getenv("PWD"));
    strcat(current, "/readme");
    if((pid = fork()) == 0) {
        execlp("more", "more", current, NULL);
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(EXIT_FAILURE);
    } else {
        waitpid(pid, NULL, 0);
    }
}

/* Simple pause function, doesn't do anything till user hits enter */
void shPause(char **argument) {
    while(getchar() != '\n') {}
}

/* Simple Quit function */
void shQuit(char **argument) {
    exit(EXIT_SUCCESS);
}

/* Set enviroment function that gets the current working directory, puts /myshell at the
    end of the path, just like readme, and sets that as the shells enviroment */
void shSetEnv(char *argument) {
    char currentDir[512];

    if(!getcwd(currentDir, sizeof(currentDir))) {
        write(STDERR_FILENO, error_message, strlen(error_message));
        return;
    }
    strcat(currentDir, "/myshell"); /* append */
    if(argument) {
        setenv(argument, currentDir, 1);
    }
}

/* Show Enviroment function, env for mac, environ for this shell. Use an array 
    to print the list of enviroments */
void shShowEnviron(char **argument) {
    int i = 0;
    while(environ[i]) {
        printf("%s\n", environ[i]);
        i++;
    }
}

/*********************** I/O Redirection ***********************/

/* Basic redirect for inputting a file to the argument the user wishes to input. These 
    markers and marked by the symbol '<' and this function checks for that symbol, 
    then inputs that file to the previous argument. Another student helped me with the
    shell pointer, which before, was causing the execute to not work. shell pointer is 
    described under its function. */
FILE *redirectIn(char **argument) {
    FILE *fp = NULL;
    int i = 1;
    int inPutting;

    /* while loop that figures out which symbol is being used 
        used a while to cycle every char in entire argument */
    while(argument[i]) {
        /* this finds the symbol */
        if(argument[i][0] == '<') {
            inPutting = i;
        /* this inputs the file */
        if(argument[i++]) {
            fp = fopen(argument[i], "r");
            open(argument[i], O_RDONLY);
            if(fp == NULL) {
                write(STDERR_FILENO, error_message, strlen(error_message));
            }
            /* this is the shell pointer that moves the redirect along */
            shPointer(argument, inPutting);
            return fp;
            }
        }
        i++;
    }
    return fp;
}

/* Basic redirect for outPutting an argument to a file of the users preference. These 
    markers and marked by the symbol '>' and '>>' and this function checks for that symbol, 
    then outputs that argument to a file, creating one if need be. 
    '>' this appends the argument to file '>>' this truncates it */
FILE *redirectOut(char **argument) {
    FILE *fp = NULL;
    int i = 1;
    char mode[] = "";
    int outPutting;
    /* while loop that figures out which symbol is being used 
        used a while to cycle every char in entire argument */
    while(argument[i]) {
        if(strcmp(argument[i], ">") == 0) {
            mode[0] = 'w';
            outPutting = i;
        } else if(strcmp(argument[i], ">>") == 0) {
            mode[0] = 'a';
            outPutting = i;
        }

        /* The check that makes sure there's an output symbol then opens file and outputs */
        if(strcmp(mode, "") != 0) {
            if(argument[i++]) {
                fp = fopen(argument[i], mode);
                if(fp == NULL) {
                    write(STDERR_FILENO, error_message, strlen(error_message));
                }
                argument[outPutting] = NULL;
                return fp;
                }
        }
            i++;
    }
    return fp;
}