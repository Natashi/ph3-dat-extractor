# ph3-dat-extractor
Just another extractor for Danmakufu ph3 .dat archives

## Installation

Download compiled releases [here](https://github.com/Natashi/ph3-dat-extractor/releases).

Or if you want to compile from source yourself, do these.

- Get these dependencies
	- [CMake 3.2](https://cmake.org/)
	- [zlib 1.2.11](https://zlib.net/)

- Then run

 ```
 git clone https://github.com/Natashi/ph3-dat-extractor
 cd ph3-dat-extractor

 cmake .
 ```

- Compile with whatever build tool you got CMake to generate

## Usage

```
ph3dat x [path to archive]
```

Files will be extracted to the current working directory, so change that unless you want them to end up in the void of Ginnungagap (aka your user folder).z