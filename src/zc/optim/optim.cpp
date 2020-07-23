#include <cstring>

#include "optim.hpp"
#include "ast.hpp"
#include "cgen.hpp"
#include "peephole.hpp"
#include "ralloc.hpp"
#include "config.hpp"

namespace zc {

   void OptimizeAST(TranslationUnit *root) {
      if (g_optim.reduce_const) {
         root->ReduceConst();
      }
   }

   void OptimizeIR(CgenEnv& env) {
      /* pass 0: register allocation */
      // RegisterAllocator::Ralloc(env);

      /* pass 1: peephole optimization */
      if (g_optim.peephole) {
         for (FunctionImpl& impl : env.impls().impls()) {
            for (PeepholeOptimization& optim : peephole_optims) {
               optim.Pass(&impl);
            }
         }

         if (g_print.peephole_stats) {
            std::cerr << "peephole-stats:" << std::endl << "NAME\t\tHITS\tTOTAL\tSAVED" << std::endl;
            for (PeepholeOptimization& optim : peephole_optims) {
               optim.Dump(std::cerr);
               std::cerr << std::endl;
            }
         }
      }
   }


}
