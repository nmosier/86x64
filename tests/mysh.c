// Nicholas Mosier
// CS 315: Systems Programming
// Assignment 5
// mysh
// last modified 11/17/18

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <assert.h>

#define DEBUG 1

#define MAX_INPUT_SIZE 4096
#define MAX_ARGS 10

typedef char **argv_t; // so triple ptrs make more sense

typedef struct {
   int argc;
   char **argv;
   int flags;         // whether to redirect I/O
   int fd_in, fd_out; // redirection file descriptors
   pid_t pid;
} cmd_t;
#define CMD_PID_NONE              -1
#define CMD_FLAGS_NONE             0
#define CMD_FLAGS_REDIRECT_STDIN   1
#define CMD_FLAGS_REDIRECT_STDOUT  2

typedef struct {
   int cmdc;
   cmd_t **cmdv;
} cmds_t;

/*** FUNCTION PROTOTYPES ***/
/* general functions */
int print_prompt();

/* command operation functions */
pid_t cmd_launch(cmd_t *cmd);
int cmd_run(cmd_t *cmd);
int cmds_run(cmds_t *cmds);
void cmd_delete(cmd_t *cmd);
cmd_t *cmd_parse(char *cmd_str);
cmds_t *cmds_parse(char *cmds_str);

/* debugging functions */
void cmd_print(cmd_t *cmd);
void cmds_print(cmds_t *cmds);

/* cleanup/maintenance/exit functions */
void mysh_cleanup();
int cmd_closefds(cmd_t *cmd, int flags);
void cmds_delete(cmds_t *cmds);
int cmds_kill(cmds_t *cmds);
int cmd_pexitstat(int exit_stat, pid_t pid);
void  mysh_signal_handler(int sig);

typedef int (*builtin_func_t)(int, argv_t);
typedef struct {
   char *cmd_str;
   builtin_func_t cmd_func;
} builtin_cmd_t;

/* function prototypes */
builtin_func_t builtin_function(char *cmd);

/* builtin command prototypes */
int builtin_cd(int argc, argv_t argv);
int builtin_exit(int argc, argv_t argv);

static cmds_t *mysh_cmds = NULL;

int main(int argc, argv_t argv) {
   FILE *f = stdin;
   char input_buffer[MAX_INPUT_SIZE+1];

   if (argc == 2) {
      if ((f = fopen(argv[1], "r")) == NULL) {
         perror("fopen");
         return 2;
      }
   } else if (argc != 1) {
      fprintf(stderr, "usage: %s [script]\n", argv[0]);
      return 1;
   }

   /* install signal handler */
   struct sigaction mysh_sigact;
   mysh_sigact.sa_handler = mysh_signal_handler;
   if (sigemptyset(&mysh_sigact.sa_mask) < 0) {
      perror("mysh: sigemptyset");
      exit(1);
   }
   mysh_sigact.sa_flags = 0;
   sigaction(SIGINT, &mysh_sigact, NULL);
   
   while (1) {
      int exit_stat;
      
      /* print prompt & get input */
      print_prompt();
      if (fgets(input_buffer, MAX_INPUT_SIZE+1, f) == NULL) {
         if (errno) {
            perror("myls");
            exit(1);
         } else {
            printf("\n");
            exit(0);
         }
      }
      
      /* parse the input into commands */
      mysh_cmds = cmds_parse(input_buffer);
      if (mysh_cmds == NULL) {
         /* parse error */
         continue;
      }
      
      if (DEBUG) {
         cmds_print(mysh_cmds);
      }

      /* run commands */
      exit_stat = cmds_run(mysh_cmds);
      if (DEBUG) {
         if (exit_stat < 0) {
            fprintf(stderr, "(failed: %d)\n", exit_stat);
         }
      }

      /* cleanup */
      //      cmds_delete(mysh_cmds);
      mysh_cleanup();
   }

   exit(0);
}

void mysh_signal_handler(int sig) {
   if (sig == SIGINT) {
      if (mysh_cmds == NULL) {
         /* no commands running */
         exit(0);
      } else {
         cmds_kill(mysh_cmds);
      }
   }
}

/* mysh_cleanup
 * DESC: performs memory cleanup of mysh before exiting (e.g. on builtin cmd 'exit'
 *       or failed exec)
 */
void mysh_cleanup() {
   cmds_t *cmds = mysh_cmds;
   mysh_cmds = NULL;
   cmds_delete(cmds);
}


