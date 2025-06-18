#ifndef NEUXTP_HDR_H
#define NEUXTP_HDR_H

#include <stdint.h>

#define NEUXTP_VERSION 0x1

// AI Tag Definitions (example)
#define AI_TAG_NONE     0x00
#define AI_TAG_TEXT     0x01
#define AI_TAG_IMAGE    0x02
#define AI_TAG_AUDIO    0x03
#define AI_TAG_VIDEO    0x04
#define AI_TAG_EMBEDDING 0x05

// Priority Levels
#define NEUXTP_PRIORITY_LOW     0x00
#define NEUXTP_PRIORITY_MEDIUM  0x01
#define NEUXTP_PRIORITY_HIGH    0x02
#define NEUXTP_PRIORITY_CRITICAL 0x03

struct neuxtp_hdr {
    uint8_t version;        // Protocol version (e.g., 1)
    uint8_t flags;          // Future use (e.g., ACK, SYN bits)
    uint16_t length;        // Payload size in bytes

    uint16_t session_id;    // AI Session tracking (e.g., prompt ID)
    uint8_t ai_tag;         // AI-specific classification (text/image/etc)
    uint8_t priority;       // QoS level (0 = low, 3 = critical)

    uint32_t checksum;      // Optional (for end-to-end integrity)
} __attribute__((__packed__));

#endif // NEUXTP_HDR_H
