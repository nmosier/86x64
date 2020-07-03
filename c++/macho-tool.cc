#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <scanopt.h>
#include <unordered_set>
#include <unordered_map>
#include <string>
#include <list>
#include <getopt.h>

#include "macho.hh"
#include "archive.hh"
#include "instruction.hh"

static const char *progname = nullptr;
static const char *usagestr =
   "usage: %1$s subcommand [options...] [args...]\n"                    \
   "       %1$s -h\n"                                                   \
   "\n"                                                                 \
   "Subcommands:\n"                                                     \
   "  %1$s help                                  print help dialog\n"   \
   "  %1$s noop [-h] inpath [outpath='a.out']    read in mach-o and write back out\n" \
   ;

static void usage(FILE *f = stderr) {
   fprintf(f, usagestr, progname);
}

struct Subcommand {
   const char *name;

   virtual std::list<std::string> subusage() const = 0;
   void usage(std::ostream& os) const {
      std::string prefix = "usage: ";
      for (std::string subusage : subusage()) {
         os << prefix << progname << " " << name << (subusage.empty() ? "" : " ")
            << subusage << std::endl;
      }
   }
   
   virtual int handle(int argc, char *argv[]) = 0;

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

   Subcommand(const char *name): name(name) {}
   virtual ~Subcommand() {}
};

struct HelpSubcommand: public Subcommand {
   virtual std::list<std::string> subusage() const override { return {std::string()}; }
   
   virtual int handle(int argc, char *argv[]) override {
      usage(std::cout);
      return 0;
   }
   
   HelpSubcommand(): Subcommand("help") {}
};

struct RSubcommand: public Subcommand {
   virtual int work(const MachO::Image& img) = 0;
   virtual int opts(int argc, char *argv[]) = 0;

   virtual std::list<std::string> subopts() const = 0;
   virtual std::list<std::string> subusage() const override {
      std::list<std::string> list;
      for (std::string subopts : subopts()) {
         if (!subopts.empty()) {
            subopts += " ";
         }
         list.push_back(subopts + "<path>");
      }
      return list;      
   }

   virtual int handle(int argc, char *argv[]) override {
      int optstat = opts(argc, argv);
      if (optstat < 0) {
         return -1;
      } else if (optstat == 0) {
         return 0;
      }

      if (argc <= optind) {
         log("missing positional argument");
         usage(std::cerr);
         return -1;
      }

      const char *path = argv[optind++];
      if (optind != argc) {
         log("excess positional arguments");
         usage(std::cerr);
         return -1;
      }

      const MachO::Image img(path, O_RDONLY);
      MachO::init();

      if (work(img) < 0) {
         return -1;
      }

      return 0;
   }

   template <typename... Args>
   RSubcommand(Args&&... args): Subcommand(args...) {}
};

struct RWSubcommand: public Subcommand {
   virtual int work(const MachO::Image& in_img, MachO::Image& out_img) = 0;
   virtual int opts(int argc, char *argv[]) = 0;

   virtual std::list<std::string> subopts() const = 0;
   virtual std::list<std::string> subusage() const override {
      std::list<std::string> list;
      for (std::string subopts : subopts()) {
         if (!subopts.empty()) {
            subopts += " ";
         }
         list.push_back(subopts + "inpath [outpath='a.out']");
      }
      return list;
   }
   
   virtual int handle(int argc, char *argv[]) override {
      /* read options */
      int optstat = opts(argc, argv);
      if (optstat < 0) {
         return -1;
      } else if (optstat == 0) {
         return 0;
      }

      /* open images */
      if (argc <= optind) {
         log("missing positional argument");
         usage(std::cerr);
         return -1;
      }
      
      const char *in_path = argv[optind++];
      const char *out_path = "a.out";
      if (optind < argc) {
         out_path = argv[optind++];
      }

      if (optind != argc) {
         log("excess positional arguments");
         usage(std::cerr);
         return -1;
      }

      const MachO::Image in_img(in_path, O_RDONLY);
      MachO::Image out_img(out_path, O_RDWR | O_CREAT | O_TRUNC);

      MachO::init();
      
      if (work(in_img, out_img) < 0) {
         return -1;
      }

      return 0;
   }

