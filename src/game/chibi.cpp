#include "chibi.h"

namespace cat {

const char* speciesName(int sp) {
  static const char* n[SP_COUNT] = {"Tabby", "Siamese", "Tuxedo", "Void Cat", "Calico", "Sphynx", "Skogkatt", "Kitten", "Rainbow Cat", "Hamster"};
  return (unsigned)sp < SP_COUNT ? n[sp] : "?";
}

}  // namespace cat
