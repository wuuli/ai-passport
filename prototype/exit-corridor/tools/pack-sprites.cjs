#!/usr/bin/env node
/**
 * Offline Sprite Atlas Packer (pack-sprites.cjs)
 * Encodes a sprite atlas PNG + JSON metadata into a compact ECSP v1 binary package.
 *
 * Requirements & Validations:
 * - Full IHDR validation (8-bit RGBA, non-interlaced, compression/filter 0).
 * - Exact decompressed byte length validation: height * (1 + width * 4).
 * - Scanline filter validation (only 0..4 allowed).
 * - Consistency check between PNG dimensions and JSON metadata.
 * - 16-bit little-endian paletteCount (safely stores up to 256 colors).
 * - Explicit rejection of any frame with > 256 unique colors.
 * - Strict contiguous record packing without trailing garbage or padding.
 * - Optional direction subsampling (e.g. dirStep = 2 for 8dirs from 16dirs) for device budget.
 * - Deterministic byte-identical output across repeated runs.
 * - Zero external npm or pip dependencies.
 *
 * Usage:
 *   node prototype/exit-corridor/tools/pack-sprites.cjs [inputPng] [inputJson] [outputBin] [--dir-step N]
 */

const fs = require('fs');
const path = require('path');
const zlib = require('zlib');

const DEFAULT_PNG = path.resolve('assets/images/exit-corridor/sprite-atlas.png');
const DEFAULT_JSON = path.resolve('assets/images/exit-corridor/sprite-atlas.json');
const DEFAULT_OUT = path.resolve('assets/images/exit-corridor/sprite-atlas.bin');

/**
 * Validates and unfilters raw RGBA pixels from a PNG buffer.
 */
