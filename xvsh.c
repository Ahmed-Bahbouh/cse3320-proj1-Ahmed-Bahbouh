// xvsh.c
#include "types.h"
#include "fcntl.h"
#include "user.h"
#include "stat.h"

#define SH_PROMPT "xvsh> "
#define NULL_PTR (void *)0
#define MAXLINE 256
#define MAXTOKENS 16

// Function Prototypes
char *strtok_custom(char *s, const char *delim);
int process_one_cmd(char *buf);
int exit_check(char **tok);
int process_normal(char **tok, int bg);
void wait_for_background_processes();
int execute_piped_commands(char **commands, int num_commands);
int execute_redirection(char **tok, int bg);

// Helper Functions

// Custom strtok implementation for xv6
char *strtok_custom(char *s, const char *delim) {
    static char *last;
    char *spanp;
    int c, sc;
    char *tok;

    if (s == NULL && (s = last) == NULL)
        return NULL_PTR;

    // Skip leading delimiters
cont:
    c = *s++;
    for (spanp = (char *)delim; (sc = *spanp++) != 0;) {
        if (c == sc)
            goto cont;
    }

    if (c == 0) {  // No non-delimiter characters
        last = NULL_PTR;
        return NULL_PTR;
    }

    tok = s - 1;

    // Scan token
    for (;;) {
        c = *s++;
        spanp = (char *)delim;
        do {
            if ((sc = *spanp++) == c) {
                if (c == 0)
                    s = NULL_PTR;
                else
                    s[-1] = 0;
                last = s;
                return tok;
            }
        } while (sc != 0);
    }
    /* NOTREACHED */
}

// Check if the command is 'exit'
int exit_check(char **tok) {
    return (tok[0] != NULL_PTR && strcmp(tok[0], "exit") == 0);
}

// Wait for all background processes to finish
void wait_for_background_processes() {
    while (1) {
        int pid = wait();
        if (pid < 0)
            break;  // No more background processes
    }
}

// Execute normal commands (foreground or background)
int process_normal(char **tok, int bg) {
    int pid = fork();

    if (pid < 0) {
        printf(2, "Fork failed\n");
        return -1;
    }

    if (pid == 0) {  // Child process
        exec(tok[0], tok);
        printf(2, "Cannot run this command %s\n", tok[0]);
        exit();
    }

    if (!bg) {  // Foreground process
        wait();
    } else {  // Background process
        printf(1, "[pid %d] runs as a background process\n", pid);
    }

    return 0;
}

// Split buffer into commands separated by '|'
int split_commands(char *buf, char **commands) {
    int count = 0;
    char *cmd = strtok_custom(buf, "|");
    while (cmd != NULL_PTR && count < MAXTOKENS) {
        // Trim leading spaces
        while (*cmd == ' ') cmd++;
        commands[count++] = cmd;
        cmd = strtok_custom(NULL_PTR, "|");
    }
    commands[count] = NULL_PTR;
    return count;
}

// Execute piped commands
int execute_piped_commands(char **commands, int num_commands) {
    int i;
    int fd[2];
    int in = 0;  // Initial input is stdin

    for (i = 0; i < num_commands; i++) {
        char *args[MAXTOKENS];
        int j = 0;

        // Tokenize each command
        char *tok = strtok_custom(commands[i], " ");
        while (tok != NULL_PTR && j < MAXTOKENS - 1) {
            args[j++] = tok;
            tok = strtok_custom(NULL_PTR, " ");
        }
        args[j] = NULL_PTR;

        if (pipe(fd) < 0) {
            printf(2, "Pipe failed\n");
            return -1;
        }

        int pid = fork();
        if (pid < 0) {
            printf(2, "Fork failed\n");
            return -1;
        }

        if (pid == 0) {  // Child process
            if (in != 0) {  // Not the first command
                dup(in);
                close(in);
            }
            if (i < num_commands - 1) {  // Not the last command
                dup(fd[1]);
                close(fd[0]);
                close(fd[1]);
            }
            exec(args[0], args);
            printf(2, "Cannot run this command %s\n", args[0]);
            exit();
        } else {  // Parent process
            if (in != 0)
                close(in);
            close(fd[1]);
            in = fd[0];
        }
    }

    // Wait for all children
    for (i = 0; i < num_commands; i++) {
        wait();
    }

    return 0;
}

