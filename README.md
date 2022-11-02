## blocs atlas

[![License](https://img.shields.io/badge/license-MIT-red.svg?style=flat-square)](https://en.wikipedia.org/wiki/C%2B%2B14)
[![Language](https://img.shields.io/badge/language-C++-yellow.svg?style=flat-square)](https://isocpp.org/)
[![Standard](https://img.shields.io/badge/c-99-blue.svg?style=flat-square)](https://en.wikipedia.org/wiki/C%2B%2B17)
[![Standard](https://img.shields.io/badge/c%2B%2B-17-green.svg?style=flat-square)](https://en.wikipedia.org/wiki/C%2B%2B17)

<br/>

Usage:
```
    pack -i [INPUT] -o [OUTPUT] [OPTIONS...] 
```

Example:
```
    pack -i ~/assets/sprites/ -o ~assets/atlas.png -s 256 -e 2 -u -v
```

Demo:
```
    pack --demo -s 960 -b 4
```

Options:
```
    -o  --output            sets output file name and destination
    -v  --verbose           print packer state to the console
    -u  --unique            remove duplicates from the atlas (TODO...)
    -e  --expand            repeat pixels along image edges
    -b  --border            empty border space between images
    -s  --size              sets atlas size (width and height equal)
    -d  --demo              generates random boxes (exclude first arg)
```

## Installation
    
Copy and paste the dependencies into either program folder (C or CPP):

[stb_image, stb_image_write](https://github.com/nothings/stb)

## Build

C99
```
clang -o pack c/main.c -std=c99
```

C++17
```
clang -o pack cpp/main.cpp -std=c++17 -lstdc++
```

## Sample Output

<img src="https://user-images.githubusercontent.com/64439681/199377410-b95fa961-01f3-4459-8fff-1ae7982ad34a.png" width="320" />

JSON Output:
```json
{
	"w": 32,
	"h": 32,
	"n": 3,
	"textures": [
		{
			"n": "image001",
			"x": 0,
			"y": 0,
			"w": 8,
			"h": 16
		},
        ...
	]
}
```

Binary Output:
```json
[int16] atlas width
[int16] atlas height
[int16] # textures
    [string] image name
    [int16]  image x
    [int16]  image y
    [int16]  image w
    [int16]  image h
```
