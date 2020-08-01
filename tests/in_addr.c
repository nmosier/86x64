#include <stdio.h>
#include <arpa/inet.h>

int main(int argc, char *argv[]) {
   const char *ip = "192.168.12.0";
   struct in_addr ia;
   inet_aton(ip, &ia);
   printf("%u\n", ia.s_addr);
   return 0;
}
