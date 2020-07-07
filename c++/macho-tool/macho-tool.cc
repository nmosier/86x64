/* TODO
 * [ ] convert command:
 *     [ ] convert MH_EXECUTE -> MH_DYLIB
 *
 */

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
#include <libgen.h>

#include "macho.hh"
#include "archive.hh"
#include "instruction.hh"
#include "tweak.hh"

#include "command.hh"
#include "util.hh"

#include "help.hh"
#include "noop.hh"

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

#define MH_FLAG(flag) {#flag + 3, flag}

const std::unordered_map<std::string, uint32_t> mach_header_filetype_map = 
   {MH_FLAG(MH_OBJECT),
    MH_FLAG(MH_EXECUTE),
    MH_FLAG(MH_FVMLIB),
    MH_FLAG(MH_CORE),
    MH_FLAG(MH_PRELOAD),
    MH_FLAG(MH_DYLIB),
    MH_FLAG(MH_DYLINKER),
    MH_FLAG(MH_BUNDLE),
    MH_FLAG(MH_DYLIB_STUB),
    MH_FLAG(MH_DSYM),
    MH_FLAG(MH_KEXT_BUNDLE),
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
      return "[-h | -i (vmaddr=<vmaddr>|offset=<offset>),bytes=<count>,[before|after] | -d (vmaddr=<vmaddr>|offset=<offset>) | -s <vmaddr>]";
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

/* change flags, etc. in-place */
struct TweakCommand: InplaceCommand {
   template <typename Flag>
   using Flags = std::unordered_map<Flag, bool>;
   
   Flags<uint32_t> mach_header_flags;
   std::optional<uint32_t> mach_header_filetype;
   
   int pie = -1;
   
   virtual std::string optusage() const override { return "[-h|-f <flag>[,<flag>]*]"; }
   virtual const char *optstring() const override { return "hf:t:"; }
   virtual std::vector<option> longopts() const override {
      return {{"help", no_argument, nullptr, 'h'},
              {"flags", required_argument, nullptr, 'f'},
              {"type", required_argument, nullptr, 't'},
              {0}};
   }

   virtual int opthandler(int optchar) override {
      switch (optchar) {
      case 'f': /* mach_header_t flags */
         parse_flags(optarg,
                     {MH_FLAG(MH_NOUNDEFS),
                      MH_FLAG(MH_INCRLINK),
                      MH_FLAG(MH_DYLDLINK),
                      MH_FLAG(MH_BINDATLOAD),
                      MH_FLAG(MH_PREBOUND),
                      MH_FLAG(MH_SPLIT_SEGS),
                      MH_FLAG(MH_LAZY_INIT),
                      MH_FLAG(MH_TWOLEVEL),
                      MH_FLAG(MH_FORCE_FLAT),
                      MH_FLAG(MH_NOMULTIDEFS),
                      MH_FLAG(MH_NOFIXPREBINDING),
                      MH_FLAG(MH_PREBINDABLE),
                      MH_FLAG(MH_ALLMODSBOUND),
                      MH_FLAG(MH_SUBSECTIONS_VIA_SYMBOLS),
                      MH_FLAG(MH_CANONICAL),
                      MH_FLAG(MH_WEAK_DEFINES),
                      MH_FLAG(MH_BINDS_TO_WEAK),
                      MH_FLAG(MH_ALLOW_STACK_EXECUTION),
                      MH_FLAG(MH_ROOT_SAFE),
                      MH_FLAG(MH_SETUID_SAFE),
                      MH_FLAG(MH_NO_REEXPORTED_DYLIBS),
                      MH_FLAG(MH_PIE),
                      MH_FLAG(MH_DEAD_STRIPPABLE_DYLIB),
                      MH_FLAG(MH_HAS_TLV_DESCRIPTORS),
                      MH_FLAG(MH_NO_HEAP_EXECUTION),
                      MH_FLAG(MH_APP_EXTENSION_SAFE),
                      MH_FLAG(MH_NLIST_OUTOFSYNC_WITH_DYLDINFO),
                      MH_FLAG(MH_SIM_SUPPORT),
                     },
                     mach_header_flags);
         return 1;

      case 't':
         {
            auto it = mach_header_filetype_map.find(optarg);
            if (it != mach_header_filetype_map.end()) {
               mach_header_filetype = it->second;
            } else {
               throw std::string("-t: unrecognized Mach-O archive filetype");
            }
         }
         return 1;
            
      case 'h':
         usage(std::cout);
         return 0;

      default: abort();
      }
   }

   virtual int work() override {
      auto macho = MachO::Tweak::MachO::Parse(*img);
      auto archive = dynamic_cast<MachO::Tweak::Archive<MachO::Bits::M64> *>(macho);

      if (mach_header_filetype) {
         archive->header.filetype = *mach_header_filetype;
      }
      
      if (!mach_header_flags.empty()) {
         if (archive == nullptr) {
            log("--flags: modifying mach header flags  only supported for 64-bit archives");
            return -1;
         }

         for (auto pair : mach_header_flags) {
            if (pair.second) {
               /* add flag */
               archive->header.flags |= pair.first;
            } else {
               archive->header.flags &= ~pair.first;
            }
         }
      }

      return 0;
   }

   template <typename Flag>
   static void parse_flags(char *optarg, const std::unordered_map<std::string, Flag>& flagset,
                           Flags<Flag>& flags) {
      const char *token;
      bool mask;
      while ((token = strsep(&optarg, ",")) != nullptr) {
         /* get mask */
         switch (*token) {
         case '\0':
            throw std::string("empty flag");
         case '-':
            ++token;
            mask = false;
            break;
         case '+':
            ++token;
         default:
            mask = true;
         }

         /* parse flag */
         std::string flagstr(token);
         auto flagset_it = flagset.find(flagstr);
         if (flagset_it == flagset.end()) {
            std::stringstream ss;
            ss << "invalid flag `" << token << "'";
            throw std::string(ss.str());
         }
         flags.insert({flagset_it->second, mask});
      }
   }

   TweakCommand(): InplaceCommand("tweak", O_RDWR) {}
   
};

struct ConvertCommand: InOutCommand {
   std::optional<uint32_t> filetype;
   
   virtual std::string optusage() const override { return "[-h|-a <type>]"; }
   virtual const char *optstring() const override { return "ha:"; }

   virtual std::vector<option> longopts() const override {
      return {{"help", no_argument, nullptr, 'h'},
              {"archive", required_argument, nullptr, 'a'},
              {0}};
   }

   virtual int opthandler(int optchar) override {
      switch (optchar) {
      case 'h':
         usage(std::cout);
         return 0;
         
      case 'a':
         {
            auto it = mach_header_filetype_map.find(optarg);
            if (it != mach_header_filetype_map.end()) {
               filetype = it->second;
            } else {
               throw std::string("invalid Mach-O archive filetype");
            }
         }
         return 1;

      default: abort();
      }
   }
   
   virtual int work() override {
      MachO::MachO *macho = MachO::MachO::Parse(*in_img);
      
      if (filetype) {
         auto archive = dynamic_cast<MachO::Archive<MachO::Bits::M64> *>(macho);
         if (archive == nullptr) {
            log("MachO binary is not an archive");
            return -1;
         }
         
         /* change filetype */
         archive->header.filetype = *filetype;
         
         switch (*filetype) {
         case MH_DYLIB:
            archive_EXECUTE_to_DYLIB(archive);
            break;
            
         default:
            log("unsupported Mach-O archive filetype for conversion");
            return -1;
         }
      }

      macho->Build();
      macho->Emit(*out_img);
      
      return 0;
   }

   ConvertCommand(): InOutCommand("convert") {}

   void archive_EXECUTE_to_DYLIB(MachO::Archive<MachO::Bits::M64> *archive) {
      /* add LC_ID_DYLIB */
      char *out_path = strdup(this->out_path);
      char *name = basename(out_path);
      auto id_dylib = MachO::DylibCommand<MachO::Bits::M64>::Create(LC_ID_DYLIB, name);
      archive->load_commands.push_back(id_dylib);
      free(out_path);

      /* check whether MH_NO_REEXPORTED_DYLIBS should be added */
      if (archive->template subcommands<MachO::LoadCommand, LC_REEXPORT_DYLIB>().empty()) {
         archive->header.flags |= MH_NO_REEXPORTED_DYLIBS;
      }
   }
   
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
       {"tweak", std::make_shared<TweakCommand>()},
       {"convert", std::make_shared<ConvertCommand>()},
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
