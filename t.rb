
require "digest/sha1"

begin
  words = [
    "foo",
    "bar",
    "hello world",
    "abcdx",
    "long text, some spaces, blah blah",
  ]
  words.each do |w|
    h = Digest::SHA1.hexdigest(w)
    printf("    [%p, %p],\n", w, h)
  end
end
