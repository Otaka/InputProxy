#ifndef PERSISTENT_STORAGE_H
#define PERSISTENT_STORAGE_H

#include <map>
#include <string>
#include <cstdint>
#include "hardware/flash.h"
#include "pico.h"

/**
 * Generic key-value storage that persists to flash memory.
 *
 * Data format in flash:
 * - [4 bytes: MAGIC_NUMBER]
 * - [4 bytes: data length]
 * - [N bytes: key1|value1|key2|value2...]
 *
 * Escaping rules:
 * - '|' (delimiter) → '`|' (backtick + pipe)
 * - '`' (escape char) → '``' (double backtick)
 *
 * Usage:
 *   PersistentStorage storage;
 *   storage.load();  // Load from flash
 *   storage.put("mode", "hid");
 *   storage.flush();  // Write to flash (only if dirty)
 */
class PersistentStorage {
public:
    PersistentStorage();

    /**
     * Load data from flash into memory.
     * Call this once during initialization.
     * Returns true if data was loaded successfully, false if flash was empty or corrupted.
     */
    bool load();

    /**
     * Write data to flash (only if dirty flag is set).
     * Dirty flag is set automatically by put().
     * Returns true if write was successful.
     */
    bool flush();

    /**
     * Get value by key (returns empty string if not found).
     */
    std::string get(const std::string& key) const;

    /**
     * Set value by key (sets dirty flag).
     */
    void put(const std::string& key, const std::string& value);

    /**
     * Check if key exists.
     */
    bool has(const std::string& key) const;

    /**
     * Remove a key (sets dirty flag).
     */
    void remove(const std::string& key);

    /**
     * Helper methods for integer values.
     */
    int getInt(const std::string& key, int defaultValue = 0) const;
    void putInt(const std::string& key, int value);

    /**
     * Clear all data (sets dirty flag).
     */
    void clear();

private:
    // Use last sector of flash (4KB before end)
    static constexpr uint32_t FLASH_TARGET_OFFSET = PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE;
    static constexpr uint32_t MAGIC_NUMBER = 0x494E5058; // "INPX"
    static constexpr uint32_t MAX_DATA_SIZE = FLASH_SECTOR_SIZE - 8;  // Sector size minus header

    std::map<std::string, std::string> data;
    bool dirty;  // Set to true when data is modified

    /**
     * Escape string for storage.
     * Rules: '|' → '`|', '`' → '``'
     */
    static std::string escape(const std::string& str);

    /**
     * Unescape string from storage.
     * Rules: '`|' → '|', '``' → '`'
     */
    static std::string unescape(const std::string& str);

    /**
     * Serialize map to string format: key1|value1|key2|value2...
     */
    std::string serialize() const;

    /**
     * Deserialize string format back to map.
     * Returns true if successful.
     */
    bool deserialize(const std::string& serialized);
};

#endif // PERSISTENT_STORAGE_H
