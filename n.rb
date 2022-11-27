
$rxsrc = '\b([a-z]\w+_\w+(_\w+(_\w+(_\w+)?)?)?)\b'
$rx = Regexp.new($rxsrc)

file = ARGV.shift
$data = File.read(file)

def doit(line)
  n = line.gsub($rx) do |s|
    brx = /#{s}\s*\(/
    #$stderr.printf("brx=%p\n", brx)
    if $data.match?(brx) then
      #$stderr.printf("%p is a call - ignoring\n", s)
      next s
    else
      next s.gsub(/_/, "")
    end
  end
  return n
end


$data.each_line do |line|
  if line.match?($rx) then
    n = doit(line)
    print(n)
  else
    print(line)
  end
end


