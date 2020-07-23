#ifndef __PEEPHOLE_HPP
#define __PEEPHOLE_HPP

#include <list>
#include <forward_list>

#include "cgen.hpp"
#include "asm.hpp"
#include "util.hpp"

namespace zc::z80 {

   class FreeRegister;
   
   class PeepholeOptimization {
      using iterator = Instructions::iterator;
      using const_iterator = Instructions::const_iterator;
   public:
      /**
       * Basic peephole optimization function.
       * @param in_begin beginning of current input instruction stream window.
       * @param in_end end of current instruction stream window.
       * @param out output list of replaced instructions (if any).
       * @return end iterator of instructions to delete
       */
      typedef const_iterator (*replace_t)
      (const_iterator in_begin, const_iterator in_end, Instructions& out);

      void Pass(FunctionImpl *impl);
      void ReplaceAll(Instructions& input);
      const_iterator ReplaceAt(Instructions& input, const_iterator begin);

      void Dump(std::ostream& os) const;

      PeepholeOptimization(const std::string& name, replace_t replace, int bytes_saved):
         name_(name), replace_(replace), bytes_saved_(bytes_saved) {}
      
   protected:
      const std::string name_;
      const replace_t replace_;
      const int bytes_saved_; /*!< bytes saved per replacement */
      int hits_ = 0;
      int total_ = 0;

      static void PassBlock(Block *block, PeepholeOptimization *optim);      

   public:
      template <class InputIt, class... Ts>
      static std::optional<std::tuple<Ts...>> Cast(InputIt begin, InputIt end) {
         return vec_to_tuple<InputIt, Ts...>(begin, end);
      }
   };

   extern std::forward_list<PeepholeOptimization> peephole_optims;
}

#endif
