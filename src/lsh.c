/*
 * Main source code file for lsh shell program
 *
 * You are free to add functions to this file.
 * If you want to add functions in a separate file
 * you will need to modify Makefile to compile
 * your additional functions.
 *
 * Add appropriate comments in your code to make it
 * easier for us while grading your assignment.
 *
 * Submit the entire lab1 folder as a tar archive (.tgz).
 * Command to create submission archive:
      $> tar cvf lab1.tgz lab1/
 *
 * All the best
 */


#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "parse.h"

// Student
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

// Constants
static const char WHEREIS_PATH[] = "/usr/bin/whereis -b -B ";
#define READ_END 0
#define WRITE_END 1
#define BUFFER_SIZE 2048

// Globals
char *PATH;


/*
 * Function declarations
 */

void PrintCommand(int, Command *);
void PrintPgm(Pgm *);
void stripwhite(char *);

// Forward declarations
void InterpretCommand(const Command cmd);
char *GetCommandPath(const char *binaryFile);
void ParsePath();
char *RunCommands(Pgm *pgm, int isBackgroundProcess);


int PipeCommands(Pgm *pgm, int isRoot, int isBackground);


/* When non-zero, this global means the user is done using this program. */
int done = 0;

/*
 * Name: main
 *
 * Description: Gets the ball rolling...
 *
 */
int main(void)
{
    Command cmd;
    int n;

    // Load PATH variable with whitespace separation
    ParsePath();

    while (!done)
    {
        char *line;
        line = readline("> ");

        if (!line)
        {
            /* Encountered EOF at top level */
            done = 1;
        }
        else
        {
            /*
            * Remove leading and trailing whitespace from the line
            * Then, if there is anything left, add it to the history list
            * and execute it.
            */
            stripwhite(line);

            if(*line)
            {
                add_history(line);
                /* execute it */
                n = parse(line, &cmd);

               // PrintCommand(n, &cmd);

                // Run commands
                //RunCommands(cmd.pgm, cmd.background);

                InterpretCommand(cmd);
            }
        }

        if(line)
        {
            free(line);
        }
    }

    return 0;
}

// Pipe data (global)
int pipeData[2];

int PipeCommands(Pgm *pgm, int isRoot, int isBackground)
{
    char *path = GetCommandPath(pgm->pgmlist[0]);    
    pid_t pid;    

    Pgm *nextCommand = pgm->next;

    if(nextCommand != NULL)
    {
        PipeCommands(nextCommand, 0, isBackground);
        
        close(pipeData[WRITE_END]);
        dup2(pipeData[READ_END], 0);

        pipe(pipeData);

        pid = fork();

        if(pid == 0)
        {

            if(!isRoot)
            {
                dup2(pipeData[WRITE_END], 1);
                close(pipeData[WRITE_END]);
            }            

            execv(path, pgm->pgmlist);

        }
        else
        {
            close(pipeData[WRITE_END]);
            close(pipeData[READ_END]);
            wait(NULL);
        }
    }
    else
    {
        pipe(pipeData);

        pid = fork();

        if(pid == 0)
        {    
            //close(0);       
            dup2(pipeData[WRITE_END], 1);
            execv(path, pgm->pgmlist);
        }
    }


    if(isRoot)
    {
        if(!isBackground)
        {
            wait(NULL);
        }
        else
        {
            exit(0);
        }
    }


}



void InterpretCommand(const Command cmd)
{
    Pgm *pgm = cmd.pgm;    
    pid_t pid;

    // Check if it's a single command
    if(pgm->next == NULL)
    {
        // create new process to handle the binary execution
        pid = fork();

        // check for child
        if(pid == 0)
        {
            // If command is a background one, create yet another process to keep the main 
            // process lsh from waiting
            if(cmd.background == 1)
            {
                // Make grandchild process run the command
                if(pid = fork() == 0)
                {
                    execv(GetCommandPath(pgm->pgmlist[0]), pgm->pgmlist);                                         
                }
                else 
                {
                    exit(0);
                }
            }
            else
            {
                // If it is not running in the background we just execute it in the child process
                execv(GetCommandPath(pgm->pgmlist[0]), pgm->pgmlist);
            }            
        }       
        else
        {   
            // wait for all the child processes to finish
            wait(NULL);
        }        
    }
    else 
    {
        if(cmd.background == 1)
        {   
            pid_t pid = fork();

            if(pid == 0)
            {
                
                PipeCommands(cmd.pgm, 1, cmd.background);
            }
            else
            {
                wait(NULL);
            }    
        }
        else
        {
            PipeCommands(cmd.pgm, 1, cmd.background);
        }
    }
    
}

