#include <stdio.h>
#include <pthread.h>
#include "rwlock.h"

// Utility functions
static void write_cancel(void*);
static void read_cancel(void*);

void rw_init(struct rwlock *L) {
	L->readers_active = 0;
	L->readers_wait = 0;
	L->writers_wait = 0;
	L->writers_active = 0;
	pthread_mutex_init(&L->mutex, NULL);
    pthread_cond_init(&L->read, NULL);
	pthread_cond_init(&L->write, NULL);
	L->valid = VALID;
}

void rw_writer_lock(struct rwlock *L) {
	int response;

	if (L->valid != VALID)
		return;
	response = pthread_mutex_lock(&L->mutex);
	if (response != 0)
		return;
	if (L->writers_active || L->readers_active > 0) {
		L->writers_wait++;
		pthread_cleanup_push (write_cancel, (void*)L);
		while (L->writers_active || L->readers_active > 0) {
			response = pthread_cond_wait(&L->write, &L->mutex);
			if (response != 0)
				break;
		}
		pthread_cleanup_pop(0);
		L->writers_wait--;
	}
	if (response == 0)
		L->writers_active = 1;
	pthread_mutex_unlock(&L->mutex);
}

void rw_writer_unlock(struct rwlock *L) {
	int response;

	if (L->valid != VALID)
		return;
	response = pthread_mutex_lock(&L->mutex);
	if (response != 0)
		return;
	L->writers_active = 0;
	if (L->readers_wait > 0) {
		response = pthread_cond_broadcast(&L->read);
		if (response != 0) {
			pthread_mutex_unlock(&L->mutex);
			return;
		}
	} else if (L->writers_wait > 0) {
		response = pthread_cond_signal(&L->write);
		if (response != 0) {
			pthread_mutex_unlock(&L->mutex);
			return;
		}
	}
	pthread_mutex_unlock(&L->mutex);
}

void rw_reader_lock(struct rwlock *L) {

	int response;

	if (L->valid != VALID)
		return;
	response = pthread_mutex_lock(&L->mutex);
	if (response != 0)
		return;
	if (L->writers_active) {
		L->readers_wait++;
		pthread_cleanup_push (read_cancel, (void*)L);
		while (L->writers_active) {
			response = pthread_cond_wait(&L->read, &L->mutex);
			if (response != 0)
				break;
		}
		pthread_cleanup_pop(0);
		L->readers_wait--;
	}
	if (response == 0)
		L->readers_active++;
	pthread_mutex_unlock(&L->mutex);
}

void rw_reader_unlock(struct rwlock *L) {
	int response;

	if (L->valid != VALID)
		return;
	response = pthread_mutex_lock(&L->mutex);
	if (response != 0)
		return;
	L->readers_active--;
	if (L->readers_active == 0 && L->writers_wait > 0)
		response = pthread_cond_signal(&L->write);
	pthread_mutex_unlock(&L->mutex);
}

void rw_destroy(struct rwlock *L) {
	int response;

	if (L->valid != VALID)
		return;
	response = pthread_mutex_lock(&L->mutex);
	if (response != 0)
		return;

	// Cannot destroy if it is still used
	if (L->readers_active > 0 || L->writers_active) {
		pthread_mutex_unlock(&L->mutex);
		return;
	}

	// Cannot destroy if it is still being waited on
	if (L->readers_wait != 0 || L->writers_wait != 0) {
		pthread_mutex_unlock(&L->mutex);
		return;
	}

	L->valid = INVALID;
	response = pthread_mutex_unlock(&L->mutex);
	if (response != 0)
		return;
	pthread_mutex_destroy(&L->mutex);
	pthread_cond_destroy(&L->read);
	pthread_cond_destroy(&L->write);
}

static void write_cancel(void *param) {
	struct rwlock *l = (struct rwlock *) param;

	l->writers_wait--;
	pthread_mutex_unlock(&l->mutex);
}

static void read_cancel(void *param) {
	struct rwlock *l = (struct rwlock *) param;

	l->readers_wait--;
	pthread_mutex_unlock(&l->mutex);
}