/* cmd_print()
 * DESC: prints the argc and argv of a command.
 * ARGS:
 *  - cmd: command to print.
 */
void cmd_print(cmd_t *cmd) {
   fprintf(stderr, "argc = %d, argv =", cmd->argc);
   for (argv_t cmd_argp = cmd->argv; *cmd_argp; ++cmd_argp) {
      fprintf(stderr, "_%s", *cmd_argp);
   }
   fprintf(stderr, "\n");
}

/* cmds_print()
 * DESC: prints a sequence of commands.
 * ARGS:
 *  - cmds: ptr to commands struct to print
 */
void cmds_print(cmds_t *cmds) {
   cmd_t **cmd_it;
   fprintf(stderr, "cmdc = %d\n", cmds->cmdc);
   for (cmd_it = cmds->cmdv; *cmd_it; ++cmd_it) {
      cmd_print(*cmd_it);
   }
}

/* print_prompt()
 * DESC: prints shell prompt
 */
int print_prompt() {
   char *current_dir_abs, *current_dir, *dir_it;
   
   current_dir_abs = getcwd(NULL, 0); // getcwd dynamically allocates string
   if (current_dir_abs == NULL) {
      return -1;
   }

   /* find last dir in absolute path */
   for (dir_it = current_dir = current_dir_abs; *dir_it; ++dir_it) {
      if (*dir_it == '/') {
         current_dir = dir_it + 1;
      }
   }

   /* print prompt */
   fprintf(stdout, "%s $ ", current_dir);
   
   free(current_dir_abs);
   return 0;
}


/* cmd_pexitstat()
 * DESC: prints exit status message, if exited abnormally
 * ARGS:
 *  - exit_stat: status of child process upon exit, set by wait(2)
 * RETV: returns 0 upon normal exit status, returns 1 otherwise
 */
int cmd_pexitstat(int exit_stat, pid_t pid) {
   const char *cmd_msg;
   int exit_no;
   
   cmd_msg = NULL;
   if (WIFEXITED(exit_stat)) {
      exit_no = WEXITSTATUS(exit_stat);
      if (exit_no) {
         cmd_msg = "[process %d exited with status %d]\n";
      }
   } else if (WIFSIGNALED(exit_stat)) {
      exit_no = WTERMSIG(exit_stat);
      cmd_msg = "[process %d terminated on signal %d]\n";
   }
   
   if (cmd_msg) {
      fprintf(stderr, cmd_msg, pid, exit_no);
      return 1;
   }

   return 0;
}



/* cmds_parse()
 * DESC: parses input string into a sequence of piped commands
 * ARGS:
 *  - cmds_str: input string to be parsed
 * RETV: ptr to parsed commands on success; NULL on failure
 * NOTE: this does _not_ parse multiple commands separated by newlines;
 *       the only valid cmd separator is the pipe symbol, '|'.
 */
cmds_t *cmds_parse(char *cmds_str) {
   cmds_t *cmds = malloc(sizeof(cmds_t));
   int cmdc;
   char *cmds_str_it, *cmd_str;
   const char *pipe_sep = "|";

   /* tokenize input */
   for (cmdc = 0, cmds_str_it = strtok(cmds_str, pipe_sep); cmds_str_it;
        cmds_str_it = strtok(NULL, pipe_sep)) {
      ++cmdc;
   }

   /* allocate command vector */
   cmds->cmdc = cmdc;
   cmds->cmdv = calloc(cmdc + 1, sizeof(cmd_t)); // +1 for null term.

   /* populate command vector */
   int i;
   for (i = 0, cmds_str_it = cmds_str; i < cmdc; ++i) {
      cmd_str = cmds_str_it;
      cmds_str_it = strchr(cmds_str_it, '\0') + 1;
      cmds->cmdv[i] = cmd_parse(cmd_str);
      if (cmds->cmdv[i] == NULL) {
         /* parse error -- abort */
         cmds_delete(cmds);
         return NULL;
      }
   }
   cmds->cmdv[cmdc] = NULL;
   
   return cmds;
}


/* skip_chars()
 * DESC: utility fn to skip over leading string composed of 
 *       characters in _skip_
 * ARGS:
 *  - s: string being operated on
 *  - skip: null-terminated array of chars to skip in prefix of _s_
 * RETV: returns ptr to first char in _s_ not in _skip_
 */
