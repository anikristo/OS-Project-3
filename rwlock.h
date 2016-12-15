#define VALID 1
#define INVALID 0

struct rwlock {
	pthread_mutex_t mutex;
	pthread_cond_t read; // wait for read
	pthread_cond_t write; // wait for write
	int valid;
	int readers_active;
	int writers_active;
	int readers_wait;
	int writers_wait;
};

void rw_init(struct rwlock *L);
void rw_writer_lock(struct rwlock *L);
void rw_writer_unlock(struct rwlock *L);
void rw_reader_lock(struct rwlock *L);
void rw_reader_unlock(struct rwlock *L);
void rw_destroy(struct rwlock *L);
