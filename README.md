blocs__atlas v0.x.y
===================

usage:
```
    pack [INPUT] -o [OUTPUT] [OPTIONS...] 
```

example:
```
    pack ~/assets/sprites/ -o ~assets/atlas.png -s 256 -e 2 -v
```

demo:
```
    pack --demo -s 960 -b 4
```

options:
```
    -o  --output            sets output file name and destination
    -v  --verbose           print packer state to the console
    -u  --unique            remove duplicates from the atlas (TODO...)
    -e  --expand            repeat pixels along image edges
    -b  --border            empty border space between images
    -s  --size              sets atlas size (width and height equal)
    -d  --demo              generates random boxes (exclude first arg)
```

deps:
    
[stb_image, stb_image_write](https://github.com/nothings/stb)
