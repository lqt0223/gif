const td = new TextDecoder()
async function readPPM(path: string) {
  const uint8 = (await Deno.readFile(path)) as Uint8Array
  const delimiters: number[] = []
  let i = 0
  while (1) {
    const byte = uint8[i]
    if (byte === 10 || byte === 32) { // space or line-wrap
      delimiters.push(i)
    }
    if (delimiters.length >= 4) {
      break
    }
    i++
  } 
  const header = td.decode(uint8.subarray(0, delimiters[0])) 
  const width  = td.decode(uint8.subarray(delimiters[0] + 1, delimiters[1]))
  const height = td.decode(uint8.subarray(delimiters[1] + 1, delimiters[2]))
  const color_max = td.decode(uint8.subarray(delimiters[2] + 1, delimiters[3]))
  const buffer = uint8.subarray(delimiters[3] + 1)
  return {
    header,
    color_max,
    width: +width,
    height: +height,
    buffer,
  }
}

const sameArr = (arr1: number[], arr2: number[]) => {
  if (arr1.length !== arr2.length) {
    return false
  }
  for (let i = 0; i < arr1.length; i++) {
    if (arr1[i] !== arr2[i]) {
      return false
    }
  }
  return true
}

// Trie impl
class TrieNode {
  coded?: number
  children: Map<number, TrieNode> = new Map()
}
class CodeTable2 {
  root: TrieNode = new TrieNode()
  _size: number = 0
  clear() {
    this.root = new TrieNode()
    this._size = 0
  }
  insert(codes: number[]) {
    const nextCode = this.size()
    this.set(codes, nextCode)
  }
  size() {
    return this._size
  }
  has(codes: number[]) {
    const a = this.get(codes)
    return a!== undefined
  }
  get(codes: number[]) {
    let current = this.root
    for (const code of codes) {
      const next = current.children.get(code)
      if (next) {
        current = next
      } else {
        return undefined
      }
    }
    if (current) {
      return current.coded
    }
    return undefined
  }
  set(codes: number[], coded: number) {
    let current = this.root
    for (const code of codes) {
      if (!current.children.has(code)) {
        const newNode = new TrieNode()
        current.children.set(code, newNode)
        current = newNode
      } else {
        current = current.children.get(code)!
      }
    }
    current.coded = coded
    this._size++
  }
}

class CodeTable {
  // the key of map is a big number from one or more u8 numbers
  map: Map<string, number>[] = []
  clear() {
    this.map = []
  }
  insert(codes: number[]) {
    const nextCode = this.size()
    this.set(codes, nextCode)
  }
  size() {
    let result = 0
    for (const submap of this.map) {
      result += submap?.size || 0
    }
    return result
  }
  has(codes: number[]) {
    if (!this.map[codes.length]) this.map[codes.length] = new Map()
    const key = codes.join(',')
    return this.map[codes.length].has(key)
  }
  get(codes: number[]) {
    const key = codes.join(',')
    return this.map[codes.length].get(key)
  }
  set(codes: number[], coded: number) {
    if (!this.map[codes.length]) this.map[codes.length] = new Map()
    const key = codes.join(',')
    this.map[codes.length].set(key, coded)
  }
}

// predefined color table
const code_table = new CodeTable2()

function toU8LE(u16: number) {
  const u81 = u16 & (0xff)
  const u82 = (u16 >> 8) & (0xff)
  return [u81, u82]
}

function init_code_table() {
  code_table.clear()
  for (let i = 0; i < 256; i++) {
    code_table.insert([i])
  }
}

const color_mapping = new Map<number, number> // single color to index mapping (this is not cleared)
/* generate a 256 colored (close-to) color_mapping */
let i = 0
for (let b = 0; b < 256; b+=42) {
  for (let g = 0; g < 256; g+=51) {
    for (let r = 0; r < 256; r+=51) {
      const rgb = (r<<16)|(g<<8)|b
      color_mapping.set(rgb, i)
      i++
    }
  }
}

