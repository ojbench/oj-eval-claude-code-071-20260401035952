#ifndef SRC_HPP
#define SRC_HPP

#include <vector>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <queue>

// Constants from PublicInformation
constexpr int kStartUp = 5;
constexpr int kSaving = 3;
constexpr double kC = 0.5;

// Task structure
struct Task {
    int id;
    int work;
    int deadline;
    int priority;
    int arrive_time;
};

// Description structure
struct Description {
    int server_count;
    int task_count;
    int deadline_min;
    int deadline_max;
    int work_min;
    int work_max;
    int total_work_min;
    int total_work_max;
    int priority_min;
    int priority_max;
    int total_priority_min;
    int total_priority_max;
};

// Policy structures
struct StartPolicy {
    int task_id;
    int server_count;
};

struct SavePolicy {
    int task_id;
};

struct CancelPolicy {
    int task_id;
};

struct NothingPolicy {};

#include <variant>
using Policy = std::variant<StartPolicy, SavePolicy, CancelPolicy, NothingPolicy>;

// Generator: Create challenging tasks for others
std::vector<Task> generator(const Description& desc) {
    std::vector<Task> tasks;
    tasks.reserve(desc.task_count);

    int total_work = 0;
    int total_priority = 0;

    // Strategy: Create tasks with varying difficulties
    // Mix of high priority short deadline tasks and lower priority flexible tasks

    for (int i = 0; i < desc.task_count; i++) {
        Task task;
        task.id = i;

        // Distribute work amounts
        if (i < desc.task_count / 3) {
            // First third: high work, far deadline
            task.work = desc.work_max - (desc.work_max - desc.work_min) / 4;
            task.deadline = desc.deadline_max - (desc.deadline_max - desc.deadline_min) / 5;
            task.priority = desc.priority_min + (desc.priority_max - desc.priority_min) / 3;
        } else if (i < 2 * desc.task_count / 3) {
            // Middle third: medium work, medium deadline
            task.work = (desc.work_min + desc.work_max) / 2;
            task.deadline = (desc.deadline_min + desc.deadline_max) / 2;
            task.priority = (desc.priority_min + desc.priority_max) / 2;
        } else {
            // Last third: varied work, tight deadlines
            task.work = desc.work_min + (desc.work_max - desc.work_min) / 3;
            task.deadline = desc.deadline_min + (desc.deadline_max - desc.deadline_min) / 3;
            task.priority = desc.priority_max - (desc.priority_max - desc.priority_min) / 4;
        }

        // Ensure constraints
        task.work = std::max(desc.work_min, std::min(desc.work_max, task.work));
        task.deadline = std::max(desc.deadline_min, std::min(desc.deadline_max, task.deadline));
        task.priority = std::max(desc.priority_min, std::min(desc.priority_max, task.priority));

        task.arrive_time = 0; // All arrive at start for simplicity

        total_work += task.work;
        total_priority += task.priority;

        tasks.push_back(task);
    }

    // Adjust to meet total constraints
    if (total_work < desc.total_work_min) {
        int deficit = desc.total_work_min - total_work;
        for (int i = 0; i < desc.task_count && deficit > 0; i++) {
            int add = std::min(deficit, desc.work_max - tasks[i].work);
            tasks[i].work += add;
            deficit -= add;
        }
    } else if (total_work > desc.total_work_max) {
        int excess = total_work - desc.total_work_max;
        for (int i = 0; i < desc.task_count && excess > 0; i++) {
            int reduce = std::min(excess, tasks[i].work - desc.work_min);
            tasks[i].work -= reduce;
            excess -= reduce;
        }
    }

    if (total_priority < desc.total_priority_min) {
        int deficit = desc.total_priority_min - total_priority;
        for (int i = 0; i < desc.task_count && deficit > 0; i++) {
            int add = std::min(deficit, desc.priority_max - tasks[i].priority);
            tasks[i].priority += add;
            deficit -= add;
        }
    } else if (total_priority > desc.total_priority_max) {
        int excess = total_priority - desc.total_priority_max;
        for (int i = 0; i < desc.task_count && excess > 0; i++) {
            int reduce = std::min(excess, tasks[i].priority - desc.priority_min);
            tasks[i].priority -= reduce;
            excess -= reduce;
        }
    }

    return tasks;
}

