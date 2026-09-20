#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "aml.h"
#include "ec.h"
#include "io.h"

extern void *memcpy(void *dst, const void *src, size_t n);
extern void *memset(void *s, int c, size_t n);
extern int   memcmp(const void *a, const void *b, size_t n);

#define OP_ZERO           0x00
#define OP_ONE            0x01
#define OP_ALIAS          0x06
#define OP_NAME           0x08
#define OP_BYTEPREFIX     0x0A
#define OP_WORDPREFIX     0x0B
#define OP_DWORDPREFIX    0x0C
#define OP_STRINGPREFIX   0x0D
#define OP_QWORDPREFIX    0x0E
#define OP_SCOPE          0x10
#define OP_BUFFER         0x11
#define OP_PACKAGE        0x12
#define OP_VARPACKAGE     0x13
#define OP_METHOD         0x14
#define OP_DUALNAME       0x2E
#define OP_MULTINAME      0x2F
#define OP_EXTPREFIX      0x5B
#define OP_ROOTCHAR       0x5C
#define OP_PARENTPREFIX   0x5E
#define OP_LOCAL0         0x60
#define OP_ARG0           0x68
#define OP_STORE          0x70
#define OP_ADD            0x72
#define OP_SUBTRACT       0x74
#define OP_MULTIPLY       0x77
#define OP_SHL            0x79
#define OP_SHR            0x7A
#define OP_AND            0x7B
#define OP_OR             0x7D
#define OP_NOTIFY         0x86
#define OP_LAND           0x90
#define OP_LOR            0x91
#define OP_LNOT           0x92
#define OP_LEQUAL         0x93
#define OP_LGREATER       0x94
#define OP_LLESS          0x95
#define OP_IF             0xA0
#define OP_ELSE           0xA1
#define OP_WHILE          0xA2
#define OP_RETURN         0xA4
#define OP_ONES           0xFF

#define EXT_MUTEX         0x01
#define EXT_OPREGION      0x80
#define EXT_FIELD         0x81
#define EXT_DEVICE        0x82

#define REGION_SYSTEMMEMORY 0
#define REGION_SYSTEMIO     1
#define REGION_PCICONFIG    2
#define REGION_EC           3

typedef struct {
    const uint8_t *p;
    const uint8_t *end;
} Cursor;

static bool cur_ok(const Cursor *c, uint32_t need) {
    return (uint32_t)(c->end - c->p) >= need;
}

static bool decode_pkglength(Cursor *c, uint32_t *out_len, const uint8_t **out_data_start) {
    if (!cur_ok(c, 1)) return false;
    uint8_t b0 = c->p[0];
    uint8_t extra = (b0 >> 6) & 0x3;
    uint32_t len;
    if (extra == 0) {
        len = b0 & 0x3F;
        *out_data_start = c->p + 1;
    } else {
        if (!cur_ok(c, 1u + extra)) return false;
        len = b0 & 0x0F;
        for (int i = 0; i < extra; i++) {
            len |= ((uint32_t)c->p[1 + i]) << (4 + 8 * i);
        }
        *out_data_start = c->p + 1 + extra;
    }
    *out_len = len;
    return true;
}

typedef struct {
    bool is_root;
    int  parent_ups;
    char segs[8][5];
    int  n_segs;
} NamePath;

static bool is_nameseg_start(uint8_t b) { return b == '_' || (b >= 'A' && b <= 'Z'); }

static bool parse_nameseg(Cursor *c, char out[5]) {
    if (!cur_ok(c, 4)) return false;
    if (!is_nameseg_start(c->p[0])) return false;
    memcpy(out, c->p, 4);
    out[4] = '\0';
    c->p += 4;
    return true;
}

