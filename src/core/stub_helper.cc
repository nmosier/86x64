extern "C" {
#include <xed/xed-interface.h>
}

#include "stub_helper.hh"
#include "instruction.hh"
#include "dyldinfo.hh"
#include "transform.hh"
#include "build.hh"
#include "parse.hh"
#include "xed-util.hh"

namespace MachO {

   template <Bits bits>
   StubHelperBlob<bits>::StubHelperBlob(const Image& img, const Location& loc_,
                                        ParseEnv<bits>& env): SectionBlob<bits>(loc_, env, false) {
      /* verify signatures match */
      assert(can_parse(img, loc_, env));

      Location loc = loc_;

      /* parse 'push imm32' */
      push_inst = Instruction<bits>::Parse(img, loc, env);
      loc += push_inst->size();
      jmp_inst = Instruction<bits>::Parse(img, loc, env);

      /* resolve bindee */
      env.lazy_bind_node_resolver.resolve(push_inst->imm->value, &bindee);
   }

   template <Bits bits>
   bool StubHelperBlob<bits>::can_parse(const Image& img, const Location& loc_, ParseEnv<bits>& env)
   {
      Location loc = loc_;
      xed_decoded_inst_t push_xedd, jmp_xedd;
      if (!xed::decode<bits>(img, loc.offset, push_xedd) ||
          xed_decoded_inst_get_iform_enum(&push_xedd) != XED_IFORM_PUSH_IMMz) {
         return false;
      }
      loc += xed_decoded_inst_get_length(&push_xedd);

      if (!xed::decode<bits>(img, loc.offset, jmp_xedd)) {
         return false;
      }
      const auto jmp_iform = xed_decoded_inst_get_iform_enum(&jmp_xedd);
      if (jmp_iform != XED_IFORM_JMP_RELBRz && jmp_iform != XED_IFORM_JMP_RELBRd) {
         return false;
      }
      
      return true;
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
      // SectionBlob<bits>::Build(env); -- this does allocation!
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
      SectionBlob<bits>(other, env), push_inst(other.push_inst->Transform_one(env)),
      jmp_inst(other.jmp_inst->Transform_one(env))
   {
      env.template resolve<LazyBindNode>(other.bindee, &bindee);
   }

   template class StubHelperBlob<Bits::M32>;
   template class StubHelperBlob<Bits::M64>;

}
