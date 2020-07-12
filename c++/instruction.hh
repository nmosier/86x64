#pragma once

#include <vector>

extern "C" {
#include <xed-interface.h>
}

#include "types.hh"
#include "section_blob.hh"

namespace MachO {

   template <Bits bits>
   class Instruction: public SectionBlob<bits> {
   public:
      static const xed_state_t& dstate();
      
      std::vector<uint8_t> instbuf;
      xed_decoded_inst_t xedd;
      unsigned memidx;
      const SectionBlob<bits> *memdisp = nullptr; /*!< memory displacement pointee */
      Immediate<bits> *imm = nullptr;
      const SectionBlob<bits> *brdisp = nullptr;  /*!< branch displacement pointee */

      RelocBlob<bits> *reloc = nullptr; /*!< relocation pointee (owned) */
      
      virtual std::size_t size() const override { return instbuf.size(); }
      virtual void Emit(Image& img, std::size_t offset) const override;
      virtual void Build(BuildEnv<bits>& env) override;
      
      static Instruction<bits> *Parse(const Image& img, const Location& loc, ParseEnv<bits>& env)
      { return new Instruction(img, loc, env); }
      
      template <typename It>
      Instruction(It begin, It end):
         SectionBlob<bits>(), instbuf(begin, end), memdisp(nullptr), brdisp(nullptr) {}
      
      Instruction<opposite<bits>> *Transform(TransformEnv<bits>& env) const override {
         return new Instruction<opposite<bits>>(*this, env);
      }

   private:
      Instruction(const Image& img, const Location& loc, ParseEnv<bits>& env);
      Instruction(const Instruction<opposite<bits>>& other, TransformEnv<opposite<bits>>& env);
      template <Bits> friend class Instruction;

      void decode();
   };   

}