static bool parse_name_string(Cursor *c, NamePath *out) {
    memset(out, 0, sizeof(*out));
    if (!cur_ok(c, 1)) return false;

    if (c->p[0] == OP_ROOTCHAR) { out->is_root = true; c->p++; }
    else {
        while (cur_ok(c, 1) && c->p[0] == OP_PARENTPREFIX) { out->parent_ups++; c->p++; }
    }
    if (!cur_ok(c, 1)) return false;

    if (c->p[0] == 0x00) { c->p++; return true; }
    if (c->p[0] == OP_DUALNAME) {
        c->p++;
        if (!parse_nameseg(c, out->segs[0])) return false;
        if (!parse_nameseg(c, out->segs[1])) return false;
        out->n_segs = 2;
        return true;
    }
    if (c->p[0] == OP_MULTINAME) {
        c->p++;
        if (!cur_ok(c, 1)) return false;
        uint8_t count = c->p[0]; c->p++;
        if (count > 8) return false;
        for (int i = 0; i < count; i++) {
            if (!parse_nameseg(c, out->segs[i])) return false;
        }
        out->n_segs = count;
        return true;
    }
    if (!parse_nameseg(c, out->segs[0])) return false;
    out->n_segs = 1;
    return true;
}

static AmlNode *ns_alloc(AmlNamespace *ns) {
    if (ns->n_nodes >= AML_MAX_NODES) return NULL;
    AmlNode *n = &ns->pool[ns->n_nodes++];
    memset(n, 0, sizeof(*n));
    return n;
}

static AmlNode *find_child(AmlNode *parent, const char *nameseg4) {
    for (AmlNode *c = parent->first_child; c; c = c->next_sibling) {
        if (memcmp(c->name, nameseg4, 4) == 0) return c;
    }
    return NULL;
}

static AmlNode *find_or_create_child(AmlNamespace *ns, AmlNode *parent, const char *nameseg4) {
    AmlNode *c = find_child(parent, nameseg4);
    if (c) return c;
    c = ns_alloc(ns);
    if (!c) return NULL;
    memcpy(c->name, nameseg4, 4);
    c->name[4] = '\0';
    c->parent = parent;
    c->type = AML_NODE_SCOPE;
    c->next_sibling = parent->first_child;
    parent->first_child = c;
    return c;
}

static AmlNode *resolve_create(AmlNamespace *ns, AmlNode *scope, const NamePath *path) {
    AmlNode *cur = path->is_root ? ns->root : scope;
    if (!path->is_root) {
        for (int i = 0; i < path->parent_ups && cur->parent; i++) cur = cur->parent;
    }
    for (int i = 0; i < path->n_segs; i++) {
        cur = find_or_create_child(ns, cur, path->segs[i]);
        if (!cur) return NULL;
    }
    return cur;
}

static AmlNode *resolve_lookup(AmlNamespace *ns, AmlNode *scope, const NamePath *path) {
    if (path->n_segs == 0) {
        AmlNode *cur = path->is_root ? ns->root : scope;
        if (!path->is_root) for (int i = 0; i < path->parent_ups && cur->parent; i++) cur = cur->parent;
        return cur;
    }
    if (path->is_root || path->parent_ups > 0 || path->n_segs > 1) {
        AmlNode *cur = path->is_root ? ns->root : scope;
        if (!path->is_root) for (int i = 0; i < path->parent_ups && cur->parent; i++) cur = cur->parent;
        for (int i = 0; i < path->n_segs; i++) {
            cur = find_child(cur, path->segs[i]);
            if (!cur) return NULL;
        }
        return cur;
    }
    for (AmlNode *s = scope; s; s = s->parent) {
        AmlNode *c = find_child(s, path->segs[0]);
        if (c) return c;
    }
    return NULL;
}

static bool parse_term_list(AmlNamespace *ns, AmlNode *scope, Cursor *c);

