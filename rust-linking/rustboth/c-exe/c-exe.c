#include <stdio.h>

typedef struct RustStatic RustStatic;
RustStatic* ruststatic_new();
void ruststatic_delete(RustStatic* t);

typedef struct RustBoth RustBoth;
RustBoth* rustboth_new();
void rustboth_delete(RustBoth* t);

int main(int arc, const char* argv[]) {
  printf("Starting\n");

  RustStatic* rs = ruststatic_new();
  ruststatic_delete(rs);

  RustBoth* rb = rustboth_new();
  rustboth_delete(rb);

  printf("Done\n");
  return 0;
}
