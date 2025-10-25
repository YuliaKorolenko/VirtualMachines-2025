//
// Created by Yuliya Karalenka on 23.10.25.
//

#ifndef MACHINE_STATE_H
#define MACHINE_STATE_H

#include <iostream>
#include <vector>

class Locals {
public:
    std::vector<int> vars;

    Locals(int n) : vars(n, -1) {
    }

    Locals() = default;
};

struct MachineState {
    Locals locals;
    std::unordered_map<int, int> globals;
    char *ip;
    std::vector<int> stack;

    MachineState(char *_ip) : ip(_ip) {
    }

    int read_next_int() {
        ip += sizeof(int);
        return *(int *) ((ip) - sizeof(int));
    }
};

#endif //MACHINE_STATE_H
