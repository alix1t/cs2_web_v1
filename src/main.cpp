#include "pch.hpp"
#include <iostream>
#include <string>
#include <curl/curl.h>
#include <random>
#include <chrono>
#include <thread>

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

class LicenseVerifier {
public:
    explicit LicenseVerifier(const std::string& url) : url_(url) {}

    bool verify(const std::string& license_key) {
        CURL* curl = curl_easy_init();
        if (!curl) {
            std::cerr << "初始化失败" << std::endl;
            return false;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url_.c_str());
        std::string post_fields = "card_key=" + license_key;
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields.c_str());
        std::string response_buffer;
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buffer);
        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
            return false;
        }

        std::cout << "验证结果: " << response_buffer << std::endl;
        return parseResponse(response_buffer);
    }

private:
    std::string url_;

    bool parseResponse(const std::string& response) {
        if (response.find("\"success\":true") != std::string::npos) {
            std::cout << "卡密验证成功！" << std::endl;
            return true;
        }
        if (response.find("无效的卡密") != std::string::npos) {
            std::cout << "卡密验证失败！" << std::endl;
            return false;
        }
        std::cerr << "未知响应！" << std::endl;
        return false;
    }
};

static easywsclient::WebSocket* web_socket = nullptr;

bool initializeSystem() {
    config_data_t config_data;

    if (!cfg::setup(config_data)) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        return false;
    }
    LOG_INFO("config system initialization completed");

    if (!exc::setup()) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        return false;
    }
    LOG_INFO("exception handler initialization completed");

    if (!m_memory->setup()) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        return false;
    }
    LOG_INFO("memory initialization completed");

    if (!i::setup()) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        return false;
    }
    LOG_INFO("interfaces initialization completed");

    if (!schema::setup()) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        return false;
    }
    LOG_INFO("schema initialization completed");

    WSADATA wsa_data = {};
    const auto wsa_startup = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (wsa_startup != 0) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        return false;
    }
    LOG_INFO("winsock initialization completed");

    const auto product_version = c_engine_client::get_product_version();
    if (product_version.find(CS2_VERSION) == std::string::npos) {
        LOG_WARNING("version mismatch! current 'cs2' version is '%s'", product_version.c_str());
    }
    else {
        LOG_INFO("version match! current 'cs2' version is up to date");
    }

    const auto ipv4_address = utils::get_ipv4_address(config_data);
    if (ipv4_address.empty()) {
        LOG_WARNING("failed to automatically get your ipv4 address!\n                 we will use '%s' from 'config.json'. if the local ip is wrong, please set it", config_data.m_local_ip);
    }

    const auto formatted_address = std::format("ws://{}:22006/cs2_webradar", ipv4_address);
    web_socket = easywsclient::WebSocket::from_url(formatted_address);
    if (!web_socket) {
        LOG_ERROR("failed to connect to the web socket ('%s')", formatted_address.c_str());
        return false;
    }
    LOG_INFO("connected to the web socket ('%s')", formatted_address.data());

    return true;
}

int main() {
    std::string license_key;
    std::cout << "请输入卡密: ";
    std::cin >> license_key;

    LicenseVerifier verifier("http://km.jujue.fun/verify_card_key.php");

    if (!verifier.verify(license_key)) {
        std::cerr << "卡密验证失败" << std::endl;
        return 0;
    }

    if (!initializeSystem()) {
        std::cerr << "初始化失败。" << std::endl;
        return 0;
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> scan_interval_dis(0.12f, 0.24f);
    std::uniform_int_distribution<int> pause_duration_dis(1, 2);
    std::uniform_int_distribution<int> total_scan_time_dis(60, 80);

    int total_scan_time = total_scan_time_dis(gen);
    auto start_time = std::chrono::steady_clock::now();

    while (true) {
        sdk::update();
        f::run();
        web_socket->send(f::m_data.dump());

        std::this_thread::sleep_for(std::chrono::duration<float>(scan_interval_dis(gen)));

        auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start_time).count();

        if (elapsed_time >= total_scan_time) {
            int pause_duration = pause_duration_dis(gen);
            LOG_INFO("pause", pause_duration);
            std::this_thread::sleep_for(std::chrono::seconds(pause_duration));

            total_scan_time = total_scan_time_dis(gen);
            start_time = std::chrono::steady_clock::now();
        }

        web_socket->poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    system("pause");
    return 0;
}
