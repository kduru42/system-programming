#include "stdlib.h"
#include "stdio.h"
#include "pthread.h"
#include "unistd.h"
#include "string.h"
#include "semaphore.h"
#include "time.h"
#define ENGINEER_COUNT 3
#define SATELLITE_LIMIT 10
#define TIMEOUT 5 // setting timeout

typedef struct {
    int id;
    int priority;
} SatelliteRequest;

pthread_mutex_t engineerMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t countMutex = PTHREAD_MUTEX_INITIALIZER; // Mutex for handling request count
int availabeEngineers = ENGINEER_COUNT;
sem_t newRequest, requestHandled;
SatelliteRequest *requestQueue;
int handledRequests = 0; // Number of handled requests
int maxRequests = 10;
int queueSize = 0;

void add_request(int id, int priority) {
    pthread_mutex_lock(&engineerMutex); // lock to mutex to protect queue datas
    requestQueue[queueSize].id = id;
    requestQueue[queueSize].priority = priority;
    queueSize++;

    // Sort the queue based on priority (highest first)
    for (int i = 0; i < queueSize - 1; i++) {
        for (int j = 0; j < queueSize - i - 1; j++) {
            if (requestQueue[j].priority < requestQueue[j + 1].priority) {
                // Swap requests
                SatelliteRequest temp = requestQueue[j];
                requestQueue[j] = requestQueue[j + 1];
                requestQueue[j + 1] = temp;
            }
        }
    }
    pthread_mutex_unlock(&engineerMutex);
}

SatelliteRequest *get_request() {
    if (queueSize == 0) {
        pthread_mutex_unlock(&engineerMutex);
        return NULL;
    }
    SatelliteRequest *request = malloc(sizeof(SatelliteRequest));
    *request = requestQueue[0];
    for (int i = 1; i < queueSize; i++) {
        requestQueue[i - 1] = requestQueue[i];
    }
    queueSize--;
    return request;
}

void remove_request(int id) {
    for (int i = 0; i < queueSize; i++) {
        if (requestQueue[i].id == id) {
            for (int j = i; j < queueSize - 1; j++) {
                requestQueue[j] = requestQueue[j + 1];
            }
            queueSize--;
            break;
        }
    }
}

void *engineer(void *arg) {
    int id = *(int *)arg;
    while (1) {
        // First check if all work is done
        pthread_mutex_lock(&countMutex);
        if (handledRequests >= maxRequests - 2) {
            pthread_mutex_unlock(&countMutex);
            printf("[ENGINEER %d] Exiting...\n", id);
            break;
        }
        pthread_mutex_unlock(&countMutex);

        // Wait for new request
        sem_wait(&newRequest);

        // Check again in case all work was completed while we were waiting
        pthread_mutex_lock(&countMutex);
        if (handledRequests >= maxRequests - 2) {
            pthread_mutex_unlock(&countMutex);
            printf("[ENGINEER %d] Exiting...\n", id);
            break;
        }
        pthread_mutex_unlock(&countMutex);
        pthread_mutex_lock(&engineerMutex);
        availabeEngineers--;
        SatelliteRequest *request = get_request();
        if (request == NULL) {
            availabeEngineers++;
            pthread_mutex_unlock(&engineerMutex);
            continue; // No request available, continue waiting
        }
        pthread_mutex_unlock(&engineerMutex);

        sem_post(&requestHandled); // Signal that a request has been handled
        printf("[ENGINEER %d] Handling Satellite %d (Priority %d)\n", id, request->id, request->priority);
        sleep(1); // Simulate work
        pthread_mutex_lock(&engineerMutex);
        availabeEngineers++;
        pthread_mutex_unlock(&engineerMutex);
        printf("[ENGINEER %d] Finished Satellite %d\n", id, request->id);
        free(request);
        pthread_mutex_lock(&countMutex);
        handledRequests++; // Increment handled requests count
        pthread_mutex_unlock(&countMutex);
        
    }
    return NULL;
}

void *satellite(void *arg) {
    int id = *(int *)arg;
    int priority = rand() % SATELLITE_LIMIT + 3; // Random priority between 3 and satellite limit (10 for now)
    printf("[SATELLITE] Satellite %d requesting (Priority %d)\n", id, priority);
    pthread_mutex_lock(&engineerMutex);
    int engineersAvailable = availabeEngineers; // to read safely, avoiding race condition
    pthread_mutex_unlock(&engineerMutex);
    add_request(id, priority);
    sem_post(&newRequest);
    if (engineersAvailable == 0) { // No engineers available, wait for one to finish
        struct timespec timeout;
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += TIMEOUT; // TIMEOUT defined as 5 seconds
        if (sem_timedwait(&requestHandled, &timeout) == -1) {
            printf("[TIMEOUT] Satellite %d timedout\n", id);
            pthread_mutex_lock(&engineerMutex);
            availabeEngineers++;
            remove_request(id);
            pthread_mutex_unlock(&engineerMutex);

            pthread_mutex_lock(&countMutex);
            handledRequests++;
            pthread_mutex_unlock(&countMutex);
        }
    }
    else
        sem_wait(&requestHandled);
    return NULL;
}



int main()
{
    pthread_t engineers[ENGINEER_COUNT];
    pthread_t *satellites;

    srand(time(NULL));
    int num_satellites = rand() % 10 + 3; // Random number of satellites between 1 and 10
    satellites = malloc(num_satellites * sizeof(pthread_t));
    requestQueue = malloc(num_satellites * sizeof(SatelliteRequest));
    maxRequests = num_satellites;

    if (satellites == NULL) {
        perror("Failed to allocate memory for satellites");
        return 1;
    }
    if (pthread_mutex_init(&engineerMutex, NULL) != 0) {
        perror("Failed to initialize mutex");
        return 1;
    }
    if (pthread_mutex_init(&countMutex, NULL) != 0) {
        perror("Failed to initialize mutex");
        return 1;
    }
    if (sem_init(&newRequest, 0, 0) != 0) {
        perror("Failed to initialize semaphore");
        return 1;
    }
    if (sem_init(&requestHandled, 0, 0) != 0) {
        perror("Failed to initialize semaphore");
        return 1;
    }
    for (int i = 0; i < ENGINEER_COUNT; i++) {
        if (pthread_create(&engineers[i], NULL, engineer, &i) != 0) {
            perror("Failed to create engineer thread");
            return 1;
        }
    }
    for (int i = 0; i < num_satellites; i++) {
        if (pthread_create(&satellites[i], NULL, satellite, &i) != 0) {
            perror("Failed to create satellite thread");
            return 1;
        }
    }
    for (int i = 0; i < num_satellites; i++) {
        pthread_join(satellites[i], NULL);
    }
    for (int i = 0; i < ENGINEER_COUNT; i++) {
        pthread_join(engineers[i], NULL);
    }
    if (pthread_mutex_destroy(&engineerMutex) != 0) {
        perror("Failed to destroy mutex");
        return 1;
    }
    if (pthread_mutex_destroy(&countMutex) != 0) {
        perror("Failed to destroy mutex");
        return 1;
    }
    if (sem_destroy(&newRequest) != 0) {
        perror("Failed to destroy semaphore");
        return 1;
    }
    if (sem_destroy(&requestHandled) != 0) {
        perror("Failed to destroy semaphore");
        return 1;
    }
    free(satellites);
    free(requestQueue);
    return 0;
}