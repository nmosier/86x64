#include <cstdio>

int yyparse(void);

int main(void) {
   if (yyparse() < 0) {
      fprintf(stderr, "parsing failed\n");
   }

   return 0;
}
