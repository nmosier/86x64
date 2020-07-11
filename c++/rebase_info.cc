#include <string>
#include <iostream>
#include <mach-o/loader.h>
#include <typeinfo>

#include "rebase_info.hh"
#include "image.hh"
#include "leb.hh"
#include "instruction.hh" // Immediate
#include "parse.hh"
#include "archive.hh"

namespace MachO {

   template <Bits bits>
   RebaseInfo<bits>::RebaseInfo(const Image& img, std::size_t offset, std::size_t size,
                                ParseEnv<bits>& env) {
      const std::size_t begin = offset;
      const std::size_t end = begin + size;
      std::size_t it = begin;
      uint8_t type = 0;
      std::size_t vmaddr = 0;
      while (it != end) {
         const uint8_t byte = img.at<uint8_t>(it);
         const uint8_t opcode = byte & REBASE_OPCODE_MASK;
         const uint8_t imm = byte & REBASE_IMMEDIATE_MASK;
         ++it;

         std::size_t uleb, uleb2;
         
         switch (opcode) {
         case REBASE_OPCODE_DONE:
            return;
            
         case REBASE_OPCODE_SET_TYPE_IMM:
            type = imm;
            break;
            
         case REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
            it += leb128_decode(img, it, vmaddr);
            vmaddr += env.archive.segment(imm)->vmaddr();
            break;

         case REBASE_OPCODE_ADD_ADDR_ULEB:
            it += leb128_decode(img, it, uleb);
            vmaddr += uleb;
            break;

         case REBASE_OPCODE_ADD_ADDR_IMM_SCALED:
            vmaddr += imm * sizeof(ptr_t);
            break;

         case REBASE_OPCODE_DO_REBASE_IMM_TIMES:
            vmaddr = do_rebase_times(imm, vmaddr, env, type);
            break;
            
         case REBASE_OPCODE_DO_REBASE_ULEB_TIMES:
            it += leb128_decode(img, it, uleb);
            vmaddr = do_rebase_times(uleb, vmaddr, env, type);
            break;

         case REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB:
            it += leb128_decode(img, it, uleb);
            vmaddr = do_rebase(vmaddr, env, type);
            vmaddr += uleb;
            break;
            
         case REBASE_OPCODE_DO_REBASE_ULEB_TIMES_SKIPPING_ULEB:
            it += leb128_decode(img, it, uleb);
            it += leb128_decode(img, it, uleb2);
            vmaddr = do_rebase_times(uleb, vmaddr, env, type, uleb2);
            break;
            
         default:
            throw error("%s: invalid rebase opcode", __FUNCTION__);
         }
         
      }
   }

   template <Bits bits>
   std::size_t RebaseInfo<bits>::do_rebase(std::size_t vmaddr, ParseEnv<bits>& env, uint8_t type) {
      // std::cerr << "[PARSE] REBASE @ 0x" << std::hex << vmaddr << std::endl;
      rebasees.push_back(RebaseNode<bits>::Parse(vmaddr, env, type));
      return vmaddr + sizeof(ptr_t);
   }

   template <Bits bits>
   std::size_t RebaseInfo<bits>::do_rebase_times(std::size_t count, std::size_t vmaddr,
                                                 ParseEnv<bits>& env, uint8_t type,
                                                 std::size_t skipping) {
      for (std::size_t i = 0; i < count; ++i) {
         do_rebase(vmaddr, env, type);
         vmaddr += sizeof(ptr_t) + skipping;
      }
      return vmaddr;
   }

   template <Bits bits>
   std::size_t RebaseInfo<bits>::size() const {
      std::size_t size = 0;
      for (const RebaseNode<bits> *node : rebasees) {
         size += node->size();
      }
      if (size > 0) {
         ++size; /* REBASE_OPCODE_DONE */
      }
      return align<bits>(size);
   }

