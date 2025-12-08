#ifndef ACPI_H
#define ACPI_H

#include <stdint.h>

// ACPI RSDP (Root System Description Pointer)
struct acpi_rsdp {
    char signature[8];      // "RSD PTR "
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;  // 32-bit physical address of RSDT
} __attribute__((packed));

// ACPI RSDP 2.0 (Extended)
struct acpi_rsdp2 {
    struct acpi_rsdp rsdp1;
    uint32_t length;
    uint64_t xsdt_address;  // 64-bit physical address of XSDT
    uint8_t extended_checksum;
    uint8_t reserved[3];
} __attribute__((packed));

// ACPI SDT Header
struct acpi_sdt_header {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed));

// ACPI NFIT (NVDIMM Firmware Interface Table)
struct acpi_nfit {
    struct acpi_sdt_header header;
    uint32_t reserved;
} __attribute__((packed));

// NFIT Structure Types
#define ACPI_NFIT_TYPE_SYSTEM_ADDRESS       0
#define ACPI_NFIT_TYPE_MEMORY_MAP           1
#define ACPI_NFIT_TYPE_INTERLEAVE           2
#define ACPI_NFIT_TYPE_SMBIOS               3
#define ACPI_NFIT_TYPE_CONTROL_REGION       4
#define ACPI_NFIT_TYPE_DATA_REGION          5
#define ACPI_NFIT_TYPE_FLUSH_ADDRESS        6

// NFIT Structure Header
struct acpi_nfit_header {
    uint16_t type;
    uint16_t length;
} __attribute__((packed));

// NFIT System Physical Address Range Structure
struct acpi_nfit_system_address {
    struct acpi_nfit_header header;
    uint16_t range_index;
    uint16_t flags;
    uint32_t reserved;
    uint32_t proximity_domain;
    uint8_t range_guid[16];
    uint64_t address;
    uint64_t length;
    uint64_t memory_mapping;
} __attribute__((packed));

// NFIT Memory Device to System Address Range Map
struct acpi_nfit_memory_map {
    struct acpi_nfit_header header;
    uint32_t device_handle;
    uint16_t physical_id;
    uint16_t region_id;
    uint16_t range_index;
    uint16_t region_index;
    uint64_t region_size;
    uint64_t region_offset;
    uint64_t address;
    uint16_t interleave_index;
    uint16_t interleave_ways;
    uint16_t flags;
    uint16_t reserved;
} __attribute__((packed));

// Function prototypes
extern struct acpi_rsdp* acpi_find_rsdp(void);
extern struct acpi_sdt_header* acpi_find_table(const char* signature);
extern void acpi_parse_nfit(void);
extern uint64_t acpi_get_nvdimm_size(void);

#endif // ACPI_H
