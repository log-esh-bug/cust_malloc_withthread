#include <benchmark/benchmark.h>
#include <iostream>
using namespace std;


extern "C" {
    #include "NoFootThread.h"
}

static void test2(benchmark::State& state){
    int n= state.range(0);
    for(auto _ : state){
        // cout << "c++\n";
        MemoryContext* mc= createMemoryContext();
        user(mc,n);
        destroyMemoryContext(mc);
        mc =NULL;
    }
}

BENCHMARK(test2)->RangeMultiplier(10)->Range(1000,10000000);
// BENCHMARK(BM_MemoryFree)->Arg(64);

BENCHMARK_MAIN();