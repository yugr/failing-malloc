/*
 * Copyright 2021 Yury Gribov
 * 
 * Use of this source code is governed by MIT license that can be
 * found in the LICENSE.txt file.
 */

#include <stdlib.h>

int main() {
  int *p = malloc(100);
#ifdef SAFE
  if (p)
#endif
  *p = 0;
  return 0;
}
