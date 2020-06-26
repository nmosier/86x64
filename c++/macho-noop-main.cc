#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

#include "macho.hh"

int main(int argc, char *argv[]) {
   const char *usage = "usage: %s path\n";

   if (argc < 2 || argc > 3) {
      fprintf(stderr, usage, argv[0]);
      return 1;
   }
   const char *out_path = argc == 2 ? "a.out" : argv[2];

   MachO::Image img(argv[1], O_RDONLY);
   MachO::Image out_img(out_path, O_RDWR | O_CREAT | O_TRUNC);

   MachO::init();
   
   MachO::MachO *macho = MachO::MachO::Parse(img);
   macho->Build();
   macho->Emit(out_img);
   (void) macho;

   return 0;
}
