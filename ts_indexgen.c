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
#include <ctype.h>

// Constant definitions
#define MAX_THREADS 8
#define MIN_THREADS 1
#define MAX_LINE_SIZE 4096
#define MAX_WORD_SIZE 64
#define MAX_BUF_SIZE 1000
#define ENG_CHAR_CNT 26

#define TRUE 1
#define FALSE 0

#define ASCII_a 97

#define TRUE 1
#define FALSE 0

// Structure definitions
typedef struct {
	char word[MAX_WORD_SIZE];
	int line;
} buffer_item_t;

typedef struct {
	buffer_item_t* buf;
	int occupied;
	int nextin;
	int nextout;
	pthread_mutex_t mutex;
	pthread_cond_t full;
	pthread_cond_t empty;
} buffer_t;

// LL structures
struct ll_lines_item_t {
	int line_nr;
	struct ll_lines_item_t* next;
};

struct ll_word_item_t {
	char word[MAX_WORD_SIZE];
	struct ll_lines_item_t* line_nrs; // where the word was found in the input
	struct ll_word_item_t* next;
};

// Function prototypes
void* consume(void*);
void produce(char*);
void insert_line_nr(struct ll_lines_item_t**, int);
void output_file(char* filename);
int word_exists(struct ll_word_item_t**, const char*, struct ll_word_item_t**);
int line_exists(struct ll_word_item_t*, int);

// Global variables
int buffer_size;
int nr_threads;
buffer_t* buffers;
pthread_t *t_ids;
void** results;

int word_thread_lookup[ENG_CHAR_CNT];

// Executing starts here
int main(int argc, char** argv) {
	// Validate input
	if (argc < 5)
		puts("Insufficient parameters passed!\nMake sure you enter: "
				"./ts_indexgen <m> <n> <infile> <outfile>");
	else if (argc > 5)
		puts("Too many parameters passed!\nMake sure you enter: "
				"./ts_indexgen <m> <n> <infile> <outfile>");
	else if (atoi(argv[1]) < MIN_THREADS || atoi(argv[1]) > MAX_THREADS)
		printf("%s Make sure the first argument <m> "
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

		for (i = ENG_CHAR_CNT % nr_threads; i > 0; i--) { // Add the last ENG_CHAR_CNT % n letter of the alphabet
			word_thread_lookup[ENG_CHAR_CNT - i] = nr_threads - 1;
		}

		// Create buffers
		buffers = calloc(nr_threads, sizeof(buffer_t));
		for (i = 0; i < nr_threads; i++) {
			buffers[i].buf = calloc(buffer_size, sizeof(buffer_item_t));
			buffers[i].nextin = 0;
			buffers[i].nextout = 0;
			buffers[i].occupied = 0;

			pthread_mutex_init(&(buffers[i].mutex), NULL);
			pthread_cond_init(&(buffers[i].empty), NULL);
			pthread_cond_init(&(buffers[i].full), NULL);
		}

		// Set all results to point to NULL
		results = calloc(nr_threads, sizeof(void*));
		for (i = 0; i < nr_threads; i++) {
			results[i] = NULL;
		}

		// Create threads
		t_ids = calloc(nr_threads, sizeof(pthread_t));
		for (i = 0; i < nr_threads; i++) {
			pthread_create(&t_ids[i], NULL, consume, &buffers[i]);
		}

		// Start producing words
		produce(infile);

		// Wait for all threads to finish
		for (i = 0; i < nr_threads; i++) {
			pthread_join(t_ids[i], &results[i]);
		}

		// Destroying conditions and mutexes
		for (i = 0; i < nr_threads; i++) {
			pthread_cond_destroy(&(buffers[i].empty));
			pthread_cond_destroy(&(buffers[i].full));
			pthread_mutex_destroy(&(buffers[i].mutex));
		}

		output_file(outfile);

		// Free memory
		for (i = 0; i < nr_threads; i++) {
			free(buffers[i].buf);
		}
		free(buffers);
		free(t_ids);

		return EXIT_SUCCESS;
	}
	return EXIT_FAILURE;
}