   template <Bits bits>
   RebaseNode<bits>::RebaseNode(std::size_t vmaddr, ParseEnv<bits>& env, uint8_t type):
      type(type), blob(nullptr)
   {
      struct callback: decltype(env.vmaddr_resolver)::functor {
         ParseEnv<bits>& env;

         callback(ParseEnv<bits>& env): env(env) {}
         
         virtual void operator()(SectionBlob<bits> *blob2) override {
            fprintf(stderr, "callback at 0x%zx type %s\n", blob2->loc.vmaddr, typeid(*blob2).name());
            auto imm = dynamic_cast<Immediate<bits> *>(blob2);
            if (imm) {
               imm->pointee = env.add_placeholder(imm->value);
            }
         }
      };
      
      env.vmaddr_resolver.resolve(vmaddr, &blob, std::make_shared<callback>(env));
   }

   template <Bits bits>
   std::size_t RebaseNode<bits>::size() const {
      if (!active()) {
         return 0;
      }
      
      /* 1   REBASE_OPCODE_SET_TYPE_IMM
       * 1+a REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB
       * 1   REBASE_OPCODE_DO_REBASE_IMM_TIMES
       * 3+a total
       */
      assert(blob->segment->id < 16);
      return 3 + leb128_size(blob->loc.offset - blob->segment->loc().offset);
   }

   template <Bits bits>
   RebaseNode<bits>::RebaseNode(const RebaseNode<opposite<bits>>& other,
                                TransformEnv<opposite<bits>>& env):
      type(other.type), blob(nullptr)
   {
      env.resolve(other.blob, &blob);
   }

   template <Bits bits>
   RebaseInfo<bits>::RebaseInfo(const RebaseInfo<opposite<bits>>& other,
                                TransformEnv<opposite<bits>>& env) {
      for (const auto other_rebasee : other.rebasees) {
         rebasees.push_back(other_rebasee->Transform(env));
      }
   }

   template <Bits bits>
   void RebaseInfo<bits>::Emit(Image& img, std::size_t offset) const {
      for (RebaseNode<bits> *rebasee : rebasees) {
         rebasee->Emit(img, offset);
         offset += rebasee->size();
      }
      if (!rebasees.empty()) {
         img.at<uint8_t>(offset) = REBASE_OPCODE_DONE;
      }
   }

   template <Bits bits>
   void RebaseNode<bits>::Emit(Image& img, std::size_t offset) const {
      if (!active()) {
         return;
      }
      
      /* REBASE_OPCODE_SET_TYPE_IMM
       * REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB
       * REBASE_OPCODE_DO_REBASE_IMM_TIMES
       */
      img.at<uint8_t>(offset++) = REBASE_OPCODE_SET_TYPE_IMM | type;
      img.at<uint8_t>(offset++) = REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB | blob->segment->id;
      const std::size_t segoff = blob->loc.vmaddr - blob->segment->loc().vmaddr;
      offset += leb128_encode(img, offset, segoff);
      // offset += leb128_encode(&img.at<uint8_t>(offset), img.size() - offset, segoff);
      img.at<uint8_t>(offset++) = REBASE_OPCODE_DO_REBASE_IMM_TIMES | 0x1;
   }

   template <Bits bits>
   void RebaseInfo<bits>::print(std::ostream& os) const {
      os << "segment\tsection\taddress\ttype" << std::endl;
      for (auto rebasee : rebasees) {
         rebasee->print(os);
         os << std::endl;
      }
   }

   template <Bits bits>
   void RebaseNode<bits>::print(std::ostream& os) const {
      os << blob->segment->name() << "\t" << blob->section->name() << "\t"
         << blob->loc.vmaddr << "\t";

      std::unordered_map<uint8_t, std::string> types =
         {{REBASE_TYPE_POINTER, "POINTER"},
          {REBASE_TYPE_TEXT_ABSOLUTE32, "TEXT_ABSOLUTE32"},
          {REBASE_TYPE_TEXT_PCREL32, "TEXT_PCREL32"}};
      auto type_it = types.find(type);
      if (type_it != types.end()) {
         os << type_it->second;
      } else {
         os << "(invalid)";
      }
   }

   template class RebaseInfo<Bits::M32>;
   template class RebaseInfo<Bits::M64>;

}
