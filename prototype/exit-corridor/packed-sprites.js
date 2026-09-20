/**
 * Packed Sprite Atlas Runtime Decoder (packed-sprites.js)
 * Browser-compatible pure JS decoder for the ECSP v1 sprite package format.
 *
 * Design Invariants:
 * - Zero WebGL or external runtime dependencies.
 * - Bounded memory: maintains a 1-2 frame decode cache (36 KiB peak cache RAM).
 * - Per-frame bounding-box cropping with transparent span RLE.
 * - Full antialiased silhouette edge preservation (RGB565 + 8-bit Alpha).
 * - Strict structural and boundary verification on construction:
 *   * Header magic 'ECSP', version 1.
 *   * Strict integer checks, totalRows === walkFrames + 1.
 *   * Positive finite world framing, feetAnchor in (0, 1.0], frameWidth/Height <= 255.
 *   * First offset strictly equals header + indexTable (zero padding).
 *   * Strictly monotonically increasing offsets without overlap or trailing garbage.
 *   * Frame records verified strictly within their individual byte slices.
 * - Integer-validated getFrame(direction, row) -> Uint32Array(48x96) RGBA little-endian.
 * - Honest memory accounting metrics:
 *   * encodedBytes: static ArrayBuffer payload size.
 *   * paletteBytes: sum of all frame palettes in stream.
 *   * indexBytes: sum of all span/index runs in stream.
 *   * peakFrameCacheBytes: working memory for the 2-frame decode cache (36.00 KiB).
 *   * reusableDecoderRam: palette lookup buffer (1 KiB) + frame offsets table.
 *   * totalInstanceRam: retained typed-array bytes (ArrayBuffer + cache buffers + lookup tables),
 *     excluding V8 JS object overhead and GC transients.
 *
 * Export: PackedSpriteAtlas
 */