function unfilterPng(buf) {
  if (!buf || buf.length < 8 || buf.readUInt32BE(0) !== 0x89504E47 || buf.readUInt32BE(4) !== 0x0D0A1A0A) {
    throw new Error('unfilterPng: Invalid PNG signature.');
  }

  let pos = 8;
  let width = 0, height = 0;
  let bitDepth = 0, colorType = 0, compressionMethod = 0, filterMethod = 0, interlaceMethod = 0;
  let ihdrFound = false;
  const idatChunks = [];

  while (pos < buf.length) {
    if (pos + 8 > buf.length) {
      throw new Error('unfilterPng: Truncated chunk header.');
    }
    const len = buf.readUInt32BE(pos);
    const type = buf.toString('ascii', pos + 4, pos + 8);
    if (pos + 12 + len > buf.length) {
      throw new Error(`unfilterPng: Truncated chunk data for '${type}'.`);
    }
    const data = buf.subarray(pos + 8, pos + 8 + len);
    pos += 12 + len;

    if (type === 'IHDR') {
      if (len !== 13) {
        throw new Error(`unfilterPng: Invalid IHDR length (${len}); expected 13.`);
      }
      width = data.readUInt32BE(0);
      height = data.readUInt32BE(4);
      bitDepth = data[8];
      colorType = data[9];
      compressionMethod = data[10];
      filterMethod = data[11];
      interlaceMethod = data[12];
      ihdrFound = true;

      if (width === 0 || height === 0) {
        throw new Error(`unfilterPng: Invalid dimensions (${width}x${height}).`);
      }
      if (bitDepth !== 8 || colorType !== 6) {
        throw new Error(`unfilterPng: Unsupported PNG format: bitDepth=${bitDepth}, colorType=${colorType} (must be 8-bit RGBA).`);
      }
      if (compressionMethod !== 0) {
        throw new Error(`unfilterPng: Unsupported compression method ${compressionMethod}; expected 0.`);
      }
      if (filterMethod !== 0) {
        throw new Error(`unfilterPng: Unsupported filter method ${filterMethod}; expected 0.`);
      }
      if (interlaceMethod !== 0) {
        throw new Error(`unfilterPng: Interlaced PNGs are not supported; interlaceMethod must be 0.`);
      }
    } else if (type === 'IDAT') {
      idatChunks.push(data);
    } else if (type === 'IEND') {
      break;
    }
  }

  if (!ihdrFound) {
    throw new Error('unfilterPng: Missing IHDR chunk.');
  }
  if (idatChunks.length === 0) {
    throw new Error('unfilterPng: No IDAT image data chunks found.');
  }

  const decompressed = zlib.inflateSync(Buffer.concat(idatChunks));
  const expectedDecompLen = height * (1 + width * 4);
  if (decompressed.length !== expectedDecompLen) {
    throw new Error(`unfilterPng: Decompressed byte length mismatch: expected ${expectedDecompLen}, got ${decompressed.length}.`);
  }

  const bpp = 4;
  const stride = width * bpp;
  const rawPixels = Buffer.alloc(width * height * bpp);
  let prevRow = Buffer.alloc(stride);

  let srcIdx = 0;
  let dstIdx = 0;

  for (let y = 0; y < height; y++) {
    const filterType = decompressed[srcIdx++];
    if (filterType < 0 || filterType > 4) {
      throw new Error(`unfilterPng: Invalid scanline filter type ${filterType} at row ${y}; must be 0..4.`);
    }

    const currRow = Buffer.from(decompressed.subarray(srcIdx, srcIdx + stride));
    srcIdx += stride;

    if (filterType === 1) { // Sub
      for (let x = bpp; x < stride; x++) {
        currRow[x] = (currRow[x] + currRow[x - bpp]) & 0xff;
      }
    } else if (filterType === 2) { // Up
      for (let x = 0; x < stride; x++) {
        currRow[x] = (currRow[x] + prevRow[x]) & 0xff;
      }
    } else if (filterType === 3) { // Average
      for (let x = 0; x < stride; x++) {
        const left = x >= bpp ? currRow[x - bpp] : 0;
        const up = prevRow[x];
        currRow[x] = (currRow[x] + ((left + up) >> 1)) & 0xff;
      }
    } else if (filterType === 4) { // Paeth
      for (let x = 0; x < stride; x++) {
        const left = x >= bpp ? currRow[x - bpp] : 0;
        const up = prevRow[x];
        const upLeft = x >= bpp ? prevRow[x - bpp] : 0;
        const p = left + up - upLeft;
        const pa = Math.abs(p - left);
        const pb = Math.abs(p - up);
        const pc = Math.abs(p - upLeft);
        let pr = left;
        if (pa <= pb && pa <= pc) pr = left;
        else if (pb <= pc) pr = up;
        else pr = upLeft;
        currRow[x] = (currRow[x] + pr) & 0xff;
      }
    }

    currRow.copy(rawPixels, dstIdx);
    prevRow = currRow;
    dstIdx += stride;
  }

  return { width, height, rawPixels };
}

/**
 * Main pack function: encodes atlas into ECSP binary format.
 * Supports optional dirStep parameter (e.g. dirStep=2 samples even directions 0,2,4... for device candidate).
 */
