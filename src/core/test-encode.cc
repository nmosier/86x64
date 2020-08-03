extern "C" {
#include <xed/xed-interface.h>
}
#include <cstdio>

int main(void) {
   xed_tables_init();

   xed_decoded_inst_t xedd;
   xed_state_t dstate = {XED_MACHINE_MODE_LONG_64, XED_ADDRESS_WIDTH_32b};
   xed_error_enum_t err;
   xed_decoded_inst_zero_set_mode(&xedd, &dstate);

   const uint8_t instbuf[] = {0x90};
   if ((err = xed_decode(&xedd, instbuf, 1)) != XED_ERROR_NONE) {
      fprintf(stderr, "xed_decode: %s\n", xed_error_enum_t2str(err));
      return 1;
   }
   
   xed_encoder_request_init_from_decode(&xedd);

   uint8_t buf[16];
   unsigned len;
   if ((err = xed_encode(&xedd, buf, 16, &len)) != XED_ERROR_NONE) {
      fprintf(stderr, "xed_encode: %s\n", xed_error_enum_t2str(err));
      return 2;
   }

   for (int i = 0; i < len; ++i) {
      printf("%hhx ", buf[i]);
   }
   printf("\n");

   return 0;
}