static bool skip_simple_dataobj_int(Cursor *c, uint64_t *out) {
    if (!cur_ok(c, 1)) return false;
    uint8_t op = c->p[0];
    if (op == OP_ZERO)  { *out = 0; c->p++; return true; }
    if (op == OP_ONE)   { *out = 1; c->p++; return true; }
    if (op == OP_ONES)  { *out = ~(uint64_t)0; c->p++; return true; }
    if (op == OP_BYTEPREFIX)  { if (!cur_ok(c,2)) return false; *out = c->p[1]; c->p += 2; return true; }
    if (op == OP_WORDPREFIX)  { if (!cur_ok(c,3)) return false; *out = (uint16_t)(c->p[1] | (c->p[2]<<8)); c->p += 3; return true; }
    if (op == OP_DWORDPREFIX) { if (!cur_ok(c,5)) return false;
        *out = (uint32_t)(c->p[1] | (c->p[2]<<8) | (c->p[3]<<16) | ((uint32_t)c->p[4]<<24)); c->p += 5; return true; }
    if (op == OP_QWORDPREFIX) { if (!cur_ok(c,9)) return false;
        uint64_t v = 0; for (int i = 0; i < 8; i++) v |= ((uint64_t)c->p[1+i]) << (8*i);
        *out = v; c->p += 9; return true; }
    return false;
}

static bool parse_dataref_into(AmlNamespace *ns, AmlNode *scope, Cursor *c, AmlNode *node) {
    if (!cur_ok(c, 1)) return false;
    uint8_t op = c->p[0];

    uint64_t ival;
    if (op == OP_ZERO || op == OP_ONE || op == OP_ONES ||
        op == OP_BYTEPREFIX || op == OP_WORDPREFIX || op == OP_DWORDPREFIX || op == OP_QWORDPREFIX) {
        if (!skip_simple_dataobj_int(c, &ival)) return false;
        node->type = AML_NODE_NAME_INT;
        node->int_value = ival;
        return true;
    }
    if (op == OP_STRINGPREFIX) {
        c->p++;
        const uint8_t *start = c->p;
        while (cur_ok(c, 1) && c->p[0] != 0x00) c->p++;
        if (!cur_ok(c, 1)) return false;
        node->type = AML_NODE_NAME_STR;
        node->data_ptr = start;
        node->data_len = (uint32_t)(c->p - start);
        c->p++;
        return true;
    }
    if (op == OP_BUFFER) {
        c->p++;
        uint32_t len; const uint8_t *data_start;
        if (!decode_pkglength(c, &len, &data_start)) return false;
        const uint8_t *block_end = data_start + len;
        if (block_end > c->end || block_end < data_start) return false;
        node->type = AML_NODE_NAME_BUF;
        node->data_ptr = data_start;
        node->data_len = (uint32_t)(block_end - data_start);
        c->p = (uint8_t *)block_end;
        return true;
    }
    if (op == OP_PACKAGE || op == OP_VARPACKAGE) {
        bool is_var = (op == OP_VARPACKAGE);
        c->p++;
        uint32_t len; const uint8_t *data_start;
        if (!decode_pkglength(c, &len, &data_start)) return false;
        const uint8_t *block_end = data_start + len;
        if (block_end > c->end || block_end < data_start) return false;

        Cursor pc = { data_start, block_end };
        node->type = AML_NODE_NAME_PKG;
        node->pkg_count = 0;
        if (cur_ok(&pc, 1)) {
            pc.p += 1;
            (void)is_var;
            while (pc.p < pc.end && node->pkg_count < AML_MAX_PKG_ELEMS) {
                uint64_t v;
                if (!skip_simple_dataobj_int(&pc, &v)) break;
                node->pkg_ints[node->pkg_count++] = v;
            }
        }
        c->p = (uint8_t *)block_end;
        return true;
    }
    (void)ns; (void)scope;
    return false;
}

