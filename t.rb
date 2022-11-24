
def runnode(src)
  nsrc = sprintf("console.log(%s)", src)
  IO.popen(["node", "-e", nsrc]) do |io|
    return io.read.strip
  end
end

sources = [

  %q(0x777 & 0xFF),
  %q(1234 & 5678),
  %q(12 << 48),
  %q(),
  %q(233 >> 2),
  %q(12 | 48),
  %q(55 ^ 48),
]

sources.each do |src|
  rt = runnode(src)
  printf("[%p, %d],\n", src, rt)
end