#pragma once

#include <list>

#include "types.hh"

namespace MachO {

   template <Bits bits>
   class RebaseNode {
   public:
      using ptr_t = select_type<bits, uint32_t, uint64_t>;

      uint8_t type;
      const SectionBlob<bits> *blob;
      
      std::size_t size() const;
      void Emit(Image& img, std::size_t offset) const;

      bool active() const { return blob == nullptr ? false : blob->active; }

      static RebaseNode<bits> *Parse(std::size_t vmaddr, ParseEnv<bits>& env, uint8_t type) {
         return new RebaseNode(vmaddr, env, type);
      }

      RebaseNode<opposite<bits>> *Transform(TransformEnv<bits>& env) {
         return new RebaseNode<opposite<bits>>(*this, env);
      }
         
   private:
      RebaseNode(std::size_t vmaddr, ParseEnv<bits>& env, uint8_t type);
      RebaseNode(const RebaseNode<opposite<bits>>& other, TransformEnv<opposite<bits>>& env);
      template <Bits> friend class RebaseNode;
   };

   template <Bits bits>
   class RebaseInfo {
   public:
      using ptr_t = select_type<bits, uint32_t, uint64_t>;
      using Rebasees = std::list<RebaseNode<bits> *>;

      Rebasees rebasees;

      std::size_t size() const;
      void Emit(Image& img, std::size_t offset) const;

      static RebaseInfo<bits> *Parse(const Image& img, std::size_t offset, std::size_t size,
                                     ParseEnv<bits>& env) {
         return new RebaseInfo(img, offset, size, env);
      }
      
      RebaseInfo<opposite<bits>> *Transform(TransformEnv<bits>& env) const {
         return new RebaseInfo<opposite<bits>>(*this, env);
      }
      
   private:
      RebaseInfo(const Image& img, std::size_t offset, std::size_t size, ParseEnv<bits>& env);
      RebaseInfo(const RebaseInfo<opposite<bits>>& other, TransformEnv<opposite<bits>>& env);
      std::size_t do_rebase(std::size_t vmaddr, ParseEnv<bits>& env, uint8_t type);
      std::size_t do_rebase_times(std::size_t count, std::size_t vmaddr, ParseEnv<bits>& env,
                                  uint8_t type, std::size_t skipping = 0);
      template <Bits> friend class RebaseInfo;
   };   

}
