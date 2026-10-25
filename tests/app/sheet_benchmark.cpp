#include <spentit/sheet_session.hpp>
#include <chrono>
#include <iostream>
#include <cstdlib>
#include <stdexcept>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/resource.h>
#endif
using namespace spentit::sheet;
using Clock = std::chrono::steady_clock;
static double ms(Clock::time_point start) { return std::chrono::duration<double,std::milli>(Clock::now()-start).count(); }
int main(int argc,char** argv) {
    try {
        const int count = argc > 1 ? std::stoi(argv[1]) : 50000;
        auto start = Clock::now(); auto rows = synthetic(count);
        std::cout << "rows_per_account=" << count << " generation_ms=" << ms(start) << '\n';
        for (int column = 0; column < 6; ++column) {
            start = Clock::now(); auto view = query(*rows,0,"",column,false);
            std::cout << "sort_column=" << column << " ms=" << ms(start) << " matched=" << view.indices.size() << '\n';
        }
        for (const auto* filter : {"ticket", "Payday", "absent"}) {
            start = Clock::now(); auto view = query(*rows,0,filter,0,false);
            std::cout << "filter=" << filter << " ms=" << ms(start) << " matched=" << view.indices.size() << '\n';
        }
        auto edit = begin_edit(*rows,1); edit.payee = "Edited fixture";
        start = Clock::now(); auto changed = save_edit(*rows,edit); auto view = query(*changed,0,"",0,false);
        std::cout << "save_and_refresh_ms=" << ms(start) << '\n';
#if defined(__unix__) || defined(__APPLE__)
        rusage usage{};
        if (getrusage(RUSAGE_SELF,&usage) == 0) {
#ifdef __APPLE__
            std::cout << "peak_rss_mib=" << double(usage.ru_maxrss) / 1048576.0 << '\n';
#else
            std::cout << "peak_rss_mib=" << double(usage.ru_maxrss) / 1024.0 << '\n';
#endif
        }
#endif
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
