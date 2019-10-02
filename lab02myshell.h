#ifndef LAB02MYSHELL_H_
#define LAB02MYSHELL_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/wait.h>

/* Definitions for buffer size and delimiter */
#define BUFF_SIZE 256 //define a buffer size
#define DELIM " \t\r\n\a"

/* Global Variables and Enviroment string variable */
const int READ = 0;
const int WRITE = 1;
extern char **environ;

/* Error handling message */
char error_message[30] = "An error has occurred\n";

/* all the built in function arguments */
void shCd(char **args);
void shClr(char **argument);
void shDir(char **argument);
void shEcho(char **argument);
void shHelp(char **argument);
void shPause(char **argument);
void shQuit(char **argument);
void shSetEnv(char *argument);
void shShowEnviron(char **argument);



/* The two first loops to start the program, first to check in the user 
    tried to run a batch file, and to run it. Then to run the shells loop
    to allow the input and output of arguments. */
void shBatch(char *batchfile);
void shLoop();

/* Read in the input from the user, parse the input, take out white space, return
    the input to be executed */
char *readLine(void);
char **parseLine(char *input);
void rmNewLn(char *temp);

/* This is the execute funtion, the heart of the program, this directs the arguments 
    to their built in function, redirection I/O and figures out the piping. */
int shExecute(char **argument);

/* This function handles the piping outside the built in functions, as well as 
    a seperate function to handle the piping should the user also want to redirect
    I/O with the pipe. */
void shPipe(void func(char **), char **argument, int pb);
int shFindPipe(char **argument);

/* The three functions that handle the redirection I/O for the program, nearly the 
    same code for both excpt with one reading, the other writing. */
FILE * redirectIn(char **argument);
FILE * redirectOut(char **argument);
void resetIO(FILE *in, FILE *out, int inCp, int outCp, int errCp);

//Helper functions to do everything else
//include users working directory of executable
void shPrompt();

//determine whether a process needs to be run in the background
int bg(char **argument);

//returns a functon based on the argument that needs to be executed - mainly bultins
void (*builtIn(char **argument))(char **argument);

//move pointer to next deimited part of argument so previous part is not accidentally
//used in execution
void shPointer(char **argument, int direction);


#endif