static bool parse_one_decl(AmlNamespace *ns, AmlNode *scope, Cursor *c) {
    if (!cur_ok(c, 1)) return false;
    uint8_t op = c->p[0];

    if (op == OP_SCOPE) {
        c->p++;
        uint32_t len; const uint8_t *data_start;
        if (!decode_pkglength(c, &len, &data_start)) return false;
        const uint8_t *block_end = data_start + len;
        if (block_end > c->end || block_end < data_start) return false;
        Cursor nc = { data_start, block_end };
        NamePath path;
        if (!parse_name_string(&nc, &path)) return false;
        AmlNode *target = resolve_create(ns, scope, &path);
        if (!target) return false;
        Cursor body = { nc.p, block_end };
        parse_term_list(ns, target, &body);
        c->p = (uint8_t *)block_end;
        return true;
    }

    if (op == OP_NAME) {
        c->p++;
        NamePath path;
        if (!parse_name_string(c, &path)) return false;
        AmlNode *target = resolve_create(ns, scope, &path);
        if (!target) return false;
        return parse_dataref_into(ns, scope, c, target);
    }

    if (op == OP_METHOD) {
        c->p++;
        uint32_t len; const uint8_t *data_start;
        if (!decode_pkglength(c, &len, &data_start)) return false;
        const uint8_t *block_end = data_start + len;
        if (block_end > c->end || block_end < data_start) return false;
        Cursor nc = { data_start, block_end };
        NamePath path;
        if (!parse_name_string(&nc, &path)) return false;
        if (!cur_ok(&nc, 1)) return false;
        uint8_t flags = nc.p[0]; nc.p++;
        AmlNode *target = resolve_create(ns, scope, &path);
        if (!target) return false;
        target->type = AML_NODE_METHOD;
        target->method_arg_count = flags & 0x7;
        target->method_start = nc.p;
        target->method_len = (uint32_t)(block_end - nc.p);
        c->p = (uint8_t *)block_end;
        return true;
    }

    if (op == OP_ALIAS) {
        c->p++;
        NamePath a, b;
        if (!parse_name_string(c, &a)) return false;
        if (!parse_name_string(c, &b)) return false;
        return true;
    }

    if (op == OP_EXTPREFIX) {
        if (!cur_ok(c, 2)) return false;
        uint8_t ext = c->p[1];

        if (ext == EXT_DEVICE) {
            c->p += 2;
            uint32_t len; const uint8_t *data_start;
            if (!decode_pkglength(c, &len, &data_start)) return false;
            const uint8_t *block_end = data_start + len;
            if (block_end > c->end || block_end < data_start) return false;
            Cursor nc = { data_start, block_end };
            NamePath path;
            if (!parse_name_string(&nc, &path)) return false;
            AmlNode *target = resolve_create(ns, scope, &path);
            if (!target) return false;
            target->type = AML_NODE_DEVICE;
            Cursor body = { nc.p, block_end };
            parse_term_list(ns, target, &body);
            c->p = (uint8_t *)block_end;
            return true;
        }
        if (ext == EXT_MUTEX) {
            c->p += 2;
            NamePath path;
            if (!parse_name_string(c, &path)) return false;
            if (!cur_ok(c, 1)) return false;
            c->p++;
            (void)resolve_create(ns, scope, &path);
            return true;
        }
        if (ext == EXT_OPREGION) {
            c->p += 2;
            NamePath path;
            if (!parse_name_string(c, &path)) return false;
            if (!cur_ok(c, 1)) return false;
            uint8_t space = c->p[0]; c->p++;
            uint64_t off, len2;
            if (!skip_simple_dataobj_int(c, &off)) return false;
            if (!skip_simple_dataobj_int(c, &len2)) return false;
            AmlNode *target = resolve_create(ns, scope, &path);
            if (!target) return false;
            target->type = AML_NODE_OPREGION;
            target->region_space = space;
            target->region_offset = (uint32_t)off;
            target->region_length = (uint32_t)len2;
            return true;
        }
        if (ext == EXT_FIELD) {
            c->p += 2;
            uint32_t len; const uint8_t *data_start;
            if (!decode_pkglength(c, &len, &data_start)) return false;
            const uint8_t *block_end = data_start + len;
            if (block_end > c->end || block_end < data_start) return false;
            Cursor nc = { data_start, block_end };
            NamePath region_path;
            if (!parse_name_string(&nc, &region_path)) return false;
            if (!cur_ok(&nc, 1)) return false;
            nc.p++;
            AmlNode *region = resolve_lookup(ns, scope, &region_path);

            uint32_t bit_offset = 0;
            while (nc.p < block_end) {
                uint8_t tag = nc.p[0];
                if (tag == 0x00) {
                    Cursor tmp = { nc.p + 1, block_end };
                    uint32_t width; const uint8_t *after;
                    if (!decode_pkglength(&tmp, &width, &after)) break;
                    bit_offset += width;
                    nc.p = after;
                } else if (tag == 0x01) {
                    if (nc.p + 3 > block_end) break;
                    nc.p += 3;
                } else if (is_nameseg_start(tag)) {
                    char nameseg[5];
                    Cursor tmp = nc;
                    if (!parse_nameseg(&tmp, nameseg)) break;
                    uint32_t width; const uint8_t *after;
                    if (!decode_pkglength(&tmp, &width, &after)) break;
                    AmlNode *field = find_or_create_child(ns, scope, nameseg);
                    if (field) {
                        field->type = AML_NODE_FIELDUNIT;
                        field->field_region = region;
                        field->field_bit_offset = bit_offset;
                        field->field_bit_length = width;
                    }
                    bit_offset += width;
                    nc.p = after;
                } else {
                    break;
                }
            }
            c->p = (uint8_t *)block_end;
            return true;
        }
        return false;
    }

    return false;
}