/*
 * Name: GetCommandPath
 *
 * Description: Gets the path for a binary program
 *
 */
char *GetCommandPath(const char *binaryFile)
{
    const int bufferSize = 256;

    FILE *binPipe;
    char *path = malloc(sizeof(char) * bufferSize);
    char whereIsCommand[bufferSize];

    // Add whereis to command
    strcpy(whereIsCommand, WHEREIS_PATH);

    // Append the PATH
    strcat(whereIsCommand, PATH);

    // Append flag
    strcat(whereIsCommand, " -f ");

    // Add the binary file to search
    strcat(whereIsCommand, binaryFile);

    // End the string
    //strcat(whereIsCommand, "\0");

    // Open a pipe and run the command in read mode
    binPipe = popen(whereIsCommand, "r");

    if(binPipe == NULL)
    {
        printf("Failed to run command\n");
        return NULL;
    }

    // Read the contents of the buffer
    while(fgets(path, bufferSize, binPipe) != NULL) {};

    // Close the pipe
    pclose(binPipe);

    // Whereis returns path in the format ls: /bin/ls
    // We need to truncate "ls: "
    char *ptr = strstr(path, ":");

    if(ptr != NULL)
    {
        // Remove everything before the actual path e.g. "ls: "
        memmove(&path[0], ptr + 2, strlen(path));
    }
    else
    {
        printf("Failed to run command\n");
        return NULL;
    }

    // Get rid of new line and replace with ' '
    ptr = strstr(path, "\n");
    *ptr = '\0';

    // Finally return binary's path
    return path;
}


/*
 * Name: ParsePath
 *
 * Description: Gets environment path, removes ':' and writes ' ' instead
 *
 */
void ParsePath()
{
    char *tempPath = getenv("PATH");
    char *ptr;

    // While there are occurences of ':', write ' '
    while(1)
    {
        ptr = strstr(tempPath, ":");

        if(ptr == NULL)
            break;

        *ptr = ' ';
    }

    /*ptr = strstr(tempPath, "\0");

    memmove(ptr, ptr + 1, strlen(tempPath));*/

    PATH = tempPath;
}


/*
 * Name: RunCommands
 *
 * RECURSIVE
 *
 * Description: Navigate through the Command structure and execute
 *
 */
char *RunSingleCommand(Pgm *pgm, int isBackgroundProcess)
{
    char *path = GetCommandPath(pgm->pgmlist[0]);

    int pid = fork();

    if(pid == 0)
    {
        execv(path, pgm->pgmlist);
    }
    else
    {
        if(isBackgroundProcess == 0)
        {
            wait(NULL);
        }
    }
}


/*
 * Name: PrintCommand
 *
 * Description: Prints a Command structure as returned by parse on stdout.
 *
 */
void PrintCommand (int n, Command *cmd)
{
    printf("Parse returned %d:\n", n);
    printf("   stdin : %s\n", cmd->rstdin  ? cmd->rstdin  : "<none>" );
    printf("   stdout: %s\n", cmd->rstdout ? cmd->rstdout : "<none>" );
    printf("   bg    : %s\n", cmd->background ? "yes" : "no");
    PrintPgm(cmd->pgm);
}

/*
 * Name: PrintPgm
 *
 * Description: Prints a list of Pgm:s
 *
 */
void PrintPgm (Pgm *p)
{
    if (p == NULL)
    {
        return;
    }
    else
    {
        char **pl = p->pgmlist;

        /* The list is in reversed order so print
         * it reversed to get right
         */
        PrintPgm(p->next);
        printf("    [");

        while (*pl)
        {
            printf("%s ", *pl++);
        }

        printf("]\n");
    }
}

/*
 * Name: stripwhite
 *
 * Description: Strip whitespace from the start and end of STRING.
 */
void stripwhite (char *string)
{
    register int i = 0;

    while (isspace( string[i] ))
    {
      i++;
    }

    if (i)
    {
        strcpy (string, string + i);
    }

    i = strlen( string ) - 1;

    while (i> 0 && isspace (string[i]))
    {
        i--;
    }

    string [++i] = '\0';
}
