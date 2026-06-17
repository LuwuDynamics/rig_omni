# BluFi WiFi Provisioning System

<cite>
**Referenced Files in This Document**
- [blufi.h](file://main/boards/common/blufi.h)
- [blufi.cpp](file://main/boards/common/blufi.cpp)
- [wifi_board.h](file://main/boards/common/wifi_board.h)
- [wifi_board.cc](file://main/boards/common/wifi_board.cc)
- [board.h](file://main/boards/common/board.h)
- [lulu-esp32s3.cc](file://main/boards/lulu-esp32s3/lulu-esp32s3.cc)
- [application.h](file://main/application.h)
</cite>

## Table of Contents
1. [Introduction](#introduction)
2. [Project Structure](#project-structure)
3. [Core Components](#core-components)
4. [Architecture Overview](#architecture-overview)
5. [Detailed Component Analysis](#detailed-component-analysis)
6. [Dependency Analysis](#dependency-analysis)
7. [Performance Considerations](#performance-considerations)
8. [Troubleshooting Guide](#troubleshooting-guide)
9. [Conclusion](#conclusion)

## Introduction
This document describes the BluFi WiFi provisioning system that enables Bluetooth-assisted network configuration for embedded devices. It explains how the ESP-IDF-based Blufi protocol transfers WiFi credentials securely from a mobile device to an embedded target, covering Bluetooth LE advertisement and scanning, pairing and security negotiation, credential validation, and error handling. It also documents step-by-step workflows for initial setup, reconfiguration, and fallback scenarios, along with security considerations and integration patterns for mobile applications and UI.

## Project Structure
The BluFi implementation centers around a Blufi class that encapsulates Bluetooth LE profile initialization, Wi-Fi scanning, security negotiation, and credential provisioning. Integration with the board and application layers ensures smooth UX transitions and device actions during provisioning.

```mermaid
graph TB
subgraph "Embedded Firmware"
BLUFI["Blufi<br/>blufi.cpp/.h"]
WBOARD["WifiBoard<br/>wifi_board.cc/.h"]
BOARD["Board<br/>board.h"]
APP["Application<br/>application.h"]
end
subgraph "Mobile App"
MOBILE["Mobile App UI<br/>User Actions"]
end
MOBILE --> |"Bluetooth LE"| BLUFI
BLUFI --> |"Wi-Fi Scan/Connect"| WBOARD
WBOARD --> |"Network Events"| BOARD
BOARD --> |"Device Actions"| APP
```

**Diagram sources**
- [blufi.cpp:104-138](file://main/boards/common/blufi.cpp#L104-L138)
- [wifi_board.cc:173-221](file://main/boards/common/wifi_board.cc#L173-L221)
- [board.h:20-93](file://main/boards/common/board.h#L20-L93)
- [application.h:42-177](file://main/application.h#L42-L177)

**Section sources**
- [blufi.h:16-48](file://main/boards/common/blufi.h#L16-L48)
- [blufi.cpp:104-138](file://main/boards/common/blufi.cpp#L104-L138)
- [wifi_board.h:9-76](file://main/boards/common/wifi_board.h#L9-L76)
- [wifi_board.cc:173-221](file://main/boards/common/wifi_board.cc#L173-L221)
- [board.h:20-93](file://main/boards/common/board.h#L20-L93)
- [application.h:42-177](file://main/application.h#L42-L177)

## Core Components
- Blufi class: Manages Bluetooth controller/host initialization, GAP/GATT profile registration, event callbacks, Wi-Fi scanning, security negotiation (Diffie-Hellman and AES-CFB128), and credential provisioning lifecycle.
- WifiBoard: Integrates provisioning into the board lifecycle, triggers UI feedback, and coordinates state transitions.
- Board: Provides hooks for provisioning start/end actions and device-specific behaviors.
- Application: Central orchestrator for UI alerts, sounds, and state transitions during provisioning.

Key responsibilities:
- Initialize and deinitialize the Blufi profile and Bluetooth stack.
- Start Wi-Fi scans and deliver AP lists to the mobile app.
- Negotiate shared keys and encrypt/decrypt messages.
- Validate received credentials and attempt connection with timeout handling.
- Report connection status and trigger post-provisioning actions.

**Section sources**
- [blufi.h:16-148](file://main/boards/common/blufi.h#L16-L148)
- [blufi.cpp:104-138](file://main/boards/common/blufi.cpp#L104-L138)
- [wifi_board.h:9-76](file://main/boards/common/wifi_board.h#L9-L76)
- [board.h:20-93](file://main/boards/common/board.h#L20-L93)
- [application.h:42-177](file://main/application.h#L42-L177)

## Architecture Overview
The system follows a layered architecture:
- Mobile App: Initiates provisioning via Bluetooth LE, sends Wi-Fi credentials, and receives status updates.
- Embedded Blufi: Handles BLE events, manages Wi-Fi operations, and executes provisioning.
- Board/Application: Provide UI feedback, device actions, and state transitions.

```mermaid
sequenceDiagram
participant Mobile as "Mobile App"
participant BLE as "BLE Controller"
participant Blufi as "Blufi Class"
participant WiFi as "Wi-Fi Manager"
participant Board as "Board/App"
Mobile->>BLE : Connect to device
BLE-->>Blufi : ESP_BLUFI_EVENT_BLE_CONNECT
Blufi->>Blufi : Initialize security context
Blufi->>WiFi : Start Wi-Fi scan
WiFi-->>Blufi : AP list
Blufi-->>Mobile : Send Wi-Fi list
Mobile->>Blufi : Send SSID, Password
Blufi->>WiFi : Attempt connection with credentials
WiFi-->>Blufi : Connection result
Blufi-->>Mobile : Connection report (success/fail)
Blufi->>Board : OnWifiConfigEnd()
Board->>Board : Trigger success action/sound
Blufi->>Blufi : Restart device
```

**Diagram sources**
- [blufi.cpp:644-936](file://main/boards/common/blufi.cpp#L644-L936)
- [wifi_board.cc:173-221](file://main/boards/common/wifi_board.cc#L173-L221)
- [board.h:90-92](file://main/boards/common/board.h#L90-L92)

## Detailed Component Analysis

### Blufi Class
The Blufi class encapsulates the entire provisioning flow:
- Initialization: Initializes Bluetooth controller/host, registers callbacks, and starts advertising.
- Security: Performs Diffie-Hellman key exchange and AES-CFB128 encryption for secure communication.
- Wi-Fi Operations: Starts scans, collects AP records, sorts by signal strength, and limits results to avoid timeouts.
- Credential Provisioning: Receives SSID/password, validates, attempts connection with a timeout, reports results, and triggers post-provisioning actions.

```mermaid
classDiagram
class Blufi {
+GetInstance() Blufi&
+init() esp_err_t
+deinit() esp_err_t
+start_wifi_scan() void
-_handle_event(event, param) void
-_security_init() void
-_security_deinit() void
-_dh_negotiate_data_handler(data, len, out, outLen, needFree) void
-_aes_encrypt(iv8, data, len) int
-_aes_decrypt(iv8, data, len) int
-_send_wifi_list() void
-_wifi_scan_event_handler(arg, base, id, data) void
-_get_softap_conn_num() int
-m_sec : BlufiSecurity*
-m_sta_config : wifi_config_t
-m_ble_is_connected : bool
-m_sta_connected : bool
-m_sta_got_ip : bool
-m_provisioned : bool
-m_ap_records : vector<wifi_ap_record_t>
}
class BlufiSecurity {
+self_public_key[128] : uint8_t
+share_key[128] : uint8_t
+share_len : size_t
+psk[16] : uint8_t
+dh_param : uint8_t*
+dh_param_len : int
+iv[16] : uint8_t
+dhm : mbedtls_dhm_context*
+aes : mbedtls_aes_context*
}
Blufi --> BlufiSecurity : "owns"
```

**Diagram sources**
- [blufi.h:16-148](file://main/boards/common/blufi.h#L16-L148)
- [blufi.cpp:348-377](file://main/boards/common/blufi.cpp#L348-L377)

**Section sources**
- [blufi.h:16-148](file://main/boards/common/blufi.h#L16-L148)
- [blufi.cpp:104-138](file://main/boards/common/blufi.cpp#L104-L138)
- [blufi.cpp:348-470](file://main/boards/common/blufi.cpp#L348-L470)
- [blufi.cpp:472-516](file://main/boards/common/blufi.cpp#L472-L516)
- [blufi.cpp:531-642](file://main/boards/common/blufi.cpp#L531-L642)
- [blufi.cpp:644-936](file://main/boards/common/blufi.cpp#L644-L936)

### Wi-Fi Scanning and List Delivery
The system performs targeted Wi-Fi scans and delivers a sorted list of APs to the mobile app, limiting the number to prevent transport timeouts.

```mermaid
flowchart TD
Start(["Start Wi-Fi Scan"]) --> CheckInProgress{"Scan in Progress?"}
CheckInProgress --> |Yes| Skip["Skip New Scan"]
CheckInProgress --> |No| SetInProgress["Set Scan In Progress"]
SetInProgress --> EnsureNetIf["Ensure STA NetIf Exists"]
EnsureNetIf --> SetMode["Set Mode to STA and Start WiFi"]
SetMode --> RegisterHandler["Register Scan Done Handler"]
RegisterHandler --> StartScan["Start ESP-IDF Scan"]
StartScan --> WaitScan["Wait for Scan Done Event"]
WaitScan --> CollectRecords["Collect AP Records"]
CollectRecords --> SortByRSSI["Sort by RSSI Descending"]
SortByRSSI --> LimitCount["Limit to 10 APs"]
LimitCount --> SendList["Send Wi-Fi List to Mobile"]
SendList --> End(["Done"])
```

**Diagram sources**
- [blufi.cpp:531-642](file://main/boards/common/blufi.cpp#L531-L642)
- [blufi.cpp:583-613](file://main/boards/common/blufi.cpp#L583-L613)

**Section sources**
- [blufi.cpp:531-642](file://main/boards/common/blufi.cpp#L531-L642)
- [blufi.cpp:583-613](file://main/boards/common/blufi.cpp#L583-L613)

### Security and Encryption
The Blufi class negotiates a shared secret using Diffie-Hellman and derives a 128-bit PSK via MD5, then uses AES in CFB128 mode for confidentiality. An IV is maintained per message to ensure uniqueness.

```mermaid
sequenceDiagram
participant Mobile as "Mobile App"
participant Blufi as "Blufi Class"
participant DH as "mbedTLS DH"
participant AES as "mbedTLS AES"
Mobile->>Blufi : Send DH Parameter Length
Mobile->>Blufi : Send DH Parameters
Blufi->>DH : Read Params
DH-->>Blufi : OK
Blufi->>DH : Make Public
DH-->>Blufi : Self Public Key
Blufi->>DH : Calc Secret
DH-->>Blufi : Shared Secret
Blufi->>AES : Set Key (MD5 of Shared Secret)
AES-->>Blufi : Ready
Blufi-->>Mobile : Send Self Public Key
Note over Blufi,AES : Subsequent messages encrypted/decrypted with AES-CFB128
```

**Diagram sources**
- [blufi.cpp:379-470](file://main/boards/common/blufi.cpp#L379-L470)
- [blufi.cpp:472-516](file://main/boards/common/blufi.cpp#L472-L516)

**Section sources**
- [blufi.cpp:379-470](file://main/boards/common/blufi.cpp#L379-L470)
- [blufi.cpp:472-516](file://main/boards/common/blufi.cpp#L472-L516)

### Credential Provisioning and Connection Flow
Upon receiving SSID and password, the system validates, attempts a direct connection, monitors for IP acquisition, and reports results. On success, it triggers board-level actions and restarts the device.

```mermaid
sequenceDiagram
participant Mobile as "Mobile App"
participant Blufi as "Blufi Class"
participant WiFi as "Wi-Fi Manager"
participant Board as "Board/App"
Mobile->>Blufi : SSID, Password
Blufi->>WiFi : Stop previous state, set config, start/connect
WiFi-->>Blufi : Connection attempt
Blufi->>WiFi : Poll for AP info and IP
alt Connected with IP
Blufi-->>Mobile : Success report
Blufi->>Board : OnWifiConfigEnd()
Board-->>Board : Play success sound/action
Blufi->>Blufi : Restart device
else Timeout/Failure
Blufi-->>Mobile : Failure report
end
```

**Diagram sources**
- [blufi.cpp:719-847](file://main/boards/common/blufi.cpp#L719-L847)
- [blufi.cpp:826-837](file://main/boards/common/blufi.cpp#L826-L837)
- [board.h:90-92](file://main/boards/common/board.h#L90-L92)

**Section sources**
- [blufi.cpp:719-847](file://main/boards/common/blufi.cpp#L719-L847)
- [blufi.cpp:826-837](file://main/boards/common/blufi.cpp#L826-L837)
- [board.h:90-92](file://main/boards/common/board.h#L90-L92)

### Integration with Board and Application
The board layer integrates provisioning into device state transitions and provides hooks for UI feedback and actions. The application layer schedules UI updates and sounds.

```mermaid
graph TB
WBOARD["WifiBoard<br/>StartWifiConfigMode()"] --> BLUFI["Blufi::init()"]
BLUFI --> WBOARD
WBOARD --> APP["Application<br/>Alert(), PlaySound()"]
BLUFI --> BOARD["Board<br/>OnWifiConfigStart/End()"]
BOARD --> APP
```

**Diagram sources**
- [wifi_board.cc:173-221](file://main/boards/common/wifi_board.cc#L173-L221)
- [blufi.cpp:104-138](file://main/boards/common/blufi.cpp#L104-L138)
- [board.h:90-92](file://main/boards/common/board.h#L90-L92)
- [application.h:82-113](file://main/application.h#L82-L113)

**Section sources**
- [wifi_board.cc:173-221](file://main/boards/common/wifi_board.cc#L173-L221)
- [board.h:90-92](file://main/boards/common/board.h#L90-L92)
- [application.h:82-113](file://main/application.h#L82-L113)

## Dependency Analysis
The Blufi class depends on ESP-IDF’s Blufi API, Wi-Fi subsystem, and mbedTLS for cryptographic operations. It integrates with the board and application layers for UI and device actions.

```mermaid
graph TB
BLUFI["Blufi (blufi.cpp)"]
IDF["ESP-IDF Blufi API"]
WIFI["ESP-IDF Wi-Fi"]
MBED["mbedTLS"]
WBOARD["WifiBoard"]
BOARD["Board"]
APP["Application"]
BLUFI --> IDF
BLUFI --> WIFI
BLUFI --> MBED
BLUFI --> WBOARD
WBOARD --> BOARD
BOARD --> APP
```

**Diagram sources**
- [blufi.cpp:1-63](file://main/boards/common/blufi.cpp#L1-L63)
- [blufi.h:3-14](file://main/boards/common/blufi.h#L3-L14)
- [wifi_board.cc:173-221](file://main/boards/common/wifi_board.cc#L173-L221)
- [board.h:20-93](file://main/boards/common/board.h#L20-L93)
- [application.h:42-177](file://main/application.h#L42-L177)

**Section sources**
- [blufi.cpp:1-63](file://main/boards/common/blufi.cpp#L1-L63)
- [blufi.h:3-14](file://main/boards/common/blufi.h#L3-L14)
- [wifi_board.cc:173-221](file://main/boards/common/wifi_board.cc#L173-L221)
- [board.h:20-93](file://main/boards/common/board.h#L20-L93)
- [application.h:42-177](file://main/application.h#L42-L177)

## Performance Considerations
- Wi-Fi scan batching: AP lists are limited to reduce Bluetooth transfer time and memory overhead.
- Connection polling: Uses periodic checks with a bounded timeout to avoid indefinite blocking.
- Task scheduling: Long-running operations are offloaded to FreeRTOS tasks to keep the event loop responsive.
- Security overhead: AES-CFB128 and DH operations are lightweight for embedded targets but should be monitored under load.

[No sources needed since this section provides general guidance]

## Troubleshooting Guide
Common issues and handling:
- Bluetooth initialization failures: The Blufi initializer logs errors and returns failure codes; check controller and host initialization paths.
- Wi-Fi scan failures: Verify Wi-Fi mode and startup; ensure scan handler registration succeeds.
- DH negotiation errors: Validate parameter lengths and mbedtls return codes; ensure proper cleanup on failure.
- Connection timeout: Confirm credentials validity, AP availability, and network reachability; review connection attempt and IP acquisition loops.
- Deinitialization: Ensure proper shutdown order for BLE and Wi-Fi stacks to avoid resource leaks.

**Section sources**
- [blufi.cpp:104-138](file://main/boards/common/blufi.cpp#L104-L138)
- [blufi.cpp:307-341](file://main/boards/common/blufi.cpp#L307-L341)
- [blufi.cpp:379-470](file://main/boards/common/blufi.cpp#L379-L470)
- [blufi.cpp:762-847](file://main/boards/common/blufi.cpp#L762-L847)

## Conclusion
The BluFi WiFi provisioning system provides a robust, secure, and user-friendly mechanism for transferring Wi-Fi credentials from a mobile device to an embedded target over Bluetooth LE. Its architecture cleanly separates BLE handling, Wi-Fi operations, security, and UI integration, enabling reliable initial setup, reconfiguration, and graceful fallbacks. By following the documented workflows and security practices, developers can integrate seamless provisioning experiences into their products.