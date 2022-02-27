#!/usr/bin/ruby

begin
  seen = []
  File.foreach("ape.h") do |l|
    l.scrub!
    #p l
    m = l.match(/\b(?<kind>(struct|union|enum))\s+\b(?<name>\w+)\b(\s+\b(?<alias>\w+)\b)?/)
    if m then
      k = m["kind"]
      n = m["name"]
      a = m["alias"]
      iden = sprintf("%s.%s", k, n)
      if not seen.include?(iden) then
        seen.push(iden)
        if !n.match?(/_t$/) then
          printf("%s %s", k, n)
          if a && (a != n) then
            printf(" -> %s", a)
          end
          printf("\n")
        end
      end
    end
  end

end



