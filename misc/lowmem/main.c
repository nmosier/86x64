#include <stdio.h>
#include <stdlib.h>

int main2(int argc, char *argv[]) {
   printf("&argv @ %p\n", &argv);
   printf("argv @ %p\n", argv);
   printf("*argv @ %p\n", *argv);

   for (int i = 0; i < argc; ++i) {
      printf("%s\n", argv[i]);
   }

   printf("testing malloc... %p\n", malloc(42));

   printf("printf @ %p\n", (void *) &printf);

   
                                        
   return 0;
}