char *skip_chars(char *s, char *skip) {
   while (*s && strchr(skip, *s)) {
      ++s;
   }
   return s;
}

/* cmd_parse_redirect()
 * DESC: parses at most one redirection clause (op. + path) from input string
 * ARGS:
 *  - cmd_arg: ptr to offset in command string (string of arguments) to parse
 *  - cmd: command in which to store parsed redirection information
 * RETV: returns ptr to next arg to parse upon success (if returned ptr is equal to cmd_arg,
 *       then no redirection clause was parsed (e.g. if first token wasn't a
 *       redirection operator); upon failure, returns NULL and sets _errno_
 * ERRS:
 *  - EINVAL: redirection argument (path) not provided
 */
#define IO_OP_IN  "<"
#define IO_OP_OUT ">"
#define IO_OP_APP ">>"
#define WHITESPACE " \t\n"

char *cmd_parse_redirect(char *cmd_arg, cmd_t *cmd) {
   const char *path;
   int *cmd_fd;
   int redirect_flag;
   int oflags;
   
   cmd_arg = skip_chars(cmd_arg, WHITESPACE);

   errno = 0;
   if (strcmp(cmd_arg, IO_OP_IN) == 0) {
      /* input redirection */
      redirect_flag = CMD_FLAGS_REDIRECT_STDIN;
      oflags = O_RDONLY;
      cmd_fd = &cmd->fd_in;
   } else if (strcmp(cmd_arg, IO_OP_OUT) == 0) {
      /* output redirection (truncate) */
      redirect_flag = CMD_FLAGS_REDIRECT_STDOUT;
      oflags = O_WRONLY | O_TRUNC | O_CREAT;
      cmd_fd = &cmd->fd_out;
   } else if (strcmp(cmd_arg, IO_OP_APP) == 0) {
      /* output redirection (append) */
      redirect_flag = CMD_FLAGS_REDIRECT_STDOUT;
      oflags = O_WRONLY | O_APPEND | O_CREAT;
      cmd_fd = &cmd->fd_out;
   } else {
      /* no IO redirections to parse */
      return cmd_arg;
   }

   /* open src/dst file */
   cmd_arg += strlen(cmd_arg) + 1;
   path = cmd_arg = skip_chars(cmd_arg, WHITESPACE);
   if (path == NULL) {
      errno = EINVAL;
      fprintf(stderr, "myls: %s (redirection operator) requires path to %s file\n",
              cmd_arg, path);
      return NULL;
   }

   if ((*cmd_fd = open(path, oflags, 0666)) < 0) {
      fprintf(stderr, "myls: %s: %s\n", path, strerror(errno));
      return NULL;
   }
   cmd->flags |= redirect_flag;

   return cmd_arg + strlen(cmd_arg) + 1;
}

/* cmd_parse()
 * DESC: parses a single command from an input string.
 * ARGS:
 *  - cmd_str: input string from which to parse the cmd.
 * RETV: returns ptr to parsed cmd upon success; returns
 *       NULL upon failure.
 */
