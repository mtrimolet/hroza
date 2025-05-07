# Hroza

A C++ implementation of [MarkovJunior](https://github.com/mxgmn/MarkovJunior) based on [StormKit](https://github.com/TapzCrew/StormKit)

## Getting started

[Install xmake](https://xmake.io/#/getting_started)

Configure xmake project and setup LLVM development kit. I suppose it can work with LLVM v19 but I recommend using v20 at least. Developement is currently done using v21.  
While developement is continuously made on `macos` using the `--HEAD` version of homebrew package `llvm`, compilation is also sometimes tested for `linux` using the AUR package `llvm-git`.  
Maybe it works with your default compiler toolchain, I can't guarantee anything but it's a good opportunity to open an issue !  

On `macos`, this should look like this
```sh
xmake f --toolchain=llvm --sdk=/usr/local/opt/llvm --runtimes=c++_shared --ldflags="-L/usr/local/opt/llvm/lib/c++"
```

On `linux`, this should look like this
```sh
xmake f --toolchain=llvm --sdk=/opt/llvm-git --runtimes=c++_shared
```

Then, build and run
```sh
xmake
xmake run
```

You can provide a path to an xml model as argument, otherwise default is the one currently used as test case for development.  
```sh
xmake run hroza models/GoToGradient.xml
```