static bool parse_term_list(AmlNamespace *ns, AmlNode *scope, Cursor *c) {
    int guard = 0;
    while (c->p < c->end && guard++ < AML_MAX_CHILDREN_SCAN) {
        if (!parse_one_decl(ns, scope, c)) return false;
    }
    return true;
}

void aml_build_namespace(AmlNamespace *ns, const AcpiInfo *acpi) {
    memset(ns, 0, sizeof(*ns));
    ns->root = ns_alloc(ns);
    if (!ns->root) return;
    memcpy(ns->root->name, "\\", 1);
    ns->root->name[1] = '\0';
    ns->root->type = AML_NODE_SCOPE;

    if (!acpi->found) return;
    for (int i = 0; i < acpi->n_blobs; i++) {
        Cursor c = { acpi->blobs[i].aml, acpi->blobs[i].aml + acpi->blobs[i].len };
        parse_term_list(ns, ns->root, &c);
    }
}

AmlNode *aml_find(AmlNamespace *ns, const char *dotted_path) {
    AmlNode *cur = ns->root;
    char seg[5];
    int si = 0;
    for (const char *p = dotted_path; ; p++) {
        if (*p == '.' || *p == '\0') {
            seg[si] = '\0';
            if (si > 0) {
                while (si < 4) seg[si++] = '_';
                seg[4] = '\0';
                cur = find_child(cur, seg);
                if (!cur) return NULL;
            }
            si = 0;
            if (*p == '\0') break;
        } else {
            if (si < 4) seg[si++] = *p;
        }
    }
    return cur;
}

static AmlNode *hid_search(AmlNode *node, const char *hid_str) {
    if (node->type == AML_NODE_DEVICE) {
        AmlNode *hid = find_child(node, "_HID");
        if (hid) {
            if (hid->type == AML_NODE_NAME_STR && hid->data_len > 0) {
                uint32_t n = hid->data_len;
                bool match = true;
                for (uint32_t i = 0; hid_str[i]; i++) {
                    if (i >= n || (char)hid->data_ptr[i] != hid_str[i]) { match = false; break; }
                }
                if (match) return node;
            }
        }
    }
    for (AmlNode *ch = node->first_child; ch; ch = ch->next_sibling) {
        AmlNode *found = hid_search(ch, hid_str);
        if (found) return found;
    }
    return NULL;
}

