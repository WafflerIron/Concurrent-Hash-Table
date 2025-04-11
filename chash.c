/* chash.c
 *
 * Main program that:
 *  - Reads configuration and commands from "commands.txt"
 *  - Spawns one thread per command
 *  - Waits for all threads to complete and prints the final summary and final hash table.
 *
 * Build with: make
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <stdarg.h>
#include "hash_table.h"

/* Global logging lock and file pointer for output.txt */
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
FILE *output_fp = NULL;   /* Defined in chash.c */
 
/* Global condition variables to ensure delete commands wait until all inserts finish */
pthread_mutex_t cv_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  insert_cv = PTHREAD_COND_INITIALIZER;
int remaining_inserts = 0;  // Total number of insert operations (set after scanning commands.txt)

/* Global counters for lock operations (updated in hash_table.c) */
extern int lockAcqCount;
extern int lockRelCount;

/* get_timestamp: returns a pointer to a static buffer holding the current timestamp (in microseconds) */
char *get_timestamp() {
    static char buf[32];
    struct timeval tv;
    gettimeofday(&tv, NULL);
    /* Use %ld for seconds and %06d for microseconds */
    snprintf(buf, sizeof(buf), "%ld%06d", tv.tv_sec, tv.tv_usec);
    return buf;
}

/* log_event: Thread-safe logging function. Each message gets a timestamp and newline. */
void log_event(const char *format, ...) {
    va_list args;
    pthread_mutex_lock(&log_mutex);
    fprintf(output_fp, "%s: ", get_timestamp());
    va_start(args, format);
    vfprintf(output_fp, format, args);
    va_end(args);
    fprintf(output_fp, "\n");
    fflush(output_fp);
    pthread_mutex_unlock(&log_mutex);
}

/* Command types */
typedef enum {
    CMD_INSERT,
    CMD_DELETE,
    CMD_SEARCH,
    CMD_PRINT
} cmdType;

/* Structure to hold one command */
typedef struct {
    cmdType type;
    char name[50];
    unsigned int salary;  // For insert commands; 0 for delete/search/print
} Command;

/* parse_command: Parses one line from commands.txt and returns a pointer to a Command */
Command *parse_command(char *line) {
    Command *cmd = malloc(sizeof(Command));
    if (!cmd) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    char *token = strtok(line, ",\n");
    if(!token) {
        free(cmd);
        return NULL;
    }
    if (strcasecmp(token, "insert") == 0) {
        cmd->type = CMD_INSERT;
        token = strtok(NULL, ",\n");
        if(token)
            strncpy(cmd->name, token, sizeof(cmd->name)-1);
        cmd->name[sizeof(cmd->name)-1] = '\0';
        token = strtok(NULL, ",\n");
        if(token)
            cmd->salary = (unsigned int) atoi(token);
    }
    else if (strcasecmp(token, "delete") == 0) {
        cmd->type = CMD_DELETE;
        token = strtok(NULL, ",\n");
        if(token)
            strncpy(cmd->name, token, sizeof(cmd->name)-1);
        cmd->name[sizeof(cmd->name)-1] = '\0';
        cmd->salary = 0;
    }
    else if (strcasecmp(token, "search") == 0) {
        cmd->type = CMD_SEARCH;
        token = strtok(NULL, ",\n");
        if(token)
            strncpy(cmd->name, token, sizeof(cmd->name)-1);
        cmd->name[sizeof(cmd->name)-1] = '\0';
        cmd->salary = 0;
    }
    else if (strcasecmp(token, "print") == 0) {
        cmd->type = CMD_PRINT;
        strcpy(cmd->name, "");
        cmd->salary = 0;
    }
    else {
        free(cmd);
        return NULL;
    }
    return cmd;
}

/* Thread routine: Each thread executes one command.
 * For delete commands, the thread waits (once) until all insert operations have finished.
 */
