#ifndef RUNTIME_H
#define RUNTIME_H

#include "definition.h"
#include <vector>
#include <map>
#include <iostream>
#include <cmath>

// Simple runtime manager for local testing
class RuntimeManager {
private:
    struct TaskState {
        int id;
        int work_remaining;
        int server_count;
        int phase; // 0: not started, 1: startup, 2: execution, 3: saving
        int phase_time; // Time spent in current phase
        bool completed;
    };

    std::map<int, TaskState> running_tasks;
    std::map<int, Task> all_tasks;
    int total_servers;
    int used_servers;

public:
    RuntimeManager(int servers) : total_servers(servers), used_servers(0) {}

    void add_task(const Task& task) {
        all_tasks[task.id] = task;
    }

    bool apply_policy(const Policy& policy, int current_time) {
        if (std::holds_alternative<StartPolicy>(policy)) {
            auto sp = std::get<StartPolicy>(policy);
            if (all_tasks.find(sp.task_id) == all_tasks.end()) return false;
            if (running_tasks.find(sp.task_id) != running_tasks.end()) return false;
            if (used_servers + sp.server_count > total_servers) return false;

            TaskState state;
            state.id = sp.task_id;
            state.work_remaining = all_tasks[sp.task_id].work;
            state.server_count = sp.server_count;
            state.phase = 1; // startup
            state.phase_time = 0;
            state.completed = false;

            running_tasks[sp.task_id] = state;
            used_servers += sp.server_count;
            return true;
        }
        else if (std::holds_alternative<SavePolicy>(policy)) {
            auto sp = std::get<SavePolicy>(policy);
            if (running_tasks.find(sp.task_id) == running_tasks.end()) return false;
            auto& state = running_tasks[sp.task_id];
            if (state.phase != 2) return false; // Can only save during execution

            state.phase = 3; // saving
            state.phase_time = 0;
            return true;
        }
        else if (std::holds_alternative<CancelPolicy>(policy)) {
            auto cp = std::get<CancelPolicy>(policy);
            if (running_tasks.find(cp.task_id) == running_tasks.end()) return false;

            auto& state = running_tasks[cp.task_id];
            used_servers -= state.server_count;
            running_tasks.erase(cp.task_id);
            return true;
        }
        return true; // NothingPolicy
    }

    void step() {
        std::vector<int> to_remove;

        for (auto& [id, state] : running_tasks) {
            state.phase_time++;

            if (state.phase == 1) { // startup
                if (state.phase_time >= PublicInformation::kStartUp) {
                    state.phase = 2;
                    state.phase_time = 0;
                }
            }
            else if (state.phase == 2) { // execution
                double work_done = std::pow(state.server_count, PublicInformation::kC);
                state.work_remaining -= work_done;
            }
            else if (state.phase == 3) { // saving
                if (state.phase_time >= PublicInformation::kSaving) {
                    if (state.work_remaining <= 0) {
                        state.completed = true;
                        to_remove.push_back(id);
                    } else {
                        // Task saved but not completed, can restart
                        state.phase = 0;
                        to_remove.push_back(id);
                    }
                }
            }
        }

        for (int id : to_remove) {
            used_servers -= running_tasks[id].server_count;
            running_tasks.erase(id);
        }
    }

    double calculate_slo(int current_time) {
        double satisfied_priority = 0;
        double total_priority = 0;

        for (const auto& [id, task] : all_tasks) {
            total_priority += task.priority;
            if (running_tasks.find(id) != running_tasks.end()) {
                if (running_tasks[id].completed && current_time <= task.deadline) {
                    satisfied_priority += task.priority;
                }
            }
        }

        return total_priority > 0 ? satisfied_priority / total_priority : 0;
    }
};

#endif // RUNTIME_H
