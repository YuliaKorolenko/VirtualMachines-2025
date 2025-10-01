#include <iomanip>
#include <iostream>
#include <random>
#include <unistd.h>
#include <chrono>
#include <vector>
#include <algorithm>
using namespace std;

long double measure_access_time(size_t H, size_t S) {
    H /= sizeof(int);
    const size_t total_elements = H * S;

    int* buffer = (int*)(std::aligned_alloc(128, total_elements * sizeof(int)));
    for (int i = 0; i < S; ++i) {
        buffer[i * H] = i * H - H;
    }
    buffer[0] = total_elements - H;

    int k = 0;
    int num_iterations = 10000000;

    for (int j = 0; j < num_iterations; ++j) {
        for (int i = 0; i < S; ++i) {
            k = buffer[k];
        }
    }

    k = 0;
    auto start = std::chrono::steady_clock::now();
    for (int j = 0; j < num_iterations; ++j) {
        for (long long i = 0; i < S; ++i) {
            k = buffer[k];
        }
    }
    auto end = std::chrono::steady_clock::now();

    delete[] buffer;

    auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    return duration_ns.count() / static_cast<long double>(num_iterations * S);
}

int get_associativity() {
    const size_t H = 512 * 1024;

    int prev_time = 0;
    for (size_t s = 1; s <= 24; s += 1) {

        long double time = measure_access_time(H, s);


        if (prev_time != 0 && prev_time * 2 <= time) {
            cout << "Associativity: " << s - 1 << "\n";
            break;
        }
        prev_time = time;
        // std::cout << std::left << std::setw(12) << s
        //           << std::setw(10) << time << "\n";
    }
}


int get_cache_line_size() {
    constexpr size_t ARRAY_SIZE = 128 * 1024 * 1024;
    constexpr int MAX_STRIDE_KB = 128;

    std::vector<char> data(ARRAY_SIZE);
    std::fill(data.begin(), data.end(), 1);


    long double avg_time_prev = 0;
    for (int stride = 1; stride <= MAX_STRIDE_KB * 4; stride += 1) {
        // warm - up
        for (long long i = 0; i < 100; ++i) {
            char temp = data[(i * stride) & (ARRAY_SIZE - 1)];
        }

        auto start_time = std::chrono::high_resolution_clock::now();

        for (long long i = 0; i < 1000000; ++i) {
            char temp = data[(i * stride) & (ARRAY_SIZE - 1)];
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);

        if (avg_time_prev != 0 && avg_time_prev * 2 <= duration.count()) {
            std::cout << "Cache line size (b) : " << stride << "\n";
            break;
        }
        avg_time_prev = duration.count();
        // std::cout << stride << "\t\t" << duration.count() / 1000000.0 << std::endl;
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

        int* buffer = (int*)(std::aligned_alloc(128, array_size_bytes));
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

        long double cur_time = (long double)duration.count() / (REPEAT * total_elements / step);
        if (prev_time != 0 && prev_time * 1.8 <= cur_time) {
            std::cout << "Cache (kb): " << size_kb / 2 << endl;
            break;
        }
        prev_time = cur_time;
        // std::cout << "Cache size: " << size_kb << " Duration: "  << (long double)duration.count() / (REPEAT * total_elements / step)  << "\n";
    }
}


int main() {
    get_associativity();
    get_cache_line_size();
    get_cache_size();
}
