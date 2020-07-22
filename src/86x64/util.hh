#pragma once

#include <string>

std::string strmap(const std::string& s, int (*map)(int));
std::string strfilter(const std::string& s, int (*filter)(int));


inline std::ostream& strjoin(std::ostream& os, const std::string& separator) {
   return os;
}

template <typename Head>
std::ostream& strjoin(std::ostream& os, const std::string& separator, Head&& head) {
   return os << head;
}

template <typename Head1, typename Head2, typename... Tail>
std::ostream& strjoin(std::ostream& os, const std::string& separator, Head1&& head1, Head2&& head2,
                      Tail&&... tail) {
   return strjoin(os << head1 << separator, separator, head2, tail...);
}