cmd_t *cmd_parse(char *cmd_str) {
   cmd_t *cmd;
   argv_t cmd_argv;
   int cmd_argc, cmd_argi;
   char *cmd_arg, *cmd_prev_arg;
   int prev_IO_tok;

   /* tokenize the input, excluding redirection clauses */
   cmd_argc = 0;
   prev_IO_tok = 0;
   for (cmd_arg = strtok(cmd_str, WHITESPACE); cmd_arg; cmd_arg = strtok(NULL, WHITESPACE)) {
      cmd_arg = skip_chars(cmd_arg, WHITESPACE);
      assert(cmd_arg);
      if (strcmp(IO_OP_IN, cmd_arg) && strcmp(IO_OP_OUT, cmd_arg) && strcmp(IO_OP_APP, cmd_arg)) {
         /* token is not IO operator */
         if (prev_IO_tok) {
            /* IO argument consumed, not counted */
            prev_IO_tok = 0;
         } else {
            /* regular exec. argument */
            ++cmd_argc;
         }
      } else {
         /* token is IO operator */
         if (prev_IO_tok) {
            /* syntax error: consecutive IO operators */
            fprintf(stderr, "mysh: syntax error: consecutive IO operators.\n");
            return NULL;
         }
         prev_IO_tok = 1;
      }
   }
   if (cmd_argc < 1) {
      // while (1) {}
      
      /* command must consist of at least one argument */
      fprintf(stderr, "mysh: not enough arguments.\n");
      return NULL;
   }
   if (prev_IO_tok) {
      fprintf(stderr, "mysh: syntax error: trailing redirection operator.\n");
      return NULL;
   }

   /* initialize command & populate argv */
   cmd = malloc(sizeof(cmd_t));
   cmd->argv = cmd_argv = calloc(cmd_argc + 1, sizeof(char *)); // +1 for null term.
   cmd->argc = cmd_argc;
   cmd->flags = CMD_FLAGS_NONE;
   cmd->pid = CMD_PID_NONE;
   
   for (cmd_argi = 0, cmd_arg = cmd_str; cmd_argi < cmd_argc; ++cmd_argi) {

      do {
         cmd_prev_arg = cmd_arg;
         cmd_arg = cmd_parse_redirect(cmd_arg, cmd);
      } while (cmd_arg && cmd_arg != cmd_prev_arg);
      /* check for errors */
      if (cmd_arg == NULL) {
         cmd_delete(cmd);
         return NULL;
      }
      
      cmd_arg = skip_chars(cmd_arg, WHITESPACE);
      cmd_argv[cmd_argi] = cmd_arg;
      cmd_arg += strlen(cmd_arg) + 1;
   }
   do {
      cmd_prev_arg = cmd_arg;
      cmd_arg = cmd_parse_redirect(cmd_arg, cmd);
   } while (cmd_arg && cmd_arg != cmd_prev_arg);
   if (cmd_arg == NULL) {
      cmd_delete(cmd);
      return NULL;
   }
   
   cmd_argv[cmd_argc] = NULL; // add null term.

   return cmd;
}



/* cmd_closefds()
 * DESC: closes the open IO file descriptors associated with a command
 * ARGS:
 *  - cmd: ptr to command
 *  - flags: a mask of CMD_FLAGS_* to bitwise-AND with cmd->flags
 *           (pass cmd->flags to just close all open fds)
 * RETV: returns 0 on success, 1 on failure.
 */
int cmd_closefds(cmd_t *cmd, int flags) {
   int retv = 0;
   
   if ((flags & cmd->flags) & CMD_FLAGS_REDIRECT_STDIN) {
      if (close(cmd->fd_in) < 0) {
         fprintf(stderr, "mysh: %s: close(%d): %s\n", cmd->argv[0], cmd->fd_in, strerror(errno));
         retv = -1;
      }
   }
   if ((flags & cmd->flags) & CMD_FLAGS_REDIRECT_STDOUT) {
      if (close(cmd->fd_out) < 0) {
         fprintf(stderr, "mysh: %s: close(%d): %s\n", cmd->argv[0], cmd->fd_out, strerror(errno));
         retv = -1;
      }      
   }

   return retv;
}

/* cmd_launch()
 * NOTE: starts execution of a command and returns immediately.
 * ARGS:
 *  - cmd: command to launch
 * RETV: returns pid of the child process corresponding to the command
 *       upon success; returns -1 upon failure.
 */
pid_t cmd_launch(cmd_t *cmd) {
   builtin_func_t builtin_func;
   pid_t cmd_pid;
   
   /* execute command */
   if ((builtin_func = builtin_function(cmd->argv[0]))) {
      /* call builtin function & return exit status */
      if (builtin_func(cmd->argc, cmd->argv) < 0) { // burden of IO redirection on builtin fn
         return -1;
      }
      return getpid();
   } else {
      /* CHILD: fork, exec, & wait for completion */
      if ((cmd_pid = fork()) == 0) {         
         /* redirect IO (replace stdin/stdout) if necessary */
         if (cmd->flags & CMD_FLAGS_REDIRECT_STDIN) {
            if (dup2(cmd->fd_in, STDIN_FILENO) < 0) {
               perror("mysh: cmd_launch: < (input redirection): dup");
               exit(-1);
            }
            /* close duplicated fd */
            if (close(cmd->fd_in) < 0) {
               fprintf(stderr, "mysh: cmd_launch: close(%d): %s\n", cmd->fd_in, strerror(errno));
               exit(-1);
            }
         }
         if (cmd->flags & CMD_FLAGS_REDIRECT_STDOUT) {
            if (dup2(cmd->fd_out, STDOUT_FILENO) < 0) {
               perror("mysh: cmd_launch: <[<] (output redirection): dup");
               exit(-1);
            }
            /* close duplicated fd */
            if (close(cmd->fd_out) < 0) {
               fprintf(stderr, "mysh: cmd_launch: close(%d): %s\n", cmd->fd_out, strerror(errno));
               exit(-1);
            }
         }

         /* exec command */
         if (execvp(cmd->argv[0], cmd->argv) < 0) {
            /* exec failed: print error and exit */
            fprintf(stderr, "mysh: cmd_launch: %s: %s\n", cmd->argv[0], strerror(errno));
         }
         //cmds_delete(mysh_cmds);
         mysh_cleanup();
         exit(-1);
      } else {
         /* PARENT: close unneeded file descriptors and return PID */
         if (cmd_pid < 0) {
            fprintf(stderr, "mysh: cmd_launch: fork: %s: %s\n", cmd->argv[0], strerror(errno));
         }
         if (cmd_closefds(cmd, cmd->flags) < 0) {
            return -1;
         }
         
         return cmd_pid;
      }
   }
}

