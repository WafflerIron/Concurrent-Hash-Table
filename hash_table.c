/* hash_table.c
 *
 * Implementation of the concurrent hash table.
 * The table is maintained as a sorted linked list (by hash value) and protected by a global read-write lock.
 * Synchronization events (lock acquisitions/releases) are logged to output.txt.
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <pthread.h>
 #include "hash_table.h"
 #include <sys/time.h>
 
 /* Declare external logging function defined in chash.c */
 extern void log_event(const char *format, ...);
 
 /* Global head pointer for the hash table */
 hashRecord *head = NULL;
 
 /* Global read-write lock for the hash table */
 pthread_rwlock_t table_lock = PTHREAD_RWLOCK_INITIALIZER;
 
 /* Global counters for lock operations */
 int lockAcqCount = 0;
 int lockRelCount = 0;
 
 /* jenkins_hash: Computes hash value using Jenkins’s one-at-a-time hash function */
 uint32_t jenkins_hash(const char *key) {
     uint32_t hash = 0;
     while(*key) {
         hash += (unsigned char)(*key++);
         hash += (hash << 10);
         hash ^= (hash >> 6);
     }
     hash += (hash << 3);
     hash ^= (hash >> 11);
     hash += (hash << 15);
     return hash;
 }
 
 /* insertRecord: Inserts (or updates) a record with a given name and salary.
  * Logs lock events and the operation (including computed hash) to output.txt.
  */
 void insertRecord(const char *name, uint32_t salary) {
     uint32_t hash = jenkins_hash(name);
     log_event("WRITE LOCK ACQUIRED");
     pthread_rwlock_wrlock(&table_lock);
     lockAcqCount++;
 
     /* Search for an existing record */
     hashRecord *cur = head, *prev = NULL;
     while(cur) {
         if(cur->hash == hash) {
             cur->salary = salary;
             pthread_rwlock_unlock(&table_lock);
             lockRelCount++;
             log_event("WRITE LOCK RELEASED");
             /* Log the update operation with hash value */
             log_event("INSERT,%u,%s,%u", hash, name, salary);
             return;
         }
         if(cur->hash > hash) break;
         prev = cur;
         cur = cur->next;
     }
     
     /* Record not found: allocate new record */
     hashRecord *new_rec = malloc(sizeof(hashRecord));
     if(!new_rec) {
         perror("malloc");
         pthread_rwlock_unlock(&table_lock);
         lockRelCount++;
         log_event("WRITE LOCK RELEASED");
         return;
     }
     new_rec->hash = hash;
     strncpy(new_rec->name, name, sizeof(new_rec->name)-1);
     new_rec->name[sizeof(new_rec->name)-1] = '\0';
     new_rec->salary = salary;
     new_rec->next = cur;
     
     if(prev == NULL) {
         head = new_rec;
     } else {
         prev->next = new_rec;
     }
     
     pthread_rwlock_unlock(&table_lock);
     lockRelCount++;
     log_event("WRITE LOCK RELEASED");
     /* Log the insert operation */
     log_event("INSERT,%u,%s,%u", hash, name, salary);
 }
 
 /* deleteRecord: Deletes the record with the given name.
  * Logs lock events and the deletion operation (with computed hash) to output.txt.
  */
 void deleteRecord(const char *name) {
     uint32_t hash = jenkins_hash(name);
     log_event("WRITE LOCK ACQUIRED");
     pthread_rwlock_wrlock(&table_lock);
     lockAcqCount++;
     
     hashRecord *cur = head, *prev = NULL;
     while(cur) {
         if(cur->hash == hash) {
             /* Remove the record from the list */
             if(prev == NULL) {
                 head = cur->next;
             } else {
                 prev->next = cur->next;
             }
             free(cur);
             pthread_rwlock_unlock(&table_lock);
             lockRelCount++;
             log_event("WRITE LOCK RELEASED");
             /* Log the deletion */
             log_event("DELETE,%u,%s", hash, name);
             return;
         }
         prev = cur;
         cur = cur->next;
     }
     
     /* Record not found: just release the lock */
     pthread_rwlock_unlock(&table_lock);
     lockRelCount++;
     log_event("WRITE LOCK RELEASED");
 }
 
 /* searchRecord: Searches for a record with the given name.
  * Acquires a read lock, sets *salary if found, and returns 1 if found, 0 if not.
  * Logs lock operations.
  */
 int searchRecord(const char *name, uint32_t *salary) {
     uint32_t hash = jenkins_hash(name);
     log_event("READ LOCK ACQUIRED");
     pthread_rwlock_rdlock(&table_lock);
     lockAcqCount++;
     
     hashRecord *cur = head;
     int found = 0;
     while(cur) {
         if(cur->hash == hash) {
             *salary = cur->salary;
             found = 1;
             break;
         }
         cur = cur->next;
     }
     
     pthread_rwlock_unlock(&table_lock);
     lockRelCount++;
     log_event("READ LOCK RELEASED");
     return found;
 }
 
 /* printTable: Prints the entire hash table (sorted by hash).
  * Logs its read lock acquire/release.
  */
 void printTable(void) {
     log_event("READ LOCK ACQUIRED");
     pthread_rwlock_rdlock(&table_lock);
     lockAcqCount++;
     
     hashRecord *cur = head;
     while(cur) {
         fprintf(output_fp, "%u,%s,%u\n", cur->hash, cur->name, cur->salary);
         cur = cur->next;
     }
     fflush(output_fp);
     
     pthread_rwlock_unlock(&table_lock);
     lockRelCount++;
     log_event("READ LOCK RELEASED");
 }
 