
begin
  File.foreach("ape.h") do |line|
    m = line.scrub.match(/\b(enum|union|struct)\b\s+\b(?<name>\w+)\b/)
    if m then
      n = m["name"]
      next if n.match?(/^Ape/)
      printf("  %p => %p,\n", n, "Ape"+n.capitalize)
    end
  end
end