   RWSubcommand(const char *name): Subcommand(name) {}
};
   
struct NoopSubcommand: public RWSubcommand {
   int help = 0;

   virtual std::list<std::string> subopts() const override { return {"[-h]"}; }
   
   virtual int opts(int argc, char *argv[]) override {
      if (scanopt(argc, argv, "h", &help) < 0) {
         return -1;
      }

      if (help) {
         usage(std::cout);
         return 0;
      }
         
      return 1;
   }
   
   virtual int work(const MachO::Image& in_img, MachO::Image& out_img) override {
      MachO::MachO *macho = MachO::MachO::Parse(in_img);
      macho->Build();
      macho->Emit(out_img);
      return 0;
   }

   NoopSubcommand(): RWSubcommand("noop") {}
};

struct ModifyCommand: public RWSubcommand {
   int help = 0;

   struct Operation {
      virtual void operator()(MachO::MachO *macho) = 0;
      virtual ~Operation() {}
   };

   struct Insert: public Operation {
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

      Insert(char *option, bool& good) {
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

   std::list<std::unique_ptr<Operation>> operations;

   virtual std::list<std::string> subopts() const override {
      return {"[-h|-i (vmaddr=<vmaddr>|offset=<offset>),bytes=<count>,[before|after]]"};
   }

   virtual int opts(int argc, char *argv[]) override {
      bool good = true;
      int optchar;
      const char *optstring = "hi:";
      struct option longopts[] =
         {{"help", no_argument, nullptr, 'h'},
          {"insert", required_argument, nullptr, 'i'},
          {0}
         };
      int longindex = -1;

      try {
         while ((optchar = getopt_long(argc, argv, optstring, longopts, nullptr)) >= 0) {
            switch (optchar) {
            case 'h':
               usage(std::cout);
               return 0;
               
            case 'i':
               operations.push_back(std::make_unique<Insert>(optarg, good));
               break;
               
            default:
               usage(std::cerr);
               return -1;
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

      return 0;
   }
   
   virtual int work(const MachO::Image& in_img, MachO::Image& out_img) override {
      MachO::MachO *macho = MachO::MachO::Parse(in_img);

      for (auto& op : operations) {
         (*op)(macho);
      }
      
      macho->Build();
      macho->Emit(out_img);
      return 0;
   }

   ModifyCommand(): RWSubcommand("modify") {}
};

struct TranslateCommand: public RSubcommand {
public:
   unsigned long offset = 0;

   virtual std::list<std::string> subopts() const override { return {"[-h|-o <offset>]"}; }
   
   virtual int opts(int argc, char *argv[]) override {
      int help = 0;
      if (scanopt(argc, argv, "ho+", &help, &offset) < 0) {
         return -1;
      }
      
      if (help) {
         usage(std::cout);
         return 0;
      }

      return 1;
   }

   virtual int work(const MachO::Image& img) override {
      MachO::MachO *macho = MachO::MachO::Parse(img);

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

   TranslateCommand(): RSubcommand("translate") {}
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

   /* prepare for subcommand */
   if (optind == argc) {
      fprintf(stderr, "%s: expected subcommand\n", progname);
      usage(stderr);
      return 1;
   }

   const char *subcommand = argv[optind++];

   std::unordered_map<std::string, std::shared_ptr<Subcommand>> subcommands
      {{"help", std::make_shared<HelpSubcommand>()},
       {"noop", std::make_shared<NoopSubcommand>()},
       {"modify", std::make_shared<ModifyCommand>()},
       {"translate", std::make_shared<TranslateCommand>()},
      };

   auto it = subcommands.find(subcommand);
   if (it == subcommands.end()) {
      fprintf(stderr, "%s: unrecognized subcommand '%s'\n", progname, subcommand);
      usage(stderr);
      return 1;
   } else {
      std::shared_ptr<Subcommand> cmd = it->second;
      if (cmd->handle(argc, argv) < 0) {
         return 1;
      }
      return 0;
   }

   return 0;
}
