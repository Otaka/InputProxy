#pragma once

#include <functional>
#include <memory>
#include <string>
#include <map>
#include <vector>

struct CoSessionImpl;
struct CoServerImpl;

using coSession = std::shared_ptr<CoSessionImpl>;
using coServer  = std::shared_ptr<CoServerImpl>;

// Handler for a matched route. vars contains only the {name} path variables.
using RouteHandler = std::function<void(coSession, std::map<std::string, std::string>)>;

// HTTP router with path-pattern matching.
// Register routes with endpoint(), then call dispatch() per incoming session.
// Pattern segments like {id} match any single path component (no '/').
// More specific routes (more literal segments) win over wildcard ones.
struct CoHttpRouter {
    void endpoint(const std::string& method, const std::string& path, RouteHandler handler);
    void dispatch(coSession session) const;

private:
    struct Route {
        std::string method;
        std::vector<std::string> segments;
        RouteHandler handler;
    };
    std::vector<Route> routes_;
};

// Create a non-blocking TCP server listening on the given port.
coServer createServer(int port);

struct CoServerImpl {
    int  sockfd  = -1;
    bool closed  = false;

    // Blocks (via wait_file) until a client connects, reads and parses the
    // full HTTP request, and returns a ready-to-use session.
    // Returns nullptr and sets closed=true on error.
    coSession accept();

    bool isClosed() const { return closed; }

    // Close the listening socket.
    void free();
};

struct CoSessionImpl {
    // Populated during accept():
    std::string method;     // GET, POST, PUT, …
    std::string path;       // /foo/bar  (no query string)
    std::string clientIp;
    std::map<std::string, std::string> queryString;  // parsed from ?k=v&…
    std::map<std::string, std::string> headers;      // lower-cased keys

    // Read the request body into buf (up to maxSize bytes).
    // Respects Content-Length; uses a wait_file loop internally.
    // Returns the number of bytes read, or -1 on error.
    int read(char* buf, int maxSize);

    // --- Response building ---
    void setStatus(int code);
    void setResponseHeader(const std::string& name, const std::string& value);

    // Send the HTTP response (status line + headers + body) using a
    // non-blocking write loop (wait_file WAIT_OUT on back-pressure).
    // On the first call the HTTP headers are prepended automatically.
    // Subsequent calls append more body data without repeating headers.
    void write(const char* data, int size);

    // Shut down and close the connection socket.
    void close();

    // ---- Internal state (not part of the public contract) ----
    int  fd           = -1;
    int  statusCode   = 200;
    std::map<std::string, std::string> responseHeaders;
    std::string bodyBuffer;  // bytes already consumed past \r\n\r\n
    size_t bodyOffset  = 0;
    bool headersSent   = false;
};