AmlNode *aml_find_device_by_hid(AmlNamespace *ns, const char *hid_str) {
    return hid_search(ns->root, hid_str);
}

typedef struct {
    AmlNamespace *ns;
    AmlNode      *scope;
    uint64_t      locals[8];
    uint64_t      args[7];
    bool          returned;
    AmlResult     retval;
    int           depth;
} ExecCtx;

#define EXEC_MAX_DEPTH 16

static bool exec_term_list(ExecCtx *ctx, Cursor *c);
static bool eval_term_arg(ExecCtx *ctx, Cursor *c, uint64_t *out);

static bool field_read(AmlNode *field, uint64_t *out) {
    if (!field || field->type != AML_NODE_FIELDUNIT || !field->field_region) return false;
    AmlNode *region = field->field_region;
    if (field->field_bit_offset % 8 != 0) return false;
    uint32_t byte_off_in_region = field->field_bit_offset / 8;

    if (region->region_space == REGION_EC) {
        if (field->field_bit_length != 8) return false;
        uint8_t v;
        if (!ec_read_byte((uint8_t)(region->region_offset + byte_off_in_region), &v)) return false;
        *out = v;
        return true;
    }
    if (region->region_space == REGION_SYSTEMIO) {
        uint16_t port = (uint16_t)(region->region_offset + byte_off_in_region);
        if (field->field_bit_length == 8)  { *out = inb(port); return true; }
        if (field->field_bit_length == 16) { *out = inw(port); return true; }
        if (field->field_bit_length == 32) { *out = inl(port); return true; }
        return false;
    }
    return false;
}

static bool field_write(AmlNode *field, uint64_t value) {
    if (!field || field->type != AML_NODE_FIELDUNIT || !field->field_region) return false;
    AmlNode *region = field->field_region;
    if (field->field_bit_offset % 8 != 0) return false;
    uint32_t byte_off_in_region = field->field_bit_offset / 8;

    if (region->region_space == REGION_EC) {
        if (field->field_bit_length != 8) return false;
        return ec_write_byte((uint8_t)(region->region_offset + byte_off_in_region), (uint8_t)value);
    }
    if (region->region_space == REGION_SYSTEMIO) {
        uint16_t port = (uint16_t)(region->region_offset + byte_off_in_region);
        if (field->field_bit_length == 8)  { outb(port, (uint8_t)value); return true; }
        if (field->field_bit_length == 16) { outw(port, (uint16_t)value); return true; }
        if (field->field_bit_length == 32) { outl(port, (uint32_t)value); return true; }
        return false;
    }
    return false;
}

static bool store_target(ExecCtx *ctx, Cursor *c, uint64_t value) {
    if (!cur_ok(c, 1)) return false;
    uint8_t op = c->p[0];
    if (op == 0x00) { c->p++; return true; }
    if (op >= OP_LOCAL0 && op <= OP_LOCAL0 + 7) { ctx->locals[op - OP_LOCAL0] = value; c->p++; return true; }
    if (op >= OP_ARG0 && op <= OP_ARG0 + 6)     { ctx->args[op - OP_ARG0] = value; c->p++; return true; }

    NamePath path;
    if (!parse_name_string(c, &path)) return false;
    AmlNode *n = resolve_lookup(ctx->ns, ctx->scope, &path);
    if (!n) return false;
    if (n->type == AML_NODE_NAME_INT) { n->int_value = value; return true; }
    if (n->type == AML_NODE_FIELDUNIT) return field_write(n, value);
    return false;
}

static bool eval_name_ref(ExecCtx *ctx, const NamePath *path, uint64_t *out) {
    AmlNode *n = resolve_lookup(ctx->ns, ctx->scope, path);
    if (!n) return false;
    if (n->type == AML_NODE_NAME_INT) { *out = n->int_value; return true; }
    if (n->type == AML_NODE_FIELDUNIT) return field_read(n, out);
    if (n->type == AML_NODE_METHOD) {
        if (ctx->depth >= EXEC_MAX_DEPTH) return false;
        AmlResult r = aml_call_method(ctx->ns, n);
        if (!r.ok || r.is_package) return false;
        *out = r.int_value;
        return true;
    }
    return false;
}

