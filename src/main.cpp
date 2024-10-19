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
            std::cerr << "��ʼ��ʧ��" << std::endl;
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

        std::cout << "��֤���: " << response_buffer << std::endl;
        return parseResponse(response_buffer);
    }

private:
    std::string url_;

    bool parseResponse(const std::string& response) {
        if (response.find("\"success\":true") != std::string::npos) {
            std::cout << "������֤�ɹ���" << std::endl;
            return true;
        }
        if (response.find("��Ч�Ŀ���") != std::string::npos) {
            std::cout << "������֤ʧ�ܣ�" << std::endl;
            return false;
        }
        std::cerr << "δ֪��Ӧ��" << std::endl;
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
    std::cout << "�����뿨��: ";
    std::cin >> license_key;

    LicenseVerifier verifier("http://km.jujue.fun/verify_card_key.php");

    if (!verifier.verify(license_key)) {
        std::cerr << "������֤ʧ��" << std::endl;
        return 0;
    }

    if (!initializeSystem()) {
        std::cerr << "��ʼ��ʧ�ܡ�" << std::endl;
        return 0;
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(0.12f, 0.24f);

    int scan_count = 0;

    for (;;) {
        sdk::update();
        f::run();
        web_socket->send(f::m_data.dump());

        std::this_thread::sleep_for(std::chrono::duration<float>(dis(gen)));

        scan_count++;

        if (scan_count >= 50) {
            LOG_INFO("pause 4");
            std::this_thread::sleep_for(std::chrono::seconds(4));
            scan_count = 0;
        }

        web_socket->poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    system("pause");
    return 0;
}
