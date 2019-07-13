# davis-to-tarsier
This code connects DAVIS cameras to the [Tarsier framework](https://github.com/neuromorphic-paris/tarsier). Find tutorials [here](https://github.com/neuromorphic-paris/tarsier/wiki)

## requirement 
building is OS agnostic as we are using premake. Make sure you have it installed.

```sh
sudo apt install premake4 # Debian/Ubuntu
brew install premake # macOS
```

## build
```sh
premake4 gmake
cd build
make
cd release
```
