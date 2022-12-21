#!/usr/bin/ruby

# prints the allocation log of mem.log in a mildly
# more readable way.
# <bytes> bytes (<index>) <location>

ALLOCRX = /^ape_allocator_alloc:\s*(?<bytes>\d+)\s*\[(?<file>[\w\.]+):(?<fline>\d+):(?<func>\w+)\] (?<expr>.*?)$/

begin
  seen = []
  
  tab = Hash.new{|h, k| h[k]=[] }
  File.foreach("mem.log") do |line|
    line.strip!
    if ((m = line.match(ALLOCRX)) != nil) then
      hm = Hash[m.names.zip(m.captures)]
      file = hm["file"]
      func = hm["func"]
      fline = hm["fline"].to_i
      ident = sprintf("%s@[%s:%d]", func, file, fline)
      hm["bytes"] = hm["bytes"].to_i
      tab[ident].push(hm)
    end
  end
  totalbytes = Hash.new{|h, k| h[k]=0 }
  tab.each do |ident, hm|
    sorted = hm.sort_by{|itm| itm["bytes"]}
    sorted.each.with_index do |itm, idx|
      printf("%d bytes (%d) %s\n", itm["bytes"], idx, ident)
      totalbytes[ident] += itm["bytes"]
    end
  end
  overall = 0
  printf("totals:\n")
  totalbytes.sort_by{|i, b| b}.each do |ident, bytes|
    printf("%d bytes allocated at %s\n", bytes, ident)
    overall += bytes
  end
  printf("total %d bytes allocated\n", overall)
end

