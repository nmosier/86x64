extern "C" {
#include <xed-interface.h>
}

#include "stub_helper.hh"

namespace MachO {

   template <Bits bits>
   StubHelperBlob<bits>::StubHelperBlob(const Image& img, const Location& loc_,
                                        ParseEnv<bits>& env): SectionBlob<bits>(loc_, env) {
      Location loc = loc_;
      
      /* parse 'push imm32' */
      push_inst = Instruction<bits>::Parse(img, loc, env);
      if (xed_decoded_inst_get_iform_enum(&push_inst->xedd) != XED_IFORM_PUSH_IMMz) {
         throw error("%s: expected instruction with iform XED_IFORM_PUSH_IMMz", __FUNCTION__);
      }
      loc += push_inst->size();

      /* parse 'jmp rel32' */
      jmp_inst = Instruction<bits>::Parse(img, loc, env);
      if (xed_decoded_inst_get_iform_enum(&jmp_inst->xedd) != XED_IFORM_JMP_RELBRz) {
         throw error("%s: expected instruction with iform XED_IFORM_JMP_RELBRz", __FUNCTION__);
      }

      /* resolve bindee */
      env.lazy_bind_node_resolver.resolve(push_inst->imm->value, &bindee);
   }

   template <Bits bits>
   void StubHelperBlob<bits>::Emit(Image& img, std::size_t offset) const {
      /* adjust push immediate */
      push_inst->imm->value = bindee->index;
      
      /* emit insturctions */
      push_inst->Emit(img, offset);
      offset += push_inst->size();
      jmp_inst->Emit(img, offset);
      offset += jmp_inst->size();
   }

   template <Bits bits>
   void StubHelperBlob<bits>::Build(BuildEnv<bits>& env) {
      SectionBlob<bits>::Build(env);
      push_inst->Build(env);
      jmp_inst->Build(env);
   }

   template <Bits bits>
   std::size_t StubHelperBlob<bits>::size() const {
      return push_inst->size() + jmp_inst->size();
   }

   template <Bits bits>
   StubHelperBlob<bits>::StubHelperBlob(const StubHelperBlob<opposite<bits>>& other,
                                        TransformEnv<opposite<bits>>& env):
      SectionBlob<bits>(other, env), push_inst(other.push_inst->Transform(env)),
      jmp_inst(other.push_inst->Transform(env))
   {
      env.resolve(other.bindee, &bindee);
   }

}
