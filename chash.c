#include <stdint.h>
#include <stdio.h>
#include "rwlock.c"

uint32_t oneTimeHash(const uint8_t* key, size_t length) {
  size_t i = 0;
  uint32_t hash = 0;
  while (i != length) {
    hash += key[i++];
    hash += hash << 10;
    hash ^= hash >> 6;
  }
  hash += hash << 3;
  hash ^= hash >> 11;
  hash += hash << 15;
  return hash;
}

typedef struct hash_struct
{
  uint32_t hash; // 32-bit unsigned integer for the hash value produced by running the name text through oneTimeHash
  char name[50]; // Arbitrary string up to 50 characters long
  uint32_t salary; // 32-bit unsigned integer to represent an annual salary.
  struct hash_struct *next; // pointer to the next node in the list
} hashRecord;

int main() {

  return 0;
}
