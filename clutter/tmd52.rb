
def add32(a, b)
    return (a + b) & 0xFFFFFFFF;
end


def unshiftright(a, b)
    na = a;
    nb = b;
    if (nb >= 32) || (nb < -32) then
        m = (nb / 32);
        nb = nb - (m * 32);
    end
    if(nb < 0) then
        nb = 32 + nb;
    end
    if (nb == 0) then
        return ((na >> 1) & 0x7fffffff) * 2 + ((na >> nb) & 1);
    end
    if (na < 0) then
      na = (na >> 1); 
      na = na & 0x7fffffff; 
      na = na | 0x40000000; 
      na = (na >> (nb - 1)); 
    else
        na = (na >> nb); 
    end
    return na; 
end

def cmn(q, a, b, x, s, t)
    a = add32(add32(a, q), add32(x, t));
    tmp1 = (a << s);
    tmp2 = unshiftright(a, (32 - s));
    return add32(tmp1 | tmp2, b);
end

def ff(a, b, c, d, x, s, t)
    r = cmn((b & c) | ((~b) & d), a, b, x, s, t);
    return r;
end

def gg(a, b, c, d, x, s, t)
    return cmn((b & d) | (c & (~d)), a, b, x, s, t);
end

def hh(a, b, c, d, x, s, t)
    return cmn((b ^ c) ^ d, a, b, x, s, t);
end

def ii(a, b, c, d, x, s, t)
  r = cmn(c ^ (b | (~d)), a, b, x, s, t);
  return r;
end


def md5cycle(ox, ok)
    x = ox;
    k = ok;
    a = x[0];
    b = x[1];
    c = x[2];
    d = x[3];
    a = ff(a, b, c, d, k[0], 7, -680876936);
    d = ff(d, a, b, c, k[1], 12, -389564586);
    c = ff(c, d, a, b, k[2], 17, 606105819);
    b = ff(b, c, d, a, k[3], 22, -1044525330);
    a = ff(a, b, c, d, k[4], 7, -176418897);
    d = ff(d, a, b, c, k[5], 12, 1200080426);
    c = ff(c, d, a, b, k[6], 17, -1473231341);
    b = ff(b, c, d, a, k[7], 22, -45705983);
    a = ff(a, b, c, d, k[8], 7, 1770035416);
    d = ff(d, a, b, c, k[9], 12, -1958414417);
    c = ff(c, d, a, b, k[10], 17, -42063);
    b = ff(b, c, d, a, k[11], 22, -1990404162);
    a = ff(a, b, c, d, k[12], 7, 1804603682);
    d = ff(d, a, b, c, k[13], 12, -40341101);
    c = ff(c, d, a, b, k[14], 17, -1502002290);
    b = ff(b, c, d, a, k[15], 22, 1236535329);
    a = gg(a, b, c, d, k[1], 5, -165796510);
    d = gg(d, a, b, c, k[6], 9, -1069501632);
    c = gg(c, d, a, b, k[11], 14, 643717713);
    b = gg(b, c, d, a, k[0], 20, -373897302);
    a = gg(a, b, c, d, k[5], 5, -701558691);
    d = gg(d, a, b, c, k[10], 9, 38016083);
    c = gg(c, d, a, b, k[15], 14, -660478335);
    b = gg(b, c, d, a, k[4], 20, -405537848);
    a = gg(a, b, c, d, k[9], 5, 568446438);
    d = gg(d, a, b, c, k[14], 9, -1019803690);
    c = gg(c, d, a, b, k[3], 14, -187363961);
    b = gg(b, c, d, a, k[8], 20, 1163531501);
    a = gg(a, b, c, d, k[13], 5, -1444681467);
    d = gg(d, a, b, c, k[2], 9, -51403784);
    c = gg(c, d, a, b, k[7], 14, 1735328473);
    b = gg(b, c, d, a, k[12], 20, -1926607734);
    a = hh(a, b, c, d, k[5], 4, -378558);
    d = hh(d, a, b, c, k[8], 11, -2022574463);
    c = hh(c, d, a, b, k[11], 16, 1839030562);
    b = hh(b, c, d, a, k[14], 23, -35309556);
    a = hh(a, b, c, d, k[1], 4, -1530992060);
    d = hh(d, a, b, c, k[4], 11, 1272893353);
    c = hh(c, d, a, b, k[7], 16, -155497632);
    b = hh(b, c, d, a, k[10], 23, -1094730640);
    a = hh(a, b, c, d, k[13], 4, 681279174);
    d = hh(d, a, b, c, k[0], 11, -358537222);
    c = hh(c, d, a, b, k[3], 16, -722521979);
    b = hh(b, c, d, a, k[6], 23, 76029189);
    a = hh(a, b, c, d, k[9], 4, -640364487);
    d = hh(d, a, b, c, k[12], 11, -421815835);
    c = hh(c, d, a, b, k[15], 16, 530742520);
    b = hh(b, c, d, a, k[2], 23, -995338651);
    a = ii(a, b, c, d, k[0], 6, -198630844);
    d = ii(d, a, b, c, k[7], 10, 1126891415);
    c = ii(c, d, a, b, k[14], 15, -1416354905);
    b = ii(b, c, d, a, k[5], 21, -57434055);
    a = ii(a, b, c, d, k[12], 6, 1700485571);
    d = ii(d, a, b, c, k[3], 10, -1894986606);
    c = ii(c, d, a, b, k[10], 15, -1051523);
    b = ii(b, c, d, a, k[1], 21, -2054922799);
    a = ii(a, b, c, d, k[8], 6, 1873313359);
    d = ii(d, a, b, c, k[15], 10, -30611744);
    c = ii(c, d, a, b, k[6], 15, -1560198380);
    b = ii(b, c, d, a, k[13], 21, 1309151649);
    a = ii(a, b, c, d, k[4], 6, -145523070);
    d = ii(d, a, b, c, k[11], 10, -1120210379);
    c = ii(c, d, a, b, k[2], 15, 718787259);
    b = ii(b, c, d, a, k[9], 21, -343485551);
    x[0] = add32(a, x[0]);
    x[1] = add32(b, x[1]);
    x[2] = add32(c, x[2]);
    x[3] = add32(d, x[3]);
    return x;
