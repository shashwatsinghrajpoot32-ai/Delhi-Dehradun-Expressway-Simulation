// Minimal embedded copy of cpp-httplib (public domain / MIT).
// Source: https://github.com/yhirose/cpp-httplib
// This is a trimmed build to keep this demo self-contained.
#pragma once

#ifndef CPPHTTPLIB_OPENSSL_SUPPORT
#define CPPHTTPLIB_OPENSSL_SUPPORT 0
#endif

// NOTE: This is intentionally a *very small* subset of cpp-httplib to serve
// static files + a few JSON endpoints on localhost for this project.

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "ws2_32.lib")
#else
#  include <arpa/inet.h>
#  include <netdb.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <sys/types.h>
#  include <unistd.h>
#endif

namespace httplib {

struct Request {
  std::string method;
  std::string path;
  std::string body;
  std::map<std::string, std::string> headers;
  std::map<std::string, std::string> params;
};

struct Response {
  int status = 200;
  std::string body;
  std::map<std::string, std::string> headers;

  void set_content(const std::string& s, const std::string& content_type) {
    body = s;
    headers["Content-Type"] = content_type;
  }
};

using Handler = std::function<void(const Request&, Response&)>;

class Server {
public:
  Server() = default;
  ~Server() { stop(); }

  void Get(const std::string& pattern, Handler h) { get_handlers_.push_back({std::regex(pattern), std::move(h)}); }
  void Post(const std::string& pattern, Handler h) { post_handlers_.push_back({std::regex(pattern), std::move(h)}); }

  void set_mount_point(const std::string& /*path*/, const std::string& /*dir*/) {
    // Not implemented in this minimal subset.
  }

  bool listen(const char* host, int port) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;
#endif

    int sock = static_cast<int>(::socket(AF_INET, SOCK_STREAM, 0));
    if (sock < 0) return false;

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (std::string(host) == "0.0.0.0") {
      addr.sin_addr.s_addr = INADDR_ANY;
    } else {
      inet_pton(AF_INET, host, &addr.sin_addr);
    }

    if (::bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
      close_socket(sock);
      return false;
    }
    if (::listen(sock, 16) < 0) {
      close_socket(sock);
      return false;
    }

    running_.store(true);
    server_sock_ = sock;

    while (running_.load()) {
      sockaddr_in caddr{};
      socklen_t clen = sizeof(caddr);
      int csock = static_cast<int>(::accept(sock, reinterpret_cast<sockaddr*>(&caddr), &clen));
      if (csock < 0) {
        if (!running_.load()) break;
        continue;
      }
      std::thread([this, csock]() { handle_client(csock); }).detach();
    }

    close_socket(sock);
#ifdef _WIN32
    WSACleanup();
#endif
    return true;
  }

  void stop() {
    running_.store(false);
    if (server_sock_ >= 0) {
      close_socket(server_sock_);
      server_sock_ = -1;
    }
  }

private:
  struct Entry {
    std::regex re;
    Handler handler;
  };

  std::vector<Entry> get_handlers_;
  std::vector<Entry> post_handlers_;
  std::atomic<bool> running_{false};
  int server_sock_ = -1;

  static void close_socket(int sock) {
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
  }

  static bool read_all(int sock, std::string& out) {
    char buf[4096];
    for (;;) {
      int n = recv(sock, buf, sizeof(buf), 0);
      if (n <= 0) break;
      out.append(buf, buf + n);
      if (out.size() > 8 * 1024 * 1024) return false;
      // crude heuristic: stop if we already got full headers+body (handled later)
      if (out.find("\r\n\r\n") != std::string::npos) break;
    }
    return true;
  }

  static std::string trim(const std::string& s) {
    auto b = s.find_first_not_of(" \t\r\n");
    auto e = s.find_last_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    return s.substr(b, e - b + 1);
  }