static bool eval_term_arg(ExecCtx *ctx, Cursor *c, uint64_t *out) {
    if (!cur_ok(c, 1)) return false;
    uint8_t op = c->p[0];

    if (op == OP_ZERO) { *out = 0; c->p++; return true; }
    if (op == OP_ONE)  { *out = 1; c->p++; return true; }
    if (op == OP_ONES) { *out = ~(uint64_t)0; c->p++; return true; }
    if (op == OP_BYTEPREFIX || op == OP_WORDPREFIX || op == OP_DWORDPREFIX || op == OP_QWORDPREFIX) {
        return skip_simple_dataobj_int(c, out);
    }
    if (op >= OP_LOCAL0 && op <= OP_LOCAL0 + 7) { *out = ctx->locals[op - OP_LOCAL0]; c->p++; return true; }
    if (op >= OP_ARG0 && op <= OP_ARG0 + 6)     { *out = ctx->args[op - OP_ARG0]; c->p++; return true; }

    if (op == OP_LNOT) {
        c->p++;
        uint64_t a;
        if (!eval_term_arg(ctx, c, &a)) return false;
        *out = (a == 0) ? 1 : 0;
        return true;
    }
    if (op == OP_LAND || op == OP_LOR || op == OP_LEQUAL || op == OP_LGREATER || op == OP_LLESS) {
        c->p++;
        uint64_t a, b;
        if (!eval_term_arg(ctx, c, &a)) return false;
        if (!eval_term_arg(ctx, c, &b)) return false;
        switch (op) {
            case OP_LAND:    *out = (a != 0 && b != 0) ? 1 : 0; break;
            case OP_LOR:     *out = (a != 0 || b != 0) ? 1 : 0; break;
            case OP_LEQUAL:  *out = (a == b) ? 1 : 0; break;
            case OP_LGREATER:*out = (a >  b) ? 1 : 0; break;
            default:         *out = (a <  b) ? 1 : 0; break;
        }
        return true;
    }
    if (op == OP_ADD || op == OP_SUBTRACT || op == OP_MULTIPLY ||
        op == OP_AND || op == OP_OR || op == OP_SHL || op == OP_SHR) {
        c->p++;
        uint64_t a, b;
        if (!eval_term_arg(ctx, c, &a)) return false;
        if (!eval_term_arg(ctx, c, &b)) return false;
        uint64_t r;
        switch (op) {
            case OP_ADD:      r = a + b; break;
            case OP_SUBTRACT: r = a - b; break;
            case OP_MULTIPLY: r = a * b; break;
            case OP_AND:      r = a & b; break;
            case OP_OR:       r = a | b; break;
            case OP_SHL:      r = a << (b & 63); break;
            default:          r = a >> (b & 63); break;
        }
        if (!store_target(ctx, c, r)) return false;
        *out = r;
        return true;
    }
    if (op == OP_STRINGPREFIX || op == OP_BUFFER || op == OP_PACKAGE || op == OP_VARPACKAGE) {
        return false;
    }

    NamePath path;
    Cursor probe = *c;
    if (!parse_name_string(&probe, &path)) return false;
    if (!eval_name_ref(ctx, &path, out)) return false;
    *c = probe;
    return true;
}

static bool eval_return_package(ExecCtx *ctx, Cursor *c, AmlResult *out) {
    c->p++;
    uint32_t len; const uint8_t *data_start;
    if (!decode_pkglength(c, &len, &data_start)) return false;
    const uint8_t *block_end = data_start + len;
    if (block_end > c->end || block_end < data_start) return false;

    Cursor pc = { data_start, block_end };
    out->is_package = true;
    out->pkg_count = 0;
    if (cur_ok(&pc, 1)) {
        pc.p += 1;
        while (pc.p < pc.end && out->pkg_count < AML_MAX_PKG_ELEMS) {
            uint64_t v;
            if (!eval_term_arg(ctx, &pc, &v)) break;
            out->pkg_ints[out->pkg_count++] = v;
        }
    }
    c->p = (uint8_t *)block_end;
    return true;
}

