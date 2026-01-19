#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <functional>
#include <unordered_map>
#include <type_traits>
#include <tuple>
#include <memory>
#include <vector>
#include <chrono>

#ifdef RPCMANAGER_STD_STRING
#include <string>
#endif

#ifdef RPCMANAGER_MUTEX
#include <mutex>
#include <condition_variable>
#define RPC_LOCK() std::lock_guard<std::mutex> lock(mutex_)
#else
#define RPC_LOCK()
#endif

namespace simplerpc {

// ---------------------------------------------
// Constants
// ---------------------------------------------
static constexpr uint16_t RPC_MAGIC = 0xABCD;  // Changed from 0xFEEF to avoid conflict with StreamFramer magic
static constexpr uint8_t RPC_FLAG_REPLY = 0x01;
static constexpr size_t RPC_HEADER_SIZE = 13;  // Magic(2) + Length(2) + ProviderID(2) + MethodID(2) + Flags(1) + CallID(4)

// ---------------------------------------------
// Error Codes
// ---------------------------------------------
static constexpr int RPC_ERROR_ARGS_TOO_LARGE = 2;
static constexpr int RPC_ERROR_PAYLOAD_TOO_LARGE = 3;
static constexpr int RPC_ERROR_INVALID_PACKET_LENGTH = 4;
static constexpr int RPC_ERROR_NO_HANDLER = 7;
static constexpr int RPC_ERROR_TIMEOUT = 8;
static constexpr int RPC_ERROR_UNEXPECTED_CALL_ID = 11;

// ---------------------------------------------
// RPC Packet Header Structure
// ---------------------------------------------
#pragma pack(push, 1)
struct RpcPacket {
    uint16_t magic;       // 0xABCD (little-endian) - different from StreamFramer's 0xFEEF
    uint16_t length;      // Total length including header
    uint16_t providerId;  // Provider ID
    uint16_t methodId;    // Method ID
    uint8_t flags;        // Flags (RPC_FLAG_REPLY, etc.)
    uint32_t callId;      // Call ID
    // Payload follows after header
};
#pragma pack(pop)

static_assert(sizeof(RpcPacket) == RPC_HEADER_SIZE, "RpcPacket size must match RPC_HEADER_SIZE");

// ---------------------------------------------
// Packet structure for StreamFramer
// ---------------------------------------------
struct Packet {
    const char* data;
    uint16_t length;
};

// Forward declarations for CRC16
inline uint16_t crc16(const uint8_t* data, size_t len);

// ---------------------------------------------
// StreamFramer - Byte-by-byte packet framing
// ---------------------------------------------
class StreamFramer {
private:
    // Constants
    static constexpr size_t BUFFER_SIZE = 10 * 1024;
    static constexpr uint8_t MAGIC_BYTE_1 = 0xEF;
    static constexpr uint8_t MAGIC_BYTE_2 = 0xFE;
    static constexpr size_t HEADER_SIZE = 10;
    static constexpr size_t MAX_CONTENT_SIZE = BUFFER_SIZE - HEADER_SIZE;
    
    // State machine states
    enum class State {
        SEARCH_MAGIC_1,
        SEARCH_MAGIC_2,
        READING_HEADER,
        READING_CONTENT
    };
    
    // Member buffers (no dynamic allocation)
    uint8_t input_buffer_[BUFFER_SIZE];
    uint8_t output_buffer_[BUFFER_SIZE];
    
    // State machine variables
    State state_;
    size_t bytes_received_;
    uint16_t content_size_;
    
    // Callback
    std::function<void(Packet&)> on_packet_;
    
    // Reset state machine
    void reset() {
        state_ = State::SEARCH_MAGIC_1;
        bytes_received_ = 0;
        content_size_ = 0;
    }
    
    // Process single byte - recursive helper
    void processByte(uint8_t byte, int recursion_level = 0) {
        if (recursion_level > 5) {
            reset();
            return;
        }
        
        switch (state_) {
            case State::SEARCH_MAGIC_1:
                if (byte == MAGIC_BYTE_1) {
                    input_buffer_[0] = byte;
                    bytes_received_ = 1;
                    state_ = State::SEARCH_MAGIC_2;
                }
                break;
                
            case State::SEARCH_MAGIC_2:
                if (byte == MAGIC_BYTE_2) {
                    input_buffer_[1] = byte;
                    bytes_received_ = 2;
                    state_ = State::READING_HEADER;
                } else {
                    // Not magic byte 2, save first 8 bytes for potential recursion
                    uint8_t temp_buffer[8];
                    temp_buffer[0] = byte;
                    reset();
                    processByte(byte, recursion_level + 1);
                }
                break;
                
            case State::READING_HEADER:
                input_buffer_[bytes_received_++] = byte;
                
                if (bytes_received_ >= HEADER_SIZE) {
                    // Header complete - parse and validate
                    content_size_ = read_u16_le(&input_buffer_[2]);
                    
                    // Validate content size
                    if (content_size_ > MAX_CONTENT_SIZE) {
                        // Invalid size - save bytes without magic and retry
                        uint8_t temp_buffer[8];
                        std::memcpy(temp_buffer, &input_buffer_[2], 8);
                        reset();
                        for (int i = 0; i < 8; i++) {
                            processByte(temp_buffer[i], recursion_level + 1);
                        }
                        return;
                    }
                    
                    // Validate header CRC
                    uint16_t expected_header_crc = read_u16_le(&input_buffer_[8]);
                    uint16_t actual_header_crc = crc16(input_buffer_, 8);
                    
                    if (expected_header_crc == actual_header_crc) {
                        // Valid header
                        if (content_size_ == 0) {
                            // Empty content - process immediately
                            Packet pkt{reinterpret_cast<char*>(input_buffer_), HEADER_SIZE};
                            if (on_packet_) {
                                on_packet_(pkt);
                            }
                            reset();
                        } else {
                            state_ = State::READING_CONTENT;
                        }
                    } else {
                        // CRC mismatch - save bytes without magic and retry
                        uint8_t temp_buffer[8];
                        std::memcpy(temp_buffer, &input_buffer_[2], 8);
                        reset();
                        for (int i = 0; i < 8; i++) {
                            processByte(temp_buffer[i], recursion_level + 1);
                        }
                    }
                }
                break;
                
            case State::READING_CONTENT:
                input_buffer_[bytes_received_++] = byte;
                
                if (bytes_received_ >= HEADER_SIZE + content_size_) {
                    // Content complete - validate content CRC
                    uint16_t expected_content_crc = read_u16_le(&input_buffer_[4]);
                    uint16_t actual_content_crc = crc16(&input_buffer_[HEADER_SIZE], content_size_);
                    
                    if (expected_content_crc == actual_content_crc) {
                        // Valid packet - execute callback
                        Packet pkt{reinterpret_cast<char*>(input_buffer_),
                            static_cast<uint16_t>(HEADER_SIZE + content_size_)};
                        if (on_packet_) {
                            on_packet_(pkt);
                        }
                    }
                    
                    reset();
                }
                break;
        }
    }
    
