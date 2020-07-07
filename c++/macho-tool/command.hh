#pragma once

#include <getopt.h>
#include <vector>
#include <string>

#include "macho-tool.h"
#include "types.hh"

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
   virtual int handle(int argc, char *argv[]);
   
   void usage(std::ostream& os) const;
   int parseopts(int argc, char *argv[]);

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
   
   const char *getarg(int argc, char *argv[], const char *init = nullptr) const;
   
   Command(const char *name): name(name) {}
   virtual ~Command() {}
};

struct InplaceCommand: public Command {
   int mode;
   std::unique_ptr<MachO::Image> img;
   
   virtual std::string subusage() const override { return "<path>"; }
   virtual void arghandler(int argc, char *argv[]) override;
   InplaceCommand(const char *name, int mode): Command(name), mode(mode) {}
};

struct InOutCommand: Command {
   const char *in_path = nullptr, *out_path = nullptr;
   std::unique_ptr<MachO::Image> in_img, out_img;
   
   virtual std::string subusage() const override { return "<inpath> [<outpath>='a.out']"; }
   virtual void arghandler(int argc, char *argv[]) override;
   InOutCommand(const char *name): Command(name) {}
};

struct Functor {
   virtual int parse(char *option) = 0;
   virtual void operator()(MachO::MachO *macho) = 0;
   virtual ~Functor() {}
};

struct Operation: Functor {
   virtual std::vector<char *> keylist() const = 0;
   virtual int subopthandler(int index, char *value) = 0;
   virtual void validate() const = 0;

   virtual int parse(char *option) override;
   virtual ~Operation() {}
};

struct Subcommand: Functor {
   std::unique_ptr<Operation> op;

   virtual std::vector<char *> keylist() const = 0;
   virtual Operation *getop(int index) = 0;

   virtual void operator()(MachO::MachO *macho) override;
   virtual int parse(char *option) override;
   virtual ~Subcommand() {}
};

