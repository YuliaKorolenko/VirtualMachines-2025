#include <iomanip>
#include <iostream>
#include <random>
#include <unistd.h>
#include <chrono>
#include <vector>
#include <algorithm>
#include <map>

using namespace std;

const int ITERATIONS = 100000;

long double measure_access_time(size_t H, size_t S) {
    const size_t total_elements = H * S;

    void **buffer = (void **) std::aligned_alloc(128, total_elements * sizeof(void *));

    for (int i = 0; i < S; ++i) {
        buffer[i * H] = (void *) &buffer[i * H - H];
    }
    buffer[0] = (void *) &buffer[total_elements - H];

    const void *p = buffer;

    for (int j = 0; j < ITERATIONS; ++j) {
        for (int i = 0; i < S; ++i) {
            p = *(const void **) p;
        }
    }

    auto start = std::chrono::steady_clock::now();
    for (int j = 0; j < ITERATIONS; ++j) {
        for (long long i = 0; i < S; ++i) {
            p = *(const void **) p;
        }
    }
    fprintf(stdin, "%p", p);
    auto end = std::chrono::steady_clock::now();

    std::free(buffer);

    auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    return duration_ns.count() / static_cast<long double>(ITERATIONS * S);
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
    long long MAX_MEMORY = 1024 * 1024 * 1024;
    long long H = 16;

    long double prev_time = 0;
    while (H * MAX_ASSOCIATIVITY < MAX_MEMORY) {
        long long S = 1;
        while (S < MAX_ASSOCIATIVITY) {
            long double current_time = measure_access_time(H, S);
            long double jump = current_time / prev_time;
            if (prev_time != 0 && current_time / prev_time > 1.9) {
                // std::cout << "Stride H: " << H
                //         << ", Count elements S: " << S
                //         << ", Jump: " << jump << std::endl;
                jumps_map[H] = make_pair(S, jump);
            }
            S += 1;
            prev_time = current_time;
        }
        if (isMovement(jumps_map, H)) {
            H = H * 2;
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
    cout << "Associativity: " << current->second.first << endl;
}


int get_cache_line_size() {
    std::vector<long double> timings_cache;
    constexpr size_t ARRAY_SIZE = 128 * 1024 * 1024;
    constexpr int MAX_STRIDE_KB = 128;

    std::vector<char> data(ARRAY_SIZE);
    std::fill(data.begin(), data.end(), 1);


    long double avg_time_prev = 0;
    for (int stride = 1; stride <= MAX_STRIDE_KB * 4; stride += 1) {
        // warm - up
        char temp;
        for (long long i = 0; i < 100; ++i) {
            temp = data[(i * stride) & (ARRAY_SIZE - 1)];
        }

        auto start_time = std::chrono::high_resolution_clock::now();

        for (long long i = 0; i < 1000000; ++i) {
            temp = data[(i * stride) & (ARRAY_SIZE - 1)];
        }
        fprintf(stdin, "%d", temp);

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);

        if (avg_time_prev != 0 && avg_time_prev * 2 <= duration.count()) {
            std::cout << "Cache line size (b) : " << stride << "\n";
            break;
        }
        avg_time_prev = duration.count();
        std::cout << stride << "\t\t" << duration.count() / 1000000.0 << std::endl;
    }

    return 0;
}

int get_cache_size() {
    constexpr int REPEAT = 1000;
    const int min_size_kb = 16;
    const int max_size_kb = 1024 * 2;

    long double prev_time = 0;
    for (long long size_kb = min_size_kb; size_kb <= max_size_kb; size_kb *= 2) {
        long long array_size_bytes = size_kb * 1024;
        long long total_elements = array_size_bytes / sizeof(int);

        int *buffer = (int *) (std::aligned_alloc(128, array_size_bytes));
        int step = 1; // cache_line_size / sizeof(int);
        for (int i = 0; i < total_elements; ++i) {
            buffer[i] = i;
        }

        std::shuffle(&buffer[0], &buffer[total_elements - 1],
                     std::default_random_engine(std::chrono::steady_clock::now().time_since_epoch().count()));

        int k = 0;
        for (int j = 0; j < REPEAT; ++j) {
            for (int i = 0; i < total_elements; i += step) {
                k = buffer[k];
                fprintf(stdin, "%d", k);
            }
        }

        int num_iterations = total_elements / step;
        auto start_time = std::chrono::high_resolution_clock::now();

        k = 0;

        for (int j = 0; j < REPEAT; ++j) {
            for (int i = 0; i < num_iterations; i++) {
                k = buffer[k];
            }
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);

        delete[] buffer;

        long double cur_time = (long double) duration.count() / (REPEAT * total_elements / step);
        if (prev_time != 0 && prev_time * 1.8 <= cur_time) {
            std::cout << "Cache (kb): " << size_kb / 2 << endl;
            break;
        }
        prev_time = cur_time;
        std::cout << "Cache size: " << size_kb << " Duration: " << (long double) duration.count() / (
            REPEAT * total_elements / step) << "\n";
    }
}


int main() {
    get_associativity();
    // get_cache_line_size();
    // get_cache_size();
}