void *thread_routine(void *arg) {
    Command *cmd = (Command *)arg;
    
    switch(cmd->type) {
        case CMD_INSERT:
            /* Execute insert and log the operation inside insertRecord */
            insertRecord(cmd->name, cmd->salary);
            /* After insert, decrement the insert counter and signal if zero */
            pthread_mutex_lock(&cv_mutex);
            remaining_inserts--;
            if (remaining_inserts == 0) {
                //log_event("SIGNALING: ALL INSERTS COMPLETE");
                pthread_cond_broadcast(&insert_cv);
            }
            pthread_mutex_unlock(&cv_mutex);
            break;
        case CMD_DELETE: {
            /* Wait until all insert operations complete before deletion.
             * Log WAITING ON INSERTS only once.
             */
            pthread_mutex_lock(&cv_mutex);
            if (remaining_inserts > 0) {
                log_event("WAITING ON INSERTS");
                pthread_cond_wait(&insert_cv, &cv_mutex);
                log_event("DELETE AWAKENED");
            }
            pthread_mutex_unlock(&cv_mutex);
            deleteRecord(cmd->name);
            break;
        }
        case CMD_SEARCH: {
            unsigned int result_salary;
            //int found = 
            searchRecord(cmd->name, &result_salary);
            /*if(found) {
                Log search found with hash included by computing hash 
                uint32_t hash = jenkins_hash(cmd->name);
                log_event("SEARCH,%u,%s,%u", hash, cmd->name, result_salary);
            } else {
                log_event("SEARCH: NOT FOUND NOT FOUND");
            }*/
            break;
        }
        case CMD_PRINT:
            printTable();  // printTable() logs the necessary read lock messages internally
            break;
        default:
            break;
    }
    free(cmd);
    return NULL;
}

int main(void) {
    FILE *fp = fopen("commands.txt", "r");
    if(!fp) {
        perror("fopen commands.txt");
        exit(EXIT_FAILURE);
    }

    output_fp = fopen("output.txt", "w");
    if(!output_fp) {
        perror("fopen output.txt");
        exit(EXIT_FAILURE);
    }
    
    /* First line: threads,<numThreads>,0 */
    char line[256];
    if(fgets(line, sizeof(line), fp) == NULL) {
        fprintf(stderr, "Empty commands file.\n");
        exit(EXIT_FAILURE);
    }
    char *token = strtok(line, ",\n");
    if(!token || strcasecmp(token, "threads") != 0) {
        fprintf(stderr, "First line must specify thread count.\n");
        exit(EXIT_FAILURE);
    }
    token = strtok(NULL, ",\n");
    int numCommands = atoi(token);
    int totalThreads = numCommands;  // Total number of command threads
    
    /* Count the number of INSERT commands in the rest of the file */
    int insert_count = 0;
    long filepos = ftell(fp);
    while (fgets(line, sizeof(line), fp) != NULL) {
        char *cmd_token = strtok(line, ",\n");
        if (cmd_token && strcasecmp(cmd_token, "insert") == 0) {
            insert_count++;
        }
    }
    remaining_inserts = insert_count;
    
    /* Reset file position to start of commands (after first line) */
    fseek(fp, filepos, SEEK_SET);
    
    /* Print initial header to output file */
    fprintf(output_fp, "Running %d threads\n", totalThreads);
    fflush(output_fp);
    
    pthread_t *threads = malloc(sizeof(pthread_t) * totalThreads);
    if(!threads) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    
    int thread_index = 0;
    while(fgets(line, sizeof(line), fp) != NULL) {
        if(strlen(line) < 2) continue;
        char *linecopy = strdup(line);
        if(!linecopy) {
            perror("strdup");
            continue;
        }
        Command *cmd = parse_command(linecopy);
        free(linecopy);
        if(cmd == NULL) continue;
        pthread_create(&threads[thread_index], NULL, thread_routine, (void *)cmd);
        thread_index++;
    }
    fclose(fp);
    
    /* Wait for all threads to finish */
    for(int i = 0; i < thread_index; i++) {
        pthread_join(threads[i], NULL);
    }
    free(threads);
    
    /* Print finished message */
    fprintf(output_fp, "Finished all threads.");
    
    /* Print summary and final sorted hash table */
    fprintf(output_fp, "\n");
    fprintf(output_fp, "Number of lock acquisitions: %d\n", lockAcqCount);
    fprintf(output_fp, "Number of lock releases: %d\n", lockRelCount);
    fflush(output_fp);
    
    printTable();
    
    fclose(output_fp);
    return 0;
}
