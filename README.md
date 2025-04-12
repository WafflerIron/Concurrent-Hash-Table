# Concurrent-Hash-Table

Used ChatGPT to create the initial files. Did this by starting prompt with "Write a program in C that accomplishes the following" and then copied the entire assignment instructions. This led it to create almost fully functioning chash.c, hash_table.c, and hash_table.h files as well as a Makefile.

From there, it was a manner of manually fixing a few bugs (mostly problems calling functions from other files) and manually fixing formatting. Also fixed one issue where it would print the result of a search function after the reader unlocked, rather than in between the reader locking and unlocking.