/* cmd_run()
 * DESC: launch & run a command to completion.
 * ARGS:
 *  - cmd: command to run.
 * RETV: returns 0 upon success, -1 upon failure
 */
int cmd_run(cmd_t *cmd) {
   int cmd_stat;
   pid_t cmd_pid;

   /* launch command */
   if ((cmd_pid = cmd_launch(cmd)) < 0) {
      /* failed to launch */
      return -1;
   }

   /* if launched cmd is executable (not builtin) , wait for child process to die */
   if (cmd_pid != getpid()) { // cmd_pid == getpid() when command is builtin
      /* wait for command to die */
      cmd_pid = waitpid(cmd_pid, &cmd_stat, 0); // 0 for no options
      if (cmd_pid < 0) {
         fprintf(stderr, "mysh: wait: %s: %s\n", cmd->argv[0], strerror(errno));
         return -1;
      }
      /* print exit status of cmd */
      if (cmd_pexitstat(cmd_stat, cmd_pid)) {
         cmd_closefds(cmd, cmd->flags);
         return -1;
      }
   }

   cmd_closefds(cmd, cmd->flags);
   return 0;
}

/* cmds_run()
 * DESC: run a series of piped commands concurrently to completion.
 * ARGS:
 *  - cmds: ptr to commands to run.
 * RETV: returns 0 upon succes, -1 upon failure.
 */
int cmds_run(cmds_t *cmds) {
   cmd_t **cmdp;
   int pipe_fds[2];
   cmd_t *cmd_inpipe, *cmd_outpipe;

   /* assemble long pipe for commands */
   for (int i = 0; i < cmds->cmdc - 1; ++i) {
      cmd_outpipe = cmds->cmdv[i];
      cmd_inpipe = cmds->cmdv[i+1];
      
      /* get pipe fds */
      if (pipe(pipe_fds) < 0) {
         perror("mysh: pipe");
         return -1;
      }

      /* assign pipe fds to commands on ends of pipe
       * (override previous IO redirection if necessary)
       */
      /* assign write end of pipe */
      if (cmd_outpipe->flags & CMD_FLAGS_REDIRECT_STDOUT) {
         if (close(cmd_outpipe->fd_out) < 0) {
            perror("mysh: cmds_run: close");
            /* close pipes */
            for (int i = 0; i < 2; ++i) {
               if (close(pipe_fds[i]) < 0) {
                  fprintf(stderr, "mysh: cmds_run: close(%d): %s\n",
                          pipe_fds[i], strerror(errno));
               }
            }
            return -1;
         }
      }
      cmd_outpipe->fd_out = pipe_fds[1];
      cmd_outpipe->flags |= CMD_FLAGS_REDIRECT_STDOUT;

      /* assign read end of pipe */
      if (cmd_inpipe->flags & CMD_FLAGS_REDIRECT_STDIN) {
         if (close(cmd_inpipe->fd_in) < 0) {
            fprintf(stderr, "mysh: cmds_run: close(%d): %s\n", cmd_inpipe->fd_in,
                    strerror(errno));
            
            /* close pipe (in only, since out was assigned to prev cmd's fd_out field) */
            if (close(pipe_fds[0]) < 0) {
               fprintf(stderr, "mysh: cmds_run: close(%d): %s\n", pipe_fds[0], strerror(errno));
            }
            
            return -1;
         }
      }
      cmd_inpipe->fd_in = pipe_fds[0];
      cmd_inpipe->flags |= CMD_FLAGS_REDIRECT_STDIN;

      if (DEBUG) {
         fprintf(stderr, "mysh: created pipe[%dr, %dw] for cmds [%s, %s].\n",
                 pipe_fds[0], pipe_fds[1], cmd_inpipe->argv[0], cmd_outpipe->argv[0]);
      }
   }

   /* launch commands */
   for (cmdp = cmds->cmdv; *cmdp; ++cmdp) {
      if (((*cmdp)->pid = cmd_launch(*cmdp)) < 0) {
         /* error on launch: kill children */
         cmds_kill(cmds); // kill kill kill
         return -1;
      }
   }

   /* wait for commands to complete */
   cmd_t **wait_it;
   wait_it = cmds->cmdv;
   while (*wait_it) {
      if ((*wait_it)->pid != getpid()) {
         pid_t cmd_pid;
         int cmd_stat;
         
         cmd_pid = wait(&cmd_stat);
         cmd_pexitstat(cmd_stat, cmd_pid);
         
         /* update dead command's status */
         for (cmdp = cmds->cmdv; *cmdp; ++cmdp) {
            if ((*cmdp)->pid == cmd_pid) {
               if (DEBUG) {
                  fprintf(stderr, "mysh: process %d (%s) died.\n", cmd_pid, (*cmdp)->argv[0]);
               }
               (*cmdp)->pid = -1;
               break;
            }
         }
      }
      ++wait_it;
   }
   
   return 0;
}

