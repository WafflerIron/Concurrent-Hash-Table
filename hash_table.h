/* hash_table.h
 *
 * Declarations for the concurrent hash table.
 */
#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <stdint.h>
#include <stdio.h>
#include <pthread.h>

/* Definition of the record in the hash table */
typedef struct hash_struct {
    uint32_t hash;
    char name[50];
    uint32_t salary;
    struct hash_struct *next;
} hashRecord;

/* Public functions */
void insertRecord(const char *name, uint32_t salary);
void deleteRecord(const char *name);
int searchRecord(const char *name, uint32_t *salary);  // returns 1 if found, 0 otherwise
void printTable(void);

/* Jenkins hash function used by search command in chash.c */
uint32_t jenkins_hash(const char *key);

/* Global read-write lock for the table (defined in hash_table.c) */
extern pthread_rwlock_t table_lock;

/* Global head pointer for the hash table */
extern hashRecord *head;

/* Global counters for lock operations */
extern int lockAcqCount;
extern int lockRelCount;

/* Declare external output file pointer so that hash_table.c can write to it */
extern FILE *output_fp;

#endif /* HASH_TABLE_H */