function gif_encode(rgbs: Uint8Array, w: number, h: number) {
  const output: number[] = []
  init_code_table()
  // quantize colors and get single rgb value from file 
  const color_indexes: number[] = []
  for (let i = 0; i < rgbs.length; i+=3) {
    let r = rgbs[i + 0]
    let g = rgbs[i + 1]
    let b = rgbs[i + 2]
    console.log(r,g,b)
    r -= r % 51
    g -= g % 51
    b -= b % 42
    const rgb = (r<<16)|(g<<8)|b
    const index = color_mapping.get(rgb)!
    color_indexes.push(index)
  }

  // header
  output.push(...('GIF89a').split('').map(a => a.charCodeAt(0)))
  // lsd
  // width, height
  output.push(
    ...toU8LE(w),
    ...toU8LE(h),
  )
  // packed lsd
  const gct_bit = 1
  const color_resolution_bits = 7
  const sort_bit = 0
  const gct_sz = 7
  const lsd_packed = (gct_bit << 7) | (color_resolution_bits << 4) | (sort_bit << 3) | (gct_sz)
  output.push(lsd_packed)
  // bci and aspect_Ratio
  output.push(0, 0) // b

  // global color table 252
  for (const color of color_mapping.keys()) {
    const r = (color & 0xff0000) >> 16
    const g = (color & 0x00ff00) >> 8
    const b = (color & 0x0000ff)
    output.push(r,g,b)
  }
  // + 4 blacks for padding
  for (let i = 0; i < 12; i++) {
    output.push(0x00)
  }

  // graphic control extension
  output.push(0x21, 0xf9, 0x04)
  // packed gce
  const disposal_bit = 0
  const user_input_bit = 0
  const transparent_color_bit = 0
  const gce_packed = (disposal_bit << 5) | (user_input_bit << 2) | (transparent_color_bit << 1)
  output.push(gce_packed)
  output.push(0x0, 0x0) // delay time
  output.push(0x0) // transparent_color_index
  output.push(0x0) // block terminator

  // image descriptor
  // left top width height
  output.push(
    0x2c,
    ...toU8LE(0),
    ...toU8LE(0),
    ...toU8LE(w),
    ...toU8LE(h),
  )
  // packed id
  const lct_bit = 0
  const interlace_bit = 0
  const lct_sort_bit = 0
  const lct_sz = 0
  const id_packed = (lct_bit << 7) | (interlace_bit << 6) | (lct_sort_bit << 1) | (lct_sz)
  output.push(id_packed)
  // compress data with lzw and append here
  const min_code_size = 8
  output.push(min_code_size)

  const compressed_unpacked = lzw_encode(color_indexes)
  const compressed = lzw_pack(compressed_unpacked)
  // data, block terminator and trailer(end of gif)
  const compressed_chunks = make_chunk(compressed, 255)
  for (const chunk of compressed_chunks) {
    output.push(chunk.length, ...chunk)
  }
  output.push(0x00, 0x3b)
  return new Uint8Array(output)
}

function make_chunk<T>(arr: T[], chunk_size: number): T[][] {
  const chunks: T[][] = []
  const num_of_chunks = Math.ceil(arr.length / chunk_size)
  for (let i = 0; i < num_of_chunks; i++) {
    const chunk: T[] = []
    for (let j = 0; j < chunk_size; j++) {
      const item = arr[i*chunk_size+j]
      item !== undefined && chunk.push(item)
    }
    chunks.push(chunk)
  }
  return chunks
}


function lzw_encode(cs: number[]) {
  const min_code_size = 8
  const clear_code = 2**min_code_size 
  const end_code = clear_code + 1
  code_table.insert([clear_code])
  code_table.insert([end_code])

  const color_indexes = cs.slice() // copied colors
  let prev: number[] = []
  let code_size = min_code_size + 1
  const output: ({ code: number, code_size: number })[] = []
  output.push({ code: clear_code, code_size })
  const first = color_indexes.shift()!
  prev = [first]
  // console.log(color_indexes.length)
  // for (const [i, a] of color_indexes.entries()) {
  //   console.log(i, a)
  // } 

  let k = 0;
  while (color_indexes.length) {
    // add clear code and reset color table here
    if (code_table.size() === 4096) { // MAX_SIZE
      init_code_table()
      code_table.insert([clear_code])
      code_table.insert([end_code])
      output.push({ code: clear_code, code_size })
      code_size = min_code_size + 1
    }

    const next = color_indexes.shift()!
    const comb = [...prev, next]
    let s1 = prev.length;
    if (code_table.has(comb)) {
      prev.push(next)
      // console.log(k, s1, next, prev.length);
      k++;
    } else {
      const code = code_table.get(prev)!
      output.push({ code, code_size })
      // raise the code_size here
      if (code_table.size() === 2**code_size) {
        code_size++
      }
      // add newcode intro color map
      code_table.insert(comb)
      prev = [next]
    }
  }
  if (prev.length) {
    // console.log(prev, prev.length)
    const code = code_table.get(prev)!
    output.push({ code, code_size })
  }
  output.push({ code: end_code, code_size })
  // for (const [i, item] of output.entries()) {
  //   console.log(i, item.code, item.code_size)
  // }
  return output
}

// pack codes by their code value and code size, output a more compact bytestream
function lzw_pack(codes: ({ code: number, code_size: number })[]) {
  const output: number[] = []
  let nbit = 0
  for (const item of codes) {
    const { code, code_size } = item
    // the bit position - amount to shift left
    const shl = nbit % 8
    // the index position of bytestream where the byte output is emitted
    const index = Math.floor(nbit / 8)
    if (!output[index]) output[index] = 0
    // fill in the bits of code onto the right position
    output[index] |= (code << shl)
    // ensure that the result is not greater than a byte
    output[index] &= 0xff
    // if there is any bits that go past the next byte
    const pass_next_byte = shl + code_size > 8
    if (pass_next_byte) {
      // output remaining bits onto next byte
      const remain_bits = code >> (8 - shl)
      if (!output[index + 1]) output[index + 1] = 0
      output[index + 1] |=  remain_bits
    }
    // if there is any bits that go past the next next byte
    const pass_next_next_byte = shl + code_size > 16
    if (pass_next_next_byte) {
      // output remaining bits onto next byte
      const remain_bits = code >> (16 - shl)
      if (!output[index + 2]) output[index + 2] = 0
      output[index + 2] |=  remain_bits
    }
    nbit += code_size
  }
  // for (let i = 0; i < output.length; i++) {
  //   console.log(output[i])
  // }
  return output
}

const { width, height, buffer } = await readPPM(Deno.args[0])
const result = gif_encode(buffer, width, height)
Deno.writeAllSync(Deno.stdout, result)

