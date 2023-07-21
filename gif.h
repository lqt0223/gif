#include <cstdint>
#include <string>
#include <map>

// declare bit fields such that msb is at last
typedef struct {
    uint8_t gct_sz: 3;
    uint8_t sort: 1;
    uint8_t cr: 3; // color resolution
    uint8_t has_gct: 1;
} lsd_packed_t;

// logical screen descriptor
typedef union __attribute__((__packed__)) {
    uint8_t raw[7];
    struct __attribute__((__packed__)) {
        uint16_t w;
        uint16_t h;
        lsd_packed_t packed;
        uint8_t bci; // background color index
        uint8_t par; // pixel aspect ratio
    };
} lsd_t;

typedef struct {
    uint8_t transparent: 1;
    uint8_t user_input: 1;
    uint8_t disposal: 3;
    uint8_t  : 3; // unused
} gce_packed_t;

// graphic control extension
typedef union {
    uint8_t raw[4];
    struct __attribute__((__packed__)) {
        gce_packed_t packed;
        uint16_t delay;
        uint8_t tran_index;
    };
} gce_t;

typedef struct {
    uint8_t lct_sz: 3;
    uint8_t  : 2; // unused
    uint8_t sort: 1;
    uint8_t interlace: 1;
    uint8_t has_lct: 1;
} image_desc_packed_t;

// image descriptor
typedef union {
    uint8_t raw[9];
    struct __attribute__((__packed__)) {
        uint16_t l;
        uint16_t t;
        uint16_t w;
        uint16_t h;
        image_desc_packed_t packed;
    };
} image_desc_t;

typedef struct ppm_t {
    std::string header;
    std::string buffer;
    uint16_t width;
    uint16_t height;
} ppm_t;

template<typename K, typename V>
class CodeTable {
public:
    std::map<K, V> _map;

    void clear() {
        _map.clear();
    }

    void insert(K codes) {
        size_t next_code = _map.size();
        _map[codes] = next_code;
    }

    bool has(K codes) {
        return _map.contains(codes);
    }

    size_t size() {
        return _map.size();
    }

    V get(K codes) {
        return _map.at(codes);
    }

    // for debug
//    void get_r(V coded) {
//        for (pair<K, V> kv: _map) {
//            auto [k, v] = kv;
//            if (v == coded) {
//                size_t sz = k.size();
//                printf("debug %d => ", coded);
//                for (size_t i = 0; i < sz; i++) {
//                    printf("%d,", k[i]);
//                }
//                printf("\n");
//            }
//        }
//    }
};