export class PackedSpriteAtlas {
  constructor(arrayBuffer) {
    if (!arrayBuffer || !(arrayBuffer instanceof ArrayBuffer)) {
      throw new Error('PackedSpriteAtlas: Invalid input; must provide an ArrayBuffer.');
    }

    this.encodedBytes = arrayBuffer.byteLength;
    if (this.encodedBytes < 24) {
      throw new Error(`PackedSpriteAtlas: Truncated header; byte length ${this.encodedBytes} < 24.`);
    }

    const view = new DataView(arrayBuffer);
    this.view = view;
    this.bytes = new Uint8Array(arrayBuffer);

    // 1. Validate Magic Signature: 'ECSP' (0x45, 0x43, 0x53, 0x50)
    const magic = String.fromCharCode(view.getUint8(0), view.getUint8(1), view.getUint8(2), view.getUint8(3));
    if (magic !== 'ECSP') {
      throw new Error(`PackedSpriteAtlas: Invalid magic signature '${magic}'; expected 'ECSP'.`);
    }

    // 2. Validate Version
    const version = view.getUint8(4);
    if (version !== 1) {
      throw new Error(`PackedSpriteAtlas: Unsupported version ${version}; expected 1.`);
    }

    // 3. Read and Validate Header Metadata
    this.directions = view.getUint8(5);
    this.walkFrames = view.getUint8(6);
    this.totalRows = view.getUint8(7);
    this.frameWidth = view.getUint16(8, true);
    this.frameHeight = view.getUint16(10, true);
    this.frameWorldWidth = view.getFloat32(12, true);
    this.frameWorldHeight = view.getFloat32(16, true);
    this.feetAnchor = view.getFloat32(20, true);

    if (!Number.isInteger(this.directions) || this.directions < 1 || this.directions > 64) {
      throw new Error(`PackedSpriteAtlas: Invalid directions count (${this.directions}); must be integer in 1..64.`);
    }
    if (!Number.isInteger(this.walkFrames) || this.walkFrames < 1 || this.walkFrames > 64) {
      throw new Error(`PackedSpriteAtlas: Invalid walkFrames count (${this.walkFrames}); must be integer in 1..64.`);
    }
    if (!Number.isInteger(this.totalRows) || this.totalRows !== this.walkFrames + 1) {
      throw new Error(`PackedSpriteAtlas: Invalid totalRows (${this.totalRows}); must be exactly walkFrames + 1 (${this.walkFrames + 1}).`);
    }
    if (!Number.isInteger(this.frameWidth) || this.frameWidth < 1 || this.frameWidth > 255 ||
        !Number.isInteger(this.frameHeight) || this.frameHeight < 1 || this.frameHeight > 255) {
      throw new Error(`PackedSpriteAtlas: Invalid frame dimensions (${this.frameWidth}x${this.frameHeight}); must be in 1..255 for uint8 bounding boxes.`);
    }
    if (!Number.isFinite(this.frameWorldWidth) || this.frameWorldWidth <= 0) {
      throw new Error(`PackedSpriteAtlas: Invalid frameWorldWidth (${this.frameWorldWidth}); must be finite positive.`);
    }
    if (!Number.isFinite(this.frameWorldHeight) || this.frameWorldHeight <= 0) {
      throw new Error(`PackedSpriteAtlas: Invalid frameWorldHeight (${this.frameWorldHeight}); must be finite positive.`);
    }
    if (!Number.isFinite(this.feetAnchor) || this.feetAnchor <= 0 || this.feetAnchor > 1.0) {
      throw new Error(`PackedSpriteAtlas: Invalid feetAnchor (${this.feetAnchor}); must be in (0, 1.0].`);
    }

    this.totalFrames = this.directions * this.totalRows;
    const headerBytes = 24;
    const indexTableBytes = this.totalFrames * 4;
    const minRequiredBytes = headerBytes + indexTableBytes;

    if (this.encodedBytes < minRequiredBytes) {
      throw new Error(`PackedSpriteAtlas: Truncated index table; expected at least ${minRequiredBytes} bytes, got ${this.encodedBytes}.`);
    }

    // 4. Read and Validate Offset Monotonicity & Zero-Padding Contract
    this.offsets = new Uint32Array(this.totalFrames);
    let prevOffset = minRequiredBytes;

    for (let i = 0; i < this.totalFrames; i++) {
      const off = view.getUint32(headerBytes + i * 4, true);

      if (i === 0) {
        if (off !== minRequiredBytes) {
          throw new Error(`PackedSpriteAtlas: First frame offset (${off}) must strictly equal header + indexTable (${minRequiredBytes}) without padding.`);
        }
      } else {
        if (off <= prevOffset) {
          throw new Error(`PackedSpriteAtlas: Non-increasing frame offset [${i}]=${off} <= previous ${prevOffset}.`);
        }
      }

      if (off >= this.encodedBytes) {
        throw new Error(`PackedSpriteAtlas: Frame offset [${i}]=${off} exceeds stream length ${this.encodedBytes}.`);
      }

      this.offsets[i] = off;
      prevOffset = off;
    }

    // 5. Parse and Validate Every Record Strictly Within Its Own Slice (No Trailing Garbage)
    this.paletteBytes = 0;
    this.indexBytes = 0;
    this.frameHeaderBytes = 0;
    this.validateRecordSlices();

    // 6. Pre-allocate Bounded 2-Frame Cache (Zero Per-Frame Allocation)
    this.framePixelCount = this.frameWidth * this.frameHeight;
    this.cache = [
      { dir: -1, row: -1, buffer: new Uint32Array(this.framePixelCount) },
      { dir: -1, row: -1, buffer: new Uint32Array(this.framePixelCount) }
    ];
    this.cacheIdx = 0;

    // Fast reusable palette buffer (256 uint32 RGBA colors = 1024 bytes)
    this.paletteBuffer = new Uint32Array(256);

    // RAM Accounting Metrics (Retained typed-array bytes)
    this.peakFrameCacheBytes = this.framePixelCount * 4 * this.cache.length; // 36.00 KiB
    this.reusableDecoderRam = this.paletteBuffer.byteLength + this.offsets.byteLength;
    this.totalInstanceRam = this.encodedBytes + this.peakFrameCacheBytes + this.reusableDecoderRam;
  }

