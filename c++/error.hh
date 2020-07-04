#pragma once

#include <exception>
#include <cstdlib>
#include <cstring>

namespace MachO {

   class exception: public std::exception {
   public:
      virtual const char *what() const noexcept override { return msg.c_str(); }

      exception(const std::string& msg): msg(msg) {}
      exception(const char *msg): msg(msg) {}
      
      template <int N, typename... Args>
      exception(const char (&fmt)[N], Args&&... args) {
         char *msg;
         if (asprintf(&msg, fmt, args...) < 0) {
            throw std::runtime_error(std::string("asprintf: ") + strerror(errno));
         }
         this->msg = std::string(msg);
         free(msg);
      }
      
   private:
      std::string msg;
   };

   class bad_format: public exception {
   public:
      template <typename... Args>
      bad_format(Args&&... args): exception(args...) {}
   };
   
   class unsupported_format: public bad_format {
   public:
      template <typename... Args>
      unsupported_format(Args&&... args): bad_format(args...) {}
   };

   class unrecognized_format: public bad_format {
   public:
      template <typename... Args>
      unrecognized_format(Args&&... args): bad_format(args...) {}
   };
   
}