static bool exec_one_stmt(ExecCtx *ctx, Cursor *c) {
    if (!cur_ok(c, 1)) return false;
    uint8_t op = c->p[0];

    if (op == OP_RETURN) {
        c->p++;
        if (!cur_ok(c, 1)) return false;
        if (c->p[0] == OP_PACKAGE || c->p[0] == OP_VARPACKAGE) {
            AmlResult r; memset(&r, 0, sizeof(r)); r.ok = true;
            if (!eval_return_package(ctx, c, &r)) return false;
            ctx->retval = r;
        } else {
            uint64_t v;
            if (!eval_term_arg(ctx, c, &v)) return false;
            ctx->retval.ok = true;
            ctx->retval.is_package = false;
            ctx->retval.int_value = v;
        }
        ctx->returned = true;
        return true;
    }

    if (op == OP_STORE) {
        c->p++;
        uint64_t v;
        if (!eval_term_arg(ctx, c, &v)) return false;
        return store_target(ctx, c, v);
    }

    if (op == OP_IF) {
        c->p++;
        uint32_t len; const uint8_t *data_start;
        if (!decode_pkglength(c, &len, &data_start)) return false;
        const uint8_t *block_end = data_start + len;
        if (block_end > c->end || block_end < data_start) return false;
        Cursor body = { data_start, block_end };
        uint64_t pred;
        if (!eval_term_arg(ctx, &body, &pred)) return false;

        bool has_else = false;
        const uint8_t *else_start = NULL, *else_end = NULL;
        if (block_end < c->end && block_end[0] == OP_ELSE) {
            Cursor ec2 = { block_end + 1, c->end };
            uint32_t elen; const uint8_t *edata;
            if (decode_pkglength(&ec2, &elen, &edata)) {
                has_else = true;
                else_start = edata;
                else_end = edata + elen;
            }
        }

        if (pred != 0) {
            if (!exec_term_list(ctx, &body)) return false;
        } else if (has_else) {
            Cursor ebody = { else_start, else_end };
            if (!exec_term_list(ctx, &ebody)) return false;
        }
        c->p = (uint8_t *)(has_else ? else_end : block_end);
        return true;
    }

    if (op == OP_NOTIFY) {
        c->p++;
        NamePath path;
        if (!parse_name_string(c, &path)) return false;
        uint64_t v;
        return eval_term_arg(ctx, c, &v);
    }

    if (op == OP_NAME) {
        return parse_one_decl(ctx->ns, ctx->scope, c);
    }

    return false;
}

static bool exec_term_list(ExecCtx *ctx, Cursor *c) {
    int guard = 0;
    while (c->p < c->end && !ctx->returned && guard++ < AML_MAX_CHILDREN_SCAN) {
        if (!exec_one_stmt(ctx, c)) return false;
    }
    return true;
}

AmlResult aml_call_method(AmlNamespace *ns, AmlNode *method) {
    AmlResult r; memset(&r, 0, sizeof(r));
    if (!method || method->type != AML_NODE_METHOD) return r;

    ExecCtx ctx; memset(&ctx, 0, sizeof(ctx));
    ctx.ns = ns;
    ctx.scope = method->parent ? method->parent : ns->root;
    ctx.depth = 0;

    Cursor body = { method->method_start, method->method_start + method->method_len };
    bool ok = exec_term_list(&ctx, &body);
    if (!ok || !ctx.returned) {
        r.ok = false;
        return r;
    }
    r = ctx.retval;
    r.ok = true;
    return r;
}
