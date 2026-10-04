import os

path = "src/execution/slipper-functions.sh"
with open(path, "r") as f:
    text = f.read()

# chr(92) is a backslash, chr(36) is the bash dollar sign
bad_type = chr(92) + "((type"
good_type = chr(36) + "(type"

bad_space = "-n" + chr(36) + "{!libdir_var}"
good_space = "-n " + chr(36) + "{!libdir_var}"

bad_one = chr(92) + "({"
bad_two = chr(92) + "){"
good_syntax = chr(36) + "{"

# Apply fixes
text = text.replace(bad_type, good_type)
text = text.replace(bad_space, good_space)
text = text.replace(bad_one, good_syntax).replace(bad_two, good_syntax)

with open(path, "w") as f:
    f.write(text)

print("slipper-functions.sh has been successfully patched.")