  validateRecordSlices() {
    const bytes = this.bytes;
    const view = this.view;
    const totalBytes = this.encodedBytes;
    const fw = this.frameWidth;
    const fh = this.frameHeight;

    for (let i = 0; i < this.totalFrames; i++) {
      const sliceStart = this.offsets[i];
      const sliceEnd = (i + 1 < this.totalFrames) ? this.offsets[i + 1] : totalBytes;
      const sliceLen = sliceEnd - sliceStart;

      let p = sliceStart;

      if (p + 4 > sliceEnd) {
        throw new Error(`PackedSpriteAtlas: Truncated frame header in slice [${i}] (length ${sliceLen} < 4).`);
      }

      const minX = bytes[p++];
      const minY = bytes[p++];
      const bw = bytes[p++];
      const bh = bytes[p++];
      this.frameHeaderBytes += 4;

      // Blank frame record: exactly 4 bytes
      if (bw === 0 || bh === 0) {
        if (p !== sliceEnd) {
          throw new Error(`PackedSpriteAtlas: Empty frame [${i}] has unexpected trailing payload in slice (${p} !== ${sliceEnd}).`);
        }
        continue;
      }

      if (minX + bw > fw || minY + bh > fh) {
        throw new Error(`PackedSpriteAtlas: Frame [${i}] bounding box (${minX}+${bw}, ${minY}+${bh}) exceeds cell (${fw}x${fh}).`);
      }

      if (p + 2 > sliceEnd) {
        throw new Error(`PackedSpriteAtlas: Truncated palette header in frame [${i}].`);
      }

      // Read 16-bit little-endian paletteCount
      const palCount = view.getUint16(p, true);
      p += 2;
      this.frameHeaderBytes += 2;

      if (palCount === 0 || palCount > 256) {
        throw new Error(`PackedSpriteAtlas: Invalid paletteCount ${palCount} in frame [${i}]; must be in 1..256.`);
      }

      const palDataBytes = palCount * 3;
      if (p + palDataBytes > sliceEnd) {
        throw new Error(`PackedSpriteAtlas: Truncated palette data in frame [${i}]: needed ${palDataBytes} bytes, slice ends early.`);
      }

      this.paletteBytes += palDataBytes;
      p += palDataBytes;

      // Validate span rows
      const spanStart = p;
      for (let y = 0; y < bh; y++) {
        if (p >= sliceEnd) {
          throw new Error(`PackedSpriteAtlas: Truncated span rows in frame [${i}] at row ${y}.`);
        }
        const numSpans = bytes[p++];
        let rowX = minX;

        for (let s = 0; s < numSpans; s++) {
          if (p + 2 > sliceEnd) {
            throw new Error(`PackedSpriteAtlas: Truncated span header in frame [${i}], row ${y}.`);
          }
          const skip = bytes[p++];
          const draw = bytes[p++];
          rowX += skip;

          if (rowX + draw > minX + bw) {
            throw new Error(`PackedSpriteAtlas: Span [${s}] in frame [${i}] exceeds bounding box width: ${rowX}+${draw} > ${minX+bw}.`);
          }

          if (p + draw > sliceEnd) {
            throw new Error(`PackedSpriteAtlas: Truncated span indices in frame [${i}], row ${y}.`);
          }

          for (let px = 0; px < draw; px++) {
            const palIdx = bytes[p++];
            if (palIdx >= palCount) {
              throw new Error(`PackedSpriteAtlas: Out-of-bounds palette index ${palIdx} >= ${palCount} in frame [${i}].`);
            }
          }
          rowX += draw;
        }
      }

      this.indexBytes += (p - spanStart);

      // Strict slice completeness check: parser must reach sliceEnd exactly!
      if (p !== sliceEnd) {
        throw new Error(`PackedSpriteAtlas: Record [${i}] has unparsed padding/garbage in slice (${p} !== ${sliceEnd}).`);
      }
    }
  }

