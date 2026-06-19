/**
 * @file Config.h
 * @brief Compile-time configuration for PhantomRF
 *
 * Defines version, board selection, and build-time constants. This header is
 * the single source of truth for things that are baked into the firmware at
 * compile time (version string, default AP name, board identification).
 *
 * Pin assignments for each supported board live in `hal/Board.h` / `Board.cpp`.
 *
 * @author PhantomRF Project
 * @date 2026
 */
#pragma once

// ---------------------------------------------------------------------------
// Version
// ---------------------------------------------------------------------------
#define PHM_VERSION_MAJOR 0
#define PHM_VERSION_MINOR 1
#define PHM_VERSION_PATCH 0
#define PHM_VERSION_STRING "0.1.0"
#define PHM_NAME "PhantomRF"
#define PHM_SHORT_NAME "phm"

// ---------------------------------------------------------------------------
// Board identifiers (must be passed in build_flags as -DPHM_BOARD=...)
// ---------------------------------------------------------------------------
#define PHM_BOARD_ESP32S3_N16R8 1  ///< ESP32-S3-WROOM-1-N16R8 (primary)
#define PHM_BOARD_ESP32S3_N8R2 2   ///< ESP32-S3-WROOM-1-N8R2
#define PHM_BOARD_ESP32S3_N8 3     ///< ESP32-S3-WROOM-1-N8 (no PSRAM)
#define PHM_BOARD_ESP32_CLASSIC 4  ///< ESP32-WROOM-32D (Xtensa LX6 dual-core)
#define PHM_BOARD_ESP32C3 5        ///< ESP32-C3-DevKitM-1 (RISC-V, single-core)
#define PHM_BOARD_ESP32S2 6        ///< ESP32-S2-Saola-1 (no BLE)

// ---------------------------------------------------------------------------
// Board sanity check
// ---------------------------------------------------------------------------
#ifndef PHM_BOARD
#error "PHM_BOARD must be defined in build_flags (e.g. -DPHM_BOARD=PHM_BOARD_ESP32S3_N16R8)"
#endif

// ---------------------------------------------------------------------------
// Default AP settings
// ---------------------------------------------------------------------------
#define PHM_DEFAULT_AP_SSID "PhantomRF"
#define PHM_DEFAULT_AP_PASSWORD_LENGTH 16

// ---------------------------------------------------------------------------
// Channel limits
// ---------------------------------------------------------------------------
#define PHM_NRF24_CHANNEL_MIN 0
#define PHM_NRF24_CHANNEL_MAX 125
#define PHM_NRF24_CHANNEL_COUNT 126
#define PHM_NRF24_CHANNEL_DEFAULT 76  // 2.476 GHz (popular drone band)

#define PHM_WIFI_CHANNEL_MIN 1
#define PHM_WIFI_CHANNEL_MAX 14
#define PHM_ZIGBEE_CHANNEL_MIN 11
#define PHM_ZIGBEE_CHANNEL_MAX 26
#define PHM_BLE_CHANNEL_MIN 0
#define PHM_BLE_CHANNEL_MAX 39
#define PHM_BLE_ADV_CHANNELS 37, 38, 39

// ---------------------------------------------------------------------------
// nRF24 + CC1101 radio defaults
// ---------------------------------------------------------------------------
#define PHM_MAX_NRF24_RADIOS 5   ///< Maximum number of nRF24 modules
#define PHM_MAX_CC1101_RADIOS 2  ///< Maximum number of CC1101 modules

// ---------------------------------------------------------------------------
// Storage paths (LittleFS)
// ---------------------------------------------------------------------------
#define PHM_FS_WEB_DIR "/web"
#define PHM_FS_RECORDINGS_DIR "/recordings"
#define PHM_FS_BACKUPS_DIR "/backups"
#define PHM_FS_LOGS_DIR "/logs"

// ---------------------------------------------------------------------------
// Logger ring buffer
// ---------------------------------------------------------------------------
#define PHM_LOG_RING_SIZE 64
#define PHM_LOG_TAG_MAX_LEN 16
#define PHM_LOG_MSG_MAX_LEN 160

// ---------------------------------------------------------------------------
// Power
// ---------------------------------------------------------------------------
#define PHM_VBAT_ADC_SAMPLES 8
#define PHM_TEMP_WARN_C 65.0f
#define PHM_TEMP_THROTTLE_C 75.0f
#define PHM_TEMP_CRITICAL_C 85.0f
