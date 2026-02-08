#include <iostream>
#include <thread>
#include <chrono>

int main() {
    std::cout << " Compilateur C++ fonctionne!" << std::endl;
    
    // Test thread
    std::thread t([]() {
        std::cout << " Threads C++ fonctionnent!" << std::endl;
    });
    t.join();
    
    // Test chrono
    auto start = std::chrono::high_resolution_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << " Mesure de temps fonctionne: " << duration.count() << " µs" << std::endl;
    std::cout << "\n Environnement C++ prêt pour le projet STR!" << std::endl;
    
    return 0;
}