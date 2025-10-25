#include <iomanip>
#include <iostream>
#include <random>
#include <unistd.h>
#include <chrono>
#include <vector>
#include <algorithm>
#include <fstream>
#include <map>

using namespace std;

const int ITERATIONS = 300000;
const long long MAX_MEMORY = 1024 * 1024 * 1024;

void **buffer;
constexpr double JUMP = 1.9;
std::ofstream outFile;


long double measure_access_time(size_t H, size_t S) {
    const size_t total_elements = H * S;

    for (int i = 1; i < S; ++i) {
        buffer[i * H] = (void *) &buffer[i * H - H];
    }
    buffer[0] = (void *) &buffer[total_elements - H];

    const void *p = buffer;

    for (int j = 0; j < ITERATIONS; ++j) {
        p = *(const void **) p;
    }
    fprintf(stdin, "%p", p);

    p = buffer;
    auto start = std::chrono::high_resolution_clock::now();
    for (int j = 0; j < ITERATIONS; ++j) {
        p = *(const void **) p;
    }
    auto end = std::chrono::high_resolution_clock::now();
    fprintf(stdin, "%p", p);


    auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    return duration_ns.count() / static_cast<long double>(ITERATIONS);
}

bool isMovement(const map<long long, std::pair<long long, long double> > &jumps_maps, long long H) {
    if (jumps_maps.size() < 2) {
        return true;
    }
    auto current = jumps_maps.find(H);
    if (current == jumps_maps.end()) {
        return true;
    }

    H = H / 2;
    if (H < 16) {
        return true;
    }
    auto prev = jumps_maps.find(H);
    if (prev == jumps_maps.end()) {
        return true;
    }

    if (prev->second.first == current->second.first) {
        return false;
    }

    return true;
}

int get_associativity() {
    map<long long, std::pair<long long, long double> > jumps_map;

    int MAX_ASSOCIATIVITY = 50;
    long long H = 16;

    long double prev_time = 0;
    while (H * MAX_ASSOCIATIVITY * sizeof(void *) < MAX_MEMORY) {
        long long S = 1;
        while (S < MAX_ASSOCIATIVITY) {
            long double current_time = measure_access_time(H, S);
            long double jump = current_time / prev_time;
            if (prev_time != 0 && jump > JUMP) {
                outFile << "Stride H: " << H
                        << ", Count elements S: " << S
                        << ", Jump: " << jump
                        << ", Current time: " << current_time
                        << std::endl;
                jumps_map[H] = make_pair(S, jump);
            }
            S += 1;
            prev_time = current_time;
        }
        if (isMovement(jumps_map, H)) {
            H = H * 2;
            prev_time = 0;
        } else {
            break;
        }
    }
    if (jumps_map.size() < 2) {
        cout << " Problem: too small jumps ";
        return -1;
    }

    auto current = jumps_map.find(H);
    if (current == jumps_map.end()) {
        cout << "Mistake: last H doesn't have jump";
        return -1;
    }
    cout << "Associativity: " << current->second.first - 1 << endl;
    return -1;
}

void get_associativity_1() {
    int H = 128;
    for (int i = 3; i < 100; ++i) {
        long double current_time = measure_access_time(H, i);
        outFile << "Stride H: " << H
                << ", Count elements S: " << i
                << ", Current time: " << current_time * 100
                << std::endl;
    }
}


int find_spots(int H) {
    long double prev_time = 0;
    long double cur_time = 0;

    for (int i = 1; i < (MAX_MEMORY / 8) / H; i = i * 2) {
        cur_time = measure_access_time(H, i);
        long double jump = cur_time / prev_time;
        if (prev_time != 0 && jump > JUMP) {
            return i;
        }
        prev_time = cur_time;
    }

    return -1;
}

enum class ResultType {
    PATTERN_S_DECREASE, // 'S'
    PATTERN_D_INCREASE, // 'D'
    PATTERN_F_ASSOCIATIVE, // 'F'
    PATTERN_Z_UNKNOWN // 'Z' (Никто не набрал > 50%)
};

char resultToChar(ResultType result) {
    switch (result) {
        case ResultType::PATTERN_S_DECREASE: return 'D';
        case ResultType::PATTERN_D_INCREASE: return 'I';
        case ResultType::PATTERN_F_ASSOCIATIVE: return 'A';
        case ResultType::PATTERN_Z_UNKNOWN: return 'Z';
        default: return '?';
    }
}

ResultType confidence_result(int H) {
    int base = find_spots(H);
    outFile << endl << "H: " << H << " base: " << base << endl;
    if (base == -1) {
        return ResultType::PATTERN_Z_UNKNOWN;
    }

    int decrease = 0;
    int increase = 0;
    int associative = 0;
    int test_count = 0;
    int L = H / 2;
    while (L > 1 && test_count < 4) {
        test_count++;
        int test = find_spots(H + L);
        outFile << "test_count_" << test_count << ": " << test << endl;
        if (test == -1) {
            continue;
        }
        if (test < base) {
            decrease++;
        } else if (test > decrease) {
            increase++;
        } else {
            associative++;
        }
        L = L / 2;
    }

    outFile << " decrease: " << decrease << " increase: " << increase << " associative: " << associative <<
            endl;
    if (decrease > test_count / 2) {
        return ResultType::PATTERN_S_DECREASE;
    }
    if (increase > test_count / 2) {
        return ResultType::PATTERN_D_INCREASE;
    }
    if (associative > test_count / 2) {
        return ResultType::PATTERN_F_ASSOCIATIVE;
    }
    return ResultType::PATTERN_Z_UNKNOWN;
}

void analyze_trend(const vector<ResultType> &trend) {
    bool is_first = true;
    for (size_t i = 0; i < trend.size() - 1; ++i) {
        if ((trend[i] == ResultType::PATTERN_S_DECREASE || trend[i] == ResultType::PATTERN_D_INCREASE) &&
            trend[i + 1] == ResultType::PATTERN_D_INCREASE) {
            size_t index_of_D = i + 1;
            int block_size = 1 << (index_of_D + 1);

            if (is_first) {
                cout << "Cache Line size: " << block_size << "." << endl;
                is_first = false;
            } else {
                cout << "Cache size: " << block_size << "." << endl;
                return;
            }
        }
    }
}

int detect_block_size() {
    int H = 16;

    vector<ResultType> trend(log(MAX_MEMORY), ResultType::PATTERN_Z_UNKNOWN);

    while (H < MAX_MEMORY / 8) {
        ResultType result = confidence_result(H);
        auto log_2_H = log(H);
        trend[log_2_H] = result;
        H = H * 2;
    }

    outFile << " Trend: " << endl;
    for (int i = 0; i < trend.size(); ++i) {
        outFile << resultToChar(trend[i]) << " ";
    }

    analyze_trend(trend);
}

int main() {
    std::string filename = "hw_1_log.log";
    outFile.open(filename, std::ios::app);
    if (outFile.is_open()) {
        std::filesystem::path full_path = std::filesystem::absolute(filename);
        std::cout << "File for logs was created: " << full_path.string() << std::endl;
    }


    buffer = (void **) std::aligned_alloc(4096, MAX_MEMORY);
    outFile << " get_associativity_1 " << std::endl;
    get_associativity_1();
    outFile << " get_associativity " << std::endl;
    get_associativity();
    outFile << " detect_block_size " << std::endl;
    detect_block_size();
    free(buffer);
}
