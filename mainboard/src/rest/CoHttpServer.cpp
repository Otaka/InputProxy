#include "CoHttpServer.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <algorithm>
#include <stdexcept>
#include <iostream>

#include "corocgo/corocgo.h"

using namespace corocgo;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static const char* statusText(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        default:  return "Unknown";
    }
}

// Percent-decode a URL-encoded string; '+' is treated as space.
static std::string urlDecode(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ) {
        if (s[i] == '%' && i + 2 < s.size()) {
            char hex[3] = { s[i+1], s[i+2], '\0' };
            out += static_cast<char>(std::strtol(hex, nullptr, 16));
            i += 3;
        } else if (s[i] == '+') {
            out += ' ';
            ++i;
        } else {
            out += s[i++];
        }
    }
    return out;
}

// Trim leading and trailing ASCII whitespace (space, tab, CR, LF).
static std::string trim(const std::string& s) {
    const auto notSpace = [](unsigned char c){ return c > ' '; };
    auto b = std::find_if(s.begin(), s.end(), notSpace);
    auto e = std::find_if(s.rbegin(), s.rend(), notSpace).base();
    return (b < e) ? std::string(b, e) : std::string{};
}

static std::string toLower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// Non-blocking write loop: keep sending until all bytes are delivered,
// yielding via wait_file(WAIT_OUT) whenever the kernel buffer is full.
static void sendAll(int fd, const char* data, int size) {
    int offset = 0;
    while (offset < size) {
        int n = static_cast<int>(::send(fd, data + offset, size - offset, MSG_NOSIGNAL));
        if (n > 0) {
            offset += n;
        } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            auto [flags, err] = wait_file(fd, WAIT_OUT);
            if (err) return;
        } else {
            return; // connection closed or hard error
        }
    }
}

// ---------------------------------------------------------------------------
// CoHttpRouter
// ---------------------------------------------------------------------------

static std::vector<std::string> splitPath(std::string path) {
    // Normalise: strip trailing slash (except bare "/")
    if (path.size() > 1 && path.back() == '/')
        path.pop_back();

    std::vector<std::string> parts;
    size_t pos = (!path.empty() && path[0] == '/') ? 1 : 0;
    while (pos < path.size()) {
        size_t slash = path.find('/', pos);
        if (slash == std::string::npos) {
            parts.push_back(path.substr(pos));
            break;
        }
        parts.push_back(path.substr(pos, slash - pos));
        pos = slash + 1;
    }
    return parts;
}

void CoHttpRouter::endpoint(const std::string& method,
                            const std::string& path,
                            RouteHandler handler) {
    routes_.push_back({ method, splitPath(path), std::move(handler) });
}

void CoHttpRouter::dispatch(coSession session) const {
    auto pathSegs = splitPath(session->path);

    const Route* best = nullptr;
    std::map<std::string, std::string> bestVars;
    int bestScore = -1;

    for (auto& route : routes_) {
        if (route.method != session->method) continue;
        if (route.segments.size() != pathSegs.size()) continue;

        std::map<std::string, std::string> vars;
        bool match = true;
        int score = 0;

        for (size_t i = 0; i < route.segments.size(); ++i) {
            const auto& seg = route.segments[i];
            if (seg.size() >= 2 && seg.front() == '{' && seg.back() == '}') {
                vars[seg.substr(1, seg.size() - 2)] = urlDecode(pathSegs[i]);
                // wildcard: score += 0
            } else if (seg == pathSegs[i]) {
                ++score;
            } else {
                match = false;
                break;
            }
        }

        if (match && score > bestScore) {
            bestScore  = score;
            best       = &route;
            bestVars   = std::move(vars);
        }
    }

    if (best) {
        best->handler(session, std::move(bestVars));
        return;
    }

    // No route matched
    session->setStatus(404);
    session->setResponseHeader("Content-Type", "application/json");
    session->setResponseHeader("Access-Control-Allow-Origin", "*");
    const char* body = "{\"error\":\"not found\"}";
    session->write(body, static_cast<int>(std::strlen(body)));
    session->close();
}

// ---------------------------------------------------------------------------
// createServer
// ---------------------------------------------------------------------------