    // Forward declarations for little-endian helpers
    static uint16_t read_u16_le(const uint8_t* p) {
        return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
    }
    
    static void write_u16_le(uint8_t* p, uint16_t v) {
        p[0] = uint8_t(v & 0xFF);
        p[1] = uint8_t((v >> 8) & 0xFF);
    }
    
public:
    StreamFramer() : state_(State::SEARCH_MAGIC_1), bytes_received_(0), content_size_(0) {}
    
    // Set packet callback
    void setOnPacket(std::function<void(Packet&)> callback) {
        on_packet_ = callback;
    }
    
    // Write bytes to the framer (bulk processing with optimization)
    void writeBytes(const char* buffer, unsigned int length) {
        const uint8_t* data = reinterpret_cast<const uint8_t*>(buffer);
        unsigned int offset = 0;
        
        while (offset < length) {
            if (state_ == State::SEARCH_MAGIC_1 || state_ == State::SEARCH_MAGIC_2) {
                // Process byte by byte in search states
                processByte(data[offset++], 0);
            } else if (state_ == State::READING_HEADER) {
                // Bulk copy for header
                size_t bytes_needed = HEADER_SIZE - bytes_received_;
                size_t bytes_available = length - offset;
                size_t bytes_to_copy = (bytes_needed < bytes_available) ? bytes_needed : bytes_available;
                
                std::memcpy(&input_buffer_[bytes_received_], &data[offset], bytes_to_copy);
                bytes_received_ += bytes_to_copy;
                offset += bytes_to_copy;
                
                if (bytes_received_ >= HEADER_SIZE) {
                    // Header complete - parse and validate
                    content_size_ = read_u16_le(&input_buffer_[2]);
                    
                    // Validate content size
                    if (content_size_ > MAX_CONTENT_SIZE) {
                        // Invalid size - save bytes without magic and retry
                        uint8_t temp_buffer[8];
                        std::memcpy(temp_buffer, &input_buffer_[2], 8);
                        reset();
                        for (int i = 0; i < 8; i++) {
                            processByte(temp_buffer[i], 1);
                        }
                        continue;
                    }
                    
                    // Validate header CRC
                    uint16_t expected_header_crc = read_u16_le(&input_buffer_[8]);
                    uint16_t actual_header_crc = crc16(input_buffer_, 8);
                    
                    if (expected_header_crc == actual_header_crc) {
                        // Valid header
                        if (content_size_ == 0) {
                            // Empty content - process immediately
                            Packet pkt{reinterpret_cast<char*>(input_buffer_), HEADER_SIZE};
                            if (on_packet_) {
                                on_packet_(pkt);
                            }
                            reset();
                        } else {
                            state_ = State::READING_CONTENT;
                        }
                    } else {
                        // CRC mismatch - save bytes without magic and retry
                        uint8_t temp_buffer[8];
                        std::memcpy(temp_buffer, &input_buffer_[2], 8);
                        reset();
                        for (int i = 0; i < 8; i++) {
                            processByte(temp_buffer[i], 1);
                        }
                    }
                }
            } else if (state_ == State::READING_CONTENT) {
                // Bulk copy for content
                size_t bytes_needed = (HEADER_SIZE + content_size_) - bytes_received_;
                size_t bytes_available = length - offset;
                size_t bytes_to_copy = (bytes_needed < bytes_available) ? bytes_needed : bytes_available;
                
                std::memcpy(&input_buffer_[bytes_received_], &data[offset], bytes_to_copy);
                bytes_received_ += bytes_to_copy;
                offset += bytes_to_copy;
                
                if (bytes_received_ >= HEADER_SIZE + content_size_) {
                    // Content complete - validate content CRC
                    uint16_t expected_content_crc = read_u16_le(&input_buffer_[4]);
                    uint16_t actual_content_crc = crc16(&input_buffer_[HEADER_SIZE], content_size_);
                    
                    if (expected_content_crc == actual_content_crc) {
                        // Valid packet - execute callback
                        Packet pkt{reinterpret_cast<char*>(input_buffer_),
                            static_cast<uint16_t>(HEADER_SIZE + content_size_)};
                        if (on_packet_) {
                            on_packet_(pkt);
                        }
                    }
                    
                    reset();
                }
            }
        }
    }
    
    // Create packet (uses output buffer)
    Packet createPacket(const char* buffer, unsigned int length) {
        // Validate length
        if (length > MAX_CONTENT_SIZE) {
            return Packet{nullptr, 0};
        }
        
        // Write magic number
        output_buffer_[0] = MAGIC_BYTE_1;
        output_buffer_[1] = MAGIC_BYTE_2;
        
        // Write content size
        write_u16_le(&output_buffer_[2], static_cast<uint16_t>(length));
        
        // Calculate and write content CRC
        uint16_t content_crc = (length > 0) ? crc16(reinterpret_cast<const uint8_t*>(buffer), length) : 0xFFFF;
        write_u16_le(&output_buffer_[4], content_crc);
        
        // Write user data (reserved, set to 0)
        output_buffer_[6] = 0x00;
        output_buffer_[7] = 0x00;
        
        // Calculate and write header CRC
        uint16_t header_crc = crc16(output_buffer_, 8);
        write_u16_le(&output_buffer_[8], header_crc);
        
        // Copy content
        if (length > 0) {
            std::memcpy(&output_buffer_[HEADER_SIZE], buffer, length);
        }
        
        return Packet{reinterpret_cast<char*>(output_buffer_),
            static_cast<uint16_t>(HEADER_SIZE + length)};
    }
};

// ---------------------------------------------
// RpcByteArray - Variable-length byte array
// ---------------------------------------------
struct RpcByteArray {
    const char* content;
    uint16_t length;
    
