#pragma once

#include <map>

namespace MachO {

   /**
    * Class for tracking non-overlapping regions.
    */
   class Regions {
   public:
      void insert(std::size_t begin, std::size_t size);
      bool contains(std::size_t value) const;
      
   private:
      std::map<std::size_t, std::size_t> map; /*!< key: beginning; value; end */
   };

                  

}
