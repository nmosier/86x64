/* TODO
 * [ ] convert command:
 *     [ ] convert MH_EXECUTE -> MH_DYLIB
 *
 */

#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <unordered_set>
#include <unordered_map>
#include <string>
#include <list>
#include <getopt.h>
#include <sstream>
#include <libgen.h>

#include "core/macho.hh"
#include "core/archive.hh"
#include "core/instruction.hh"
#include "tweak.hh"

#include "command.hh"
#include "util.hh"

#include "help.hh"
#include "noop.hh"
#include "modify.hh"
#include "translate.hh"
#include "convert.hh"
#include "transform.hh"
#include "print.hh"
#include "rebasify.hh"

const char *progname = nullptr;
static const char *usagestr =
   "usage: %1$s subcommand [options...] [args...]\n"                    \
   "       %1$s -h\n"                                                   \
   "\n"                                                                 \
   "Commands:\n"                                                     \
   "  %1$s help                                  print help dialog\n"   \
   "  %1$s noop [-h] inpath [outpath='a.out']    read in mach-o and write back out\n" \
   ;

static void usage(FILE *f = stderr) {
   fprintf(f, usagestr, progname);
}

int main(int argc, char *argv[]) {
   progname = argv[0];

   const char *main_optstr = "hi";
   bool inplace;

   /* read main options */
   int optchar;
   while ((optchar = getopt(argc, argv, main_optstr)) >= 0) {
      switch (optchar) {
      case 'h':
         usage(stdout);
         return 0;

      case 'i':
         inplace = true;
         break;
         
      default:
         usage(stderr);
         return 1;
      }
   }
   
   MachO::init();

   /* prepare for subcommand */
   if (optind == argc) {
      fprintf(stderr, "%s: expected subcommand\n", progname);
      usage(stderr);
      return 1;
   }

   const char *subcommand = argv[optind++];

   std::unordered_map<std::string, std::shared_ptr<Command>> subcommands
      {{"help", std::make_shared<HelpCommand>()},
       {"noop", std::make_shared<NoopCommand>()},
       {"modify", std::make_shared<ModifyCommand>()},
       {"translate", std::make_shared<TranslateCommand>()},
       {"tweak", std::make_shared<TweakCommand>()},
       {"convert", std::make_shared<ConvertCommand>()},
       {"transform", std::make_shared<TransformCommand>()},
       {"print", std::make_shared<PrintCommand>()},
       {"rebasify", std::make_shared<Rebasify>()},
      };

   auto it = subcommands.find(subcommand);
   if (it == subcommands.end()) {
      fprintf(stderr, "%s: unrecognized subcommand '%s'\n", progname, subcommand);
      usage(stderr);
      return 1;
   } else {
      std::shared_ptr<Command> cmd = it->second;
      if (cmd->handle(argc, argv) < 0) {
         return 1;
      }
      return 0;
   }

   return 0;
}