// Execute commands with redirection
int execute_redirection(char **tok, int bg) {
    int i = 0;
    int fd;
    char *cmd_args[MAXTOKENS];
    char *outfile = NULL_PTR;

    // Split tokens and identify redirection
    while (tok[i] != NULL_PTR && i < MAXTOKENS - 1) {
        if (strcmp(tok[i], ">") == 0) {
            outfile = tok[i + 1];
            tok[i] = NULL_PTR;  // Terminate the command before '>'
            break;
        }
        cmd_args[i] = tok[i];
        i++;
    }
    cmd_args[i] = NULL_PTR;

    if (outfile != NULL_PTR) {
        fd = open(outfile, O_CREATE | O_WRONLY);
        if (fd < 0) {
            printf(2, "Cannot open file %s\n", outfile);
            return -1;
        }
    }

    int pid = fork();

    if (pid < 0) {
        printf(2, "Fork failed\n");
        return -1;
    }

    if (pid == 0) {  // Child process
        if (outfile != NULL_PTR) {
            dup(fd);
            close(fd);
        }
        exec(cmd_args[0], cmd_args);
        printf(2, "Cannot run this command %s\n", cmd_args[0]);
        exit();
    }

    if (outfile != NULL_PTR)
        close(fd);

    if (!bg) {  // Foreground process
        wait();
    } else {  // Background process
        printf(1, "[pid %d] runs as a background process\n", pid);
    }

    return 0;
}

// Process a single command line
int process_one_cmd(char* buf) {
    int i, num_tok;
    char **tok;
    int bg = 0;

    // Count tokens
    num_tok = 0;
    for (i = 0; buf[i]; i++) {
        if (buf[i] == ' ') num_tok++;
    }
    num_tok++; // for the last token

    tok = malloc((num_tok + 1) * sizeof(char *));
    if (tok == NULL_PTR) {
        printf(1, "malloc failed\n");
        exit();
    }

    i = 0;
    tok[i++] = strtok_custom(buf, " ");

    // Check for special symbols
    while ((tok[i] = strtok_custom(NULL_PTR, " ")) != NULL_PTR) {
        if (strcmp(tok[i], "&") == 0) {
            bg = 1;  // Background process
            tok[i] = NULL_PTR;  // Terminate tokens
            break;
        }
        i++;
    }

    // Check for built-in exit command
    if (exit_check(tok)) {
        wait_for_background_processes();  // Wait for background processes
        free(tok);
        exit();
    }

    // Check for pipe
    int pipe_present = 0;
    for (i = 0; tok[i] != NULL_PTR; i++) {
        if (strcmp(tok[i], "|") == 0) {
            pipe_present = 1;
            break;
        }
    }

    if (pipe_present) {
        // Split the command by '|'
        char *commands[MAXTOKENS];
        int num_commands = 0;
        char *cmd = strtok_custom(buf, "|");
        while (cmd != NULL_PTR && num_commands < MAXTOKENS) {
            // Trim leading spaces
            while (*cmd == ' ') cmd++;
            commands[num_commands++] = cmd;
            cmd = strtok_custom(NULL_PTR, "|");
        }
        commands[num_commands] = NULL_PTR;

        execute_piped_commands(commands, num_commands);
    }
    else {
        // Check for redirection
        int redirection_present = 0;
        for (i = 0; tok[i] != NULL_PTR; i++) {
            if (strcmp(tok[i], ">") == 0) {
                redirection_present = 1;
                break;
            }
        }

        if (redirection_present) {
            execute_redirection(tok, bg);
        }
        else {
            // Execute normal command
            process_normal(tok, bg);
        }
    }

    free(tok);
    return 0;
}

int main(int argc, char *argv[]) {
    char buf[MAXLINE];
    int n;

    printf(1, SH_PROMPT);  // Print prompt

    while ((n = read(0, buf, sizeof(buf))) > 0) {
        if (n == 1) {  // Handle empty input
            printf(1, SH_PROMPT);
            continue;
        }

        buf[n - 1] = 0;  // Replace newline with null terminator

        // Process the command
        process_one_cmd(buf);

        printf(1, SH_PROMPT);
        memset(buf, 0, sizeof(buf));
    }

    exit();
}
