#include <stdio.h>

void target_v1() {
  printf("Called interposer v1 fn\n");
}
__asm__(".symver target_v1,target@TARGET_1_0_0");


int target_v2() {
  printf("Called interposer v2 fn\n");
  return 42;
}
__asm__(".symver target_v2,target@TARGET_2_0_0");
