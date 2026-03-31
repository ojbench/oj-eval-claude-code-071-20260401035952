#pragma once
#include "interface.h"
#include "definition.h"
#include <vector>
#include <algorithm>
#include <cmath>
#include <map>

namespace oj {

auto generate_tasks(const Description &desc) -> std::vector<Task> {
    std::vector<Task> tasks;
    tasks.reserve(desc.task_count);

    // Calculate the minimum deadline needed for a given execution time
    // deadline > launch_time + kStartUp + kSaving + execution_time / (kCPUCount^kAccel)
    const double max_cpu_power = std::pow(static_cast<double>(PublicInformation::kCPUCount), PublicInformation::kAccel);
    const time_t overhead = PublicInformation::kStartUp + PublicInformation::kSaving;

    for (task_id_t i = 0; i < desc.task_count; i++) {
        Task task;
        
        double ratio = static_cast<double>(i) / std::max(1UL, desc.task_count - 1);
        
        // Assign execution time
        task.execution_time = desc.execution_time_single.min + 
            static_cast<time_t>(ratio * (desc.execution_time_single.max - desc.execution_time_single.min));
        
        // Calculate minimum time needed to complete this task with all CPUs
        time_t min_time_needed = overhead + static_cast<time_t>(std::ceil(task.execution_time / max_cpu_power));
        
        // Assign launch time (spread tasks over time)
        task.launch_time = static_cast<time_t>(ratio * desc.deadline_time.max * 0.3);
        
        // Deadline must be at least launch_time + min_time_needed + some buffer
        time_t min_deadline = task.launch_time + min_time_needed + 1;
        time_t max_deadline = desc.deadline_time.max;
        
        // Ensure min_deadline is within range
        if (min_deadline < desc.deadline_time.min) {
            min_deadline = desc.deadline_time.min;
        }
        if (min_deadline > max_deadline) {
            // Task cannot be completed, adjust execution time
            time_t max_allowed_exec = static_cast<time_t>((max_deadline - task.launch_time - overhead - 1) * max_cpu_power);
            task.execution_time = std::max(desc.execution_time_single.min, 
                std::min(task.execution_time, max_allowed_exec));
            min_deadline = task.launch_time + overhead + static_cast<time_t>(std::ceil(task.execution_time / max_cpu_power)) + 1;
        }
        
        // Set deadline
        task.deadline = min_deadline + static_cast<time_t>(ratio * (max_deadline - min_deadline));
        task.deadline = std::max(desc.deadline_time.min, std::min(desc.deadline_time.max, task.deadline));
        
        // Assign priority
        task.priority = desc.priority_single.min + 
            static_cast<priority_t>(ratio * (desc.priority_single.max - desc.priority_single.min));
        
        tasks.push_back(task);
    }

    // Adjust totals
    time_t total_exec = 0;
    priority_t total_pri = 0;
    for (const auto &t : tasks) {
        total_exec += t.execution_time;
        total_pri += t.priority;
    }

    // Adjust execution time total
    if (total_exec < desc.execution_time_sum.min) {
        time_t deficit = desc.execution_time_sum.min - total_exec;
        for (auto &task : tasks) {
            if (deficit == 0) break;
            
            // Calculate max execution time this task can have and still be completable
            time_t max_feasible = static_cast<time_t>((task.deadline - task.launch_time - overhead - 1) * max_cpu_power);
            time_t add = std::min({deficit, desc.execution_time_single.max - task.execution_time, max_feasible - task.execution_time});
            if (add > 0) {
                task.execution_time += add;
                deficit -= add;
            }
        }
    } else if (total_exec > desc.execution_time_sum.max) {
        time_t excess = total_exec - desc.execution_time_sum.max;
        for (auto &task : tasks) {
            if (excess == 0) break;
            time_t reduce = std::min(excess, task.execution_time - desc.execution_time_single.min);
            task.execution_time -= reduce;
            excess -= reduce;
        }
    }

    // Adjust priority
    total_pri = 0;
    for (const auto &t : tasks) {
        total_pri += t.priority;
    }

    if (total_pri < desc.priority_sum.min) {
        priority_t deficit = desc.priority_sum.min - total_pri;
        for (auto &task : tasks) {
            if (deficit == 0) break;
            priority_t add = std::min(deficit, desc.priority_single.max - task.priority);
            task.priority += add;
            deficit -= add;
        }
    } else if (total_pri > desc.priority_sum.max) {
        priority_t excess = total_pri - desc.priority_sum.max;
        for (auto &task : tasks) {
            if (excess == 0) break;
            priority_t reduce = std::min(excess, task.priority - desc.priority_single.min);
            task.priority -= reduce;
            excess -= reduce;
        }
    }

    return tasks;
}

} // namespace oj

