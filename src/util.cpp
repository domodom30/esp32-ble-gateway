#include "util.h"
#include <Arduino.h>

void meminfo()
{
  const int min_free_8bit_cap = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const int min_free_32bit_cap = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_32BIT);

  Serial.printf("||   Miniumum Free DRAM\t|   Minimum Free IRAM\t|| \n");
  Serial.printf("||\t%-6d\t\t|\t%-6d\t\t||\n",
         min_free_8bit_cap, (min_free_32bit_cap - min_free_8bit_cap));

  // fragmentation indicator: free heap vs largest contiguous free block
  const size_t free8 = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  const size_t largest8 = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  Serial.printf("Heap: free=%u largest=%u frag=%u%%\n",
         (unsigned)free8, (unsigned)largest8,
         free8 ? (unsigned)(100 - (largest8 * 100) / free8) : 0);
}

void meminfo(const char description[]) {
  Serial.println(description);
  meminfo();
}