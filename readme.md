# Hroza

A C++ implementation of [MarkovJunior](https://github.com/mxgmn/MarkovJunior) based on [StormKit](https://github.com/TapzCrew/StormKit)

## Getting started

Configure LLVM development environment
```
xmake f --toolchain=llvm --sdk=/usr/local/opt/llvm --runtimes=c++_shared
```
I suppose it can work with llvm v19 but I recommend using v20 at least.
I personnally use llvm-git (--HEAD option of homebrew) which is currently at v21.
Maybe it works with your default compiler toolchain, I can't guarantee anything but it's a good opportunity to open an issue !


Then, build and run
```
xmake
xmake run
```

You can provide the name of a model as argument (spaces are removed)
```
xmake run hroza "Go To Gradient"
```
