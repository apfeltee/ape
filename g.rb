#!/usr/bin/ruby

begin
  data = {}
  names = []
  unused = []
  errcnt = 0
  File.foreach("prot.inc") do |line|
    m = line.match(/\b(?<name>\w+)\b\s*\(/)
    if m then
      n = m["name"]
      if data.key?(n) then
        $stderr.printf("key already exists: %p\n", n)
      end
      nn = n
      if !nn.match?(/^ape_/) then
        nn = "ape_"+nn
      end
      data[n] = nn
      if names.include?(nn) then
        re = nn.gsub(/^ape_/, "(ape_)?")
        $stderr.printf("duplicate entry: %p -> \\b%s\\b\n", n, re)
        errcnt += 1
      else
        names.push(nn)
      end
    end
  end
  if errcnt > 0 then
    exit
  end
  data.sort.each do |old, new|
    printf("  %p => %p,\n", old, new)
  end


end