    // Optional: For internal use to keep buffer alive
    std::shared_ptr<std::vector<uint8_t>> _buffer;
};

// ---------------------------------------------
// RpcFuture<T> - Custom future replacement for embedded systems
// Uses shared state so copies share the same underlying future
// ---------------------------------------------
template<typename T>
class RpcFuture {
private:
    struct SharedState {
        T value_;
        volatile bool value_set_;
        int error_code_;
        bool has_error_;
        std::function<void(const T&)> on_complete_;
        std::function<void(int)> on_error_;
#ifdef RPCMANAGER_MUTEX
        mutable std::mutex mutex_;
        std::condition_variable cv_;
#endif
        
        SharedState() : value_set_(false), error_code_(0), has_error_(false) {}
    };
    
    std::shared_ptr<SharedState> state_;
    
public:
    RpcFuture() : state_(std::make_shared<SharedState>()) {}
    
    // Copy constructor and assignment share state
    RpcFuture(const RpcFuture&) = default;
    RpcFuture& operator=(const RpcFuture&) = default;
    
    // Move constructor and assignment
    RpcFuture(RpcFuture&&) noexcept = default;
    RpcFuture& operator=(RpcFuture&&) noexcept = default;
    
    // Set the value (called by RpcManager or application)
    void setValue(const T& value) {
#ifdef RPCMANAGER_MUTEX
        std::unique_lock<std::mutex> lock(state_->mutex_);
        state_->value_ = value;
        state_->value_set_ = true;
        state_->cv_.notify_all();
        auto callback = state_->on_complete_;  // Copy callback before unlock
        lock.unlock();
        if (callback) {
            callback(value);
        }
#else
        state_->value_ = value;
        state_->value_set_ = true;
        if (state_->on_complete_) {
            state_->on_complete_(value);
        }
#endif
    }
    
    // Set error (called by RpcManager)
    void setError(int error_code) {
#ifdef RPCMANAGER_MUTEX
        std::unique_lock<std::mutex> lock(state_->mutex_);
        state_->error_code_ = error_code;
        state_->has_error_ = true;
        state_->value_set_ = true;  // Unblock waiters
        state_->cv_.notify_all();
        auto callback = state_->on_error_;  // Copy callback before unlock
        lock.unlock();
        if (callback) {
            callback(error_code);
        }
#else
        state_->error_code_ = error_code;
        state_->has_error_ = true;
        state_->value_set_ = true;
        if (state_->on_error_) {
            state_->on_error_(error_code);
        }
#endif
    }
    
    // Get the value (blocking until set)
    T get() {
#ifdef RPCMANAGER_MUTEX
        std::unique_lock<std::mutex> lock(state_->mutex_);
        state_->cv_.wait(lock, [this] { return state_->value_set_; });
        if (state_->has_error_) {
            return T{};  // Return default value on error
        }
        return state_->value_;
#else
        // Busy wait
        while (!state_->value_set_) {
            // Spin
        }
        if (state_->has_error_) {
            return T{};
        }
        return state_->value_;
#endif
    }
    
    // Get with timeout (returns default value on timeout)
    T get(uint32_t timeout_ms) {
#ifdef RPCMANAGER_MUTEX
        std::unique_lock<std::mutex> lock(state_->mutex_);
        if (state_->cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this] { return state_->value_set_; })) {
            if (state_->has_error_) {
                return T{};
            }
            return state_->value_;
        }
        // Timeout
        return T{};
#else
        // Busy wait with timeout
        auto start = std::chrono::steady_clock::now();
        while (!state_->value_set_) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                                                                 std::chrono::steady_clock::now() - start).count();
            if (elapsed >= timeout_ms) {
                return T{};  // Timeout
            }
        }
        if (state_->has_error_) {
            return T{};
        }
        return state_->value_;
#endif
    }
    
    // Check if value is ready
    bool isReady() const {
#ifdef RPCMANAGER_MUTEX
        std::lock_guard<std::mutex> lock(state_->mutex_);
        return state_->value_set_;
#else
        return state_->value_set_;
#endif
    }
    
    // Get error code
    int errorCode() const {
#ifdef RPCMANAGER_MUTEX
        std::lock_guard<std::mutex> lock(state_->mutex_);
        return state_->error_code_;
#else
        return state_->error_code_;
#endif
    }
    
    // Set callback for completion
    void onComplete(std::function<void(const T&)> callback) {
#ifdef RPCMANAGER_MUTEX
        std::lock_guard<std::mutex> lock(state_->mutex_);
#endif
        state_->on_complete_ = callback;
        // If already completed, call immediately
        if (state_->value_set_ && !state_->has_error_ && state_->on_complete_) {
            state_->on_complete_(state_->value_);
        }
    }
    
    // Set callback for error
    void onError(std::function<void(int)> callback) {
#ifdef RPCMANAGER_MUTEX
        std::lock_guard<std::mutex> lock(state_->mutex_);
#endif
        state_->on_error_ = callback;
        // If already has error, call immediately
        if (state_->has_error_ && state_->on_error_) {
            state_->on_error_(state_->error_code_);
        }
    }
};

// ---------------------------------------------
// RpcCallContext - Optional context for per-call settings
// ---------------------------------------------
struct RpcCallContext {
    uint32_t timeout_ms = 0;  // 0 = use configured timeout
    
    static RpcCallContext withTimeout(uint32_t ms) {
        RpcCallContext ctx;
        ctx.timeout_ms = ms;
        return ctx;
    }
};

