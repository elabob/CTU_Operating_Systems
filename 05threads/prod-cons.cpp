//
// Created by bobenade on 25/10/2025.
//

#include <iostream>
#include <pthread.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>

using namespace std;

// strukturka pre polozky v zozname
struct Item {
    int x;
    char* word;
    Item* next;
};

// globalny zoznam a synchronizacne veci
Item* head = NULL;
Item* tail = NULL;
pthread_mutex_t mutex;
pthread_mutex_t stdout_mutex;
pthread_cond_t cond;
bool producer_done = false;
bool invalid_input = false;
int items_in_list = 0;

// struktura pre data threadu
struct ThreadData {
    int number;
};

// funkcia pre konzumenta
void* consumer_function(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    int my_number = data->number;

    while (true) {
        pthread_mutex_lock(&mutex);

        // cakam kym nieco nie je v zozname alebo producent skonci
        while (head == NULL && !producer_done) {
            pthread_cond_wait(&cond, &mutex);
        }

        // ak je zoznam prazdny a producent skoncil, koncim
        if (head == NULL && producer_done) {
            pthread_mutex_unlock(&mutex);
            break;
        }

        // vyberiem prvu polozku
        Item* item = head;
        head = head->next;
        if (head == NULL) {
            tail = NULL;
        }
        items_in_list--;

        pthread_mutex_unlock(&mutex);

        // zamknem stdout mutex aby sa vypisovanie nemiasalo
        pthread_mutex_lock(&stdout_mutex);

        // vypis na stdout
        cout << "Thread " << my_number << ":";
        for (int i = 0; i < item->x; i++) {
            cout << " " << item->word;
        }
        cout << "\n";
        cout.flush();

        pthread_mutex_unlock(&stdout_mutex);

        // uvolnim pamat
        free(item->word);
        delete item;
    }

    return NULL;
}

// funkcia pre producenta
void* producer_function(void* arg) {
    int ret, x;
    char* text;

    // citam prikazy zo stdin
    while ((ret = scanf("%d %ms", &x, &text)) == 2) {
        // kontrola ze x nie je negativne
        if (x < 0) {
            free(text);
            invalid_input = true;
            break;
        }

        // vytvorim novu polozku
        Item* new_item = new Item;
        new_item->x = x;
        new_item->word = text;
        new_item->next = NULL;

        // pridam do zoznamu
        pthread_mutex_lock(&mutex);
        if (tail == NULL) {
            head = new_item;
            tail = new_item;
        } else {
            tail->next = new_item;
            tail = new_item;
        }
        items_in_list++;
        pthread_cond_signal(&cond); // vzbudim jeden thread
        pthread_mutex_unlock(&mutex);
    }

    // ak scanf vratil nieco ine ako 2 alebo EOF, je to chyba
    if (ret != EOF && ret != -1 && !invalid_input) {
        invalid_input = true;
    }

    // oznacim ze som skoncil
    pthread_mutex_lock(&mutex);
    producer_done = true;
    pthread_cond_broadcast(&cond); // vzbudim vsetkych
    pthread_mutex_unlock(&mutex);

    return NULL;
}

int main(int argc, char* argv[]) {
    int N = 1;

    // spracujem argumenty
    if (argc > 1) {
        N = atoi(argv[1]);
    }

    // zistim pocet cpu
    int cpu_count = sysconf(_SC_NPROCESSORS_ONLN);

    // kontrola rozsahu N
    if (N < 1 || N > cpu_count) {
        return 1;
    }

    // inicializujem mutex a condition variable
    pthread_mutex_init(&mutex, NULL);
    pthread_mutex_init(&stdout_mutex, NULL);
    pthread_cond_init(&cond, NULL);

    // vytvorim producenta
    pthread_t producer_thread;
    pthread_create(&producer_thread, NULL, producer_function, NULL);

    // vytvorim konzumentov
    pthread_t* consumer_threads = new pthread_t[N];
    ThreadData* thread_data = new ThreadData[N];

    for (int i = 0; i < N; i++) {
        thread_data[i].number = i + 1;
        pthread_create(&consumer_threads[i], NULL, consumer_function, &thread_data[i]);
    }

    // cakam na producenta
    pthread_join(producer_thread, NULL);

    // cakam na konzumentov
    for (int i = 0; i < N; i++) {
        pthread_join(consumer_threads[i], NULL);
    }

    // upratujem
    pthread_mutex_destroy(&mutex);
    pthread_mutex_destroy(&stdout_mutex);
    pthread_cond_destroy(&cond);
    delete[] consumer_threads;
    delete[] thread_data;

    // ak bol neplatny vstup, vratim 1
    if (invalid_input) {
        return 1;
    }

    return 0;
}
