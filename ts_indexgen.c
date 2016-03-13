/*
 ============================================================================
 Name        : ts_indexgen.c
 Author      : Ani Kristo
 ============================================================================
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

// Constant definitions
#define MAX_THREADS 8
#define MIN_THREADS 1
#define MAX_LINE_SIZE 4096
#define MAX_WORD_SIZE 64
#define MAX_BUF_SIZE 1000
#define ENG_CHAR_CNT 26

#define ASCII_a 97

#define TRUE 1
#define FALSE 0

// Function prototypes
void* runner(void*);

// Global variables
int buffer_size;
int nr_threads;
struct item **buffers;
pthread_t *t_ids;
int word_thread_lookup[26]; // 26 for all the letters in the English alphabet

// Structure definitions
struct item {
	char word[MAX_WORD_SIZE];
	int line;
	struct item* next;
};

// Executing starts here
int main(int argc, char** argv) {
	// Validate input
	if (argc < 5)
		puts("Insufficient parameters passed!\nMake sure you enter: "
				"indexgen_t <n> <infile> <outfile>");
	else if (argc > 5)
		puts("Too many parameters passed!\nMake sure you enter: "
				"indexgen_t <n> <infile> <outfile>");
	else if (atoi(argv[1]) < MIN_THREADS || atoi(argv[1]) > MAX_THREADS)
		printf("%s Make sure the first argument <n> "
				"is an integer between %d and %d!\n", argv[1], MIN_THREADS,
		MAX_THREADS);
	else if (atoi(argv[2]) < 0 || atoi(argv[2]) > MAX_BUF_SIZE)
		printf("%s Make sure the second argument <n> "
				"is an integer between %d and %d!\n", argv[2], 0, MAX_BUF_SIZE);
	else {

		// Valid input given. Parsing.
		nr_threads = atoi(argv[1]);
		buffer_size = atoi(argv[2]);
		char *infile = argv[3];
		char *outfile = argv[4];

		// Creating the word - thread lookup
		/*
		 * Assign threads to some words according to the letter they start with
		 *
		 * At index 0 the cell will show the number of the worker for words that start with letter a
		 * At index 1 -> b
		 * ...
		 * At index 25 --> z
		 */
		int i, j, step = ENG_CHAR_CNT / nr_threads;
		for (i = 0; i < nr_threads; i++) {
			for (j = 0; j < step; j++) {
				word_thread_lookup[i * step + j] = i;
			}

		}

		// Add the last ENG_CHAR_CNT % n letter of the alphabet
		for (i = ENG_CHAR_CNT % nr_threads; i > 0; i--) {
			word_thread_lookup[ENG_CHAR_CNT - i] = nr_threads - 1;
		}


		// Create buffers
		buffers = calloc(nr_threads, sizeof(struct item*));
		for (i = 0; i < nr_threads; i++) {
			buffers[i] = calloc(buffer_size, sizeof(struct item));
		}

		// Create threads
		t_ids = calloc(nr_threads, sizeof(pthread_t));
		for (i = 0; i < nr_threads; i++) {
			pthread_create(&t_ids[i], NULL, runner, (void*) &i);
		}

		// Wait for all threads to finish
		for (i = 0; i < nr_threads; i++) {
			pthread_join(t_ids[i], NULL);
		}

		// Free memory
		for (i = 0; i < nr_threads; i++) {
			free(buffers[i]);
		}
		free(buffers);

		return EXIT_SUCCESS;
	}
	return EXIT_FAILURE;
}

void* runner(void* param) {
	int index = *((int*)(param));

	printf("%d\n", index);

	return NULL;
}