/* cmds_kill()
 * DESC: kills all processes associated with commands
 * ARGS:
 *  - cmds: commands struct of commands to kill
 * RETV: returns 0 on success, 1 on failure
 */
int cmds_kill(cmds_t *cmds) {
   cmd_t **cmdp;
   pid_t pid;
   int retv;

   retv = 0;
   for (cmdp = cmds->cmdv; *cmdp; ++cmdp) {
      if ((pid = (*cmdp)->pid) != CMD_PID_NONE) {
         if (kill(pid, SIGINT) < 0) {
            fprintf(stderr, "mysh: kill(%d): %s\n", pid, strerror(errno));
            retv = -1;
         }
      }
   }

   return retv;
}


/* cmd_delete() 
 * DESC: destructs (frees) command
 * ARGS:
 *  - cmd: ptr to command to delete
 */
void cmd_delete(cmd_t *cmd) {
   free(cmd->argv);
   free(cmd);
}


/* cmds_delete()
 * DESC: deletes (frees) commands pointer and all struct members
 * ARGS:
 *  - cmds: ptr to commands to delete
 */
void cmds_delete(cmds_t *cmds) {
   cmd_t **cmdp;

   /* delete individual commands */
   for (cmdp = cmds->cmdv; *cmdp; ++cmdp) {
      cmd_delete(*cmdp);
   }

   free(cmds->cmdv);
   free(cmds);
}

static const builtin_cmd_t builtin_cmds[] = {
   {"cd", builtin_cd},
   {"exit", builtin_exit}
};
#define N_BUILTIN_CMDS (sizeof(builtin_cmds)/sizeof(builtin_cmd_t))

builtin_func_t builtin_function(char *cmd) {
   const builtin_cmd_t *builtin_cmdp;
   size_t i;
   for (i = 0, builtin_cmdp = builtin_cmds; i < N_BUILTIN_CMDS; ++i, ++builtin_cmdp) {
      if (strcmp(cmd, builtin_cmdp->cmd_str) == 0) {
         /* found builtin command */
         return builtin_cmdp->cmd_func;
      }
   }

   /* not found */
   return NULL;
}

int builtin_cd(int argc, argv_t argv) {
   const char *dir;

   if (argc == 1) {
      /* change dir to home */
      dir = getenv("HOME");
   } else if (argc == 2) {
      /* change dir to arg1 */
      dir = argv[1];
   } else {
      fprintf(stderr, "cd: too many arguments\n");
      errno = EINVAL;
      return 1;
   }

   /* change the directory */
   if (chdir(dir) < 0) {
      perror("cd");
      return 2;
   }

   return 0;
}

int builtin_exit(int argc, argv_t argv) {
   /* delete variables in the heap */
   mysh_cleanup();
   exit(0);
}