coServer createServer(int port) {
    int sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        throw std::runtime_error(std::string("socket: ") + strerror(errno));

    int yes = 1;
    ::setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(static_cast<uint16_t>(port));

    if (::bind(sockfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(sockfd);
        throw std::runtime_error(std::string("bind: ") + strerror(errno));
    }

    if (::listen(sockfd, SOMAXCONN) < 0) {
        ::close(sockfd);
        throw std::runtime_error(std::string("listen: ") + strerror(errno));
    }

    ::fcntl(sockfd, F_SETFL, ::fcntl(sockfd, F_GETFL, 0) | O_NONBLOCK);

    auto server    = std::make_shared<CoServerImpl>();
    server->sockfd = sockfd;
    return server;
}

// ---------------------------------------------------------------------------
// CoServerImpl::accept
// ---------------------------------------------------------------------------

coSession CoServerImpl::accept() {
    // Retry loop: client disconnects / bad requests should not kill the server.
    // Only a failure on the *listening* socket returns nullptr.
    for (;;) {

    // 1. Wait for an incoming connection.
    auto [flags, err] = wait_file(sockfd, WAIT_IN);
    if (err) {
        closed = true;
        return nullptr;
    }

    // 2. Accept the connection.
    sockaddr_in clientAddr{};
    socklen_t   addrLen = sizeof(clientAddr);
    int connfd = ::accept(sockfd,
                          reinterpret_cast<sockaddr*>(&clientAddr),
                          &addrLen);
    if (connfd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) continue; // spurious wakeup
        closed = true;
        std::cout<<"connfd error:"<<errno<<std::endl;
        return nullptr;
    }
    ::fcntl(connfd, F_SETFL, ::fcntl(connfd, F_GETFL, 0) | O_NONBLOCK);

    // 3. Read until we have the full header block (\r\n\r\n).
    std::string rawBuf;
    rawBuf.reserve(4096);
    size_t headerEnd = std::string::npos;
    bool clientFail = false;

    while (headerEnd == std::string::npos) {
        auto [rf, re] = wait_file(connfd, WAIT_IN);
        if (re) { clientFail = true; break; }
        char tmp[4096];
        int n = static_cast<int>(::recv(connfd, tmp, sizeof(tmp), 0));
        if (n <= 0) { clientFail = true; break; }
        rawBuf.append(tmp, n);
        headerEnd = rawBuf.find("\r\n\r\n");
        if (rawBuf.size() > 65536) { clientFail = true; break; }
    }
    if (clientFail) { ::close(connfd); continue; }

    // 4. Split header block and remainder (start of body).
    std::string headerPart = rawBuf.substr(0, headerEnd);
    std::string bodyPart   = rawBuf.substr(headerEnd + 4);

    // 5. Parse request line.
    size_t firstLine = headerPart.find("\r\n");
    if (firstLine == std::string::npos) { ::close(connfd); continue; }
    std::string requestLine = headerPart.substr(0, firstLine);

    // "METHOD /path?qs HTTP/1.1"
    size_t sp1 = requestLine.find(' ');
    size_t sp2 = (sp1 != std::string::npos) ? requestLine.find(' ', sp1 + 1) : std::string::npos;
    if (sp1 == std::string::npos || sp2 == std::string::npos) { ::close(connfd); continue; }
    std::string method  = requestLine.substr(0, sp1);
    std::string rawPath = requestLine.substr(sp1 + 1, sp2 - sp1 - 1);

    // Split path and query string.
    std::string path;
    std::map<std::string, std::string> queryStringMap;
    size_t qmark = rawPath.find('?');
    if (qmark == std::string::npos) {
        path = rawPath;
    } else {
        path = rawPath.substr(0, qmark);
        std::string qs = rawPath.substr(qmark + 1);
        // Parse key=value&key2=value2
        size_t pos = 0;
        while (pos < qs.size()) {
            size_t amp = qs.find('&', pos);
            if (amp == std::string::npos) amp = qs.size();
            std::string pair = qs.substr(pos, amp - pos);
            size_t eq = pair.find('=');
            if (eq != std::string::npos) {
                queryStringMap[urlDecode(pair.substr(0, eq))] =
                    urlDecode(pair.substr(eq + 1));
            } else if (!pair.empty()) {
                queryStringMap[urlDecode(pair)] = "";
            }
            pos = amp + 1;
        }
    }

    // 6. Parse remaining header lines.
    std::map<std::string, std::string> headersMap;
    size_t lineStart = firstLine + 2;
    while (lineStart < headerPart.size()) {
        size_t lineEnd = headerPart.find("\r\n", lineStart);
        if (lineEnd == std::string::npos) lineEnd = headerPart.size();
        std::string line = headerPart.substr(lineStart, lineEnd - lineStart);
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            headersMap[toLower(trim(line.substr(0, colon)))] =
                trim(line.substr(colon + 1));
        }
        lineStart = lineEnd + 2;
    }

    // 7. Build session.
    auto session          = std::make_shared<CoSessionImpl>();
    session->fd           = connfd;
    session->method       = std::move(method);
    session->path         = std::move(path);
    session->clientIp     = ::inet_ntoa(clientAddr.sin_addr);
    session->queryString  = std::move(queryStringMap);
    session->headers      = std::move(headersMap);
    session->bodyBuffer   = std::move(bodyPart);
    session->bodyOffset   = 0;
    return session;

    } // for(;;) retry loop
}