// ---------------------------------------------
// DataFilter - Abstract class for filtering data
// ---------------------------------------------
class DataFilter {
public:
    virtual ~DataFilter() = default;
    
    // Process data through filter
    // Returns filtered data, or empty array if data should be dropped
    virtual RpcByteArray onData(const RpcByteArray& data) = 0;
};

constexpr size_t STREAM_HEADER_SIZE = 10;
// ---------------------------------------------
// StreamFramerInputFilter - Input filter using StreamFramer
// ---------------------------------------------
class StreamFramerInputFilter : public DataFilter {
private:
    StreamFramer framer_;
    std::vector<uint8_t> packet_buffer_;
    bool packet_ready_;
    
public:
    StreamFramerInputFilter() : packet_ready_(false) {
        framer_.setOnPacket([this](Packet& pkt) {
            // StreamFramer header is 10 bytes (HEADER_SIZE)
            // Strip it before passing to RPC layer
           
            if (pkt.length > STREAM_HEADER_SIZE) {
                packet_buffer_.assign(
                                      reinterpret_cast<const uint8_t*>(pkt.data) + STREAM_HEADER_SIZE,
                                      reinterpret_cast<const uint8_t*>(pkt.data) + pkt.length
                                      );
                packet_ready_ = true;
            }
        });
    }
    
    // Process incoming data through framer
    RpcByteArray onData(const RpcByteArray& data) override {
        packet_ready_ = false;
        
        // Feed data to framer
        framer_.writeBytes(data.content, data.length);
        
        // If a packet was assembled, return it (with StreamFramer header stripped)
        if (packet_ready_) {
            return RpcByteArray{
                reinterpret_cast<const char*>(packet_buffer_.data()),
                static_cast<uint16_t>(packet_buffer_.size())
            };
        }
        
        // No complete packet yet, return empty
        return RpcByteArray{nullptr, 0};
    }
};

// ---------------------------------------------
// StreamFramerOutputFilter - Output filter using StreamFramer
// ---------------------------------------------
class StreamFramerOutputFilter : public DataFilter {
private:
    StreamFramer framer_;
    std::vector<uint8_t> packet_buffer_;
    
public:
    StreamFramerOutputFilter() {}
    // Wrap outgoing data into framed packet
    RpcByteArray onData(const RpcByteArray& data) override {
        // Create framed packet
        Packet pkt = framer_.createPacket(data.content, data.length);
        // Store in buffer to keep it alive
        packet_buffer_.assign(
                              reinterpret_cast<const uint8_t*>(pkt.data),
                              reinterpret_cast<const uint8_t*>(pkt.data) + pkt.length
                              );
        return RpcByteArray{
            reinterpret_cast<const char*>(packet_buffer_.data()),
            static_cast<uint16_t>(packet_buffer_.size())
        };
    }
};

// ---------------------------------------------
// Little-endian helpers
// ---------------------------------------------
inline uint16_t read_u16_le(const uint8_t* p) {
    return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
}

inline uint32_t read_u32_le(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

inline void write_u16_le(uint8_t* p, uint16_t v) {
    p[0] = uint8_t(v & 0xFF);
    p[1] = uint8_t((v >> 8) & 0xFF);
}

inline void write_u32_le(uint8_t* p, uint32_t v) {
    p[0] = uint8_t(v & 0xFF);
    p[1] = uint8_t((v >> 8) & 0xFF);
    p[2] = uint8_t((v >> 16) & 0xFF);
    p[3] = uint8_t((v >> 24) & 0xFF);
}

// ---------------------------------------------
// CRC16 (IBM polynomial 0xA001)
// ---------------------------------------------
static const uint16_t CRC16_TABLE[256] = {
    0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
    0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
    0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
    0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
    0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
    0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
    0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
    0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
    0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
    0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
    0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
    0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
    0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
    0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
    0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
    0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
    0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
    0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
    0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
    0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
    0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
    0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
    0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
    0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
    0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
    0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
    0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
    0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
    0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
    0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
    0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
    0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
};

inline uint16_t crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        uint8_t idx = (crc ^ data[i]) & 0xFF;
        crc = (crc >> 8) ^ CRC16_TABLE[idx];
    }
    return crc;
}

// ---------------------------------------------
// Type traits
// ---------------------------------------------
template<typename T>
struct is_rpc_future : std::false_type {};

template<typename T>
struct is_rpc_future<RpcFuture<T>> : std::true_type {
    using value_type = T;
};

template<typename T>
inline constexpr bool is_rpc_future_v = is_rpc_future<T>::value;

template<typename T>
struct is_byte_array : std::false_type {};

template<>
struct is_byte_array<RpcByteArray> : std::true_type {};

template<typename T>
inline constexpr bool is_byte_array_v = is_byte_array<T>::value;

#ifdef RPCMANAGER_STD_STRING
template<typename T>
struct is_std_string : std::false_type {};

template<>
struct is_std_string<std::string> : std::true_type {};

template<typename T>
inline constexpr bool is_std_string_v = is_std_string<T>::value;
#endif

// Extract function signature from std::function
template<typename T>
struct function_traits;

template<typename R, typename... Args>
struct function_traits<std::function<R(Args...)>> {
    using return_type = R;
    using args_tuple = std::tuple<Args...>;
    static constexpr size_t arity = sizeof...(Args);
};

// ---------------------------------------------
// Serialization
// ---------------------------------------------
template<typename T>
inline size_t serialize_value(uint8_t* buf, const T& value) {
    static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");
    std::memcpy(buf, &value, sizeof(T));
    return sizeof(T);
}

template<typename T>
inline T deserialize_value(const uint8_t* buf) {
    static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");
    T value;
    std::memcpy(&value, buf, sizeof(T));
    return value;
}

// Helper: get size of single argument
template<typename T>
inline size_t get_arg_size(const T& value) {
    if constexpr (is_byte_array_v<T>) {
        return value.length;
#ifdef RPCMANAGER_STD_STRING
    } else if constexpr (is_std_string_v<T>) {
        return value.size();
#endif
    } else {
        static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");
        return sizeof(T);
    }
}

