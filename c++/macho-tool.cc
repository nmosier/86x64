#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <scanopt.h>
#include <unordered_set>
#include <unordered_map>

#include "macho.hh"
#include "archive.hh"

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

   virtual std::string subusage() const = 0;
   void usage(FILE *f) const {
      std::string subusage_;
      if (!subusage().empty()) {
         subusage_ = std::string(" ") + subusage();
      }
      fprintf(stderr, "usage: %s %s%s\n", progname, name, subusage_.c_str());
   }
   
   virtual int handle(int argc, char *argv[]) = 0;

   void log(const char *str) const {
      fprintf(stderr, "%s %s: %s\n", progname, name, str);
   }

   Subcommand(const char *name): name(name) {}
   virtual ~Subcommand() {}
};

struct HelpSubcommand: public Subcommand {
   virtual std::string subusage() const override { return std::string(); }
   
   virtual int handle(int argc, char *argv[]) override {
      usage(stdout);
      return 0;
   }
   
   HelpSubcommand(): Subcommand("help") {}
};

struct RWSubcommand: public Subcommand {
   virtual int work(const MachO::Image& in_img, MachO::Image& out_img) = 0;
   virtual int opts(int argc, char *argv[]) = 0;

   virtual std::string subopts() const = 0;
   virtual std::string subusage() const override {
      std::string subopts_;
      if (!subopts().empty()) {
         subopts_ = subopts() + " ";
      }
      return subopts_ + "inpath [outpath='a.out']";
   }
   
   virtual int handle(int argc, char *argv[]) override {
      /* read options */
      if (opts(argc, argv) < 0) {
         return -1;
      }

      /* open images */
      if (argc <= optind) {
         log("missing positional argument");
         usage(stderr);
         return -1;
      }
      
      const char *in_path = argv[optind++];
      const char *out_path = "a.out";
      if (optind < argc) {
         out_path = argv[optind++];
      }

      if (optind != argc) {
         log("excess positional arguments");
         usage(stderr);
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

   virtual std::string subopts() const override { return "[-h]"; }
   
   virtual int opts(int argc, char *argv[]) override {
      if (scanopt(argc, argv, "h", &help) < 0) {
         return -1;
      }
      return 0;
   }
   
   virtual int work(const MachO::Image& in_img, MachO::Image& out_img) override {
      if (help) {
         usage(stdout);
         return 0;
      }

      MachO::MachO *macho = MachO::MachO::Parse(in_img);
      macho->Build();
      macho->Emit(out_img);

      return 0;
   }

   NoopSubcommand(): RWSubcommand("noop") {}
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
       {"noop", std::make_shared<NoopSubcommand>()}
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

   
#if 0
   
   if (subcommand == "help") {
      usage(stdout);
      return 0;
   } else if (subcommand == "noop") {
      int help = 0;
      if (scanopt(argc, argv, "h", &help) < 0) {
         return 1;
      }

      auto usage = [=] (FILE *f) {
                      fprintf(f, "usage: %s %s inpath [outpath=a.out]\n",
                              progname, subcommand.c_str());
                   };

      if (help) {
         usage(stdout);
         return 0;
      }

      if (argc < optind + 1) {
         fprintf(stderr, "%s %s: missing arguments\n", progname, subcommand.c_str());
         usage(stderr);
         return 1;
      }
      const char *in_path = argv[optind++];

      const char *out_path = "a.out";
      if (argc < optind + 1) {
         out_path = argv[optind++];
      }

      const MachO::Image in_img(in_path, O_RDONLY);
      MachO::Image out_img(out_path, O_RDWR | O_CREAT | O_TRUNC);
      MachO::init();
      MachO::MachO *macho = MachO::MachO::Parse(in_img);
      macho->Build();
      macho->Emit(out_img);
   } else {
      fprintf(stderr, "%s: unrecognized subcommand\n", progname);
      usage(stderr);
      return 1;
   }
#endif
   
   return 0;
}
