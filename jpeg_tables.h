// these are typicals tables are from spec
// see https://www.w3.org/Graphics/JPEG/itu-t81.pdf annex K

#ifndef JPEG_TABLES
#define JPEG_TABLES
#include <cstdint>
#include <map>
#include <utility>
// annex K p143
const std::uint8_t qt_luma[] = {
	16,	11,	10,	16,	24,	40,	51,	61,
 	12,	12,	14,	19,	26,	58,	60,	55,
        14,	13,	16,	24,	40,	57,	69,	56,   
        14,	17,	22,	29,	51,	87,	80,	62,   
        18,	22,	37,	56,	68,	109,	103,	77,   
        24,	35,	55,	64,	81,	104,	113,	92,   
        49,	64,	78,	87,	103,	121,	120,	101,   
        72,	92,	95,	98,	112,	100,	103,	99,
};

const std::uint8_t qt_chroma[] = {
	17,	18,	24,	47,	99,	99,	99,	99,
 	18,	21,	26,	66,	99,	99,	99,	99,
        24,	26,	56,	99,	99,	99,	99,	99,   
        47,	66,	99,	99,	99,	99,	99,	99,   
        99,	99,	99,	99,	99,	99,	99,	99,   
        99,	99,	99,	99,	99,	99,	99,	99,   
        99,	99,	99,	99,	99,	99,	99,	99,   
        99,	99,	99,	99,	99,	99,	99,	99,
};

// a macro for defining entries in huffman table
#define htdc(category, code_length, code) { category, { code_length, 0b##code } }
#define htac(rrrr, ssss, code_length, code) { (rrrr<<4) | ssss, { code_length, 0b##code } }

typedef std::pair<uint8_t, std::pair<uint8_t, uint16_t>> huffman_table_entry_t;
typedef std::map<uint8_t, std::pair<uint8_t, uint16_t>> huffman_table_t;
// the rrrrssss to huffman code mapping
// rrrrssss is packed in one byte

// annex K p149
const huffman_table_t ht_luma_dc = {
    htdc(0,2,00),
    htdc(1,3,010),
    htdc(2,3,011),
    htdc(3,3,100),
    htdc(4,3,101),
    htdc(5,3,110),
    htdc(6,4,1110),
    htdc(7,5,11110),
    htdc(8,6,111110),
    htdc(9,7,1111110),
    htdc(10,8,11111110),
    htdc(11,9,111111110),
};
const huffman_table_t ht_chroma_dc = {
    htdc(0,2,00),
    htdc(1,2,01),
    htdc(2,2,10),
    htdc(3,3,110),
    htdc(4,4,1110),
    htdc(5,5,11110),
    htdc(6,6,111110),
    htdc(7,7,1111110),
    htdc(8,8,11111110),
    htdc(9,9,111111110),
    htdc(10,10,1111111110),
    htdc(11,11,11111111110),
};