// Serialize tuple of arguments with length table
template<typename Tuple, size_t... I>
inline size_t serialize_args_impl(uint8_t* buf, const Tuple& args, std::index_sequence<I...>) {
    constexpr size_t N = sizeof...(I);
    if constexpr (N == 0) {
        return 0;
    }
    
    size_t offset = 0;
    
    // Write length table
    ((write_u16_le(buf + offset, get_arg_size(std::get<I>(args))), offset += 2), ...);
    
    // Write argument data
    (([&]() {
        const auto& arg = std::get<I>(args);
        using ArgType = std::remove_cv_t<std::remove_reference_t<decltype(arg)>>;
        if constexpr (is_byte_array_v<ArgType>) {
            std::memcpy(buf + offset, arg.content, arg.length);
            offset += arg.length;
#ifdef RPCMANAGER_STD_STRING
        } else if constexpr (is_std_string_v<ArgType>) {
            std::memcpy(buf + offset, arg.data(), arg.size());
            offset += arg.size();
#endif
        } else {
            std::memcpy(buf + offset, &arg, sizeof(arg));
            offset += sizeof(arg);
        }
    }()), ...);
    
    return offset;
}

template<typename... Args>
inline size_t serialize_args(uint8_t* buf, const Args&... args) {
    if constexpr (sizeof...(Args) == 0) {
        return 0;
    } else {
        auto tuple = std::make_tuple(args...);
        return serialize_args_impl(buf, tuple, std::index_sequence_for<Args...>{});
    }
}

// Deserialize tuple of arguments with length table
template<typename Tuple, size_t... I>
inline Tuple deserialize_args_impl(const uint8_t* buf, std::index_sequence<I...>) {
    constexpr size_t N = sizeof...(I);
    if constexpr (N == 0) {
        return Tuple{};
    }
    
    // Read length table
    uint16_t lengths[N];
    for (size_t i = 0; i < N; i++) {
        lengths[i] = read_u16_le(buf + i * 2);
    }
    
    // Data starts after length table
    size_t data_offset = N * 2;
    size_t arg_offset = 0;
    
    return std::make_tuple(
                           [&]() {
                               using T = std::tuple_element_t<I, Tuple>;
                               T val;
                               
                               if constexpr (is_byte_array_v<T>) {
                                   // Point to data in buffer (valid until handler completes)
                                   val.content = reinterpret_cast<const char*>(buf + data_offset + arg_offset);
                                   val.length = lengths[I];
#ifdef RPCMANAGER_STD_STRING
                               } else if constexpr (is_std_string_v<T>) {
                                   // Construct string from buffer data
                                   val = std::string(reinterpret_cast<const char*>(buf + data_offset + arg_offset), lengths[I]);
#endif
                               } else {
                                   std::memcpy(&val, buf + data_offset + arg_offset, sizeof(T));
                               }
                               
                               arg_offset += lengths[I];
                               return val;
                           }()...
                           );
}

