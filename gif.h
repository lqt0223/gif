#include <fstream>
#include <string>

// logical screen descriptor
typedef struct lsd {
    uint16_t w;
    uint16_t h;
    bool has_gct; // global color table
    uint8_t cr; // color resolution
    bool sort;
    uint16_t gct_sz;
    uint8_t bci; // background color index
    uint8_t par; // pixel aspect ratio
} lsd_t;


class Gif {
private:
    std::ifstream stream;
public:
    lsd_t lsd;
    uint8_t* gct;
    uint16_t loop;

    Gif(std::string filename);
};
