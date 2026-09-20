#ifndef AML_H
#define AML_H
#include <stdint.h>
#include <stdbool.h>
#include "acpi.h"

#define AML_MAX_NODES   512
#define AML_MAX_CHILDREN_SCAN 4096
#define AML_MAX_PKG_ELEMS 16

typedef enum {
    AML_NODE_SCOPE,
    AML_NODE_DEVICE,
    AML_NODE_METHOD,
    AML_NODE_NAME_INT,
    AML_NODE_NAME_STR,
    AML_NODE_NAME_BUF,
    AML_NODE_NAME_PKG,
    AML_NODE_OPREGION,
    AML_NODE_FIELDUNIT,
} AmlNodeType;

typedef struct AmlNode {
    char             name[5];
    struct AmlNode  *parent;
    struct AmlNode  *first_child;
    struct AmlNode  *next_sibling;
    AmlNodeType      type;

    const uint8_t   *method_start;
    uint32_t         method_len;
    uint8_t          method_arg_count;

    uint64_t         int_value;

    const uint8_t   *data_ptr;
    uint32_t         data_len;

    uint64_t         pkg_ints[AML_MAX_PKG_ELEMS];
    int              pkg_count;

    uint8_t          region_space;
    uint32_t         region_offset;
    uint32_t         region_length;

    struct AmlNode  *field_region;
    uint32_t         field_bit_offset;
    uint32_t         field_bit_length;
} AmlNode;

typedef struct {
    AmlNode  pool[AML_MAX_NODES];
    int      n_nodes;
    AmlNode *root;
} AmlNamespace;

void aml_build_namespace(AmlNamespace *ns, const AcpiInfo *acpi);

AmlNode *aml_find(AmlNamespace *ns, const char *dotted_path);

AmlNode *aml_find_device_by_hid(AmlNamespace *ns, const char *hid_str);

typedef struct {
    bool     ok;
    bool     is_package;
    uint64_t int_value;
    uint64_t pkg_ints[AML_MAX_PKG_ELEMS];
    int      pkg_count;
} AmlResult;

AmlResult aml_call_method(AmlNamespace *ns, AmlNode *method);

#endif
