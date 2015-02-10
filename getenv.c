#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define MAXSTRINGSZ 4096

pthread_key_t key;
pthread_once_t init_once = PTHREAD_ONCE_INIT;
pthread_mutex_t env_mutex = PTHREAD_MUTEX_INITIALIZER;
extern char **environ;

void destructor(void *ptr)
{
	free(ptr);
}

void init_fun(void)
{
	pthread_key_create(&key, destructor);
}

char *getenv2(const char *name)
{
	size_t len;
	int i;
	char *envbuf;

	/* Thread-once function. it should be called only once in a process
	 * for a specified key */
	pthread_once(&key, init_fun);
	pthread_mutex_lock(&env_mutex);	
	envbuf = pthread_getspecific(key);
	if (envbuf == NULL) {
		/* malloc() is __never__ a reentrant function, so just lock it */
		envbuf = malloc(MAXSTRINGSZ);
		if (envbuf == NULL) {
			pthread_mutex_unlock(&env_mutex);
			return NULL;
		}
		pthread_setspecific(key, envbuf);
	}
	len = strlen(name);
	for (i = 0; environ[i]; i++) {
		if (strncmp(environ[i], name, len) == 0 &&
			environ[i][len] == '=') {
			strncpy(envbuf, environ[i] + len + 1, MAXSTRINGSZ - 1);
			pthread_mutex_unlock(&env_mutex);
			return envbuf;
		}
	}
	pthread_mutex_unlock(&env_mutex);
	return NULL;
}

int main(void)
{
	char *enval;

	enval = getenv2("PWD");
	if (enval)
		fputs(enval, stdout);
	else
		printf("NULL\n");
	return 0;
}
