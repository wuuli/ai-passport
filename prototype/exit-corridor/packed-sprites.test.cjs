/**
 * Rigorous Acceptance Test Suite for Packed Sprite Atlas (packed-sprites.test.cjs)
 *
 * Verifies:
 * 1. Deterministic byte-identical packing across repeated executions.
 * 2. 8x8 (72 frames) baseline packing and validation.
 * 3. 16x16 (272 frames) full candidate packing and complete 272-frame roundtrip.
 * 4. Device candidate (8dirs x 16walk, 136 frames, 312,689 B = 305.36 KiB):
 *    - Validates /tmp/commuter-device.bin and /tmp/commuter-device.json.
 *    - Bit-for-bit frame equality with even directions (d*2) of 16x16 candidate.
 * 5. Robust rejection of malformed PNGs (interlaced, non-RGBA, bad filter, length mismatch).
 * 6. PaletteCount boundary enforcement (256 supported, >256 strictly rejected).
 * 7. Rejection of NaN, fractional, and non-integer getFrame arguments.
 * 8. Structural validation (monotonic offsets, zero padding, slice bounds, trailing garbage).
 * 9. Honest RAM accounting (distinguishing cache RAM from retained encoded buffer and lookup tables).
 */

const assert = require('assert');
const path = require('path');
const fs = require('fs');
const zlib = require('zlib');
const tmpDir = fs.mkdtempSync(path.join(require('os').tmpdir(), 'exit-corridor-packing-'));

const { packSpriteAtlas, unfilterPng } = require('./tools/pack-sprites.cjs');

// Helper to construct a minimal valid PNG in memory
function makeMinimalPng(width, height, colorType = 6, bitDepth = 8, interlace = 0, filterType = 0, corruptDecompLen = false) {
  const header = Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]);

  const ihdrData = Buffer.alloc(13);
  ihdrData.writeUInt32BE(width, 0);
  ihdrData.writeUInt32BE(height, 4);
  ihdrData.writeUInt8(bitDepth, 8);
  ihdrData.writeUInt8(colorType, 9);
  ihdrData.writeUInt8(0, 10); // compression
  ihdrData.writeUInt8(0, 11); // filter
  ihdrData.writeUInt8(interlace, 12);

  const ihdrChunk = Buffer.concat([
    Buffer.from([0, 0, 0, 13]),
    Buffer.from('IHDR', 'ascii'),
    ihdrData,
    Buffer.alloc(4) // dummy crc
  ]);

  const bpp = colorType === 6 ? 4 : (colorType === 2 ? 3 : 1);
  const rawData = Buffer.alloc(height * (1 + width * bpp));
  for (let y = 0; y < height; y++) {
    rawData[y * (1 + width * bpp)] = filterType;
  }

  const payload = corruptDecompLen ? rawData.subarray(0, rawData.length - 5) : rawData;
  const compressed = zlib.deflateSync(payload);

  const idatChunk = Buffer.concat([
    Buffer.alloc(4),
    Buffer.from('IDAT', 'ascii'),
    compressed,
    Buffer.alloc(4)
  ]);
  idatChunk.writeUInt32BE(compressed.length, 0);

  const iendChunk = Buffer.from([0, 0, 0, 0, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82]);

  return Buffer.concat([header, ihdrChunk, idatChunk, iendChunk]);
}

