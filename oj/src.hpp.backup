#pragma once
#include "interface.h"
#include "definition.h"
// You should not use those functions in runtime.h

namespace oj {

auto generate_tasks(const Description &desc) -> std::vector <Task> {
    /// TODO: Add your implementation here :)
    return {};
}

} // namespace oj

namespace oj {

auto schedule_tasks(time_t time, std::vector <Task> list, const Description &desc) -> std::vector<Policy> {
    static task_id_t task_id = 0;
    const task_id_t first_id = task_id;
    const task_id_t last_id = task_id + list.size();
    task_id += list.size();

    /// Task id in [first_id, last_id) is the task id range of the current list.
    /// TODO: Add your implementation here :)

    return {};
}

} // namespace oj
