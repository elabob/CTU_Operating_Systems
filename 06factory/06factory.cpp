//
// Created by bobenade on 11/11/2025.
//

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <vector>

using namespace std;

namespace Config {
    // Operation durations in microseconds
    const int OPERATION_DURATION[] = {100000, 200000, 150000, 300000, 400000, 250000, 500000};

    // Production sequences: [product][phase] = tool_id
    const int PRODUCTION_SEQUENCE[3][6] = {
        {0, 1, 2, 3, 1, 4},  // Product A
        {1, 0, 6, 1, 4, 5},  // Product B
        {6, 1, 5, 1, 6, 4}   // Product C
    };

    const int TOOL_TYPES = 7;
    const int PRODUCT_TYPES = 3;
    const int PRODUCTION_PHASES = 6;
}

namespace Mappings {
    const char* TOOL_NAMES[] = {"nuzky", "vrtacka", "ohybacka", "svarecka", "lakovna", "sroubovak", "freza"};

    int parse_tool_name(const char* name) {
        for(int i = 0; i < Config::TOOL_TYPES; i++) {
            if(strcmp(name, TOOL_NAMES[i]) == 0) return i;
        }
        return -1;
    }

    char product_to_letter(int product_id) {
        return (char)('A' + product_id);
    }

    int letter_to_product(char letter) {
        if(letter >= 'A' && letter <= 'C') return letter - 'A';
        return -1;
    }
}

struct WorkItem {
    int product_type;
    int phase;
};

struct Employee {
    pthread_t thread;
    char* name;
    int specialized_tool;
    bool termination_requested;
    bool actively_working;
};

struct FactoryState {
    // Tool availability
    int available_tools[Config::TOOL_TYPES];
    int tools_being_used[Config::TOOL_TYPES];

    // Work tracking: [product][phase]
    int pending_work[Config::PRODUCT_TYPES][Config::PRODUCTION_PHASES];
    int total_work[Config::PRODUCT_TYPES][Config::PRODUCTION_PHASES];

    // Employee management
    vector<Employee*> workforce;

    // Synchronization
    pthread_mutex_t state_lock;
    pthread_cond_t work_available;

    // Shutdown flag
    bool shutdown_initiated;
};

FactoryState factory;


// Check if a tool type has operators available
bool has_available_operators(int tool_type) {
    if(factory.available_tools[tool_type] == 0) return false;

    for(size_t i = 0; i < factory.workforce.size(); i++) {
        if(factory.workforce[i]->specialized_tool == tool_type &&
           !factory.workforce[i]->termination_requested) {
            return true;
        }
    }
    return false;
}

// Verify all prerequisite phases can be completed
bool prerequisites_achievable(int product, int target_phase) {
    for(int phase = 0; phase < target_phase; phase++) {
        int required_tool = Config::PRODUCTION_SEQUENCE[product][phase];
        if(!has_available_operators(required_tool)) {
            return false;
        }
    }
    return true;
}

// Determine if employee will have future work opportunities
bool has_future_opportunities(Employee* emp) {
    int tool = emp->specialized_tool;

    // Scan all possible work
    for(int p = 0; p < Config::PRODUCT_TYPES; p++) {
        for(int ph = 0; ph < Config::PRODUCTION_PHASES; ph++) {
            // Skip if no work exists
            if(factory.total_work[p][ph] == 0 && factory.pending_work[p][ph] == 0)
                continue;

            // Check if this phase needs employee's tool
            if(Config::PRODUCTION_SEQUENCE[p][ph] != tool) continue;

            // Verify prerequisites can be met
            if(prerequisites_achievable(p, ph)) {
                return true;
            }
        }
    }
    return false;
}

// Attempt to assign work to employee (returns true if work found)
bool assign_work_to_employee(Employee* emp, WorkItem* assignment) {
    if(emp->termination_requested) return false;

    int tool = emp->specialized_tool;

    // Check tool availability
    if(factory.tools_being_used[tool] >= factory.available_tools[tool]) {
        return false;
    }

    // Priority: later phases first (finish products sooner)
    for(int ph = Config::PRODUCTION_PHASES - 1; ph >= 0; ph--) {
        for(int p = 0; p < Config::PRODUCT_TYPES; p++) {
            // Must match employee's specialization
            if(Config::PRODUCTION_SEQUENCE[p][ph] != tool) continue;

            // Must have pending work
            if(factory.pending_work[p][ph] <= 0) continue;

            // Prerequisites must be achievable
            if(!prerequisites_achievable(p, ph)) continue;

            // Work found!
            assignment->product_type = p;
            assignment->phase = ph;
            return true;
        }
    }

    return false;
}