// Consumer
void* consume(void* param) {
	buffer_t* respective_buffer = ((buffer_t*) param);
	buffer_item_t* new_item = NULL;
	struct ll_word_item_t* ll = NULL;

	while (1) {

		// Start reading
		pthread_mutex_lock(&respective_buffer->mutex); // beginning of critical section

		while (respective_buffer->occupied == 0)
			pthread_cond_wait(&respective_buffer->empty,
					&respective_buffer->mutex);

		// consume item
		new_item = &respective_buffer->buf[respective_buffer->nextout];

		respective_buffer->nextout = (respective_buffer->nextout + 1)
				% buffer_size;
		respective_buffer->occupied--;

		// Check if end of input
		if (new_item->line == -1) { // end
			pthread_mutex_unlock(&respective_buffer->mutex);
			break;
		}

		struct ll_word_item_t* nu = malloc(sizeof(struct ll_word_item_t));
		strcpy(nu->word, new_item->word);
		nu->next = NULL;
		nu->line_nrs = malloc(sizeof(struct ll_lines_item_t));
		nu->line_nrs->line_nr = new_item->line;
		nu->line_nrs->next = NULL;

		struct ll_word_item_t* current;
		// Special case for the head end
		if (ll == NULL || strcmp(ll->word, new_item->word) > 0) {
			nu->next = ll;
			ll = nu;
		}
		// head is equal
		else if (strcmp(ll->word, new_item->word) == 0) {
			insert_line_nr(&(ll->line_nrs), new_item->line);
		}
		// insert in the middle or end
		else {
			// Locate the node before the point of insertion
			current = ll;
			while (current->next != NULL
					&& strcmp(current->next->word, nu->word) < 0) {
				current = current->next;
			}

			if (current->next && strcmp(current->next->word, nu->word) == 0) // stopped because it is equal
				insert_line_nr(&(current->next->line_nrs), new_item->line);

			else {
				nu->next = current->next;
				current->next = nu;
			}
		}

		// wake up waiting producers
		if (respective_buffer->occupied == buffer_size - 1)
			pthread_cond_signal(&respective_buffer->full);

		pthread_mutex_unlock(&respective_buffer->mutex); // end of critical section

	}
	pthread_exit((void*) ll);
	return NULL;
}

// Producer
void produce(char* filename) {
	FILE* infile = fopen(filename, "r");

// Open input file
	if (!infile) {
		perror("Cannot open input file! Make sure it exists.");
		exit(EXIT_FAILURE);
	} else {

		// Start reading the input
		int line_cnt = 0;
		char* cur_line = NULL;
		size_t length = 0;
		ssize_t read_size;
		while ((read_size = getline(&cur_line, &length, infile)) != -1) {

			// increment line count
			line_cnt++;

			// remove the end-of-line character
			if (length > 1 && cur_line[read_size - 1] == '\n') {
				cur_line[read_size - 1] = '\0';
				if (read_size > 1 && cur_line[read_size - 2] == '\r')
					cur_line[read_size - 2] = '\0';
			}

			// convert to lower case
			char* p;
			for (p = cur_line; *p; p++) {
				*p = tolower(*p);
			}

			// read words
			int initial_char_distance;
			char* dlmt = " ";
			char* word = strtok(cur_line, dlmt);

			while (word != NULL) {

				// Calculate the distance of the first letter of the word from the letter 'a'
				initial_char_distance = word[0] - ASCII_a;

				// Lookup the list where it should be working on
				buffer_t* respective_buffer =
						&buffers[word_thread_lookup[initial_char_distance]];

				// Add this node to the respective buffer
				pthread_mutex_lock(&respective_buffer->mutex); // start critical section

				while (respective_buffer->occupied == buffer_size)
					pthread_cond_wait(&respective_buffer->full,
							&respective_buffer->mutex);

				// insert item
				respective_buffer->buf[respective_buffer->nextin].line =
						line_cnt;
				strcpy(respective_buffer->buf[respective_buffer->nextin].word,
						word);
				respective_buffer->nextin = (respective_buffer->nextin + 1)
						% buffer_size;
				respective_buffer->occupied++;

				// signal waiting consumers
				if (respective_buffer->occupied == 1)
					pthread_cond_signal(&respective_buffer->empty);

				pthread_mutex_unlock(&respective_buffer->mutex); // end of critical section

				// Read next word
				word = strtok(NULL, dlmt);
			}
		}

		// Done reading. Now adding stop flags
		int i;
		for (i = 0; i < nr_threads; i++) {

			pthread_mutex_lock(&(buffers[i].mutex));
			while (buffers[i].occupied == buffer_size)
				pthread_cond_wait(&(buffers[i].full), &(buffers[i].mutex));

			buffers[i].buf[buffers[i].nextin].line = -1;
			buffers[i].nextin = (buffers[i].nextin + 1) % buffer_size;
			buffers[i].occupied++;

			if (buffers[i].occupied == 1)
				pthread_cond_signal(&(buffers[i].empty));

			pthread_mutex_unlock(&(buffers[i].mutex));
		}
		free(cur_line);
	}
	fclose(infile);
}

