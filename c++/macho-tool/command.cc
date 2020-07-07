#include <string>
#include <iostream>
#include <sstream>
#include <fcntl.h>

#include "command.hh"
#include "image.hh"

int Command::handle(int argc, char *argv[]) {
   int optstat = parseopts(argc, argv);
   if (optstat <= 0) {
      return optstat;
   }
   
   try {
      arghandler(argc, argv);
   } catch (const std::string& s) {
      log(s);
      usage(std::cerr);
      return -1;
   }
   
   return work();
}

void Command::usage(std::ostream& os) const {
   std::string prefix = "usage: ";
   os << "usage: " << progname << " " << name << (optusage().empty() ? "" : " ")
      << optusage() << (subusage().empty() ? "" : " ") << subusage() << std::endl;
}

int Command::parseopts(int argc, char *argv[]) {
   int optchar;
   int longindex = -1;
   const struct option *longopts = this->longopts().data();
   
   try {
      while ((optchar = getopt_long(argc, argv, optstring(), longopts, nullptr)) >= 0) {
         if (optchar == '?') {
            std::stringstream ss;
            ss << "invalid option `";
            if (longindex < 0) {
               ss << (char) optchar;
            } else {
               ss << longopts[longindex].name;
            }
            ss << "'";
            log(ss.str());
            return -1;
         } else {
            int optstat = opthandler(optchar);
            if (optstat <= 0) {
               return optstat;
            }
         }
         longindex = -1;
      }
   } catch (const std::string& s) { 
      std::string prefix;
      if (longindex >= 0) {
         prefix = std::string("--") + longopts[longindex].name;
      } else {
         prefix = std::string("-");
         prefix.push_back(optchar);
      }
      log(prefix + ": " + s);
      usage(std::cerr);
      return -1;        
   }

   return 1;
}

const char *Command::getarg(int argc, char *argv[], const char *init) const {
   if (optind >= argc) {
      if (init) {
         return init;
      } else {
         throw std::string("missing positional argument");
      }
   } else {
      return argv[optind++];
   }
}


void InplaceCommand::arghandler(int argc, char *argv[]) {
   const char *path = getarg(argc, argv);
   img = std::make_unique<MachO::Image>(path, mode);
}

void InOutCommand::arghandler(int argc, char *argv[]) {
   in_path = getarg(argc, argv);
   out_path = getarg(argc, argv, "a.out");
   in_img = std::make_unique<MachO::Image>(in_path, O_RDONLY);
   out_img = std::make_unique<MachO::Image>(out_path, O_RDWR | O_CREAT | O_TRUNC);
}   

int Subcommand::parse(char *option) {
   char *value;
   int index;
   if ((index = getsubopt(&option, keylist().data(), &value)) < 0) {
      if (*suboptarg) {
         throw "unrecognized subcommand";
      } else {
         throw "missing subcommand";
      }
   } else {
      op = std::unique_ptr<Operation>(getop(index));
      return op->parse(option);
   }
}

void Subcommand::operator()(MachO::MachO *macho) {
   return (*op)(macho);
}

int Operation::parse(char *option) {
   while (*option) {
      char *value;
      int index = getsubopt(&option, keylist().data(), &value);
      if (index < 0) {
         if (suboptarg) {
            throw std::string("invalid suboption `") + suboptarg + "'";
         } else {
            throw std::string("missing suboption");
         }
      }

      try {
         int stat = subopthandler(index, value);
         if (stat <= 0) {
            return stat;
         }
      } catch (const std::string& s) {
         throw std::string(suboptarg) + ": " + s;
      }
   }

   validate();

   return 1;
}