  static void parse_query_params(const std::string& q, std::map<std::string, std::string>& params) {
    std::istringstream iss(q);
    std::string kv;
    while (std::getline(iss, kv, '&')) {
      auto pos = kv.find('=');
      if (pos == std::string::npos) continue;
      params[kv.substr(0, pos)] = kv.substr(pos + 1);
    }
  }

  static bool parse_request(const std::string& raw, Request& req) {
    auto header_end = raw.find("\r\n\r\n");
    if (header_end == std::string::npos) return false;
    std::string header = raw.substr(0, header_end);
    std::istringstream iss(header);
    std::string request_line;
    if (!std::getline(iss, request_line)) return false;
    request_line = trim(request_line);

    {
      std::istringstream rl(request_line);
      rl >> req.method;
      std::string full_path;
      rl >> full_path;
      if (full_path.empty()) return false;
      auto qpos = full_path.find('?');
      if (qpos == std::string::npos) {
        req.path = full_path;
      } else {
        req.path = full_path.substr(0, qpos);
        parse_query_params(full_path.substr(qpos + 1), req.params);
      }
    }

    std::string line;
    while (std::getline(iss, line)) {
      line = trim(line);
      if (line.empty()) continue;
      auto pos = line.find(':');
      if (pos == std::string::npos) continue;
      auto key = trim(line.substr(0, pos));
      auto val = trim(line.substr(pos + 1));
      std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      req.headers[key] = val;
    }

    // body (best-effort, Content-Length only)
    auto it = req.headers.find("content-length");
    if (it != req.headers.end()) {
      std::size_t len = static_cast<std::size_t>(std::stoll(it->second));
      std::size_t start = header_end + 4;
      if (raw.size() >= start + len) {
        req.body = raw.substr(start, len);
      }
    }

    return true;
  }

  static std::string status_text(int status) {
    switch (status) {
      case 200: return "OK";
      case 400: return "Bad Request";
      case 404: return "Not Found";
      case 500: return "Internal Server Error";
      default: return "OK";
    }
  }

  static void write_response(int sock, const Response& res) {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << res.status << " " << status_text(res.status) << "\r\n";
    for (const auto& [k, v] : res.headers) oss << k << ": " << v << "\r\n";
    oss << "Content-Length: " << res.body.size() << "\r\n";
    oss << "Connection: close\r\n\r\n";
    oss << res.body;
    auto s = oss.str();
    send(sock, s.data(), static_cast<int>(s.size()), 0);
  }

  void handle_client(int csock) {
    std::string raw;
    if (!read_all(csock, raw)) {
      close_socket(csock);
      return;
    }

    // If Content-Length indicates more body, keep reading.
    auto header_end = raw.find("\r\n\r\n");
    if (header_end != std::string::npos) {
      auto header = raw.substr(0, header_end);
      std::smatch m;
      if (std::regex_search(header, m, std::regex("Content-Length:\\s*(\\d+)", std::regex::icase))) {
        std::size_t len = static_cast<std::size_t>(std::stoll(m[1].str()));
        std::size_t start = header_end + 4;
        while (raw.size() < start + len) {
          char buf[4096];
          int n = recv(csock, buf, sizeof(buf), 0);
          if (n <= 0) break;
          raw.append(buf, buf + n);
        }
      }
    }

    Request req;
    Response res;

    if (!parse_request(raw, req)) {
      res.status = 400;
      res.set_content("Bad Request", "text/plain");
      write_response(csock, res);
      close_socket(csock);
      return;
    }

    auto& handlers = (req.method == "POST") ? post_handlers_ : get_handlers_;
    bool matched = false;
    for (auto& e : handlers) {
      if (std::regex_match(req.path, e.re)) {
        matched = true;
        e.handler(req, res);
        break;
      }
    }

    if (!matched) {
      res.status = 404;
      res.set_content("Not Found", "text/plain");
    }
    write_response(csock, res);
    close_socket(csock);
  }
};

} // namespace httplib

