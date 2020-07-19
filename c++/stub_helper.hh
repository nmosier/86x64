#pragma once

#include "types.hh"
#include "section.hh"

namespace MachO {

#if 0
   template <Bits bits>
   class StubHelper: public Section<bits> {
   public:
      
      
   private:
   };
#endif

   template <Bits bits>
   class StubHelperBlob: public SectionBlob<bits> {
   public:
      Instruction<bits> *push_inst = nullptr;
      Instruction<bits> *jmp_inst = nullptr;
      const BindNode<bits, true> *bindee = nullptr;

      static StubHelperBlob<bits> *Parse(const Image& img, const Location& loc, ParseEnv<bits>& env)
      { return new StubHelperBlob<bits>(img, loc, env); }

      virtual std::size_t size() const override;
      virtual void Build(BuildEnv<bits>& env) override;
      virtual void Emit(Image& img, std::size_t offset) const override;
      virtual StubHelperBlob<opposite<bits>> *Transform_one(TransformEnv<bits>& env) const override {
         return new StubHelperBlob<opposite<bits>>(*this, env);
      }

   private:
      StubHelperBlob(const Image& img, const Location& loc, ParseEnv<bits>& env);
      StubHelperBlob(const StubHelperBlob<opposite<bits>>& other, TransformEnv<opposite<bits>>& env);
   };

}