end

def md5blk(s)
  i = 0;
  md5blks = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
  while i < 64 do
    md5blks[(i >> 2)] = (s[i].ord + (s[i + 1].ord << 8) + (s[i + 2].ord << 16) + (s[i + 3].ord << 24));
    i += 4
  end
  return md5blks;
end


def md51(s)
    txt = "*pad*";
    n = s.length;
    state = [1732584193, -271733879, -1732584194, 271733878];
    i = 0;
    i = 64;
    while i < s.length do
        chunk = s[i - 64 .. i]
        state = md5cycle(state, md5blk(chunk));
        i += 64
    end
    s = s[i - 64 .. -1]
    tail = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    i = 0
    while i < s.length do
        chtail = tail[(i >> 2)];
        if (chtail == nil) then
            chtail = 0;
        end
        tail[(i >> 2)] = chtail | ((s[i].ord) << ((i % 4) << 3));
        i += 1
    end
    tail[(i >> 2)] = tail[(i >> 2)] | (0x80 << ((i % 4) << 3));
    if(i > 55)
        state = md5cycle(state, tail);
        i = 0
        while (i < 16) do
            tail[i] = 0;
            i += 1
        end
    end
    tail[14] = n * 8;
    state = md5cycle(state, tail);
    return state;
end

def rhex(n)
  s = "";
  j = 0;
  hex_chr = ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "a", "b", "c", "d", "e", "f"];
  while (j < 4) do
    s += hex_chr[((n >> (j * 8 + 4))) & 0x0F] + hex_chr[((n >> (j * 8))) & 0x0F];
    j += 1
  end
  return s;
end

def hex(x)
  i = 0;
  s = "";
  while (i < x.length) do
    s = s + rhex(x[i]);
    i += 1
  end
  return s
end

def md5(s)
  raw = md51(s);
  #println("-- raw: " + raw);
  return hex(raw);
end


demo = [
  ["hello", "5d41402abc4b2a76b9719d911017c592"],
  ["message digest", "f96b697d7cb7938d525a2f31aaf161d0"],
  ["a", "0cc175b9c0f1b6a831c399e269772661"],
  ["abc", "900150983cd24fb0d6963f7d28e17f72"],
  ["abcdefghijklmnopqrstuvwxyz", "c3fcd3d76192e4007dfb496cca67e13b"],
  ["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", "d174ab98d277d9f5a5611c2c9f419d9f"],
  ["12345678901234567890123456789012345678901234567890123456789012345678901234567890", "57edf4a22be3c955ac49da2e2107b67a"]
];

idx = 0
while (idx < demo.length) do
  itm = demo[idx];
  idx += 1
  k = itm[0];
  v = itm[1];
  ma = md5(k);
  m = ma;
  okstr = "FAIL";
  if(m == v)
      okstr = "OK  ";
  end
  print(okstr, ": \"",k, "\" => ", m, "\n");
end
