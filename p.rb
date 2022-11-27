
require "yaml"

begin
  nd = {}
  data = YAML.load(File.read("syms.yml"))
  data.each do |old, new|
    if ((m = new.match(/\bvmpriv_(?<t>\w+)\b/)) != nil) then
      t = m["t"].gsub(/_/, "")
      new = sprintf("ape_vm_%s", t)
    elsif ((m = new.match(/\bvm_(?<t>\w+)\b/)) != nil) then
      t = m["t"].gsub(/_/, "")
      new = sprintf("ape_vm_%s", t)
    elsif ((m = new.match(/\b(?<t>\w+)_make\b/)) != nil) then
      t = m["t"].gsub(/_/, "")
      new = sprintf("ape_make_%s", t)

    end
    printf("  %p: %p\n", old, new)
  end
end