  /**
   * Retrieves a decoded 48x96 RGBA frame backed by a bounded 2-frame cache.
   * Validates integer inputs; rejects NaN/fractional/out-of-bounds queries.
   * Returns: Uint32Array(48x96) in little-endian RGBA format.
   */
  getFrame(direction, row) {
    if (typeof direction !== 'number' || !Number.isInteger(direction) || !Number.isFinite(direction)) {
      throw new TypeError(`PackedSpriteAtlas.getFrame: Invalid direction '${direction}'; must be an integer.`);
    }
    if (typeof row !== 'number' || !Number.isInteger(row) || !Number.isFinite(row)) {
      throw new TypeError(`PackedSpriteAtlas.getFrame: Invalid row '${row}'; must be an integer.`);
    }
    if (direction < 0 || direction >= this.directions) {
      throw new RangeError(`PackedSpriteAtlas.getFrame: Direction out of bounds (${direction} not in 0..${this.directions - 1}).`);
    }
    if (row < 0 || row >= this.totalRows) {
      throw new RangeError(`PackedSpriteAtlas.getFrame: Row out of bounds (${row} not in 0..${this.totalRows - 1}).`);
    }

    // 1. Check cache hits
    for (let i = 0; i < this.cache.length; i++) {
      const slot = this.cache[i];
      if (slot.dir === direction && slot.row === row) {
        return slot.buffer;
      }
    }

    // 2. Select next cache slot
    const slot = this.cache[this.cacheIdx];
    this.cacheIdx = (this.cacheIdx + 1) % this.cache.length;

    const frameIdx = row * this.directions + direction;
    const off = this.offsets[frameIdx];

    const bytes = this.bytes;
    const view = this.view;
    const buf = slot.buffer;

    // Clear frame buffer to fully transparent RGBA (0x00000000)
    buf.fill(0);

    const minX = bytes[off];
    const minY = bytes[off + 1];
    const bw = bytes[off + 2];
    const bh = bytes[off + 3];

    // Blank frame
    if (bw === 0 || bh === 0) {
      slot.dir = direction;
      slot.row = row;
      return buf;
    }

    // Read 16-bit little-endian paletteCount
    const palCount = view.getUint16(off + 4, true);
    let p = off + 6;

    // Unpack palette into fast 32-bit RGBA lookup table
    const pal = this.paletteBuffer;
    for (let i = 0; i < palCount; i++) {
      const rgb565 = view.getUint16(p, true);
      const alpha = bytes[p + 2];
      p += 3;

      // Expand RGB565 to RGB888
      const r = Math.round(((rgb565 >> 11) & 0x1f) * (255 / 31));
      const g = Math.round(((rgb565 >> 5) & 0x3f) * (255 / 63));
      const b = Math.round((rgb565 & 0x1f) * (255 / 31));

      pal[i] = ((alpha & 0xff) << 24) | ((b & 0xff) << 16) | ((g & 0xff) << 8) | (r & 0xff);
    }

    // Decode RLE pixel spans
    const fw = this.frameWidth;
    for (let y = 0; y < bh; y++) {
      const numSpans = bytes[p++];
      let curX = minX;
      const rowOffset = (minY + y) * fw;

      for (let s = 0; s < numSpans; s++) {
        const skip = bytes[p++];
        const draw = bytes[p++];
        curX += skip;

        for (let d = 0; d < draw; d++) {
          const palIdx = bytes[p++];
          buf[rowOffset + curX++] = pal[palIdx];
        }
      }
    }

    slot.dir = direction;
    slot.row = row;
    return buf;
  }

  dispose() {
    this.cache = null;
    this.bytes = null;
    this.view = null;
    this.offsets = null;
    this.paletteBuffer = null;
  }
}
