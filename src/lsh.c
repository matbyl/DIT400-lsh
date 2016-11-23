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
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

// Constants
static const char WHEREIS_PATH[] = "/usr/bin/whereis -b -B ";
#define READ_END 0
#define WRITE_END 1
#define BUFFER_SIZE 2048

// Globals
char *PATH;

pid_t MAIN_PROCESS;
pid_t CURRENT_PROCESS;

/*
 * Function declarations
 */

void PrintCommand(int, Command *);
void PrintPgm(Pgm *);
void stripwhite(char *);

// Student
void InterpretCommand(const Command cmd);
char *GetCommandPath(const char *binaryFile);
void ParsePath();
int PipeCommands(Pgm *pgm, int isRoot, int isBackground);
void InteruptHandler(int signal);

/* When non-zero, this global means the user is done using this program. */
int done = 0;

/* Default in- and output of a command */
int INPUT_FD;
int OUTPUT_FD;

void sigchld_handler (int signum)
{
  int pid, status;
  while ((pid = waitpid(WAIT_ANY, &status, WNOHANG)) > 0);
}

/*
 * Name: main
 *
 * Description: Gets the ball rolling...
 *
 */
int main(void)
{
    INPUT_FD = dup(0);
    OUTPUT_FD = dup(1);

    Command cmd;
    int n;

    MAIN_PROCESS = getpid();
    signal(SIGINT, InteruptHandler);
    signal(SIGCHLD, sigchld_handler);

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

                // Interpret and execute the current set of commands
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

// Pipe data file descriptor (global)
int pipeData[2];

/*
 * Name: PipeCommands
 *
 * Description: Recursive function used to execute several piped commands
 *
 */
int PipeCommands(Pgm *pgm, int isRoot, int isBackground)
{
    char *path = GetCommandPath(pgm->pgmlist[0]);
    pid_t pid;

    // Get next command in the list (if there is one)
    Pgm *nextCommand = pgm->next;

    // Validate next command, and run recursive part if valid
    if(nextCommand != NULL)
    {
        // Go one level deeper
        PipeCommands(nextCommand, 0, isBackground);

        // We won't write to the pipe, close it
        close(pipeData[WRITE_END]);

        // Substitute stdin for the read-end of the pipe
        dup2(pipeData[READ_END], 0);

        // Create yet another pipe for communication with next command
        pipe(pipeData);

        // Create child process to run next command
        pid = fork();

        if(pid == 0)
        {
            // Check if this is not the original function call
            // This would mean that we still need to write to a pipe, not stdout
            if(!isRoot)
            {
                // Substitute writing end
                dup2(pipeData[WRITE_END], 1);
            }
            // We are back from all recursive madness, this is the top level
            else
            {
                // Write to the output file if there is one
                dup2(OUTPUT_FD, 1);
            }

            // Execute the command
            execv(path, pgm->pgmlist);
        }
        else
        {
          if(isRoot)
            wait(NULL);
        }


    }
    // This is the last command in the linked list
    else
    {
        // Create a pipe to communicate with next command
        pipe(pipeData);

        // Create piped, child process
        pid = fork();

        // Child process will run the command
        if(pid == 0)
        {
            // Redirect input to file stream if there is one
            dup2(INPUT_FD, 0);

            // We don't need to read from pipe
            close(pipeData[READ_END]);

            // Substitute stdout for the write-end of the pipe
            dup2(pipeData[WRITE_END], 1);

            // Run the command and write output to pipe
            execv(path, pgm->pgmlist);
        }
    }

}

/*
 * Name: InterpretCommand
 *
 * Description: Interprets and executes either single or multiple commands
 *
 */
void InterpretCommand(const Command cmd)
{
    Pgm *pgm = cmd.pgm;
    pid_t pid;

    // Check for a possible output stream and create it
    if(cmd.rstdout != NULL)
        OUTPUT_FD = creat(cmd.rstdout, O_WRONLY | S_IRWXU);

    // Check for possible input stream and assign it
    if(cmd.rstdin != NULL)
    {
        mode_t mode = O_RDONLY;
        INPUT_FD = open(cmd.rstdin, mode);
    }

    // If command is "cd" then manually execute chdir
    if(strstr(pgm->pgmlist[0], "cd") != NULL)
    {
        chdir(pgm->pgmlist[1]);
    }
    // Check for "exit" command, if so, terminate the process
    else if(strstr(pgm->pgmlist[0], "exit") != NULL)
    {
        exit(0);
    }
    // Check if it's a single command
    else if(pgm->next == NULL)
    {
        // create new process to handle the binary execution
        pid = fork();

        // check for child
        if(pid == 0)
        {
            // Save process id
            CURRENT_PROCESS = getpid();

            // Handle possible input and/or output streams
            dup2(INPUT_FD, 0);
            dup2(OUTPUT_FD, 1);

            // If it is not running in the background we just execute it in the child process
            execv(GetCommandPath(pgm->pgmlist[0]), pgm->pgmlist);
        }
        else
        {
            // wait for all the child processes to finish
            if(cmd.background)
              waitpid(-1, NULL, WNOHANG);
            else
              waitpid(-1, NULL, 0);
        }
    }
    // There where multiple commands, run them recursively
    else
    {
        // Fork a new process and make it run the recursive function
        pid_t pid = fork();

        if(pid == 0)
        {
          printf("HERE\n");
            PipeCommands(cmd.pgm, 1, cmd.background);
        }
        else if(pid < 0)
        {
          printf("Something went wrong during forking\n");
        }
        else
        {
          // wait for all the child processes to finish
          waitpid(-1, NULL, 0);
        }
    }

    // Reset Input and Output file descriptors
    INPUT_FD = dup(0);
    OUTPUT_FD = dup(1);
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

    // Check if we found a valid path
    if(strstr(path, "/") != NULL)
    {
        // Whereis returns path in the format ls: /bin/ls
        // We need to truncate "ls: "
        char *ptr = strstr(path, ":");

        if(ptr != NULL)
        {
            // Remove everything before the actual path e.g. "ls: "
            memmove(&path[0], ptr + 2, strlen(path));
        }

        // Get rid of new line and replace with ' '
        ptr = strstr(path, "\n");
        *ptr = '\0';

        // Finally return binary's path
        return path;
    }
    else
    {
        printf("%s command not found\n", path);
    }

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

    // Write the environment path on the global variable
    PATH = tempPath;
}

/*
 * Name: InteruptHandler
 *
 * Description: Takes care of the ctrl+c command
 *
 */
void InteruptHandler(int signal)
{


    pid_t pid = getpid();


    if(pid != MAIN_PROCESS)
    {
        //printf("PID_current: %d\n", CURRENT_PROCESS );
        //printf("PID: %d\n", pid );

        if(pid == CURRENT_PROCESS)
        {
            exit(0);
          //  int err = kill(CURRENT_PROCESS, SIGTERM);
            //printf("Error: %i", err);
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