void insert_line_nr(struct ll_lines_item_t** item, int number) {

	// Add to head
	if ((*item)->line_nr > number) {
		// create new node
		struct ll_lines_item_t* new_line_node = malloc(
				sizeof(struct ll_lines_item_t));
		new_line_node->line_nr = number;
		new_line_node->next = *item;

		// assign it to the previous node
		*item = new_line_node;
	} else {
		struct ll_lines_item_t* cur = *item;
		for (; cur != NULL; cur = cur->next) {
			if (cur->next == NULL || cur->next->line_nr > number) {
				// insert here (keeping it sorted)
				if (cur->line_nr != number) {
					// create new node
					struct ll_lines_item_t* new_line_node = malloc(
							sizeof(struct ll_lines_item_t));
					new_line_node->line_nr = number;
					new_line_node->next = cur->next;

					// assign it to the previous node
					cur->next = new_line_node;
					break;
				} else
					break;
			}
		}
	}
}

void output_file(char* filename) {
// Output the results in a file
// Open file
	FILE* f = fopen(filename, "w");
	if (!f) {
		printf("Cannot create output file!\n");
		exit(EXIT_FAILURE);
	}

	struct ll_word_item_t* w_iter;
	struct ll_lines_item_t* l_iter;
	int i;
	for (i = 0; i < nr_threads; i++) {
		for (w_iter = ((struct ll_word_item_t*) results[i]); w_iter != NULL;
				w_iter = w_iter->next) {
			fprintf(f, "%s ", w_iter->word);
			for (l_iter = w_iter->line_nrs; l_iter != NULL;
					l_iter = l_iter->next) {
				if (l_iter == w_iter->line_nrs)
					// First number
					fprintf(f, "%d", l_iter->line_nr);
				else
					fprintf(f, ", %d", l_iter->line_nr);
			}
			fprintf(f, "\n");
		}
	}

	fclose(f);
}

int word_exists(struct ll_word_item_t** root, const char* word,
		struct ll_word_item_t** word_position) {
	if (root == NULL || *root == NULL || word == NULL) {
		return FALSE;
	} else {
		// check and return it in word_position
		struct ll_word_item_t* cur;
		for (cur = *root; cur != NULL; cur = cur->next) {
			if (strcmp(cur->word, word) == 0) {
				*word_position = cur;
				return TRUE;
			}
		}
		return FALSE;
	}
}

int line_exists(struct ll_word_item_t* word_position, int line) {
	if (word_position == NULL || word_position->line_nrs == NULL || line < 1) {
		return FALSE;
	} else {
		struct ll_lines_item_t* cur;
		for (cur = word_position->line_nrs; cur != NULL; cur = cur->next)
			if (cur->line_nr == line)
				return TRUE;
		return FALSE;
	}
}
