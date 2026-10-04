#include <iostream>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>

int main(int argc, char* argv[]) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "[!] Slipper CLI: Failed to create socket." << std::endl;
        return 1;
    }

    sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    constexpr std::string_view socket_path = "/run/slipper/slipper.sock";
    strncpy(addr.sun_path, socket_path.data(), sizeof(addr.sun_path) - 1);

    if (connect(sock, reinterpret_cast< sockaddr* >(&addr), sizeof(addr)) == -1) {
        std::cerr << "[!] Slipper CLI: Cannot connect to daemon. Is slipperd running via OpenRC?" << std::endl;
        close(sock);
        return 1;
    }

    std::vector< std::string_view > args(argv + 1, argv + argc);
    std::string payload;
    
    for (const auto& arg : args) {
        if (!payload.empty()) payload += " ";
        payload += arg;
    }
    if (payload.empty()) payload = " "; 

    write(sock, payload.c_str(), payload.size());

    char buffer[1024];
    ssize_t bytes_read;
    std::string stream_buf = "";
    constexpr std::string_view PROMPT_TOKEN = "__SLIP_PROMPT__";

    while ((bytes_read = read(sock, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        stream_buf += buffer;

        size_t pos;
        while ((pos = stream_buf.find(PROMPT_TOKEN)) != std::string::npos) {
            std::cout << stream_buf.substr(0, pos);
            std::cout.flush();
            stream_buf.erase(0, pos + PROMPT_TOKEN.length());

            std::string response;
            std::getline(std::cin, response);
            
            response += "\n";
            write(sock, response.c_str(), response.length());
        }

        if (stream_buf.length() >= PROMPT_TOKEN.length()) {
            size_t safe_len = stream_buf.length() - PROMPT_TOKEN.length() + 1;
            std::cout << stream_buf.substr(0, safe_len);
            stream_buf.erase(0, safe_len);
        }
    }

    if (!stream_buf.empty()) {
        std::cout << stream_buf;
    }

    close(sock);
    return 0;
}