// annex K p150
const huffman_table_t ht_luma_ac = {
    htac(0,0,4,1010), // EOB

    htac(0,1,2,00),
    htac(0,2,2,01),
    htac(0,3,3,100),
    htac(0,4,4,1011),
    htac(0,5,5,11010),
    htac(0,6,7,1111000),
    htac(0,7,8,11111000),
    htac(0,8,10,1111110110),
    htac(0,9,16,1111111110000010),
    htac(0,10,16,1111111110000011),

    htac(1,1,4,1100),
    htac(1,2,5,11011),
    htac(1,3,7,1111001),
    htac(1,4,9,111110110),
    htac(1,5,11,11111110110),
    htac(1,6,16,1111111110000100),
    htac(1,7,16,1111111110000101),
    htac(1,8,16,1111111110000110),
    htac(1,9,16,1111111110000111),
    htac(1,10,16,1111111110001000),

    htac(2,1,5,11100),
    htac(2,2,8,11111001),
    htac(2,3,10,1111110111),
    htac(2,4,12,111111110100),
    htac(2,5,16,1111111110001001),
    htac(2,6,16,1111111110001010),
    htac(2,7,16,1111111110001011),
    htac(2,8,16,1111111110001100),
    htac(2,9,16,1111111110001101),
    htac(2,10,16,1111111110001110),

    htac(3,1,6,111010),
    htac(3,2,9,111110111),
    htac(3,3,12,111111110101),
    htac(3,4,16,1111111110001111),
    htac(3,5,16,1111111110010000),
    htac(3,6,16,1111111110010001),
    htac(3,7,16,1111111110010010),
    htac(3,8,16,1111111110010011),
    htac(3,9,16,1111111110010100),
    htac(3,10,16,1111111110010101),

    htac(4,1,6,111011),
    htac(4,2,10,1111111000),
    htac(4,3,16,1111111110010110),
    htac(4,4,16,1111111110010111),
    htac(4,5,16,1111111110011000),
    htac(4,6,16,1111111110011001),
    htac(4,7,16,1111111110011010),
    htac(4,8,16,1111111110011011),
    htac(4,9,16,1111111110011100),
    htac(4,10,16,1111111110011101),

    htac(5,1,7,1111010),
    htac(5,2,11,11111110111),
    htac(5,3,16,1111111110011110),
    htac(5,4,16,1111111110011111),
    htac(5,5,16,1111111110100000),
    htac(5,6,16,1111111110100001),
    htac(5,7,16,1111111110100010),
    htac(5,8,16,1111111110100011),
    htac(5,9,16,1111111110100100),
    htac(5,10,16,1111111110100101),

    htac(6,1,7,1111011),
    htac(6,2,12,111111110110),
    htac(6,3,16,1111111110100110),
    htac(6,4,16,1111111110100111),
    htac(6,5,16,1111111110101000),
    htac(6,6,16,1111111110101001),
    htac(6,7,16,1111111110101010),
    htac(6,8,16,1111111110101011),
    htac(6,9,16,1111111110101100),
    htac(6,10,16,1111111110101101),

    htac(7,1,8,11111010),
    htac(7,2,12,111111110111),
    htac(7,3,16,1111111110101110),
    htac(7,4,16,1111111110101111),
    htac(7,5,16,1111111110110000),
    htac(7,6,16,1111111110110001),
    htac(7,7,16,1111111110110010),
    htac(7,8,16,1111111110110011),
    htac(7,9,16,1111111110110100),
    htac(7,10,16,1111111110110101),

    htac(8,1,9,111111000),
    htac(8,2,15,111111111000000),
    htac(8,3,16,1111111110110110),
    htac(8,4,16,1111111110110111),
    htac(8,5,16,1111111110111000),
    htac(8,6,16,1111111110111001),
    htac(8,7,16,1111111110111010),
    htac(8,8,16,1111111110111011),
    htac(8,9,16,1111111110111100),
    htac(8,10,16,1111111110111101),

    htac(9,1,9,111111001),
    htac(9,2,16,1111111110111110),
    htac(9,3,16,1111111110111111),
    htac(9,4,16,1111111111000000),
    htac(9,5,16,1111111111000001),
    htac(9,6,16,1111111111000010),
    htac(9,7,16,1111111111000011),
    htac(9,8,16,1111111111000100),
    htac(9,9,16,1111111111000101),
    htac(9,10,16,1111111111000110),

    htac(10,1,9,111111010),
    htac(10,2,16,1111111111000111),
    htac(10,3,16,1111111111001000),
    htac(10,4,16,1111111111001001),
    htac(10,5,16,1111111111001010),
    htac(10,6,16,1111111111001011),
    htac(10,7,16,1111111111001100),
    htac(10,8,16,1111111111001101),
    htac(10,9,16,1111111111001110),
    htac(10,10,16,1111111111001111),

    htac(11,1,10,1111111001),
    htac(11,2,16,1111111111010000),
    htac(11,3,16,1111111111010001),
    htac(11,4,16,1111111111010010),
    htac(11,5,16,1111111111010011),
    htac(11,6,16,1111111111010100),
    htac(11,7,16,1111111111010101),
    htac(11,8,16,1111111111010110),
    htac(11,9,16,1111111111010111),
    htac(11,10,16,1111111111011000),

    htac(12,1,10,1111111010),
    htac(12,2,16,1111111111011001),
    htac(12,3,16,1111111111011010),
    htac(12,4,16,1111111111011011),
    htac(12,5,16,1111111111011100),
    htac(12,6,16,1111111111011101),
    htac(12,7,16,1111111111011110),
    htac(12,8,16,1111111111011111),
    htac(12,9,16,1111111111100000),
    htac(12,10,16,1111111111100001),

    htac(13,1,11,11111111000),
    htac(13,2,16,1111111111100010),
    htac(13,3,16,1111111111100011),
    htac(13,4,16,1111111111100100),
    htac(13,5,16,1111111111100101),
    htac(13,6,16,1111111111100110),
    htac(13,7,16,1111111111100111),
    htac(13,8,16,1111111111101000),
    htac(13,9,16,1111111111101001),
    htac(13,10,16,1111111111101010),

    htac(14,1,16,1111111111101011),
    htac(14,2,16,1111111111101100),
    htac(14,3,16,1111111111101101),
    htac(14,4,16,1111111111101110),
    htac(14,5,16,1111111111101111),
    htac(14,6,16,1111111111110000),
    htac(14,7,16,1111111111110001),
    htac(14,8,16,1111111111110010),
    htac(14,9,16,1111111111110011),
    htac(14,10,16,1111111111110100),

    htac(15,0,11,11111111001), // ZRL

    htac(15,1,16,1111111111110101),
    htac(15,2,16,1111111111110110),
    htac(15,3,16,1111111111110111),
    htac(15,4,16,1111111111111000),
    htac(15,5,16,1111111111111001),
    htac(15,6,16,1111111111111010),
    htac(15,7,16,1111111111111011),
    htac(15,8,16,1111111111111100),
    htac(15,9,16,1111111111111101),
    htac(15,10,16,1111111111111110),
};

