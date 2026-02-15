#include "PersistentStorage.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include <cstring>
#include <sstream>
#include <vector>

PersistentStorage::PersistentStorage() : dirty(false) {
}

std::string PersistentStorage::escape(const std::string& str) {
    std::string result;
    result.reserve(str.size() * 2);  // Reserve space for worst case

    for (char c : str) {
        if (c == '`') {
            result += "``";  // Backtick → double backtick
        } else if (c == '|') {
            result += "`|";  // Pipe → backtick + pipe
        } else {
            result += c;
        }
    }

    return result;
}

std::string PersistentStorage::unescape(const std::string& str) {
    std::string result;
    result.reserve(str.size());

    bool escapeNext = false;
    for (char c : str) {
        if (escapeNext) {
            // After backtick, take the next character literally
            result += c;
            escapeNext = false;
        } else if (c == '`') {
            // Backtick is escape character
            escapeNext = true;
        } else {
            result += c;
        }
    }

    return result;
}

std::string PersistentStorage::serialize() const {
    std::ostringstream oss;
    bool first = true;

    for (const auto& pair : data) {
        if (!first) {
            oss << '|';  // Separator between key-value pairs
        }
        first = false;

        oss << escape(pair.first) << '|' << escape(pair.second);
    }

    return oss.str();
}

bool PersistentStorage::deserialize(const std::string& serialized) {
    data.clear();

    if (serialized.empty()) {
        return true;  // Empty data is valid
    }

    // Parse key|value|key|value...
    std::vector<std::string> tokens;
    std::string current;
    bool escapeNext = false;

    for (char c : serialized) {
        if (escapeNext) {
            current += c;
            escapeNext = false;
        } else if (c == '`') {
            escapeNext = true;
            current += c;  // Keep backtick for unescape
        } else if (c == '|' && !escapeNext) {
            // Unescaped pipe is a delimiter
            tokens.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }

    // Add last token
    if (!current.empty() || !tokens.empty()) {
        tokens.push_back(current);
    }

    // Tokens should be in pairs (key, value, key, value...)
    if (tokens.size() % 2 != 0) {
        return false;  // Invalid format
    }

    // Unescape and populate map
    for (size_t i = 0; i < tokens.size(); i += 2) {
        std::string key = unescape(tokens[i]);
        std::string value = unescape(tokens[i + 1]);
        data[key] = value;
    }

    return true;
}

bool PersistentStorage::load() {
    // Read flash memory
    const uint8_t* flash_ptr = reinterpret_cast<const uint8_t*>(XIP_BASE + FLASH_TARGET_OFFSET);

    // Check magic number
    uint32_t magic;
    memcpy(&magic, flash_ptr, sizeof(magic));
    if (magic != MAGIC_NUMBER) {
        // Flash is empty or corrupted, start with empty data
        data.clear();
        dirty = false;
        return false;
    }

    // Read data length
    uint32_t dataLength;
    memcpy(&dataLength, flash_ptr + 4, sizeof(dataLength));

    if (dataLength == 0 || dataLength > MAX_DATA_SIZE) {
        // Invalid length
        data.clear();
        dirty = false;
        return false;
    }

    // Read serialized data
    std::string serialized;
    serialized.resize(dataLength);
    memcpy(&serialized[0], flash_ptr + 8, dataLength);

    // Deserialize
    if (!deserialize(serialized)) {
        data.clear();
        dirty = false;
        return false;
    }

    dirty = false;
    return true;
}

bool PersistentStorage::flush() {
    if (!dirty) {
        return true;  // Nothing to write
    }

    // Serialize data
    std::string serialized = serialize();

    if (serialized.size() > MAX_DATA_SIZE) {
        return false;  // Data too large
    }

    // Prepare buffer (must be aligned to 256 bytes for flash write)
    uint8_t buffer[FLASH_SECTOR_SIZE];
    memset(buffer, 0xFF, FLASH_SECTOR_SIZE);  // Flash erased state is 0xFF

    // Write magic number
    uint32_t magic = MAGIC_NUMBER;
    memcpy(buffer, &magic, sizeof(magic));

    // Write data length
    uint32_t dataLength = serialized.size();
    memcpy(buffer + 4, &dataLength, sizeof(dataLength));

    // Write serialized data
    if (!serialized.empty()) {
        memcpy(buffer + 8, serialized.c_str(), serialized.size());
    }

    // Disable interrupts during flash operations
    uint32_t ints = save_and_disable_interrupts();

    // Erase sector (required before writing)
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);

    // Write data (must be in 256-byte pages)
    flash_range_program(FLASH_TARGET_OFFSET, buffer, FLASH_SECTOR_SIZE);

    // Restore interrupts
    restore_interrupts(ints);

    dirty = false;
    return true;
}

std::string PersistentStorage::get(const std::string& key) const {
    auto it = data.find(key);
    if (it != data.end()) {
        return it->second;
    }
    return "";
}

void PersistentStorage::put(const std::string& key, const std::string& value) {
    data[key] = value;
    dirty = true;
}

bool PersistentStorage::has(const std::string& key) const {
    return data.find(key) != data.end();
}

void PersistentStorage::remove(const std::string& key) {
    if (data.erase(key) > 0) {
        dirty = true;
    }
}

int PersistentStorage::getInt(const std::string& key, int defaultValue) const {
    std::string value = get(key);
    if (value.empty()) {
        return defaultValue;
    }

    // Manual integer parsing (exceptions disabled)
    const char* str = value.c_str();
    int result = 0;
    bool negative = false;

    if (*str == '-') {
        negative = true;
        str++;
    }

    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }

    return negative ? -result : result;
}

void PersistentStorage::putInt(const std::string& key, int value) {
    put(key, std::to_string(value));
}

void PersistentStorage::clear() {
    if (!data.empty()) {
        data.clear();
        dirty = true;
    }
}
