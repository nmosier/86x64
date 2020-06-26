#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <scanopt.h>

#include "macho.hh"

int main(int argc, char *argv[]) {
   const char *usagestr =
      "usage: %1$s -S segname -s sectname -i index inpath [outpath=a.out]\n" \
      "       %1$s -h\n"
      ;
      
   auto usage = [=] (FILE *f) { fprintf(f, usagestr, argv[0]); };
   const char *optstring = "S:s:i+h";

   const char *segname = nullptr;
   const char *sectname = nullptr;
   unsigned long instruction_index = 0;
   int help = 0;

   if (scanopt(argc, argv, optstring, &segname, &sectname, &instruction_index, &help) < 0) {
      usage(stderr);
      return 1;
   }

   if (help) {
      usage(stdout);
      return 0;
   }

   const char *inpath;
   const char *outpath = "a.out";

   if (optind + 1 > argc) {
      usage(stderr);
      return 1;
   }
   inpath = argv[optind++];
   
   if (optind + 1 <= argc) {
      outpath = argv[optind++];
   }
   
   MachO::Image in_img(inpath, O_RDONLY);
   MachO::Image out_img(outpath, O_RDWR | O_CREAT | O_TRUNC);

   MachO::init();

   MachO::MachO *macho = MachO::MachO::Parse(in_img);
   macho->Build();

   macho->Emit(out_img);

   return 0;
}
