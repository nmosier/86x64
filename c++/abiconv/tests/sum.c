#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
   for (int i = 0; i < argc; ++i) {
      printf("%s %s\n", argv[i], argv[i]);
   }
   return 0;
}