// Scheduler state
static std::map<int, Task> all_tasks_map;
static std::map<int, int> task_work_remaining;
static std::map<int, int> task_phase; // 0: not started, 1: startup, 2: execution, 3: saving
static std::map<int, int> phase_time;
static std::map<int, int> task_servers;
static int total_servers_global = 0;
static int current_time_global = 0;
static std::set<int> completed_tasks;
static bool first_call = true;

// Scheduler: Efficiently schedule tasks
std::vector<Policy> schedule_tasks(int current_time, const std::vector<Task>& new_tasks) {
    std::vector<Policy> policies;

    // Initialize on first call
    if (first_call) {
        first_call = false;
        all_tasks_map.clear();
        task_work_remaining.clear();
        task_phase.clear();
        phase_time.clear();
        task_servers.clear();
        completed_tasks.clear();
        current_time_global = 0;

        // Assume we get total servers from somewhere (will be in first new_tasks context)
        total_servers_global = 100; // Default, should be adjusted
    }

    current_time_global = current_time;

    // Add new tasks
    for (const auto& task : new_tasks) {
        all_tasks_map[task.id] = task;
        task_work_remaining[task.id] = task.work;
        task_phase[task.id] = 0;
    }

    // Update running tasks
    std::vector<int> to_complete;
    for (auto& [id, phase] : task_phase) {
        if (phase == 0 || completed_tasks.count(id)) continue;

        phase_time[id]++;

        if (phase == 1) { // startup
            if (phase_time[id] >= kStartUp) {
                phase = 2;
                phase_time[id] = 0;
            }
        } else if (phase == 2) { // execution
            double work_done = std::pow((double)task_servers[id], kC);
            task_work_remaining[id] -= (int)work_done;

            // Decide when to save
            if (task_work_remaining[id] <= 0) {
                policies.push_back(SavePolicy{id});
                phase = 3;
                phase_time[id] = 0;
            }
        } else if (phase == 3) { // saving
            if (phase_time[id] >= kSaving) {
                to_complete.push_back(id);
                completed_tasks.insert(id);
            }
        }
    }

    // Calculate available servers
    int used_servers = 0;
    for (auto& [id, phase] : task_phase) {
        if (phase >= 1 && phase <= 3 && !completed_tasks.count(id)) {
            used_servers += task_servers[id];
        }
    }
    int available_servers = total_servers_global - used_servers;

    // Start new tasks based on priority and deadline
    std::vector<std::pair<double, int>> task_scores;
    for (const auto& [id, task] : all_tasks_map) {
        if (task_phase[id] == 0 && !completed_tasks.count(id)) {
            // Score based on priority/deadline urgency
            double urgency = (double)task.priority / std::max(1, task.deadline - current_time);
            task_scores.push_back({urgency, id});
        }
    }

    std::sort(task_scores.rbegin(), task_scores.rend());

    for (auto [score, id] : task_scores) {
        if (available_servers <= 0) break;

        // Allocate servers based on work and deadline
        int servers_needed = std::min(available_servers, std::max(1, (int)std::sqrt(task_work_remaining[id])));

        if (servers_needed > 0) {
            policies.push_back(StartPolicy{id, servers_needed});
            task_phase[id] = 1;
            phase_time[id] = 0;
            task_servers[id] = servers_needed;
            available_servers -= servers_needed;
        }
    }

    if (policies.empty()) {
        policies.push_back(NothingPolicy{});
    }

    return policies;
}

#endif // SRC_HPP
