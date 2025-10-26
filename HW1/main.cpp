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

using namespace std;

const int ITERATIONS = 500000;
const long long MAX_MEMORY = 1024 * 1024 * 1024;
const int TEST_COUNT = 4;

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

int get_associativity() {
    map<long long, std::pair<long long, long double> > jumps_map;

    int MAX_ASSOCIATIVITY = 50;
    long long H = 1;

    long double prev_time = 0;
    long double last_H_with_jump = 0;
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
                last_H_with_jump = H;
            }
            S += 1;
            prev_time = current_time;
        }
        H = H * 2;
    }

    auto current = jumps_map.find(last_H_with_jump);
    cout << "Associativity: " << current->second.first - 1 << endl;
    return -1;
}

void get_associativity_1() {
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

void analyze_trend(const vector<ResultType> &trend) {
    bool is_first = true;
    for (size_t i = 0; i < trend.size() - 1; ++i) {
        if ((trend[i] == ResultType::PATTERN_S_DECREASE || trend[i] == ResultType::PATTERN_Z_UNKNOWN) &&
            trend[i + 1] == ResultType::PATTERN_D_INCREASE) {
            size_t index_of_D = i;
            int block_size = 1 << (index_of_D);

            if (is_first) {
                cout << "Cache Line size: " << block_size * 8 << "b." << endl;
                is_first = false;
            } else {
                cout << "Cache size: " << block_size << "kb." << endl;
                return;
            }
        }
    }
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
    std::string filename = "hw_1_log.log";
    outFile.open(filename, std::ios::app);
    if (outFile.is_open()) {
        std::filesystem::path full_path = std::filesystem::absolute(filename);
        std::cout << "File for logs was created: " << full_path.string() << std::endl;
    }


    buffer = (void **) std::aligned_alloc(4096, MAX_MEMORY);
    get_associativity_1();
    outFile << " get_associativity " << std::endl;
    get_associativity();
    outFile << " detect_block_size " << std::endl;
    detect_block_size();
    free(buffer);
}
