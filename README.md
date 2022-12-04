# wlrect, a rectangle for your wlroots compositor
wlrect makes a rectangle wherever you want, useful for example to mark which region of your screen is being recorded.

It expects argv to be what [slurp] outputs.

So `wlrect "$(slurp)"` is a good start. Then just terminate wlrect when you want it gone!

Also try adding arguments, like `wlrect -b"Do Stuff"` to add a button, or `wlrect -t` for a timer.

## Building
Like any other Meson project.

## todo
Too much.

[slurp]:https://github.com/emersion/slurp