template<typename Tuple>
inline Tuple deserialize_args(const uint8_t* buf) {
    return deserialize_args_impl<Tuple>(buf, std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

// Calculate payload size including length table
template<typename... Args>
inline size_t calc_payload_size(const Args&... args) {
    constexpr size_t N = sizeof...(Args);
    if constexpr (N == 0) {
        return 0;
    }
    size_t table_size = N * 2;  // 2 bytes per argument for length
    size_t data_size = (get_arg_size(args) + ... + 0);
    return table_size + data_size;
}

// ---------------------------------------------
// RpcManager - Main class
// ---------------------------------------------
template<size_t BUF_SIZE = 10240>
class RpcManager {
public:
    using SendCallback = std::function<void(const char*, int)>;
    using ErrorCallback = std::function<void(int, const char*)>;
    using PacketHookCallback = std::function<bool(const RpcPacket*)>;
    
    RpcManager() : next_call_id_(1), send_cb_(nullptr), error_cb_(nullptr),
    default_timeout_ms_(0), send_packet_hook_(nullptr), receive_packet_hook_(nullptr) {}  // 0 = infinite timeout
    
    // Set transport callback
    void setOnSendCallback(SendCallback cb) {
        send_cb_ = std::move(cb);
    }
    
    // Set error callback
    void onError(ErrorCallback cb) {
        error_cb_ = std::move(cb);
    }
    
    // Set packet hooks (return true to continue, false to skip packet)
    void onSendPacketHook(PacketHookCallback cb) {
        send_packet_hook_ = std::move(cb);
    }
    
    void onReceivePacketHook(PacketHookCallback cb) {
        receive_packet_hook_ = std::move(cb);
    }
    
    // Add input filter
    void addInputFilter(DataFilter* filter) {
        if (filter) {
            input_filters_.push_back(filter);
        }
    }
    
    // Add output filter
    void addOutputFilter(DataFilter* filter) {
        if (filter) {
            output_filters_.push_back(filter);
        }
    }
    
    // Clear all filters
    void clearFilters() {
        input_filters_.clear();
        output_filters_.clear();
    }
    
    // Set global default timeout for all RPC calls (0 = infinite)
    void setDefaultTimeout(uint32_t timeout_ms) {
        RPC_LOCK();
        default_timeout_ms_ = timeout_ms;
    }
    
    // Set timeout for specific provider/method (0 = use default, infinite if default is 0)
    template<typename Provider>
    void setMethodTimeout(uint16_t methodIndex, uint32_t timeout_ms) {
        setMethodTimeoutImpl(Provider::providerId, methodIndex, timeout_ms);
    }
    
    template<typename Provider>
    void setMethodTimeout(uint16_t providerId, uint16_t methodIndex, uint32_t timeout_ms) {
        setMethodTimeoutImpl(providerId, methodIndex, timeout_ms);
    }
    
    // Clear timeout for specific method (revert to default)
    template<typename Provider>
    void clearMethodTimeout(uint16_t methodIndex) {
        RPC_LOCK();
        method_timeouts_.erase(makeKey(Provider::providerId, methodIndex));
    }
    
    // Process input data (assumes complete packet)
    void processInput(const char* data, int length) {
        // Apply input filters
        RpcByteArray filtered_data{data, static_cast<uint16_t>(length)};
        
        for (auto* filter : input_filters_) {
            filtered_data = filter->onData(filtered_data);
            if (filtered_data.content == nullptr || filtered_data.length == 0) {
                // Data was dropped by filter
                return;
            }
        }
        
        // Call receive packet hook after applying filters
        if (receive_packet_hook_) {
            const RpcPacket* packet_ptr = reinterpret_cast<const RpcPacket*>(filtered_data.content);
            if (!receive_packet_hook_(packet_ptr)) {
                // Hook returned false - skip processing this packet
                return;
            }
        }
        
        // Process as RPC packet
        parsePacket(reinterpret_cast<const uint8_t*>(filtered_data.content), filtered_data.length);
    }
    
    // Register server provider
    template<typename Provider>
    void registerServer(Provider& provider) {
        registerServerImpl(provider, Provider::providerId);
    }
    
    template<typename Provider>
    void registerServer(Provider& provider, uint16_t customProviderId) {
        registerServerImpl(provider, customProviderId);
    }
    
    // Deregister server provider
    template<typename Provider>
    void deregisterServer() {
        deregisterServerImpl<Provider>(Provider::providerId);
    }
    
    template<typename Provider>
    void deregisterServer(uint16_t customProviderId) {
        deregisterServerImpl<Provider>(customProviderId);
    }
    
    // Create client provider
    template<typename Provider>
    Provider createClient() {
        return createClientImpl<Provider>(Provider::providerId);
    }
    
    template<typename Provider>
    Provider createClient(uint16_t customProviderId) {
        return createClientImpl<Provider>(customProviderId);
    }
    
private:
    // Internal: register server
    template<typename Provider>
    void registerServerImpl(Provider& provider, uint16_t providerId) {
        constexpr size_t numMethods = std::tuple_size_v<decltype(Provider::methods)>;
        registerMethods<Provider>(provider, providerId, std::make_index_sequence<numMethods>{});
    }
    
    template<typename Provider, size_t... I>
    void registerMethods(Provider& provider, uint16_t providerId, std::index_sequence<I...>) {
        (registerMethod<Provider, I>(provider, providerId), ...);
    }
    
    // Internal: deregister server
    template<typename Provider>
    void deregisterServerImpl(uint16_t providerId) {
        constexpr size_t numMethods = std::tuple_size_v<decltype(Provider::methods)>;
        deregisterMethods<Provider>(providerId, std::make_index_sequence<numMethods>{});
    }
    
    template<typename Provider, size_t... I>
    void deregisterMethods(uint16_t providerId, std::index_sequence<I...>) {
        RPC_LOCK();
        (server_handlers_.erase(makeKey(providerId, I)), ...);
    }
    
    template<typename Provider, size_t MethodIndex>
    void registerMethod(Provider& provider, uint16_t providerId) {
        auto methodPtr = std::get<MethodIndex>(Provider::methods);
        auto& func = provider.*methodPtr;
        
        using FuncType = std::remove_reference_t<decltype(func)>;
        using Traits = function_traits<FuncType>;
        using RetType = typename Traits::return_type;
        using ArgsTuple = typename Traits::args_tuple;
        
        uint32_t key = makeKey(providerId, MethodIndex);
        
        RPC_LOCK();
        server_handlers_[key] = [this, &func, providerId](uint32_t callId, const uint8_t* payload, size_t len) {
            // Deserialize arguments
            ArgsTuple args = deserialize_args<ArgsTuple>(payload);
            
            // Call method and handle response
            if constexpr (std::is_void_v<RetType>) {
                // Void - fire and forget
                std::apply(func, args);
            } else if constexpr (is_rpc_future_v<RetType>) {
                // RpcFuture - register callback to send reply when ready
                // NOTE: User must ensure the RpcFuture gets resolved (setValue called) in their application
                // For Raspberry Pi Pico: call processInput from interrupt handler, resolve futures in main loop
                using FutureValueType = typename is_rpc_future<RetType>::value_type;
                RetType future = std::apply(func, args);
                
                // Store future and set up callback to send reply when resolved
                // Use shared_ptr to keep future alive across callback invocations
                auto future_ptr = std::make_shared<RetType>(std::move(future));
                constexpr uint16_t method_idx = MethodIndex;
                
                // Store in pending map so user can access if needed
                {
                    RPC_LOCK();
                    pending_server_futures_[callId] = future_ptr;
                }
                
                future_ptr->onComplete([this, providerId, method_idx, callId](const FutureValueType& result) {
                    sendReply(providerId, method_idx, callId, result);
                    // Clean up
                    {
                        RPC_LOCK();
                        pending_server_futures_.erase(callId);
                    }
                });
            } else {
                // Regular return - send reply
                RetType result = std::apply(func, args);
                sendReply(providerId, MethodIndex, callId, result);
            }
        };
    }
    
    // Internal: create client
    template<typename Provider>
    Provider createClientImpl(uint16_t providerId) {
        Provider client;
        constexpr size_t numMethods = std::tuple_size_v<decltype(Provider::methods)>;
        setupClientMethods<Provider>(client, providerId, std::make_index_sequence<numMethods>{});
        return client;
    }
    
    template<typename Provider, size_t... I>
    void setupClientMethods(Provider& client, uint16_t providerId, std::index_sequence<I...>) {
        (setupClientMethod<Provider, I>(client, providerId), ...);
    }
    
    template<typename Provider, size_t MethodIndex>
    void setupClientMethod(Provider& client, uint16_t providerId) {
        auto methodPtr = std::get<MethodIndex>(Provider::methods);
        auto& func = client.*methodPtr;
        
        using FuncType = std::remove_reference_t<decltype(func)>;
        using Traits = function_traits<FuncType>;
        using RetType = typename Traits::return_type;
        using ArgsTuple = typename Traits::args_tuple;
        
        func = [this, providerId](auto&&... args) -> RetType {
            return invokeRemote<RetType, decltype(args)...>(
                                                            providerId, MethodIndex, std::forward<decltype(args)>(args)...
                                                            );
        };
    }
    
    // Invoke remote method
    template<typename RetType, typename... Args>
    RetType invokeRemote(uint16_t providerId, uint16_t methodId, Args&&... args) {
        // Check payload size
        size_t payloadSize = calc_payload_size(args...);
        if (payloadSize > BUF_SIZE - RPC_HEADER_SIZE) {
            error(RPC_ERROR_ARGS_TOO_LARGE, "Arguments too large");
            if constexpr (std::is_void_v<RetType>) {
                return;
            } else {
                return RetType{};
            }
        }
        
        // Serialize arguments
        uint8_t payload[BUF_SIZE];
        size_t payloadLen = serialize_args(payload, std::forward<Args>(args)...);
        
        if constexpr (std::is_void_v<RetType>) {
            // Void - fire and forget (still use call ID)
            uint32_t callId = generateCallId();
            sendPacket(providerId, methodId, callId, 0, payload, payloadLen);
        } else if constexpr (is_rpc_future_v<RetType>) {
            // Async - return RpcFuture
            using FutureValueType = typename is_rpc_future<RetType>::value_type;
            uint32_t callId = generateCallId();
            
            auto future = std::make_shared<RpcFuture<FutureValueType>>();
            
            {
                RPC_LOCK();
                pending_futures_[callId] = [future](uint32_t received_call_id, const uint8_t* data, size_t len) {
                    if constexpr (is_byte_array_v<FutureValueType>) {
                        // Read length from payload
                        uint16_t length = read_u16_le(data);
                        // Allocate persistent buffer and keep it alive in the struct
                        auto buffer = std::make_shared<std::vector<uint8_t>>(data + 2, data + 2 + length);
                        FutureValueType value;
                        value.content = reinterpret_cast<const char*>(buffer->data());
                        value.length = length;
                        value._buffer = buffer;  // Keep buffer alive
                        future->setValue(value);
#ifdef RPCMANAGER_STD_STRING
                    } else if constexpr (is_std_string_v<FutureValueType>) {
                        // Read length from payload and construct string
                        uint16_t length = read_u16_le(data);
                        FutureValueType value(reinterpret_cast<const char*>(data + 2), length);
                        future->setValue(std::move(value));
#endif
                    } else {
                        FutureValueType value = deserialize_value<FutureValueType>(data);
                        future->setValue(std::move(value));
                    }
                };
            }
            
            sendPacket(providerId, methodId, callId, 0, payload, payloadLen);
            return *future;  // Return copy of RpcFuture (shares state via shared_ptr)
        } else {
            // Synchronous - block and wait (with optional timeout)
            uint32_t callId = generateCallId();
            
            auto future = std::make_shared<RpcFuture<RetType>>();
            
            {
                RPC_LOCK();
                pending_futures_[callId] = [future](uint32_t received_call_id, const uint8_t* data, size_t len) {
                    if constexpr (is_byte_array_v<RetType>) {
                        // Read length from payload
                        uint16_t length = read_u16_le(data);
                        // Allocate persistent buffer and keep it alive in the struct
                        auto buffer = std::make_shared<std::vector<uint8_t>>(data + 2, data + 2 + length);
                        RetType value;
                        value.content = reinterpret_cast<const char*>(buffer->data());
                        value.length = length;
                        value._buffer = buffer;  // Keep buffer alive
                        future->setValue(value);
#ifdef RPCMANAGER_STD_STRING
                    } else if constexpr (is_std_string_v<RetType>) {
                        // Read length from payload and construct string
                        uint16_t length = read_u16_le(data);
                        RetType value(reinterpret_cast<const char*>(data + 2), length);
                        future->setValue(std::move(value));
#endif
                    } else {
                        RetType value = deserialize_value<RetType>(data);
                        future->setValue(std::move(value));
                    }
                };
            }
            
            sendPacket(providerId, methodId, callId, 0, payload, payloadLen);
            
            // Apply timeout: use configured timeout
            uint32_t timeout_ms = getEffectiveTimeout(providerId, methodId, 0);
            
            if (timeout_ms > 0) {
                // Wait with timeout
                RetType result = future->get(timeout_ms);
                if (!future->isReady()) {
                    // Timeout - cleanup pending call
                    {
                        RPC_LOCK();
                        pending_futures_.erase(callId);
                    }
                    error(RPC_ERROR_TIMEOUT, "RPC call timeout");
                    return RetType{};  // Return default-constructed value on timeout
                }
                return result;
            } else {
                // Infinite wait
                return future->get();
            }
        }
    }
    
    // Send packet
    void sendPacket(uint16_t providerId, uint16_t methodId, uint32_t callId,
                    uint8_t flags, const uint8_t* payload, size_t payloadLen) {
        if (payloadLen > BUF_SIZE - RPC_HEADER_SIZE) {
            error(RPC_ERROR_PAYLOAD_TOO_LARGE, "Payload too large");
            return;
        }
        
        uint8_t packet[BUF_SIZE];
        size_t pos = 0;
        
        // Header (no CRC - handled by StreamFramer if needed)
        write_u16_le(packet + pos, RPC_MAGIC); pos += 2;
        uint16_t totalLen = RPC_HEADER_SIZE + payloadLen;
        write_u16_le(packet + pos, totalLen); pos += 2;
        
        // Provider ID, Method ID, Flags, Call ID
        write_u16_le(packet + pos, providerId); pos += 2;
        write_u16_le(packet + pos, methodId); pos += 2;
        packet[pos++] = flags;
        write_u32_le(packet + pos, callId); pos += 4;
        
        // Payload
        if (payloadLen > 0) {
            std::memcpy(packet + pos, payload, payloadLen);
            pos += payloadLen;
        }
        
        // Call send packet hook before applying filters
        if (send_packet_hook_) {
            const RpcPacket* packet_ptr = reinterpret_cast<const RpcPacket*>(packet);
            if (!send_packet_hook_(packet_ptr)) {
                // Hook returned false - skip sending this packet
                return;
            }
        }
        
        // Apply output filters
        RpcByteArray filtered_data{reinterpret_cast<const char*>(packet), static_cast<uint16_t>(pos)};
        
        for (auto* filter : output_filters_) {
            filtered_data = filter->onData(filtered_data);
            if (filtered_data.content == nullptr || filtered_data.length == 0) {
                // Data was dropped by filter
                return;
            }
        }
        
        // Send via callback
        if (send_cb_) {
            send_cb_(filtered_data.content, static_cast<int>(filtered_data.length));
        }
    }
    
    // Send reply
    template<typename T>
    void sendReply(uint16_t providerId, uint16_t methodId, uint32_t callId, const T& result) {
        uint8_t payload[BUF_SIZE];
        size_t payloadLen;
        
        if constexpr (is_byte_array_v<T>) {
            // For byte array: write length then data
            write_u16_le(payload, result.length);
            std::memcpy(payload + 2, result.content, result.length);
            payloadLen = 2 + result.length;
#ifdef RPCMANAGER_STD_STRING
        } else if constexpr (is_std_string_v<T>) {
            // For string: write length then data (same as byte array)
            uint16_t length = static_cast<uint16_t>(result.size());
            write_u16_le(payload, length);
            std::memcpy(payload + 2, result.data(), result.size());
            payloadLen = 2 + result.size();
#endif
        } else {
            payloadLen = serialize_value(payload, result);
        }
        
        sendPacket(providerId, methodId, callId, RPC_FLAG_REPLY, payload, payloadLen);
    }
    
    // Parse packet (assumes complete, valid packet from filters)
    void parsePacket(const uint8_t* data, size_t length) {
        // Assume packet is valid (filters handle validation)
        if (length < RPC_HEADER_SIZE) {
            error(RPC_ERROR_INVALID_PACKET_LENGTH, "Invalid packet length");
            return;
        }
        
        // Cast data to RpcPacket struct for easy field access
        const RpcPacket* packet = reinterpret_cast<const RpcPacket*>(data);
        
        // Extract fields from struct (they are in little-endian format)
        uint16_t providerId = read_u16_le(reinterpret_cast<const uint8_t*>(&packet->providerId));
        uint16_t methodId = read_u16_le(reinterpret_cast<const uint8_t*>(&packet->methodId));
        uint8_t flags = packet->flags;
        uint32_t callId = read_u32_le(reinterpret_cast<const uint8_t*>(&packet->callId));
        uint16_t totalLen = read_u16_le(reinterpret_cast<const uint8_t*>(&packet->length));
        
        size_t payloadLen = totalLen - RPC_HEADER_SIZE;
        const uint8_t* payload = data + RPC_HEADER_SIZE;
        
        // Dispatch
        if (flags & RPC_FLAG_REPLY) {
            handleReply(callId, payload, payloadLen);
        } else {
            handleRequest(providerId, methodId, callId, payload, payloadLen);
        }
    }
    
    // Handle request
    void handleRequest(uint16_t providerId, uint16_t methodId, uint32_t callId,
                       const uint8_t* payload, size_t len) {
        uint32_t key = makeKey(providerId, methodId);
        
        std::function<void(uint32_t, const uint8_t*, size_t)> handler;
        {
            RPC_LOCK();
            auto it = server_handlers_.find(key);
            if (it != server_handlers_.end()) {
                handler = it->second;  // Copy handler while holding lock
            }
        }  // Release lock before calling handler
        
        if (handler) {
            handler(callId, payload, len);
        } else {
            error(RPC_ERROR_NO_HANDLER, "No handler for provider/method");
        }
    }
    
    // Handle reply
    void handleReply(uint32_t callId, const uint8_t* payload, size_t len) {
        RPC_LOCK();
        auto it = pending_futures_.find(callId);
        if (it != pending_futures_.end()) {
            it->second(callId, payload, len);
            pending_futures_.erase(it);
        } else {
            // Call ID not found - might be from previous invocation or duplicate
            error(RPC_ERROR_UNEXPECTED_CALL_ID, "Unexpected call ID in reply");
        }
    }
    
    // Utility
    uint32_t generateCallId() {
        RPC_LOCK();
        return next_call_id_++;
    }
    
    uint32_t makeKey(uint16_t providerId, uint16_t methodId) const {
        return (uint32_t(providerId) << 16) | uint32_t(methodId);
    }
    
    void error(int code, const char* msg) {
        if (error_cb_) {
            error_cb_(code, msg);
        }
    }
    
    // Internal: set method timeout
    void setMethodTimeoutImpl(uint16_t providerId, uint16_t methodId, uint32_t timeout_ms) {
        RPC_LOCK();
        method_timeouts_[makeKey(providerId, methodId)] = timeout_ms;
    }
    
    // Internal: get effective timeout for a method (per-method > global)
    uint32_t getEffectiveTimeout(uint16_t providerId, uint16_t methodId, uint32_t) const {
        // Check per-method timeout
        uint32_t key = makeKey(providerId, methodId);
        auto it = method_timeouts_.find(key);
        if (it != method_timeouts_.end() && it->second > 0) {
            return it->second;
        }
        
        // Fall back to global default (0 = infinite)
        return default_timeout_ms_;
    }
    
    // State
    uint32_t next_call_id_;
    
    SendCallback send_cb_;
    ErrorCallback error_cb_;
    PacketHookCallback send_packet_hook_;
    PacketHookCallback receive_packet_hook_;
    
    uint32_t default_timeout_ms_;  // Global default timeout (0 = infinite)
    std::unordered_map<uint32_t, uint32_t> method_timeouts_;  // Per-method timeouts
    
    // Server handlers: key = (providerId << 16 | methodId)
    std::unordered_map<uint32_t, std::function<void(uint32_t, const uint8_t*, size_t)>> server_handlers_;
    
    // Pending client calls with call ID validation
    std::unordered_map<uint32_t, std::function<void(uint32_t, const uint8_t*, size_t)>> pending_futures_;
    
    // Pending server-side async responses (stored as type-erased shared_ptr)
    std::unordered_map<uint32_t, std::shared_ptr<void>> pending_server_futures_;
    
    // Data filters
    std::vector<DataFilter*> input_filters_;
    std::vector<DataFilter*> output_filters_;
    
#ifdef RPCMANAGER_MUTEX
    std::mutex mutex_;
#endif
};
}