// Check if any work remains doable
bool work_remains_achievable() {
    // First check: does any work exist?
    bool work_exists = false;
    for(int p = 0; p < Config::PRODUCT_TYPES; p++) {
        for(int ph = 0; ph < Config::PRODUCTION_PHASES; ph++) {
            if(factory.total_work[p][ph] > 0 || factory.pending_work[p][ph] > 0) {
                work_exists = true;
                goto check_achievability;
            }
        }
    }
    if(!work_exists) return false;

    check_achievability:
    // Second check: can any of it be done?
    for(int p = 0; p < Config::PRODUCT_TYPES; p++) {
        for(int ph = 0; ph < Config::PRODUCTION_PHASES; ph++) {
            if(factory.total_work[p][ph] == 0 && factory.pending_work[p][ph] == 0)
                continue;

            int needed_tool = Config::PRODUCTION_SEQUENCE[p][ph];

            // Look for available operator
            for(size_t i = 0; i < factory.workforce.size(); i++) {
                if(factory.workforce[i]->specialized_tool == needed_tool &&
                   !factory.workforce[i]->termination_requested &&
                   factory.available_tools[needed_tool] > 0) {

                    if(prerequisites_achievable(p, ph)) {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

void* employee_work_loop(void* arg) {
    Employee* self = (Employee*)arg;

    while(true) {
        pthread_mutex_lock(&factory.state_lock);

        // Check termination request
        if(self->termination_requested) {
            printf("%s goes home\n", self->name);
            pthread_mutex_unlock(&factory.state_lock);
            pthread_exit(NULL);
        }

        // During shutdown, check if still needed
        if(factory.shutdown_initiated && !has_future_opportunities(self)) {
            printf("%s goes home\n", self->name);
            pthread_mutex_unlock(&factory.state_lock);
            pthread_exit(NULL);
        }

        // Try to get work assignment
        WorkItem task;
        if(!assign_work_to_employee(self, &task)) {
            // No work available, wait for notification
            pthread_cond_wait(&factory.work_available, &factory.state_lock);
            pthread_mutex_unlock(&factory.state_lock);
            continue;
        }

        // Accept the assignment
        factory.pending_work[task.product_type][task.phase]--;
        factory.tools_being_used[self->specialized_tool]++;
        self->actively_working = true;

        // Report activity
        printf("%s %s %d %c\n",
               self->name,
               Mappings::TOOL_NAMES[self->specialized_tool],
               task.phase + 1,
               Mappings::product_to_letter(task.product_type));

        pthread_mutex_unlock(&factory.state_lock);

        // Perform work (simulated delay)
        usleep(Config::OPERATION_DURATION[self->specialized_tool]);

        pthread_mutex_lock(&factory.state_lock);

        // Complete work
        self->actively_working = false;
        factory.tools_being_used[self->specialized_tool]--;
        factory.total_work[task.product_type][task.phase]--;

        // Handle completion or progression
        if(task.phase < Config::PRODUCTION_PHASES - 1) {
            // Move to next phase
            factory.pending_work[task.product_type][task.phase + 1]++;
        } else {
            // Product completed!
            printf("done %c\n", Mappings::product_to_letter(task.product_type));
        }

        // Notify other workers
        pthread_cond_broadcast(&factory.work_available);
        pthread_mutex_unlock(&factory.state_lock);
    }

    return NULL;
}

void process_make_command(char* product_code) {
    int product = Mappings::letter_to_product(product_code[0]);
    if(product < 0) return;

    pthread_mutex_lock(&factory.state_lock);

    // Initialize work for all phases
    for(int ph = 0; ph < Config::PRODUCTION_PHASES; ph++) {
        factory.total_work[product][ph]++;
    }
    factory.pending_work[product][0]++;

    pthread_cond_broadcast(&factory.work_available);
    pthread_mutex_unlock(&factory.state_lock);
}

void process_start_command(char* employee_name, char* tool_name) {
    int tool_id = Mappings::parse_tool_name(tool_name);
    if(tool_id < 0) return;

    pthread_mutex_lock(&factory.state_lock);

    Employee* new_emp = (Employee*)malloc(sizeof(Employee));
    new_emp->name = strdup(employee_name);
    new_emp->specialized_tool = tool_id;
    new_emp->termination_requested = false;
    new_emp->actively_working = false;

    factory.workforce.push_back(new_emp);
    pthread_create(&new_emp->thread, NULL, employee_work_loop, new_emp);

    pthread_cond_broadcast(&factory.work_available);
    pthread_mutex_unlock(&factory.state_lock);
}

void process_end_command(char* employee_name) {
    pthread_mutex_lock(&factory.state_lock);

    for(size_t i = 0; i < factory.workforce.size(); i++) {
        if(strcmp(factory.workforce[i]->name, employee_name) == 0) {
            factory.workforce[i]->termination_requested = true;
            pthread_cond_broadcast(&factory.work_available);
            break;
        }
    }

    pthread_mutex_unlock(&factory.state_lock);
}

void process_add_command(char* tool_name) {
    int tool_id = Mappings::parse_tool_name(tool_name);
    if(tool_id < 0) return;

    pthread_mutex_lock(&factory.state_lock);
    factory.available_tools[tool_id]++;
    pthread_cond_broadcast(&factory.work_available);
    pthread_mutex_unlock(&factory.state_lock);
}

void process_remove_command(char* tool_name) {
    int tool_id = Mappings::parse_tool_name(tool_name);
    if(tool_id < 0) return;

    pthread_mutex_lock(&factory.state_lock);
    if(factory.available_tools[tool_id] > 0) {
        factory.available_tools[tool_id]--;
        pthread_cond_broadcast(&factory.work_available);
    }
    pthread_mutex_unlock(&factory.state_lock);
}

int main() {
    // Initialize factory state
    pthread_mutex_init(&factory.state_lock, NULL);
    pthread_cond_init(&factory.work_available, NULL);

    for(int i = 0; i < Config::TOOL_TYPES; i++) {
        factory.available_tools[i] = 0;
        factory.tools_being_used[i] = 0;
    }

    for(int p = 0; p < Config::PRODUCT_TYPES; p++) {
        for(int ph = 0; ph < Config::PRODUCTION_PHASES; ph++) {
            factory.pending_work[p][ph] = 0;
            factory.total_work[p][ph] = 0;
        }
    }

    factory.shutdown_initiated = false;

    // Command processing loop
    char* command_line = NULL;
    size_t buffer_size = 0;

    while(true) {
        ssize_t read_len = getline(&command_line, &buffer_size, stdin);

        if(read_len == -1) {
            // EOF received - initiate shutdown
            break;
        }

        char* saveptr;
        char* cmd = strtok_r(command_line, " \r\n", &saveptr);
        char* arg1 = strtok_r(NULL, " \r\n", &saveptr);
        char* arg2 = strtok_r(NULL, " \r\n", &saveptr);
        char* arg3 = strtok_r(NULL, " \r\n", &saveptr);

        if(!cmd) continue;

        // Dispatch commands
        if(strcmp(cmd, "make") == 0 && arg1 && !arg2) {
            process_make_command(arg1);
        }
        else if(strcmp(cmd, "start") == 0 && arg1 && arg2 && !arg3) {
            process_start_command(arg1, arg2);
        }
        else if(strcmp(cmd, "end") == 0 && arg1 && !arg2) {
            process_end_command(arg1);
        }
        else if(strcmp(cmd, "add") == 0 && arg1 && !arg2) {
            process_add_command(arg1);
        }
        else if(strcmp(cmd, "remove") == 0 && arg1 && !arg2) {
            process_remove_command(arg1);
        }
        else {
            fprintf(stderr, "Invalid command: %s\n", command_line);
        }
    }

    free(command_line);

    // Shutdown sequence
    pthread_mutex_lock(&factory.state_lock);
    factory.shutdown_initiated = true;
    pthread_cond_broadcast(&factory.work_available);

    // Wait for all achievable work to complete
    while(work_remains_achievable()) {
        pthread_cond_wait(&factory.work_available, &factory.state_lock);
    }

    // Request all employees to terminate
    for(size_t i = 0; i < factory.workforce.size(); i++) {
        factory.workforce[i]->termination_requested = true;
    }
    pthread_cond_broadcast(&factory.work_available);
    pthread_mutex_unlock(&factory.state_lock);

    // Wait for all threads to finish
    for(size_t i = 0; i < factory.workforce.size(); i++) {
        pthread_join(factory.workforce[i]->thread, NULL);
    }

    // Cleanup resources
    for(size_t i = 0; i < factory.workforce.size(); i++) {
        free(factory.workforce[i]->name);
        free(factory.workforce[i]);
    }

    pthread_mutex_destroy(&factory.state_lock);
    pthread_cond_destroy(&factory.work_available);

    return 0;
}