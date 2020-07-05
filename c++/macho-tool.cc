#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <scanopt.h>
#include <unordered_set>
#include <unordered_map>
#include <string>
#include <list>
#include <getopt.h>
#include <sstream>

#include "macho.hh"
#include "archive.hh"
#include "instruction.hh"

static const char *progname = nullptr;
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

struct Command {
   const char *name;

   /* usage functions */
   virtual std::string optusage() const = 0;
   virtual std::string subusage() const = 0;

   /* option parsers */
   virtual const char *optstring() const = 0;
   virtual std::vector<option> longopts() const = 0;
   virtual int opthandler(int optchar) = 0;
   virtual void arghandler(int argc, char *argv[]) = 0;
   virtual int work() = 0;
   virtual int handle(int argc, char *argv[]) {
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
   
   void usage(std::ostream& os) const {
      std::string prefix = "usage: ";
      os << "usage: " << progname << " " << name << (optusage().empty() ? "" : " ")
         << optusage() << (subusage().empty() ? "" : " ") << subusage() << std::endl;
   }

   int parseopts(int argc, char *argv[]) {
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
   

   template <typename... Args>
   constexpr void log(const char *str, Args&&... args) const {
      fprintf(stderr, "%s %s: ", progname, name);
      fprintf(stderr, str, args...);
      fprintf(stderr, "\n");
   }

   template <typename... Args>
   constexpr void log(const std::string& str, Args&&... args) const {
      log(str.c_str(), args...);
   }

   template <>
   constexpr void log(const std::string& s) const {
      log(s.c_str());
   }
   
   const char *getarg(int argc, char *argv[], const char *init = nullptr) const {
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
   
   Command(const char *name): name(name) {}
   virtual ~Command() {}
};

struct HelpCommand: Command {
   virtual std::string optusage() const override { return ""; }
   virtual std::string subusage() const override { return ""; }

   virtual const char *optstring() const override { return ""; }
   virtual std::vector<option> longopts() const override { return {}; }
   virtual int opthandler(int optchar) override { return 1; }
   virtual void arghandler(int argc, char *argv[]) override {}

   virtual int work() override {
      usage(std::cout);
      return 0;
   }

   HelpCommand(): Command("help") {}
};

struct InplaceCommand: public Command {
   int mode;
   std::unique_ptr<MachO::Image> img;
   
   virtual std::string subusage() const override { return "<path>"; }

   virtual void arghandler(int argc, char *argv[]) override {
      const char *path = getarg(argc, argv);
      img = std::make_unique<MachO::Image>(path, mode);
   }
   
   InplaceCommand(const char *name, int mode): Command(name), mode(mode) {}
};

struct InOutCommand: Command {
   std::unique_ptr<MachO::Image> in_img;
   std::unique_ptr<MachO::Image> out_img;
   
   virtual std::string subusage() const override { return "<inpath> [<outpath>='a.out']"; }

   virtual void arghandler(int argc, char *argv[]) override {
      const char *in_path = getarg(argc, argv);
      const char *out_path = getarg(argc, argv, "a.out");
      in_img = std::make_unique<MachO::Image>(in_path, O_RDONLY);
      out_img = std::make_unique<MachO::Image>(out_path, O_RDWR | O_CREAT | O_TRUNC);
   }   

   InOutCommand(const char *name): Command(name) {}
};
   
struct NoopCommand: InOutCommand {
   int help = 0;

   virtual std::string optusage() const override { return "[-h]"; }

   virtual const char *optstring() const override { return "h"; }
   virtual std::vector<option> longopts() const override {
      return {{"help", no_argument, nullptr, 'h'},
              {0}};
   }

   virtual int opthandler(int optchar) override {
      switch (optchar) {
      case 'h':
         usage(std::cout);
         return 0;
      default: abort();
      }
   }
   
   virtual int work() override {
      MachO::MachO *macho = MachO::MachO::Parse(*in_img);
      macho->Build();
      macho->Emit(*out_img);
      return 0;
   }

   NoopCommand(): InOutCommand("noop") {}
};

struct ModifyCommand: public InOutCommand {
   int help = 0;

   struct Operation {
      virtual void operator()(MachO::MachO *macho) = 0;
      virtual ~Operation() {}
   };

   struct Insert;
   struct Delete;
   struct Start;

   virtual const char *optstring() const override { return "hi:d:s:"; }
   virtual std::vector<option> longopts() const override {
      return {{"help", no_argument, nullptr, 'h'},
              {"insert", required_argument, nullptr, 'i'},
              {"delete", required_argument, nullptr, 'd'},
              {"start", required_argument, nullptr, 's'},
              {0}};
   }
   virtual int opthandler(int optchar) override;

   std::list<std::unique_ptr<Operation>> operations;

   virtual std::string optusage() const override {
      return "[-h|-i (vmaddr=<vmaddr>|offset=<offset>),bytes=<count>,[before|after]]";
   }

   virtual int work() override {
      MachO::MachO *macho = MachO::MachO::Parse(*in_img);

      for (auto& op : operations) {
         (*op)(macho);
      }
      
      macho->Build();
      macho->Emit(*out_img);
      return 0;
   }

   ModifyCommand(): InOutCommand("modify") {}
};

struct ModifyCommand::Insert: public ModifyCommand::Operation {
   MachO::Location loc;
   MachO::Relation relation = MachO::Relation::BEFORE;
   int bytes = 0;

   virtual void operator()(MachO::MachO *macho) override {
      MachO::Archive<MachO::Bits::M64> *archive = dynamic_cast<MachO::Archive<MachO::Bits::M64> *>(macho);
      if (!archive) {
         throw std::logic_error("only 64-bit archives currently supported");
      }
         
      /* read bytes */
      char *buf = new char[bytes];
      assert(fread(buf, 1, bytes, stdin) == bytes);
         
         
      archive->insert(new MachO::Instruction<MachO::Bits::M64>(archive->segment(SEG_TEXT), buf, buf + bytes),
                      loc, relation);
   }

   Insert(char *option) {
      char * const keylist[] = {"vmaddr", "offset", "before", "after", "bytes", nullptr};
      char *value;
      while (*optarg) {
         switch (getsubopt(&optarg, keylist, &value)) {
         case 0: // vmaddr
            loc.vmaddr = std::stoul(value, nullptr, 0);
            break;
         case 1: // offset
            loc.offset = std::stoul(value, nullptr, 0);
            break;
         case 2: // before
            relation = MachO::Relation::BEFORE;
            break;
         case 3: // after
            relation = MachO::Relation::AFTER;
            break;
         case 4: // bytes
            bytes = std::stoi(value, nullptr, 0);
            break;
         case -1:
            if (suboptarg) {
               throw std::string("invalid suboption '") + suboptarg + "'";
            } else {
               throw std::string("missing suboption");
            }
         }
      }
         
      if (bytes == 0) {
         throw std::string("specify byte length with 'bytes'");
      }
   }
};

struct ModifyCommand::Delete: ModifyCommand::Operation {
   enum class LocationKind {OFFSET, VMADDR} kind;
   std::size_t loc;

   Delete(char *option) {
      char * const keylist[] = {"vmaddr", "offset", nullptr};
      char *value;

      while (*optarg) {
         switch (getsubopt(&optarg, keylist, &value)) {
         case 0: // vmaddr
            kind = LocationKind::VMADDR;
            loc = std::stoul(value, nullptr, 0);
            break;
         case 1: // offset
            kind = LocationKind::OFFSET;
            loc = std::stoul(value, nullptr, 0);
            break;
         case -1:
            if (suboptarg) {
               throw std::string("invalid suboption `") + suboptarg + "'";
            } else {
               throw std::string("missing suboption `vmaddr' or `offset'");
            }
         }
      }
   }

   virtual void operator()(MachO::MachO *macho) override {
      auto archive = dynamic_cast<MachO::Archive<MachO::Bits::M64> *>(macho);
      if (archive) {
         if (kind == LocationKind::OFFSET) {
            loc = archive->offset_to_vmaddr(loc);
         }
         auto blob = archive->find_blob<MachO::Instruction>(loc);
         blob->active = false;
      } else {
         std::stringstream ss;
         ss << "no blob found at vmaddr " << std::hex << loc;
         throw ss.str();
      }
   }
};

struct ModifyCommand::Start: ModifyCommand::Operation {
   std::size_t vmaddr;

   Start(char *option) {
      if (option == nullptr) {
         throw std::string("missing argument");
      }
      
      vmaddr = std::stoll(option, 0, 0);
   }

   virtual void operator()(MachO::MachO *macho) override {
      auto archive = dynamic_cast<MachO::Archive<MachO::Bits::M64> *>(macho);
      if (archive) {
         archive->vmaddr = this->vmaddr;
      } else {
         throw std::string("binary is not an archive");
      }
   }
};

int ModifyCommand::opthandler(int optchar) {
   switch (optchar) {
   case 'h':
      usage(std::cout);
      return 0;         
   case 'i':
      operations.push_back(std::make_unique<Insert>(optarg));
      return 1;
   case 'd':
      operations.push_back(std::make_unique<Delete>(optarg));
      return 1;
   case 's':
      operations.push_back(std::make_unique<Start>(optarg));
      return 1;
   default:
      abort();
   }
}


struct TranslateCommand: public InplaceCommand {
public:
   unsigned long offset = 0;

   virtual std::string optusage() const override { return "[-h|-o <offset>]"; }
   virtual const char *optstring() const override { return "ho:"; }
   virtual std::vector<option> longopts() const override {
      return {{"help", no_argument, nullptr, 'h'},
              {"offset", required_argument, nullptr, 'o'},
              {0}};
   }

   virtual int opthandler(int optchar) override {
      switch (optchar) {
      case 'h':
         usage(std::cout);
         return 0;
      case 'o':
         {
            char *endptr;
            offset = std::strtoul(optarg, &endptr, 0);
            if (*optarg == '\0' || *endptr) {
               throw std::string("invalid offset");
            }
         }
         return 1;
         
      default: abort();
      }
   }
   
   virtual int work() override {
      MachO::MachO *macho = MachO::MachO::Parse(*img);
      
      auto archive = dynamic_cast<MachO::Archive<MachO::Bits::M64> *>(macho);
      if (archive == nullptr) {
         log("only 64-bit archives supported");
         return -1;
      }
      
      if (offset) {
         std::cout << std::hex << "0x" << archive->offset_to_vmaddr(offset) << std::endl;
      }

      return 0;
   }

   TranslateCommand(): InplaceCommand("translate", O_RDONLY) {}
};

int main(int argc, char *argv[]) {
   progname = argv[0];

   const char *main_optstr = "h";
   int help = 0;

   /* read main options */
   if (scanopt(argc, argv, main_optstr, &help) < 0) {
      usage();
      return 1;
   }

   /* handle main options */
   if (help) {
      usage(stdout);
      return 0;
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