// ---------------------------------------------------------------------------
// CoServerImpl::free
// ---------------------------------------------------------------------------

void CoServerImpl::free() {
    if (sockfd >= 0) {
        ::close(sockfd);
        sockfd = -1;
    }
    closed = true;
}

// ---------------------------------------------------------------------------
// CoSessionImpl::read
// ---------------------------------------------------------------------------

int CoSessionImpl::read(char* buf, int maxSize) {
    if (maxSize <= 0) return 0;

    // How many bytes are we expecting in total?
    int contentLength = -1;
    auto it = headers.find("content-length");
    if (it != headers.end()) {
        try { contentLength = std::stoi(it->second); } catch (...) {}
    }

    int total = 0;

    // 1. Drain whatever was read past the header block.
    if (bodyOffset < bodyBuffer.size()) {
        int available = static_cast<int>(bodyBuffer.size() - bodyOffset);
        int toCopy    = std::min(available, maxSize);
        std::memcpy(buf, bodyBuffer.data() + bodyOffset, toCopy);
        bodyOffset += toCopy;
        total      += toCopy;
    }

    // 2. Read more from the socket only if Content-Length tells us there is more.
    //    Without Content-Length there is no body (e.g. GET), so don't block.
    if (contentLength >= 0) {
        while (total < maxSize && total < contentLength) {
            auto [flags, err] = wait_file(fd, WAIT_IN);
            if (err) break;

            int n = static_cast<int>(::recv(fd, buf + total, maxSize - total, 0));
            if (n <= 0) break;
            total += n;
        }
    }

    return total;
}

// ---------------------------------------------------------------------------
// CoSessionImpl response helpers
// ---------------------------------------------------------------------------

void CoSessionImpl::setStatus(int code) {
    statusCode = code;
}

void CoSessionImpl::setResponseHeader(const std::string& name, const std::string& value) {
    responseHeaders[name] = value;
}

void CoSessionImpl::write(const char* data, int size) {
    if (!headersSent) {
        // Build the complete response: status line + headers + blank line + body.
        std::string response;
        response.reserve(256 + size);
        response  = "HTTP/1.1 ";
        response += std::to_string(statusCode);
        response += ' ';
        response += statusText(statusCode);
        response += "\r\nContent-Length: ";
        response += std::to_string(size);
        response += "\r\n";
        for (auto& [k, v] : responseHeaders) {
            response += k;
            response += ": ";
            response += v;
            response += "\r\n";
        }
        response += "\r\n";
        response.append(data, size);
        sendAll(fd, response.c_str(), static_cast<int>(response.size()));
        headersSent = true;
    } else {
        sendAll(fd, data, size);
    }
}

// ---------------------------------------------------------------------------
// CoSessionImpl::close
// ---------------------------------------------------------------------------

void CoSessionImpl::close() {
    if (fd >= 0) {
        ::shutdown(fd, SHUT_RDWR);
        ::close(fd);
        fd = -1;
    }
}
