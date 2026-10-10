const fs = require('fs');
const b = fs.readFileSync('apk_out/classes.dex');
const u32 = o => b.readUInt32LE(o);
const u16 = o => b.readUInt16LE(o);
const u8 = o => b.readUInt8(o);

const stringIdsOff = u32(0x3c);
const typeIdsOff = u32(0x44);
const protoIdsOff = u32(0x4c);
const fieldIdsOff = u32(0x54);
const methodIdsOff = u32(0x5c);
const classDefsOff = u32(0x64);
const classDefsSize = u32(0x60);

function uleb(o) { let r = 0, s = 0, x; do { x = u8(o); o++; r |= (x & 0x7f) << s; s += 7; } while (x & 0x80); return [r, o]; }
function str(i) {
  let o = u32(stringIdsOff + i * 4);
  [, o] = uleb(o);
  let e = o; while (u8(e) !== 0) e++;
  return b.toString('utf8', o, e);
}
function typeName(i) { return str(u32(typeIdsOff + i * 4)); }
function short(t) { return t.replace(/^L|;$/g, '').replace(/\//g, '.'); }

function methodId(i) {
  const o = methodIdsOff + i * 8;
  const classIdx = u16(o), protoIdx = u16(o + 2), nameIdx = u32(o + 4);
  // proto: shorty, ret, params
  const p = protoIdsOff + protoIdx * 12;
  const ret = typeName(u32(p + 4));
  const paramsOff = u32(p + 8);
  let params = [];
  if (paramsOff) {
    const sz = u32(paramsOff);
    for (let k = 0; k < sz; k++) params.push(short(typeName(u16(paramsOff + 4 + k * 2))));
  }
  return `${short(typeName(classIdx))}.${str(nameIdx)}(${params.join(',')}):${short(ret)}`;
}
function fieldId(i) {
  const off = fieldIdsOff + i * 8;
  return `${short(typeName(u16(off)))}.${str(u32(off + 4))}`;
}

const W = {}; // opcode width table
function initW() {
  for (let i = 0; i < 256; i++) W[i] = 1;
  [[0x02,2],[0x03,3],[0x05,2],[0x06,3],[0x08,2],[0x09,3],
   [0x13,2],[0x14,3],[0x15,2],[0x16,2],[0x17,3],[0x18,5],[0x19,2],
   [0x1a,2],[0x1b,3],[0x1c,2],[0x1f,2],[0x20,2],[0x22,2],[0x23,2],
   [0x24,3],[0x25,3],[0x26,3],[0x29,2],[0x2a,3],[0x2b,3],[0x2c,3],
   [0x2d,2],[0x2e,2],[0x2f,2],[0x30,2],[0x31,2],
   [0x32,2],[0x33,2],[0x34,2],[0x35,2],[0x36,2],[0x37,2],
   [0x38,2],[0x39,2],[0x3a,2],[0x3b,2],[0x3c,2],[0x3d,2],
   [0x44,2],[0x45,2],[0x46,2],[0x47,2],[0x48,2],[0x49,2],[0x4a,2],[0x4b,2],[0x4c,2],[0x4d,2],[0x4e,2],[0x4f,2],[0x50,2],[0x51,2],
   [0x52,2],[0x53,2],[0x54,2],[0x55,2],[0x56,2],[0x57,2],[0x58,2],[0x59,2],[0x5a,2],[0x5b,2],[0x5c,2],[0x5d,2],[0x5e,2],[0x5f,2],
   [0x60,2],[0x61,2],[0x62,2],[0x63,2],[0x64,2],[0x65,2],[0x66,2],[0x67,2],[0x68,2],[0x69,2],[0x6a,2],[0x6b,2],[0x6c,2],[0x6d,2],
   [0x6e,3],[0x6f,3],[0x70,3],[0x71,3],[0x72,3],
   [0x74,3],[0x75,3],[0x76,3],[0x77,3],[0x78,3],
   [0x90,2],[0x91,2],[0x92,2],[0x93,2],[0x94,2],[0x95,2],[0x96,2],[0x97,2],[0x98,2],[0x99,2],[0x9a,2],[0x9b,2],[0x9c,2],[0x9d,2],[0x9e,2],[0x9f,2],[0xa0,2],[0xa1,2],[0xa2,2],[0xa3,2],[0xa4,2],[0xa5,2],[0xa6,2],[0xa7,2],[0xa8,2],[0xa9,2],[0xaa,2],[0xab,2],[0xac,2],[0xad,2],[0xae,2],[0xaf,2],
   [0xd0,2],[0xd1,2],[0xd2,2],[0xd3,2],[0xd4,2],[0xd5,2],[0xd6,2],[0xd7,2],
   [0xd8,2],[0xd9,2],[0xda,2],[0xdb,2],[0xdc,2],[0xdd,2],[0xde,2],[0xdf,2],[0xe0,2],[0xe1,2],[0xe2,2]
  ].forEach(([k, v]) => W[k] = v);
}
initW();

const NAMES = {
  0x1a: 'const-string', 0x1b: 'const-string/jumbo',
  0x22: 'new-instance', 0x1c: 'const-class', 0x1f: 'check-cast',
  0x6e: 'invoke-virtual', 0x6f: 'invoke-super', 0x70: 'invoke-direct', 0x71: 'invoke-static', 0x72: 'invoke-interface',
  0x74: 'invoke-virtual/range', 0x75: 'invoke-super/range', 0x76: 'invoke-direct/range', 0x77: 'invoke-static/range', 0x78: 'invoke-interface/range',
  0x6b: 'sget-object', 0x6a: 'sget', 0x69: 'sput-object', 0x68: 'sput',
  0x52: 'iget', 0x54: 'iget-boolean', 0x55: 'iget-object', 0x5b: 'iput-object', 0x5a: 'iput-boolean', 0x59: 'iput',
  0x0e: 'return-void', 0x12: 'const/4', 0x13: 'const/16', 0x14: 'const',
  0x38: 'if-eqz', 0x39: 'if-nez', 0x3a: 'if-ltz', 0x3b: 'if-gez', 0x3c: 'if-gtz', 0x3d: 'if-lez',
  0x32: 'if-eq', 0x33: 'if-ne', 0x34: 'if-lt', 0x35: 'if-ge', 0x36: 'if-gt', 0x37: 'if-le',
  0x28: 'goto', 0x29: 'goto/16'
};

function disasm(codeOff, regsSize, insSize) {
  let pc = codeOff;
  const end = codeOff + u32(codeOff - 4); // insns_size at codeOff-4? no: code_item: registers(2) ins(2) outs(2) tries(2) debug(4) insns_size(4) insns...
  // handled outside
  const out = [];
  return out;
}

function dumpMethod(codeOff) {
  // code_item at codeOff: registers_size u16, ins_size u16, outs_size u16, tries_size u16, debug_info_off u32, insns_size u32, insns[insns_size]
  const insnsSize = u32(codeOff + 12);
  let p = codeOff + 16;
  const lines = [];
  while (p < codeOff + 16 + insnsSize * 2) {
    const op = u8(p);
    // payloads
    if (op === 0) {
      const ident = u16(p);
      if (ident === 0x0100) { const size = u16(p + 2); p += 8 + size * 8; lines.push('    packed-switch payload'); continue; }
      if (ident === 0x0200) { const size = u16(p + 2); p += 4 + size * 8; lines.push('    sparse-switch payload'); continue; }
      if (ident === 0x0300) { const w = u16(p + 2), size = u32(p + 4); const bytes = w * size; p += 8 + Math.ceil(bytes / 2); lines.push('    fill-array-data payload'); continue; }
    }
    const w = W[op] || 1;
    const vA = u8(p + 1);
    let s = `    ${NAMES[op] ? NAMES[op] : 'op_' + op.toString(16)}`;
    if (op === 0x1a) s += ` v${vA}, "${str(u16(p + 2))}"`;
    else if (op === 0x1b) s += ` v${vA}, "${str(u32(p + 2))}"`;
    else if (op === 0x12) s += ` v${vA & 0xF}, ${(vA >> 4) << 28 >> 28}`;
    else if (op === 0x13) s += ` v${vA}, ${u16(p + 2) << 16 >> 16}`;
    else if (op === 0x14) s += ` v${vA}, ${u32(p + 2)}`;
    else if (op === 0x15) s += ` v${vA}, ${u16(p + 2)}0000h`;
    else if (op >= 0x32 && op <= 0x3d) s += ` v?, +${(u16(p + 2) << 16 >> 16) * 2}b`;
    else if ([0x22, 0x1c, 0x1f].includes(op)) s += ` v${vA}, ${short(typeName(u16(p + 2)))}`;
    else if (op >= 0x6e && op <= 0x72) s += ` {..}, ${methodId(u16(p + 2))}`;
    else if (op >= 0x74 && op <= 0x78) s += `/range {v${vA}..}, ${methodId(u16(p + 2))}`;
    else if (op >= 0x60 && op <= 0x6d) s += ` v${vA}, ${fieldId(u16(p + 2))}`;
    else if (op >= 0x52 && op <= 0x5f) s += ` v${vA}, ${fieldId(u16(p + 2))}`;
    lines.push(s);
    p += w * 2;
  }
  return lines;
}

for (let c = 0; c < classDefsSize; c++) {
  const o = classDefsOff + c * 32;
  const classIdx = u32(o);
  const classDataOff = u32(o + 24);
  if (!classDataOff) continue;
  const cls = short(typeName(classIdx));
  console.log(`\n==== ${cls} ====`);
  let p = classDataOff;
  const [staticFieldsSize, p1] = uleb(p);
  const [instanceFieldsSize, p2] = uleb(p1);
  const [directMethodsSize, p3] = uleb(p2);
  const [virtualMethodsSize, p4] = uleb(p3);
  p = p4;
  // skip fields
  for (let i = 0; i < staticFieldsSize + instanceFieldsSize; i++) { let a, b2; [a, p] = uleb(p); [b2, p] = uleb(p); }
  let idx = 0;
  const dumpMethods = (count, kind) => {
    idx = 0; // 每个列表首个 idx_diff 为绝对索引
    for (let i = 0; i < count; i++) {
      let d, acc; [d, p] = uleb(p); [acc, p] = uleb(p);
      idx += d;
      const mo = methodIdsOff + idx * 8;
      let name = '?', cn = '?';
      try {
        name = str(u32(mo + 4));
        cn = short(typeName(u16(mo)));
      } catch (e) {
        console.log(`  !! ${cls} ${kind}[${i}] d=${d} idx=${idx} mo=${mo} -> ${e.message}`);
        return;
      }
      const codeOff = u32(p); // handle? no
      // code_off is uleb
      let codeOffVal, np; [codeOffVal, np] = uleb(p); p = np;
      if (!codeOffVal) continue;
      console.log(`\n  -- ${kind} ${cn}.${name} --`);
      for (const l of dumpMethod(codeOffVal)) console.log(l);
    }
  };
  dumpMethods(directMethodsSize, 'direct');
  dumpMethods(virtualMethodsSize, 'virtual');
}
