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
static const char WHEREIS_PATH[] = "/usr/bin/whereis -b ";

/*
 * Function declarations
 */

void PrintCommand(int, Command *);
void PrintPgm(Pgm *);
void stripwhite(char *);

// Student defined
void InterpretCommand(Command * const cmd);
char *GetCommandPath(const char *binaryFile);

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
                PrintCommand(n, &cmd);

                // InterpretCommand(&cmd);
                char *commandPath  = GetCommandPath("ls");                
            }
        }

        if(line) 
        {
            free(line);
        }
    }

    return 0;
}



void InterpretCommand(Command * const cmd)
{
   /* char **pl = cmd->pgm->pgmlist;

    printf("Blah: %s\n", *pl);

    int pid = fork();

    if(pid == 0)
    {
        execlp("/bin", *pl, NULL);
    }
    else 
    {
        wait(NULL);
    }*/
}

/*
 * Name: GetCommandPath
 *
 * Description: Gets the path for a binary program
 *
 */
char *GetCommandPath(const char *binaryFile)
{
    const int bufferSize = 128;

    FILE *binPipe;
    char *path = malloc(sizeof(char) * bufferSize); //[bufferSize];
    char whereIsCommand[bufferSize];
    
    // Add whereis to command
    strcpy(whereIsCommand, WHEREIS_PATH);

    // Add the binary file to search
    strcat(whereIsCommand, binaryFile);

    // End the string 
    strcat(whereIsCommand, "\0");

//    printf("command: %s\n", whereIsCommand);

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
    
    // Finally return binary's path
    return path;
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
    printf("   bg    : %s\n", cmd->bakground ? "yes" : "no");
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
