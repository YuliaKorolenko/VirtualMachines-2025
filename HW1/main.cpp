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

const int ITERATIONS = 100000;
const long long MAX_MEMORY = 1024 * 1024 * 1024;
const int TEST_COUNT = 3;
const int WINDOW_SIZE = 3;
const int MAX_SPOTS = 1000;

void **buffer;
double JUMP = 1.8;
double JUMP_CURRENT = 1.19;
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

bool isMovement(const map<long long, std::vector<long long> > &jumps_maps, long long H) {
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

    const long long min_s_c = current->second[0];
    for (const long long element: prev->second) {
        if (element == min_s_c) {
            return false;
        }
    }

    return true;
}

long double get_median(const std::vector<long double> &v, int start_idx, int end_idx) {
    if (start_idx < 0 || end_idx > v.size() || start_idx >= end_idx) {
        return 0.0;
    }

    std::vector<long double> slice;
    for (int i = start_idx; i < end_idx; ++i) {
        slice.push_back(v[i]);
    }

    std::sort(slice.begin(), slice.end());

    int n = slice.size();
    if (n % 2 == 0) {
        return (slice[n / 2 - 1] + slice[n / 2]) / 2.0;
    }
    return slice[n / 2];
}

bool isJump(long long H, long double current_time, long long S, std::vector<long double> &measurements) {
    int i = S - 1;
    long double avg_before = get_median(measurements, i - WINDOW_SIZE, i);
    long double avg_after = get_median(measurements, i, i + WINDOW_SIZE);

    if (avg_before == 0.0) {
        return false;
    }

    long double jump_ratio = avg_after / avg_before;

    if (jump_ratio > JUMP && current_time / avg_before > JUMP_CURRENT) {
        outFile << "Stride H: " << H
                << ", Count elements S: " << S
                << ", Jump: " << jump_ratio
                << ", Current time: " << current_time
                << std::endl;
        return true;
    }
    return false;
}

int get_associativity() {
    map<long long, std::vector<long long> > jumps_map;

    int MAX_ASSOCIATIVITY = 50;
    long long H = 1;

    long double prev_time = 0;
    while (H * MAX_ASSOCIATIVITY * sizeof(void *) < MAX_MEMORY) {
        std::vector<long double> measurements;
        for (long long S = 1; S < MAX_ASSOCIATIVITY; ++S) {
            measurements.push_back(measure_access_time(H, S) * 100);
        }

        long long S = 1;
        while (S < MAX_ASSOCIATIVITY) {
            long double current_time = measure_access_time(H, S) * 100;
            if (prev_time != 0 && isJump(H, current_time, S, measurements)) {
                jumps_map[H].emplace_back(S);
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


    auto current_min_s = jumps_map.find(H)->second[0];
    H = H / 2;
    auto first_small_S_H = H;
    while (H >= 16) {
        auto map_iterator = jumps_map.find(H);

        if (map_iterator != jumps_map.end()) {
            const auto &s_jump_pairs = map_iterator->second;

            bool found_s = false;
            for (const auto &elem: s_jump_pairs) {
                if (elem == current_min_s) {
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

void calculate_jump(std::vector<long double> &timings) {
    outFile << " Timings array: ";
    for (int i = 0; i < timings.size(); ++i) {
        outFile << timings[i] << " ";
    }

    outFile << endl;
    double sum = 0;
    for (double t: timings) sum += t;
    double avg = sum / timings.size();
    outFile << "Average: " << avg << endl;

    double diff_sum = 0;
    for (double t: timings) diff_sum += (t - avg) * (t - avg);
    double stddev = sqrt(diff_sum / timings.size());
    outFile << "stddev: " << stddev << endl;

    outFile << "My jump: " << avg + 2 * stddev << endl;
    JUMP = avg + stddev;
}

void create_table() {
    std::vector<long double> timings;
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


    long double prev_time = -1;
    for (int H = 1; H * sizeof(void *) <= MAX_MEMORY; H *= 2) {
        csvTable << H;
        for (int S = 1; S < 30; ++S) {
            if (H * sizeof(void *) * S < MAX_MEMORY) {
                long double current_time = measure_access_time(H, S);
                if (S != 1) {
                    timings.push_back(current_time / prev_time);
                }
                csvTable << "," << static_cast<long long>(std::round(current_time * 100));
                prev_time = current_time;
            } else {
                csvTable << "," << -1;
            }
        }
        csvTable << "\n";
    }
    csvTable.close();
    std::cout << "File '" << filename << "' created successfully." << std::endl;

    calculate_jump(timings);
}

long double average_time_for_spots(int H) {
    long double all_time = 0;
    for (int S = 1; S < MAX_SPOTS; S = S + 1) {
        all_time += measure_access_time(H, S);
    }
    return all_time / MAX_SPOTS;
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
    long double avg_base = average_time_for_spots(H);
    outFile << endl << "H: " << H << " base: " << avg_base * 100 << endl;
    if (avg_base == -1) {
        return ResultType::PATTERN_Z_UNKNOWN;
    }

    int decrease = 0;
    int increase = 0;
    int associative = 0;
    int test_count_cur = 0;
    int L = H / 2;
    while (L > 1 && test_count_cur < TEST_COUNT) {
        test_count_cur++;
        long double test = average_time_for_spots(H + L);
        outFile << "test_count_" << test_count_cur << ": time: " << test * 100;
        if (test == -1) {
            continue;
        }
        long double diff = avg_base / test;
        outFile << " diff: " << diff << endl;
        if (0.9 < diff && diff < 1.1) {
            associative++;
        } else if (test < avg_base) {
            decrease++;
        } else if (test > avg_base) {
            increase++;
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
        if ((trend[i] == ResultType::PATTERN_S_DECREASE) &&
            (trend[i + 1] == ResultType::PATTERN_D_INCREASE || trend[i + 1] == ResultType::PATTERN_F_ASSOCIATIVE)) {
            indexes.push_back(i);
        }
    }

    if (indexes.size() > 1) {
        cout << "There are detected more than one entity. The first one should be cache line: ";
    }
    if (indexes.size() == 1) {
        cout << "Cache line: ";
    }
    for (int i = 0; i < indexes.size(); ++i) {
        cout << (1 << indexes[i]) << "B ";
    }
    cout << endl;
}

void detect_block_size() {
    int H = 16;

    vector<ResultType> trend(std::log2(MAX_MEMORY), ResultType::PATTERN_Z_UNKNOWN);

    int plato_count = 0;
    while (H < MAX_MEMORY / 8 && plato_count < 2) {
        ResultType result = confidence_result(H);
        if (result == ResultType::PATTERN_D_INCREASE) {
            plato_count++;
        }
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