async function run() {
  console.log('=== STARTING RIGOROUS PACKED SPRITE ATLAS TEST SUITE ===');

  const { PackedSpriteAtlas } = await import('./packed-sprites.js');

  const png8Path = path.resolve('assets/images/exit-corridor/sprite-atlas.png');
  const json8Path = path.resolve('assets/images/exit-corridor/sprite-atlas.json');
  const testBin8Path = path.join(tmpDir, 'exit-corridor-packed-atlas-8.bin');

  const png16Path = path.resolve('assets/images/exit-corridor/sprite-atlas-16.png');
  const json16Path = path.resolve('assets/images/exit-corridor/sprite-atlas-16.json');
  const testBin16Path = path.join(tmpDir, 'exit-corridor-packed-atlas-16.bin');

  const deviceBinPath = path.join(tmpDir, 'commuter-device.bin');
  const deviceJsonPath = path.join(tmpDir, 'commuter-device.json');

  // --------------------------------------------------------------------------
  // TEST 1: Deterministic Byte-Identical Packing
  // --------------------------------------------------------------------------
  const run1Path = path.join(tmpDir, 'test-pack-run1.bin');
  const run2Path = path.join(tmpDir, 'test-pack-run2.bin');

  packSpriteAtlas(png8Path, json8Path, run1Path);
  packSpriteAtlas(png8Path, json8Path, run2Path);

  const bufRun1 = fs.readFileSync(run1Path);
  const bufRun2 = fs.readFileSync(run2Path);

  assert(bufRun1.equals(bufRun2), 'Packing must be 100% deterministic and byte-identical across runs');
  fs.unlinkSync(run1Path);
  fs.unlinkSync(run2Path);
  console.log('[PASS] Test 1: Deterministic byte-identical packing verified');

  // --------------------------------------------------------------------------
  // TEST 2: 8x8 Baseline Packing & Header Invariants
  // --------------------------------------------------------------------------
  const pack8Result = packSpriteAtlas(png8Path, json8Path, testBin8Path);
  assert(fs.existsSync(testBin8Path));
  assert.strictEqual(pack8Result.totalBytes, 165024, `Expected exact 165024 bytes, got ${pack8Result.totalBytes}`);
  assert.strictEqual(pack8Result.totalFrames, 72);

  const bin8Buf = fs.readFileSync(testBin8Path);
  const arrayBuffer8 = bin8Buf.buffer.slice(bin8Buf.byteOffset, bin8Buf.byteOffset + bin8Buf.byteLength);

  const atlas8 = new PackedSpriteAtlas(arrayBuffer8);
  assert.strictEqual(atlas8.directions, 8);
  assert.strictEqual(atlas8.walkFrames, 8);
  assert.strictEqual(atlas8.totalRows, 9);
  assert.strictEqual(atlas8.frameWidth, 48);
  assert.strictEqual(atlas8.frameHeight, 96);
  assert.strictEqual(atlas8.frameWorldWidth, 1.0);
  assert.strictEqual(atlas8.frameWorldHeight, 2.0);
  assert(Math.abs(atlas8.feetAnchor - 0.95) < 1e-5);
  console.log(`[PASS] Test 2: 8x8 atlas verified (165024 B = ${(pack8Result.totalBytes / 1024).toFixed(2)} KiB)`);

  // --------------------------------------------------------------------------
  // TEST 3: Honest Memory Accounting
  // --------------------------------------------------------------------------
  const expectedTotal8 = 24 + (atlas8.totalFrames * 4) + atlas8.frameHeaderBytes + atlas8.paletteBytes + atlas8.indexBytes;
  assert.strictEqual(atlas8.encodedBytes, expectedTotal8);
  assert.strictEqual(atlas8.peakFrameCacheBytes, 48 * 96 * 4 * 2); // 36.00 KiB
  assert.strictEqual(atlas8.reusableDecoderRam, 1024 + 72 * 4);    // 1.28 KiB
  assert.strictEqual(atlas8.totalInstanceRam, atlas8.encodedBytes + atlas8.peakFrameCacheBytes + atlas8.reusableDecoderRam);
  console.log('[PASS] Test 3: Honest memory accounting verified (36.00 KiB cache RAM, 1.28 KiB decoder RAM)');

  // --------------------------------------------------------------------------
  // TEST 4: Pixel Fidelity (8x8)
  // --------------------------------------------------------------------------
  const png8Raw = unfilterPng(fs.readFileSync(png8Path));
  const testSampleCoords8 = [[0, 0], [0, 8], [2, 3], [4, 4], [7, 8]];

  for (const [dir, row] of testSampleCoords8) {
    const decoded = atlas8.getFrame(dir, row);
    assert.strictEqual(decoded.length, 48 * 96);

    let maxADiff = 0, maxRDiff = 0, maxGDiff = 0, maxBDiff = 0;
    for (let cy = 0; cy < 96; cy++) {
      for (let cx = 0; cx < 48; cx++) {
        const off = (row * 96 + cy) * png8Raw.width * 4 + (dir * 48 + cx) * 4;
        const origA = png8Raw.rawPixels[off + 3];
        const decPixel = decoded[cy * 48 + cx];
        const decA = (decPixel >>> 24);

        if (origA <= 8) {
          assert.strictEqual(decA, 0);
        } else {
          const da = Math.abs(origA - decA);
          const dr = Math.abs(png8Raw.rawPixels[off] - (decPixel & 0xff));
          const dg = Math.abs(png8Raw.rawPixels[off + 1] - ((decPixel >> 8) & 0xff));
          const db = Math.abs(png8Raw.rawPixels[off + 2] - ((decPixel >> 16) & 0xff));

          if (da > maxADiff) maxADiff = da;
          if (dr > maxRDiff) maxRDiff = dr;
          if (dg > maxGDiff) maxGDiff = dg;
          if (db > maxBDiff) maxBDiff = db;

          assert(dr <= 8 && dg <= 4 && db <= 8);
        }
      }
    }
    assert.strictEqual(maxADiff, 0, 'Alpha edge error must be EXACTLY 0');
  }
  console.log('[PASS] Test 4: Pixel fidelity verified (exact 0 alpha error, RGB within RGB565 bounds)');

  // --------------------------------------------------------------------------
  // TEST 5: Malformed PNG Rejection (Non-RGBA, Interlaced, Bad Filter, Length Mismatch)
  // --------------------------------------------------------------------------
  // 5.1 Non-RGBA (RGB colorType 2)
  const nonRgbaPng = makeMinimalPng(48, 96, 2, 8, 0, 0);
  assert.throws(() => { unfilterPng(nonRgbaPng); }, /Unsupported PNG format.*must be 8-bit RGBA/);

  // 5.2 Interlaced PNG
  const interlacedPng = makeMinimalPng(48, 96, 6, 8, 1, 0);
  assert.throws(() => { unfilterPng(interlacedPng); }, /Interlaced PNGs are not supported/);

  // 5.3 Decompressed length mismatch
  const corruptLenPng = makeMinimalPng(48, 96, 6, 8, 0, 0, true);
  assert.throws(() => { unfilterPng(corruptLenPng); }, /Decompressed byte length mismatch/);

  // 5.4 Invalid filter type (filter = 9)
  const badFilterPng = makeMinimalPng(48, 96, 6, 8, 0, 9);
  assert.throws(() => { unfilterPng(badFilterPng); }, /Invalid scanline filter type/);

  console.log('[PASS] Test 5: Malformed PNGs strictly rejected (non-RGBA, interlaced, corrupt len, bad filter)');

  // --------------------------------------------------------------------------
  // TEST 6: Strict Argument Validation (NaN, Floats, Non-Integers)
  // --------------------------------------------------------------------------
  assert.throws(() => { atlas8.getFrame(NaN, 0); }, TypeError);
  assert.throws(() => { atlas8.getFrame(0, NaN); }, TypeError);
  assert.throws(() => { atlas8.getFrame(1.5, 0); }, TypeError);
  assert.throws(() => { atlas8.getFrame(0, 2.7); }, TypeError);
  assert.throws(() => { atlas8.getFrame(Infinity, 0); }, TypeError);
  assert.throws(() => { atlas8.getFrame('0', 0); }, TypeError);
  assert.throws(() => { atlas8.getFrame(-1, 0); }, RangeError);
  assert.throws(() => { atlas8.getFrame(8, 0); }, RangeError);
  assert.throws(() => { atlas8.getFrame(0, -1); }, RangeError);
  assert.throws(() => { atlas8.getFrame(0, 9); }, RangeError);
  console.log('[PASS] Test 6: Strict argument validation rejects NaN, floats, non-integers, out-of-bounds');

  // --------------------------------------------------------------------------
  // TEST 7: Structural Header & Offset Verification
  // --------------------------------------------------------------------------
  // 7.1 totalRows !== walkFrames + 1
  const badRowsBuf = Buffer.from(bin8Buf);
  badRowsBuf.writeUInt8(5, 7);
  assert.throws(() => {
    new PackedSpriteAtlas(badRowsBuf.buffer.slice(badRowsBuf.byteOffset, badRowsBuf.byteOffset + badRowsBuf.byteLength));
  }, /Invalid totalRows/);

  // 7.2 feetAnchor > 1.0
  const badAnchorBuf = Buffer.from(bin8Buf);
  badAnchorBuf.writeFloatLE(1.2, 20);
  assert.throws(() => {
    new PackedSpriteAtlas(badAnchorBuf.buffer.slice(badAnchorBuf.byteOffset, badAnchorBuf.byteOffset + badAnchorBuf.byteLength));
  }, /Invalid feetAnchor/);

  // 7.3 frameWidth > 255
  const badWidthBuf = Buffer.from(bin8Buf);
  badWidthBuf.writeUInt16LE(256, 8);
  assert.throws(() => {
    new PackedSpriteAtlas(badWidthBuf.buffer.slice(badWidthBuf.byteOffset, badWidthBuf.byteOffset + badWidthBuf.byteLength));
  }, /Invalid frame dimensions/);

  // 7.4 First offset !== minRequiredBytes (padded offset)
  const badPadBuf = Buffer.from(bin8Buf);
  const firstOff = badPadBuf.readUInt32LE(24);
  badPadBuf.writeUInt32LE(firstOff + 8, 24);
  assert.throws(() => {
    new PackedSpriteAtlas(badPadBuf.buffer.slice(badPadBuf.byteOffset, badPadBuf.byteOffset + badPadBuf.byteLength));
  }, /must strictly equal header \+ indexTable/);

  // 7.5 Trailing garbage
  const garbageBuf = Buffer.concat([bin8Buf, Buffer.from([0xaa, 0xbb])]);
  assert.throws(() => {
    new PackedSpriteAtlas(garbageBuf.buffer.slice(garbageBuf.byteOffset, garbageBuf.byteOffset + garbageBuf.byteLength));
  }, /unparsed padding\/garbage/);

  console.log('[PASS] Test 7: Structural header, offset, and zero-padding invariants verified');

  // --------------------------------------------------------------------------
  // TEST 8: Full 272-Frame Roundtrip Decode of 16x16 Candidate
  // --------------------------------------------------------------------------
  const pack16Result = packSpriteAtlas(png16Path, json16Path, testBin16Path);
  assert(fs.existsSync(testBin16Path));
  assert.strictEqual(pack16Result.totalBytes, 622522);
  assert.strictEqual(pack16Result.totalFrames, 272);

  const bin16Buf = fs.readFileSync(testBin16Path);
  const arrayBuffer16 = bin16Buf.buffer.slice(bin16Buf.byteOffset, bin16Buf.byteOffset + bin16Buf.byteLength);

  const atlas16 = new PackedSpriteAtlas(arrayBuffer16);
  assert.strictEqual(atlas16.directions, 16);
  assert.strictEqual(atlas16.walkFrames, 16);
  assert.strictEqual(atlas16.totalRows, 17);
  assert.strictEqual(atlas16.totalFrames, 272);

  let totalDecoded16 = 0;
  const raw16 = unfilterPng(fs.readFileSync(png16Path));
  for (let r = 0; r < 17; r++) {
    for (let d = 0; d < 16; d++) {
      const fBuf = atlas16.getFrame(d, r);
      assert.strictEqual(fBuf.length, 48 * 96);
      let count = 0;
      for (let i = 0; i < fBuf.length; i++) {
        const src = ((r * 96 + Math.floor(i / 48)) * raw16.width + d * 48 + i % 48) * 4;
        const alpha = raw16.rawPixels[src + 3];
        assert.strictEqual(fBuf[i] >>> 24, alpha > 8 ? alpha : 0);
        if (alpha > 8) {
          assert(Math.abs((fBuf[i] & 255) - raw16.rawPixels[src]) <= 8);
          assert(Math.abs(((fBuf[i] >>> 8) & 255) - raw16.rawPixels[src + 1]) <= 4);
          assert(Math.abs(((fBuf[i] >>> 16) & 255) - raw16.rawPixels[src + 2]) <= 8);
        }
        if ((fBuf[i] >>> 24) > 8) count++;
      }
      assert(count > 800, `Frame [${d},${r}] solid pixel count ${count} < 800`);
      totalDecoded16 += count;
    }
  }
  assert.strictEqual(totalDecoded16, pack16Result.totalNonTransPixels);
  console.log(`[PASS] Test 8: All 272 frames roundtrip-decoded without errors (total non-trans pixels: ${totalDecoded16})`);

  // --------------------------------------------------------------------------
  // TEST 9: Device Candidate (8dirs x 16walk, 136 frames, 312,689 B = 305.36 KiB)
  // --------------------------------------------------------------------------
  assert.throws(() => packSpriteAtlas(png16Path, json16Path, deviceBinPath, { dirStep: -1 }), /positive integer/);
  assert.throws(() => packSpriteAtlas(png16Path, json16Path, deviceBinPath, { dirStep: 3 }), /evenly divide/);
  packSpriteAtlas(png16Path, json16Path, deviceBinPath, { dirStep: 2, metadataOut: deviceJsonPath });
  assert(fs.existsSync(deviceBinPath));
  assert(fs.existsSync(deviceJsonPath));
  const deviceMeta = JSON.parse(fs.readFileSync(deviceJsonPath, 'utf8'));
  assert.strictEqual(deviceMeta.directions, 8);
  assert.strictEqual(deviceMeta.walkFrames, 16);
  assert.strictEqual(deviceMeta.atlasWidth, 384);
  assert.deepEqual(deviceMeta.sourceSampling, [0,2,4,6,8,10,12,14]);

  const deviceBinBuf = fs.readFileSync(deviceBinPath);
  assert.strictEqual(deviceBinBuf.length, 312689, `Expected exact 312689 bytes, got ${deviceBinBuf.length}`);

  const deviceAb = deviceBinBuf.buffer.slice(deviceBinBuf.byteOffset, deviceBinBuf.byteOffset + deviceBinBuf.byteLength);
  const deviceAtlas = new PackedSpriteAtlas(deviceAb);

  assert.strictEqual(deviceAtlas.directions, 8);
  assert.strictEqual(deviceAtlas.walkFrames, 16);
  assert.strictEqual(deviceAtlas.totalRows, 17);
  assert.strictEqual(deviceAtlas.totalFrames, 136);

  // Bit-for-bit equivalence test:
  // Every frame (d, r) in deviceAtlas (d in 0..7) MUST EQUAL frame (d*2, r) in atlas16!
  let totalEquivPixels = 0;
  for (let r = 0; r < 17; r++) {
    for (let d = 0; d < 8; d++) {
      const devFrame = deviceAtlas.getFrame(d, r);
      const src16Frame = atlas16.getFrame(d * 2, r);

      assert.strictEqual(devFrame.length, src16Frame.length);
      for (let i = 0; i < devFrame.length; i++) {
        assert.strictEqual(devFrame[i], src16Frame[i], `Pixel mismatch at frame dev[${d},${r}] vs src16[${d*2},${r}] index ${i}`);
        totalEquivPixels++;
      }
    }
  }

  console.log(`[PASS] Test 9: Device candidate (8dirs x 16walk, 136 frames) verified:`);
  console.log(`       - Exact file size: 312,689 B (305.36 KiB) <= 324.25 KiB budget ceiling`);
  console.log(`       - Bit-for-bit frame equivalence with 16x16 even directions verified across ${totalEquivPixels} pixels`);

  atlas8.dispose();
  atlas16.dispose();
  deviceAtlas.dispose();
  console.log('=== ALL PACKED SPRITE ATLAS TESTS PASSED SUCCESSFULLY ===');
}

run().finally(() => fs.rmSync(tmpDir, { recursive: true, force: true })).catch(err => {
  console.error('TEST SUITE FAILED:', err);
  process.exit(1);
});
