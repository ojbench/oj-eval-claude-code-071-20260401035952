#ifndef DEFINITION_H
#define DEFINITION_H

#include <vector>
#include <variant>
#include <string>

// Task structure
struct Task {
    int id;
    int work;      // Work amount w_i
    int deadline;  // Deadline ddl_i
    int priority;  // Priority p_i
    int arrive_time; // Time when task arrives
};

// Description for task generation
struct Description {
    int server_count;           // Number of servers 'a'
    int task_count;             // Number of tasks 'm'
    int deadline_min;           // Min deadline
    int deadline_max;           // Max deadline
    int work_min;               // Min work per task
    int work_max;               // Max work per task
    int total_work_min;         // Min total work
    int total_work_max;         // Max total work
    int priority_min;           // Min priority per task
    int priority_max;           // Max priority per task
    int total_priority_min;     // Min total priority
    int total_priority_max;     // Max total priority
};

// Public constants
class PublicInformation {
public:
    static constexpr int kStartUp = 5;   // Startup time
    static constexpr int kSaving = 3;    // Save time
    static constexpr double kC = 0.5;    // Exponent c for work calculation
};

// Policy actions for scheduling
struct StartPolicy {
    int task_id;
    int server_count;  // k servers to allocate
};

struct SavePolicy {
    int task_id;
};

struct CancelPolicy {
    int task_id;
};

struct NothingPolicy {};

using Policy = std::variant<StartPolicy, SavePolicy, CancelPolicy, NothingPolicy>;

// Test case descriptions (placeholder - actual values from OJ)
extern std::vector<Description> testcase_array;

#endif // DEFINITION_H
