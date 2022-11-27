
dt=[]
$stdin.each_line{|l|
  m=l.match(/did you mean\s*.(\b\w+\b)./)
  if m then
    dt.push(m[1]) unless dt.include?(m[1])
  end
}
print("\\b(" + dt.join("|") + ")\\b")

