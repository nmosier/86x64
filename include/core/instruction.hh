#pragma once

#include <vector>

extern "C" {
#include <xed/xed-interface.h>
}

#include "types.hh"
#include "section_blob.hh"
#include "opcodes.hh"

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
      
      static Instruction<bits> *Parse(const Image& img, const Location& loc, ParseEnv<bits>& env,
                                      bool add_to_map = true) {
         return new Instruction(img, loc, env, add_to_map);
      }
      
      template <typename It>
      Instruction(It begin, It end):
         SectionBlob<bits>(), instbuf(begin, end), memdisp(nullptr), brdisp(nullptr) {
         decode();
      }

      Instruction(const opcode_t& opcode): SectionBlob<bits>(), instbuf(opcode) {
         decode();
      }

      /* sometimes needs to return multiple instructions */
      virtual typename SectionBlob<bits>::SectionBlobs Transform(TransformEnv<bits>& env)
         const override;

      virtual Instruction<opposite<bits>> *Transform_one(TransformEnv<bits>& env) const override {
         return new Instruction<opposite<bits>>(*this, env);
      }

   private:
      Instruction(const Image& img, const Location& loc, ParseEnv<bits>& env, bool add_to_map);
      Instruction(const Instruction<opposite<bits>>& other, TransformEnv<opposite<bits>>& env);
      template <Bits> friend class Instruction;

      void decode();
      static void decode(xed_decoded_inst_t& xedd, const opcode_t& instbuf);
      // void patch_disp(ssize_t disp);
      void patch_relbr(xed_decoded_inst_t& xedd, opcode_t& instbuf, ssize_t disp) const;
   };   

}
