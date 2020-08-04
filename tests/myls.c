/*
 * myls.c
 *
 * usage: ./myls [-al] [file ...]
 *
 * Created by Nicholas Mosier & Seardang Pa
 * last modified 09.20.2018
 */

#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>

int list_directory_contents(const char *path, bool list_all, bool list_long);
int list_entry_long(const char *name, const char *path);
void format_mode(mode_t mode, char *sbuf);

// FLAGS
// bool list_all = false, list_long = false;

int main(int argc, char *argv[]) {
   /* myls options */
   bool list_all, list_long, list_usage;
   list_usage = list_all = list_long = false;
   
   /* get command-line options */
   const char *options = "alh";
   int option;
   while ((option = getopt(argc, argv, options)) >= 0) {
      switch (option) {
      case 'a':
         list_all = true;
         break;
      case 'l':
         list_long = true;
         break;
      case 'h':
      case '?':
      default:
         list_usage = true;
         break;
      }
   }

   if (list_usage) {
      printf("usage: %s [-ahl] [file ...]\n", argv[0]);
      exit(1);
   }
   
   
   /* count number of file/directory args
    * (this number affects behavior)
    */
   int npaths = 0;
   for (int argi = 1; argi < argc; ++argi) {
      if (argv[argi][0] != '-') {
         ++npaths;
      }
   }
   
   /* print list for each file/directory argument */
   if (npaths == 0) {
      /* assume current directory */
      list_directory_contents(".", list_all, list_long);
   } else {
      /* process path arguments */
      for (int argi = 1; argi < argc; ++argi) {
         char *arg = argv[argi];
         struct stat arg_info;
         
         /* skip if arg is option */
         if (arg[0] == '\0' || arg[0] == '-') {
            continue;
         }
         
         /* get file status */
         if (stat(arg, &arg_info)) {
            fprintf(stderr, "stat: %s: %s\n", arg, strerror(errno));
            continue;
         }
         
         /* is it a reg file or dir? */
         if (S_ISDIR(arg_info.st_mode)) {
            /* dir: list contents */
            if (npaths != 1) {
               printf("%s:\n", arg);
            }
            list_directory_contents(arg, list_all, list_long);
         } else {
            /* reg file: list entry */
            if (list_long) {
               list_entry_long(arg, arg);
            } else {
               printf("%s\n", arg);
            }
            printf("\n");
         }
      }
   } 
   
   exit(EXIT_SUCCESS);
}


/* list_directory_contents
 * DESC: list the file contents of a directory
 * PARAMS:
 *  - dir_path: path to directory
 * RETURN: 0 on success, -1 on failure
 *  
 */  
int list_directory_contents(const char *dir_path, bool list_all, bool list_long) {
   DIR *dir;
   struct dirent *entry;
   int retval = 0;
   
   /* open directory */
   dir = opendir(dir_path);
   if (dir == NULL) {
      fprintf(stderr, "opendir: %s: %s\n", dir_path, strerror(errno));
      return -1;
   }
   
   /* print out entries */
   const char *sep = "/";
   while ((entry = (readdir)(dir))) {
      if (list_all || entry->d_name[0] != '.') {
         if (list_long) {
            char entry_path[strlen(entry->d_name) + strlen(dir_path)
                            + strlen(sep) + 1];
            sprintf(entry_path, "%s%s%s", dir_path, sep, entry->d_name);
            list_entry_long(entry->d_name, entry_path);
         } else {
            // only print filename
            printf("%s  ", entry->d_name);
         }
      }
      errno = 0; // reset to detect readdir error
   }
   
   /* did a readdir error occur? */
   if (errno) {
      fprintf(stderr, "readdir: %s: %s\n", dir_path, strerror(errno));
      retval = -1;
   }
   
   /* close directory */
   if (closedir(dir) < 0) {
      fprintf(stderr, "closedir: %s: %s\n", dir_path, strerror(errno));
      retval = -1;
   }
   
   printf("\n");
   return retval;
}

/* list_entry_long
 * DESC: prints out ls-style long entry for file
 * PARAMS:
 *  - name: name of file
 *  - path: path to file
 * RETURN: returns 0 on success, -1 on failure
 */
int list_entry_long(const char *name, const char *path) {
   const int MODESTR_LEN = 10;
   char modestr[MODESTR_LEN + 1];
   struct stat entry_info;
   struct passwd *userpw;
   struct group *grouppw;
   char *username, *groupname;
   
   const int TIMESTR_LEN = 25;
   char timestr[TIMESTR_LEN + 1];
   struct tm *timem;
   
   if (stat(path, &entry_info)) {
      perror("stat");
      return -1; // don't exit() in case program wants to recover from error
   }
   format_mode(entry_info.st_mode, modestr); // readable permissions string
   unsigned long nlinks = entry_info.st_nlink; // number of hard links to path
   
   /* get owner name */
   userpw = getpwuid(entry_info.st_uid);
   if (userpw == NULL && errno) {
      fprintf(stderr, "getpwuid: %s: %s\n", path, strerror(errno));
      return -1;
   }
   username = (userpw) ? userpw->pw_name : "???";
   
   /* get group name */
   grouppw = getgrgid(entry_info.st_gid);
   if (grouppw == NULL && errno) {
      fprintf(stderr, "getgrid: %s: %s\n", path, strerror(errno));
      return -1;
   }
   groupname = (grouppw) ? grouppw->gr_name : "???";
   
   /* get time last modified */
#if 0
   timem = localtime(&entry_info.st_mtim.tv_sec);
   if (strftime(timestr, TIMESTR_LEN, "%b %e %H:%M", timem) < 0) {
      fprintf(stderr, "strftime: %s: %s\n", path, strerror(errno));
      return -1;
   }
#else
   timestr[0] = '\0';
#endif
   
   /* print listing */
   printf("%s %4ld %s\t%s\t%6ld %s\t%s\n", modestr, nlinks, username,
          groupname, (long) entry_info.st_size, timestr, name
          );
   return 0;
}

/* format_mode
 * DESC: print permissions & file type as readable string
 * PARAMS:
 *  - mode: mode of file
 *  - sbuf: string to print results into
 * RETURN: (none)
 */
void format_mode(mode_t mode, char *sbuf) {
   /* file format (reg., dir., etc) */
   switch (mode & S_IFMT) {
   case S_IFSOCK:
      *sbuf++ = 's';
      break;
   case S_IFLNK:
      *sbuf++ = 'l';
      break;
   case S_IFBLK:
      *sbuf++ = 'b';
      break;
   case S_IFDIR:
      *sbuf++ = 'd';
      break;
  case S_IFCHR:
     *sbuf++ = 'c';
     break;
   case S_IFIFO:
      *sbuf++ = 'p';
      break;
   case S_IFREG:
   default:
      *sbuf++ = '-';
      break;
   }

   *sbuf++ = (mode & S_IRUSR) ? 'r' : '-';
   *sbuf++ = (mode & S_IWUSR) ? 'w' : '-';
   *sbuf++ = (mode & S_IXUSR) ? 'x' : '-';
   *sbuf++ = (mode & S_IRGRP) ? 'r' : '-';
   *sbuf++ = (mode & S_IWGRP) ? 'w' : '-';
   *sbuf++ = (mode & S_IXGRP) ? 'x' : '-';
   *sbuf++ = (mode & S_IROTH) ? 'r' : '-';
   *sbuf++ = (mode & S_IWOTH) ? 'w' : '-';
   *sbuf++ = (mode & S_IXOTH) ? 'x' : '-';
   *sbuf = '\0'; // terminate string
}
