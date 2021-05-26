# wlrect, a rectangle for your wlroots compositor
wlrect makes a rectangle wherever you want, useful for example to mark which region of your screen is being recorded.

It expects argv to be what [slurp] outputs.

So `wlrect "$(slurp)"` is a good start. Then just terminate wlrect when you want it gone!

## Building
Like any other Meson project.

If you didn't recursively clone don't forget to `git submodule update --init` or something like that.

## todo
Too much.

[slurp]:https://github.com/emersion/slurp
