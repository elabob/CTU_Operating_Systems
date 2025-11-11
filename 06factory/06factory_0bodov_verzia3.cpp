//
// Created by bobenade on 11/11/2025.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <vector>
#include <string>
#include <queue>

enum place {
    NUZKY, VRTACKA, OHYBACKA, SVARECKA, LAKOVNA, SROUBOVAK, FREZA,
    _PLACE_COUNT
};

const char *place_str[_PLACE_COUNT] = {
    [NUZKY] = "nuzky",
    [VRTACKA] = "vrtacka",
    [OHYBACKA] = "ohybacka",
    [SVARECKA] = "svarecka",
    [LAKOVNA] = "lakovna",
    [SROUBOVAK] = "sroubovak",
    [FREZA] = "freza",
};

enum product {
    A, B, C,
    _PRODUCT_COUNT
};

const char *product_str[_PRODUCT_COUNT] = {
    [A] = "A",
    [B] = "B",
    [C] = "C",
};

int workflows[_PRODUCT_COUNT][6] = {
    {NUZKY, VRTACKA, OHYBACKA, SVARECKA, VRTACKA, LAKOVNA},
    {VRTACKA, NUZKY, FREZA, VRTACKA, LAKOVNA, SROUBOVAK},
    {FREZA, VRTACKA, SROUBOVAK, VRTACKA, FREZA, LAKOVNA}
};

int times[_PLACE_COUNT] = {100, 200, 150, 300, 400, 250, 500};

struct Part {
    product prod;
    int step;
};

struct Worker {
    std::string name;
    place profession;
    pthread_t thread;
    bool should_end;
};

pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

std::vector<Worker*> workers;
int places[_PLACE_COUNT] = {0};
std::vector<Part> parts;
bool eof_received = false;
std::vector<place> pending_removals;

int find_string_in_array(const char **array, int length, char *what) {
    for (int i = 0; i < length; i++)
        if (strcmp(array[i], what) == 0)
            return i;
    return -1;
}

void* worker_func(void* arg) {
    Worker *w = (Worker*)arg;

    while (true) {
        pthread_mutex_lock(&mtx);

        while (true) {
            if (w->should_end) {
                pthread_mutex_unlock(&mtx);
                printf("%s goes home\n", w->name.c_str());
                return NULL;
            }

            int best_idx = -1;
            int best_step = -1;
            product best_prod = A;

            for (size_t i = 0; i < parts.size(); i++) {
                if (workflows[parts[i].prod][parts[i].step] == w->profession) {
                    if (places[w->profession] > 0) {
                        if (best_idx == -1 || parts[i].step > best_step ||
                            (parts[i].step == best_step && parts[i].prod < best_prod)) {
                            best_idx = i;
                            best_step = parts[i].step;
                            best_prod = parts[i].prod;
                        }
                    }
                }
            }

            if (best_idx >= 0) {
                Part p = parts[best_idx];
                parts.erase(parts.begin() + best_idx);
                places[w->profession]--;

                pthread_mutex_unlock(&mtx);

                printf("%s %s %d %s\n", w->name.c_str(), place_str[w->profession],
                       p.step + 1, product_str[p.prod]);

                usleep(times[w->profession] * 1000);

                pthread_mutex_lock(&mtx);
                places[w->profession]++;

                if (p.step == 5) {
                    printf("done %s\n", product_str[p.prod]);
                } else {
                    p.step++;
                    parts.push_back(p);
                }

                while (pending_removals.size() > 0) {
                    place to_rem = pending_removals[0];
                    if (places[to_rem] > 0) {
                        places[to_rem]--;
                        pending_removals.erase(pending_removals.begin());
                    } else {
                        break;
                    }
                }

                pthread_cond_broadcast(&cond);
                pthread_mutex_unlock(&mtx);
                break;
            } else {
                bool part_exists_for_profession = false;
                for (size_t i = 0; i < parts.size(); i++) {
                    if (workflows[parts[i].prod][parts[i].step] == w->profession) {
                        part_exists_for_profession = true;
                        break;
                    }
                }

                if (eof_received && !part_exists_for_profession) {
                    pthread_mutex_unlock(&mtx);
                    printf("%s goes home\n", w->name.c_str());
                    return NULL;
                }
            }

            pthread_cond_wait(&cond, &mtx);
        }
    }

    return NULL;
}

int main() {
    char *line = NULL;
    size_t sz = 0;

    while (1) {
        char *cmd, *arg1, *arg2, *arg3, *saveptr;

        if (getline(&line, &sz, stdin) == -1) {
            pthread_mutex_lock(&mtx);
            eof_received = true;
            pthread_cond_broadcast(&cond);
            pthread_mutex_unlock(&mtx);
            break;
        }

        cmd = strtok_r(line, " \r\n", &saveptr);
        arg1 = strtok_r(NULL, " \r\n", &saveptr);
        arg2 = strtok_r(NULL, " \r\n", &saveptr);
        arg3 = strtok_r(NULL, " \r\n", &saveptr);

        if (!cmd) {
            continue;
        } else if (strcmp(cmd, "start") == 0 && arg1 && arg2 && !arg3) {
            int prof = find_string_in_array(place_str, _PLACE_COUNT, arg2);
            if (prof >= 0) {
                Worker *w = new Worker();
                w->name = std::string(arg1);
                w->profession = (place)prof;
                w->should_end = false;

                pthread_mutex_lock(&mtx);
                workers.push_back(w);
                pthread_create(&w->thread, NULL, worker_func, w);
                pthread_cond_broadcast(&cond);
                pthread_mutex_unlock(&mtx);
            }
        } else if (strcmp(cmd, "make") == 0 && arg1 && !arg2) {
            int prod = find_string_in_array(product_str, _PRODUCT_COUNT, arg1);
            if (prod >= 0) {
                pthread_mutex_lock(&mtx);
                Part p;
                p.prod = (product)prod;
                p.step = 0;
                parts.push_back(p);
                pthread_cond_broadcast(&cond);
                pthread_mutex_unlock(&mtx);
            }
        } else if (strcmp(cmd, "end") == 0 && arg1 && !arg2) {
            pthread_mutex_lock(&mtx);
            for (auto w : workers) {
                if (w->name == arg1) {
                    w->should_end = true;
                    break;
                }
            }
            pthread_cond_broadcast(&cond);
            pthread_mutex_unlock(&mtx);
        } else if (strcmp(cmd, "add") == 0 && arg1 && !arg2) {
            int prof = find_string_in_array(place_str, _PLACE_COUNT, arg1);
            if (prof >= 0) {
                pthread_mutex_lock(&mtx);
                places[prof]++;
                pthread_cond_broadcast(&cond);
                pthread_mutex_unlock(&mtx);
            }
        } else if (strcmp(cmd, "remove") == 0 && arg1 && !arg2) {
            int prof = find_string_in_array(place_str, _PLACE_COUNT, arg1);
            if (prof >= 0) {
                pthread_mutex_lock(&mtx);
                if (places[prof] > 0) {
                    places[prof]--;
                } else {
                    pending_removals.push_back((place)prof);
                }
                pthread_mutex_unlock(&mtx);
            }
        }
    }

    free(line);

    for (auto w : workers) {
        pthread_join(w->thread, NULL);
        delete w;
    }

    pthread_mutex_destroy(&mtx);
    pthread_cond_destroy(&cond);

    return 0;
}