#ifndef INTERFACE_H
#define INTERFACE_H

#include "definition.h"
#include <vector>

// Client interface: Generate tasks based on description
std::vector<Task> generator(const Description& desc);

// Server interface: Schedule tasks at each time step
std::vector<Policy> schedule_tasks(int current_time, const std::vector<Task>& new_tasks);

#endif // INTERFACE_H
