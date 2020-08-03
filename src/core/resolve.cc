#include "resolve.hh"
#include "parse.hh"
#include "section_blob.hh"
#include "dyldinfo.hh"

namespace MachO {

      template <typename T, typename U, bool lazy>
      Resolver<T, U, lazy>::~Resolver() {
#if 1
         if (!todo.empty()) {
            std::cerr << "Resolver: " << name << std::endl;
            for (auto todo_pair : todo) {
               auto first = todo_pair.first;
               for (auto second : todo_pair.second) {
                  std::cerr << "  unresolved pair (" << std::hex << first << "," << second.first
                            << ") ";

                  if constexpr (std::is_pointer<T>()) {
                        std::cerr << typeid(*first).name();
                     }
                  
                  std::cerr << std::endl;
               }
            }
         }
#endif
      }

   template class Resolver<const Node *, Node, false>;
   template class Resolver<std::size_t, SectionBlob<Bits::M32>, true>;
   template class Resolver<std::size_t, SectionBlob<Bits::M64>, true>;
   template class Resolver<uint32_t, BindNode<Bits::M32, true>, false>;
   template class Resolver<uint32_t, BindNode<Bits::M64, true>, false>;

   template class Resolver<std::size_t, DylibCommand<Bits::M32>, false>;
   template class Resolver<std::size_t, DylibCommand<Bits::M64>, false>;
   template class Resolver<std::size_t, Segment<Bits::M32>, false>;
   template class Resolver<std::size_t, Segment<Bits::M64>, false>;
   template class Resolver<std::size_t, Section<Bits::M32>, false>;
   template class Resolver<std::size_t, Section<Bits::M64>, false>;


   
}
