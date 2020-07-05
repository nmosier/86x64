#include <stdio.h>

int main2(int argc, char *argv[]) {
   printf("&argv @ %p\n", &argv);
   printf("argv @ %p\n", argv);
   printf("*argv @ %p\n", *argv);

   for (int i = 0; i < argc; ++i) {
      printf("%s\n", argv[i]);
   }
                                     
   
   return 0;
}