function packSpriteAtlas(pngPath, jsonPath, outBinPath, options = {}) {
  const dirStep = options.dirStep ?? 1;
  if (!Number.isInteger(dirStep) || dirStep < 1) throw new Error('dirStep must be a positive integer');
  if (options.metadataOut && path.resolve(options.metadataOut) === path.resolve(jsonPath)) {
    throw new Error('Output metadata must not overwrite source metadata');
  }

  console.log(`[PACKER] Reading PNG: ${pngPath}`);
  const pngBuf = fs.readFileSync(pngPath);
  const { width: atlasWidth, height: atlasHeight, rawPixels } = unfilterPng(pngBuf);

  console.log(`[PACKER] Atlas dimensions: ${atlasWidth}x${atlasHeight}`);

  let meta = {};
  if (fs.existsSync(jsonPath)) {
    console.log(`[PACKER] Reading metadata: ${jsonPath}`);
    meta = JSON.parse(fs.readFileSync(jsonPath, 'utf8'));
  } else {
    throw new Error(`packSpriteAtlas: Missing JSON metadata file at ${jsonPath}`);
  }

  const srcDirections = meta.directions;
  if (!Number.isInteger(srcDirections) || srcDirections < 1 || srcDirections % dirStep !== 0) {
    throw new Error('dirStep must evenly divide source directions');
  }
  const walkFrames = meta.walkFrames;
  const totalRows = meta.totalRows || (walkFrames + 1);
  const frameWidth = meta.frameWidth || meta.width;
  const frameHeight = meta.frameHeight || meta.height;
  const frameWorldWidth = meta.frameWorldWidth ?? 1.0;
  const frameWorldHeight = meta.frameWorldHeight ?? 2.0;
  const feetAnchor = meta.feetAnchor ?? 0.95;

  // Metadata vs PNG consistency validation
  if (!srcDirections || !walkFrames || !frameWidth || !frameHeight) {
    throw new Error(`packSpriteAtlas: Missing required metadata fields in ${jsonPath}`);
  }
  if (totalRows !== walkFrames + 1) {
    throw new Error(`packSpriteAtlas: Invalid totalRows (${totalRows}); expected exactly walkFrames + 1 (${walkFrames + 1}).`);
  }
  if (frameWidth > 255 || frameHeight > 255) {
    throw new Error(`packSpriteAtlas: Frame dimensions (${frameWidth}x${frameHeight}) exceed 255 (uint8 limit for bounding box).`);
  }
  if (feetAnchor <= 0 || feetAnchor > 1.0) {
    throw new Error(`packSpriteAtlas: feetAnchor (${feetAnchor}) must be in (0, 1.0].`);
  }
  if (atlasWidth !== meta.atlasWidth || atlasHeight !== meta.atlasHeight) {
    throw new Error(`packSpriteAtlas: PNG dimensions (${atlasWidth}x${atlasHeight}) mismatch metadata (${meta.atlasWidth}x${meta.atlasHeight}).`);
  }
  if (atlasWidth !== srcDirections * frameWidth) {
    throw new Error(`packSpriteAtlas: Atlas width ${atlasWidth} !== directions(${srcDirections}) * frameWidth(${frameWidth}).`);
  }
  if (atlasHeight !== totalRows * frameHeight) {
    throw new Error(`packSpriteAtlas: Atlas height ${atlasHeight} !== totalRows(${totalRows}) * frameHeight(${frameHeight}).`);
  }

  // Determine which source directions to sample
  const sampledDirs = [];
  for (let d = 0; d < srcDirections; d += dirStep) {
    sampledDirs.push(d);
  }
  const outputDirections = sampledDirs.length;
  const totalFrames = outputDirections * totalRows;

  console.log(`[PACKER] Direction sampling: dirStep=${dirStep} (sampled directions: ${sampledDirs.join(',')})`);
  console.log(`[PACKER] Total frames to pack: ${totalFrames} (${outputDirections} directions x ${totalRows} rows)`);

  // Build File Header (24 bytes)
  const headerBuf = Buffer.alloc(24);
  headerBuf.write('ECSP', 0, 4, 'ascii');
  headerBuf.writeUInt8(1, 4); // version
  headerBuf.writeUInt8(outputDirections, 5);
  headerBuf.writeUInt8(walkFrames, 6);
  headerBuf.writeUInt8(totalRows, 7);
  headerBuf.writeUInt16LE(frameWidth, 8);
  headerBuf.writeUInt16LE(frameHeight, 10);
  headerBuf.writeFloatLE(frameWorldWidth, 12);
  headerBuf.writeFloatLE(frameWorldHeight, 16);
  headerBuf.writeFloatLE(feetAnchor, 20);

  const frameRecords = [];
  let totalPaletteBytes = 0;
  let totalSpanBytes = 0;
  let totalNonTransPixels = 0;
  let maxUniqueColorsSeen = 0;

  for (let r = 0; r < totalRows; r++) {
    for (let di = 0; di < sampledDirs.length; di++) {
      const srcD = sampledDirs[di];
      const cellPixels = [];
      let minX = frameWidth, maxX = -1;
      let minY = frameHeight, maxY = -1;

      for (let cy = 0; cy < frameHeight; cy++) {
        const row = [];
        for (let cx = 0; cx < frameWidth; cx++) {
          const gx = srcD * frameWidth + cx;
          const gy = r * frameHeight + cy;
          const off = (gy * atlasWidth + gx) * 4;
          const red = rawPixels[off];
          const green = rawPixels[off + 1];
          const blue = rawPixels[off + 2];
          const alpha = rawPixels[off + 3];

          row.push({ red, green, blue, alpha });

          if (alpha > 8) {
            if (cx < minX) minX = cx;
            if (cx > maxX) maxX = cx;
            if (cy < minY) minY = cy;
            if (cy > maxY) maxY = cy;
          }
        }
        cellPixels.push(row);
      }

      // Empty / blank frame (exactly 4 bytes)
      if (maxX < 0) {
        const emptyBuf = Buffer.alloc(4);
        frameRecords.push(emptyBuf);
        continue;
      }

      const bw = maxX - minX + 1;
      const bh = maxY - minY + 1;

      // Collect unique colors in frame
      const paletteMap = new Map();
      const paletteEntries = [];

      for (let y = minY; y <= maxY; y++) {
        for (let x = minX; x <= maxX; x++) {
          const { red, green, blue, alpha } = cellPixels[y][x];
          if (alpha > 8) {
            totalNonTransPixels++;
            const r5 = red >> 3;
            const g6 = green >> 2;
            const b5 = blue >> 3;
            const rgb565 = (r5 << 11) | (g6 << 5) | b5;
            const key = (rgb565 << 8) | alpha;

            if (!paletteMap.has(key)) {
              if (paletteEntries.length >= 256) {
                throw new Error(`packSpriteAtlas: Frame [${srcD},${r}] contains > 256 unique colors (${paletteEntries.length + 1}), violating 256-color lossless limit.`);
              }
              paletteMap.set(key, paletteEntries.length);
              paletteEntries.push({ rgb565, alpha });
            }
          }
        }
      }

      if (paletteEntries.length > maxUniqueColorsSeen) {
        maxUniqueColorsSeen = paletteEntries.length;
      }

      const palCount = paletteEntries.length;
      totalPaletteBytes += 2 + palCount * 3; // uint16LE paletteCount + 3 bytes per entry

      const recordChunks = [];
      const fHead = Buffer.alloc(6);
      fHead.writeUInt8(minX, 0);
      fHead.writeUInt8(minY, 1);
      fHead.writeUInt8(bw, 2);
      fHead.writeUInt8(bh, 3);
      fHead.writeUInt16LE(palCount, 4);
      recordChunks.push(fHead);

      // Palette data (RGB565 uint16LE + Alpha uint8)
      const palBuf = Buffer.alloc(palCount * 3);
      for (let i = 0; i < palCount; i++) {
        palBuf.writeUInt16LE(paletteEntries[i].rgb565, i * 3);
        palBuf.writeUInt8(paletteEntries[i].alpha, i * 3 + 2);
      }
      recordChunks.push(palBuf);

      // Span encoding
      let spanBytesForFrame = 0;
      for (let y = minY; y <= maxY; y++) {
        let x = minX;
        const spans = [];

        while (x <= maxX) {
          let skip = 0;
          while (x <= maxX && cellPixels[y][x].alpha <= 8) {
            skip++;
            x++;
            if (skip === 255) break;
          }

          let draw = 0;
          const indices = [];
          while (x <= maxX && cellPixels[y][x].alpha > 8) {
            const { red, green, blue, alpha } = cellPixels[y][x];
            const r5 = red >> 3;
            const g6 = green >> 2;
            const b5 = blue >> 3;
            const rgb565 = (r5 << 11) | (g6 << 5) | b5;
            const key = (rgb565 << 8) | alpha;
            const idx = paletteMap.get(key);
            indices.push(idx);
            draw++;
            x++;
            if (draw === 255) break;
          }

          if (skip > 0 || draw > 0) {
            spans.push({ skip, draw, indices });
          }
        }

        const rowHead = Buffer.alloc(1);
        rowHead.writeUInt8(spans.length, 0);
        recordChunks.push(rowHead);
        spanBytesForFrame += 1;

        for (const s of spans) {
          const sBuf = Buffer.alloc(2 + s.draw);
          sBuf.writeUInt8(s.skip, 0);
          sBuf.writeUInt8(s.draw, 1);
          for (let i = 0; i < s.draw; i++) {
            sBuf.writeUInt8(s.indices[i], 2 + i);
          }
          recordChunks.push(sBuf);
          spanBytesForFrame += sBuf.length;
        }
      }

      totalSpanBytes += spanBytesForFrame;
      frameRecords.push(Buffer.concat(recordChunks));
    }
  }

  // Build Frame Offset Index Table (strictly contiguous from header + indexTable, no padding!)
  const indexTableSize = totalFrames * 4;
  const frameDataOffset = headerBuf.length + indexTableSize;

  const indexTableBuf = Buffer.alloc(indexTableSize);
  let currentOffset = frameDataOffset;

  for (let i = 0; i < totalFrames; i++) {
    indexTableBuf.writeUInt32LE(currentOffset, i * 4);
    currentOffset += frameRecords[i].length;
  }

  const finalPackage = Buffer.concat([headerBuf, indexTableBuf, ...frameRecords]);

  console.log(`[PACKER] Writing binary package: ${outBinPath}`);
  fs.writeFileSync(outBinPath, finalPackage);
  if (options.metadataOut) {
    const packedMeta = { ...meta, profile: `packed_${outputDirections}x${walkFrames}`,
      directions: outputDirections, atlasWidth: outputDirections * frameWidth,
      totalCells: totalFrames, sourceSampling: sampledDirs,
      sourcePng: path.basename(pngPath), packedBinary: path.basename(outBinPath),
      packedBinaryBytes: finalPackage.length };
    fs.writeFileSync(options.metadataOut, JSON.stringify(packedMeta, null, 2) + '\n');
  }

  const totalBytes = finalPackage.length;
  console.log(`=== PACKING METRICS SUMMARY ===`);
  console.log(`  Source RGBA size:       ${rawPixels.length} bytes (${(rawPixels.length / 1024).toFixed(2)} KiB)`);
  console.log(`  Packed ECSP binary:     ${totalBytes} bytes (${(totalBytes / 1024).toFixed(2)} KiB)`);
  console.log(`  Compression ratio:      ${(rawPixels.length / totalBytes).toFixed(2)}x smaller than raw RGBA`);
  console.log(`  Header + Index table:   ${headerBuf.length + indexTableSize} bytes`);
  console.log(`  Palette payload:        ${totalPaletteBytes} bytes (${(totalPaletteBytes / 1024).toFixed(2)} KiB)`);
  console.log(`  Span + index payload:   ${totalSpanBytes} bytes (${(totalSpanBytes / 1024).toFixed(2)} KiB)`);
  console.log(`  Non-transparent pixels: ${totalNonTransPixels} (avg ${(totalNonTransPixels / totalFrames).toFixed(1)}/frame)`);
  console.log(`  Max colors in a frame:  ${maxUniqueColorsSeen}`);

  return {
    totalBytes,
    totalPaletteBytes,
    totalSpanBytes,
    totalFrames,
    maxUniqueColorsSeen,
    outputDirections,
    totalNonTransPixels
  };
}

if (require.main === module) {
  const args = process.argv.slice(2);
  let dirStep = 1;
  let metadataOut;
  const filteredArgs = [];
  for (let i = 0; i < args.length; i++) {
    if (args[i] === '--dir-step' && i + 1 < args.length) {
      dirStep = Number(args[++i]);
    } else if (args[i] === '--metadata-out' && i + 1 < args.length) {
      metadataOut = args[++i];
    } else {
      filteredArgs.push(args[i]);
    }
  }

  const inputPng = filteredArgs[0] || DEFAULT_PNG;
  const inputJson = filteredArgs[1] || DEFAULT_JSON;
  const outputBin = filteredArgs[2] || DEFAULT_OUT;

  try {
    packSpriteAtlas(inputPng, inputJson, outputBin, { dirStep, metadataOut });
  } catch (err) {
    console.error(`[PACKER ERROR]`, err.message);
    process.exit(1);
  }
}

module.exports = { packSpriteAtlas, unfilterPng };
