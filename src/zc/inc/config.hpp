#ifndef __CONFIG_HPP
#define __CONFIG_HPP

namespace zc {

   /**
    * @tparam Derived a class that derives from Config<Derived>.
    */
   template <class Derived>
   struct Config {
      // static_assert(std::is_base_of<Config<Derived>, Derived>::value);
      typedef std::unordered_map< std::string, bool Derived::* > NameTable;
      const NameTable nametab;

      Config(const NameTable& nametab): nametab(nametab) {}

      /**
       * Clears all optimization options present.
       */ 
      void clear_all() {
         for (auto pair : nametab) {
            ((Derived *) this)->*(pair.second) = false;
         }
      }
      
      /**
       * Set all optimization flags.
       */
      void set_all() {
         for (auto pair : nametab) {
            ((Derived *) this)->*(pair.second) = true;
         }
      }

      template <typename Err, typename Help>
      int set_flags(const char *flags, Err err, Help help) {
         char *buf = strdup(flags);
         assert(buf);
         
         int retv = 0;
         const char *flag;
         while ((flag = strsep(&buf, ",")) != NULL) {
            if (set_flag(flag, err, help) < 0) {
               retv = -1;
            }
         }
         
         return retv;
      }

      template <typename Err, typename Help>
      int set_flag(const char *flag, Err err, Help help) {
         /* check for special flags: `all', `none' */
         if (strcmp(flag, "all") == 0) {
            set_all();
            return 0;
         }
         if (strcmp(flag, "none") == 0) {
            clear_all();
            return 0;
         }
         if (strcmp(flag, "help") == 0) {
            for (auto pair : nametab) {
               help(pair.first.c_str());
            }
            return -1;
         }
      
         const char *no = "no-";
         auto it = nametab.find(flag);
         if (it != nametab.end()) {
            ((Derived *) this)->*(it->second) = true;
            return 0;
         }

         /* search for `no-' */
         if (strncmp(flag, no, strlen(no)) == 0) {
            const char *no_flag = flag + strlen(no);
            auto it = nametab.find(no_flag);
            if (it != nametab.end()) {
               ((Derived *) this)->*(it->second) = false;
               return 0;
            }
         }

         err(flag);
         return -1;
      }
      
      
   };


   struct PrintOpts: public Config<PrintOpts> {
      bool peephole_stats = false;
      bool ralloc_info = false;
      
      PrintOpts(const NameTable& nametab): Config(nametab) {}
   };

   extern PrintOpts g_print;
   
}

#endif
