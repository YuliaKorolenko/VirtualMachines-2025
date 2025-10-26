#include <iomanip>
#include <iostream>
#include <random>
#include <unistd.h>
#include <chrono>
#include <vector>
#include <algorithm>
#include <fstream>
#include <map>
#include <filesystem>
#include <cmath>
#include <ranges>


#ifdef __APPLE__
#include <mach/mach.h>

bool pin_to_core_minimal_macos(int core_id) {
    thread_affinity_policy_data_t policy = {core_id};
    return thread_policy_set(mach_thread_self(), THREAD_AFFINITY_POLICY, (thread_policy_t) &policy,
                             THREAD_AFFINITY_POLICY_COUNT) == KERN_SUCCESS;
}

#endif

using namespace std;

const int ITERATIONS = 500000;
const long long MAX_MEMORY = 1024 * 1024 * 1024;
const int TEST_COUNT = 5;

void **buffer;
constexpr double JUMP = 1.9;
std::ofstream outFile;
std::ofstream csvTable;


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

bool isMovement(const map<long long, std::vector<std::pair<long long, long double> > > &jumps_maps, long long H) {
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

    const long long min_s_c = current->second[0].first;
    for (const auto key: prev->second | views::keys) {
        if (key == min_s_c) {
            return false;
        }
    }

    return true;
}

int get_associativity() {
    map<long long, std::vector<std::pair<long long, long double> > > jumps_map;

    int MAX_ASSOCIATIVITY = 50;
    long long H = 1;

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
                jumps_map[H].emplace_back(S, jump);
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


    auto current_min_s = jumps_map.find(H)->second[0].first;
    H = H / 2;
    auto first_small_S_H = H;
    while (H >= 16) {
        auto map_iterator = jumps_map.find(H);

        if (map_iterator != jumps_map.end()) {
            const auto &s_jump_pairs = map_iterator->second;

            bool found_s = false;
            for (const auto &pair: s_jump_pairs) {
                if (pair.first == current_min_s) {
                    found_s = true;
                    break;
                }
            }

            if (found_s) {
                first_small_S_H = H;
            }
        }

        H /= 2;
    }

    cout << "Associativity: " << (current_min_s - 1) << endl;
    cout << "Cache size: " << (current_min_s - 1) * sizeof(void *) * first_small_S_H << "b" << endl;

    outFile << "Associativity: " << (current_min_s - 1) << endl;
    outFile << "Cache size: " << (current_min_s - 1) * sizeof(void *) * first_small_S_H << "b" << endl;
    return -1;
}

void create_table() {
    std::string filename = "timing_table.csv";
    std::ofstream csvTable(filename);
    if (!csvTable.is_open()) {
        std::cerr << "Error: Failed to create file '" << filename << "'." << std::endl;
        return;
    }
    csvTable << "H\\S"; // First cell
    for (int s = 1; s <= 30; ++s) {
        csvTable << "," << s;
    }
    csvTable << "\n";


    for (int H = 1; H * sizeof(void *) <= MAX_MEMORY; H *= 2) {
        csvTable << H;
        for (int S = 1; S < 30; ++S) {
            if (H * sizeof(void *) * S < MAX_MEMORY) {
                long double current_time = measure_access_time(H, S);
                csvTable << "," << static_cast<long long>(std::round(current_time * 100));
            } else {
                csvTable << "," << -1;
            }
        }
        csvTable << "\n";
    }
    csvTable.close();
    std::cout << "File '" << filename << "' created successfully." << std::endl;
}


int find_spots(int H) {
    long double prev_time = 0;
    long double cur_time = 0;

    for (int S = 1; S < MAX_MEMORY; S = S * 2) {
        if (H * (sizeof(void *)) * S <= MAX_MEMORY) {
            cur_time = measure_access_time(H, S);
            long double jump = cur_time / prev_time;
            if (prev_time != 0 && jump > JUMP) {
                return S;
            }
        }
        prev_time = cur_time;
    }

    return -1;
}

enum class ResultType {
    PATTERN_S_DECREASE, // 'S'
    PATTERN_D_INCREASE, // 'D'
    PATTERN_F_ASSOCIATIVE, // 'F'
    PATTERN_Z_UNKNOWN // 'Z' (No one more than > 50%)
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
    int test_count_cur = 0;
    int L = H / 2;
    while (L > 1 && test_count_cur < TEST_COUNT) {
        test_count_cur++;
        int test = find_spots(H + L);
        outFile << "test_count_" << test_count_cur << ": " << test << endl;
        if (test == -1) {
            continue;
        }
        if (test < base) {
            decrease++;
        } else if (test > base) {
            increase++;
        } else if (test == base) {
            associative++;
        }
        L = L / 2;
    }

    outFile << " decrease: " << decrease << " increase: " << increase << " associative: " << associative <<
            endl;
    if (decrease > TEST_COUNT / 2) {
        return ResultType::PATTERN_S_DECREASE;
    }
    if (increase > TEST_COUNT / 2) {
        return ResultType::PATTERN_D_INCREASE;
    }
    if (associative > TEST_COUNT / 2) {
        return ResultType::PATTERN_F_ASSOCIATIVE;
    }
    return ResultType::PATTERN_Z_UNKNOWN;
}

bool analyze_nearest_res_type() {
    return true;
}

void analyze_trend(const vector<ResultType> &trend) {
    vector<int> indexes;
    for (size_t i = 0; i < trend.size() - 1; ++i) {
        if ((trend[i] == ResultType::PATTERN_S_DECREASE || trend[i] == ResultType::PATTERN_Z_UNKNOWN) &&
            (trend[i + 1] == ResultType::PATTERN_D_INCREASE || trend[i + 1] == ResultType::PATTERN_F_ASSOCIATIVE)) {
            indexes.push_back(i);
        }
    }

    if (indexes.size() > 1) {
        cout << "There are detected more than one entity. The first one should be cache line: ";
    }
    for (int i = 0; i < indexes.size(); ++i) {
        cout << (1 << indexes[i]) * sizeof(void *) << "b ";
    }
    cout << endl;
}

void detect_block_size() {
    int H = 1;

    vector<ResultType> trend(std::log2(MAX_MEMORY), ResultType::PATTERN_Z_UNKNOWN);

    while (H < MAX_MEMORY / 8) {
        ResultType result = confidence_result(H);
        int log_2_H = static_cast<int>(std::log2(H));
        outFile << "H: " << H << " position: " << log_2_H << " result: " << resultToChar(result) << endl;
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
#ifdef __APPLE__
    pin_to_core_minimal_macos(2);
#endif
    std::string filename = "hw_1_log.log";
    outFile.open(filename, std::ios::app);
    if (outFile.is_open()) {
        std::filesystem::path full_path = std::filesystem::absolute(filename);
        std::cout << "File for logs was created: " << full_path.string() << std::endl;
    }


    buffer = (void **) std::aligned_alloc(4096, MAX_MEMORY);
    create_table();
    outFile << " get_associativity " << std::endl;
    get_associativity();
    outFile << " detect_block_size " << std::endl;
    detect_block_size();
    free(buffer);
}
