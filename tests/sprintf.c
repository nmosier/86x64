#include <stdio.h>

int main(int argc, char *argv[]) {
   char s[64];
   (sprintf)(s, "%d%d%d%d\n", argc, argc, argc, argc);
   puts(s);
   return 0;
}