namespace oj {

struct TaskState {
    time_t execution_time;
    time_t deadline;
    priority_t priority;
    double work_remaining;
    int phase;
    time_t phase_start_time;
    cpu_id_t allocated_cpus;
    bool completed;
};

static std::map<task_id_t, TaskState> task_states;
static time_t last_time = 0;
static bool initialized = false;
static cpu_id_t total_cpus = 0;

auto schedule_tasks(time_t time, std::vector<Task> list, const Description &desc) -> std::vector<Policy> {
    std::vector<Policy> policies;

    if (!initialized) {
        initialized = true;
        task_states.clear();
        last_time = 0;
        total_cpus = desc.cpu_count;
    }

    static task_id_t task_id = 0;
    const task_id_t first_id = task_id;
    const task_id_t last_id = task_id + list.size();

    for (size_t i = 0; i < list.size(); i++) {
        TaskState state;
        state.execution_time = list[i].execution_time;
        state.deadline = list[i].deadline;
        state.priority = list[i].priority;
        state.work_remaining = list[i].execution_time;
        state.phase = 0;
        state.phase_start_time = 0;
        state.allocated_cpus = 0;
        state.completed = false;
        task_states[first_id + i] = state;
    }

    task_id = last_id;

    // Update running tasks
    for (auto &[id, state] : task_states) {
        if (state.phase == 0 || state.completed) continue;

        time_t elapsed = time - state.phase_start_time;

        if (state.phase == 1) {
            if (elapsed >= PublicInformation::kStartUp) {
                state.phase = 2;
                state.phase_start_time = time;
            }
        } else if (state.phase == 2) {
            double work_done = oj::time_policy(elapsed + PublicInformation::kStartUp, state.allocated_cpus);
            state.work_remaining = state.execution_time - work_done;

            if (state.work_remaining <= 0 || time >= state.deadline - PublicInformation::kSaving - 2) {
                policies.push_back(Saving{id});
                state.phase = 3;
                state.phase_start_time = time;
            }
        } else if (state.phase == 3) {
            if (elapsed >= PublicInformation::kSaving) {
                state.completed = true;
            }
        }
    }

    cpu_id_t used_cpus = 0;
    for (const auto &[id, state] : task_states) {
        if (state.phase >= 1 && state.phase <= 3 && !state.completed) {
            used_cpus += state.allocated_cpus;
        }
    }
    cpu_id_t available_cpus = total_cpus - used_cpus;

    std::vector<std::pair<double, task_id_t>> task_priorities;
    for (const auto &[id, state] : task_states) {
        if (state.phase == 0 && !state.completed && time < state.deadline) {
            double urgency = static_cast<double>(state.priority) / std::max(1.0, static_cast<double>(state.deadline - time));
            double score = urgency * std::sqrt(state.work_remaining);
            task_priorities.push_back({score, id});
        }
    }

    std::sort(task_priorities.rbegin(), task_priorities.rend());

    for (const auto &[score, id] : task_priorities) {
        if (available_cpus == 0) break;

        auto &state = task_states[id];

        time_t time_available = state.deadline - time - PublicInformation::kStartUp - PublicInformation::kSaving;
        if (time_available <= 0) continue;

        double min_cpus = std::pow(state.work_remaining / time_available, 1.0 / PublicInformation::kAccel);
        cpu_id_t cpus_needed = std::max(1UL, static_cast<cpu_id_t>(std::ceil(min_cpus)));
        cpus_needed = std::min(cpus_needed, available_cpus);
        cpus_needed = std::min(cpus_needed, total_cpus);

        if (cpus_needed > 0) {
            policies.push_back(Launch{cpus_needed, id});
            state.phase = 1;
            state.phase_start_time = time;
            state.allocated_cpus = cpus_needed;
            available_cpus -= cpus_needed;
        }
    }

    last_time = time;
    return policies;
}

} // namespace oj