// annex K p154
const huffman_table_t ht_chroma_ac = {
    htac(0,0,2,00), // EOB

    htac(0,1,2,01),
    htac(0,2,3,100),
    htac(0,3,4,1010),
    htac(0,4,5,11000),
    htac(0,5,5,11001),
    htac(0,6,6,111000),
    htac(0,7,7,1111000),
    htac(0,8,9,111110100),
    htac(0,9,10,1111110110),
    htac(0,10,12,111111110100),

    htac(1,1,4,1011),
    htac(1,2,6,111001),
    htac(1,3,8,11110110),
    htac(1,4,9,111110101),
    htac(1,5,11,11111110110),
    htac(1,6,12,111111110101),
    htac(1,7,16,1111111110001000),
    htac(1,8,16,1111111110001001),
    htac(1,9,16,1111111110001010),
    htac(1,10,16,1111111110001011),

    htac(2,1,5,11010),
    htac(2,2,8,11110111),
    htac(2,3,10,1111110111),
    htac(2,4,12,111111110110),
    htac(2,5,15,111111111000010),
    htac(2,6,16,1111111110001100),
    htac(2,7,16,1111111110001101),
    htac(2,8,16,1111111110001110),
    htac(2,9,16,1111111110001111),
    htac(2,10,16,1111111110010000),

    htac(3,1,5,11011),
    htac(3,2,8,11111000),
    htac(3,3,10,1111111000),
    htac(3,4,12,111111110111),
    htac(3,5,16,1111111110010001),
    htac(3,6,16,1111111110010010),
    htac(3,7,16,1111111110010011),
    htac(3,8,16,1111111110010100),
    htac(3,9,16,1111111110010101),
    htac(3,10,16,1111111110010110),

    htac(4,1,6,111010),
    htac(4,2,9,111110110),
    htac(4,3,16,1111111110010111),
    htac(4,4,16,1111111110011000),
    htac(4,5,16,1111111110011001),
    htac(4,6,16,1111111110011010),
    htac(4,7,16,1111111110011011),
    htac(4,8,16,1111111110011100),
    htac(4,9,16,1111111110011101),
    htac(4,10,16,1111111110011110),

    htac(5,1,6,111011),
    htac(5,2,10,1111111001),
    htac(5,3,16,1111111110011111),
    htac(5,4,16,1111111110100000),
    htac(5,5,16,1111111110100001),
    htac(5,6,16,1111111110100010),
    htac(5,7,16,1111111110100011),
    htac(5,8,16,1111111110100100),
    htac(5,9,16,1111111110100101),
    htac(5,10,16,1111111110100110),

    htac(6,1,7,1111001),
    htac(6,2,11,11111110111),
    htac(6,3,16,1111111110100111),
    htac(6,4,16,1111111110101000),
    htac(6,5,16,1111111110101001),
    htac(6,6,16,1111111110101010),
    htac(6,7,16,1111111110101011),
    htac(6,8,16,1111111110101100),
    htac(6,9,16,1111111110101101),
    htac(6,10,16,1111111110101110),

    htac(7,1,7,1111010),
    htac(7,2,11,11111111000),
    htac(7,3,16,1111111110101111),
    htac(7,4,16,1111111110110000),
    htac(7,5,16,1111111110110001),
    htac(7,6,16,1111111110110010),
    htac(7,7,16,1111111110110011),
    htac(7,8,16,1111111110110100),
    htac(7,9,16,1111111110110101),
    htac(7,10,16,1111111110110110),

    htac(8,1,8,11111001),
    htac(8,2,16,1111111110110111),
    htac(8,3,16,1111111110111000),
    htac(8,4,16,1111111110111001),
    htac(8,5,16,1111111110111010),
    htac(8,6,16,1111111110111011),
    htac(8,7,16,1111111110111100),
    htac(8,8,16,1111111110111101),
    htac(8,9,16,1111111110111110),
    htac(8,10,16,1111111110111111),

    htac(9,1,9,111110111),
    htac(9,2,16,1111111111000000),
    htac(9,3,16,1111111111000001),
    htac(9,4,16,1111111111000010),
    htac(9,5,16,1111111111000011),
    htac(9,6,16,1111111111000100),
    htac(9,7,16,1111111111000101),
    htac(9,8,16,1111111111000110),
    htac(9,9,16,1111111111000111),
    htac(9,10,16,1111111111001000),

    htac(10,1,9,111111000),
    htac(10,2,16,1111111111001001),
    htac(10,3,16,1111111111001010),
    htac(10,4,16,1111111111001011),
    htac(10,5,16,1111111111001100),
    htac(10,6,16,1111111111001101),
    htac(10,7,16,1111111111001110),
    htac(10,8,16,1111111111001111),
    htac(10,9,16,1111111111010000),
    htac(10,10,16,1111111111010001),

    htac(11,1,9,111111001),
    htac(11,2,16,1111111111010010),
    htac(11,3,16,1111111111010011),
    htac(11,4,16,1111111111010100),
    htac(11,5,16,1111111111010101),
    htac(11,6,16,1111111111010110),
    htac(11,7,16,1111111111010111),
    htac(11,8,16,1111111111011000),
    htac(11,9,16,1111111111011001),
    htac(11,10,16,1111111111011010),

    htac(12,1,9,111111010),
    htac(12,2,16,1111111111011011),
    htac(12,3,16,1111111111011100),
    htac(12,4,16,1111111111011101),
    htac(12,5,16,1111111111011110),
    htac(12,6,16,1111111111011111),
    htac(12,7,16,1111111111100000),
    htac(12,8,16,1111111111100001),
    htac(12,9,16,1111111111100010),
    htac(12,10,16,1111111111100011),

    htac(13,1,11,11111111001),
    htac(13,2,16,1111111111100100),
    htac(13,3,16,1111111111100101),
    htac(13,4,16,1111111111100110),
    htac(13,5,16,1111111111100111),
    htac(13,6,16,1111111111101000),
    htac(13,7,16,1111111111101001),
    htac(13,8,16,1111111111101010),
    htac(13,9,16,1111111111101011),
    htac(13,10,16,1111111111101100),

    htac(14,1,14,11111111100000),
    htac(14,2,16,1111111111101101),
    htac(14,3,16,1111111111101110),
    htac(14,4,16,1111111111101111),
    htac(14,5,16,1111111111110000),
    htac(14,6,16,1111111111110001),
    htac(14,7,16,1111111111110010),
    htac(14,8,16,1111111111110011),
    htac(14,9,16,1111111111110100),
    htac(14,10,16,1111111111110101),

    htac(15,0,10,1111111010), // FZL

    htac(15,1,15,111111111000011),
    htac(15,2,16,1111111111110110),
    htac(15,3,16,1111111111110111),
    htac(15,4,16,1111111111111000),
    htac(15,5,16,1111111111111001),
    htac(15,6,16,1111111111111010),
    htac(15,7,16,1111111111111011),
    htac(15,8,16,1111111111111100),
    htac(15,9,16,1111111111111101),
    htac(15,10,16,1111111111111110),
};
#endif
