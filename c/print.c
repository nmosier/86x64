#include <stdio.h>

int main(int argc, char *argv[]) {
   for (int i = 0; i < argc; ++i) {
      printf("%s%c", argv[i], i == argc - 1 ? '\n' : ' ');
   }

   return 0;
}
