# launch [![Build Status](https://api.cirrus-ci.com/github/helloSystem/launch.svg)](https://cirrus-ci.com/github/helloSystem/launch)

Command line tool to launch applications, will first search `$PATH` and then `.app` bundles and `.AppDir` directories in various directories, and will show launch errors in the GUI. Filer, Dock, and other applications are supposed to use `launch` to launch applications. It can also be invoked from the command line.

## Build

```shell
mkdir build
cd build
cmake ..
make
```

## Types of error messages

In general, `launch` shows error messages that would otherwise get printed to stderr (and hence be invisible for GUI users) in a dialog box.

![image](https://user-images.githubusercontent.com/2480569/96336678-be08b780-1081-11eb-8665-32eee927f231.png)

Some of the default error messages are less then user friendly, and do not give any clues to the user on how the situation can be resolved:

![image](https://user-images.githubusercontent.com/2480569/96020556-84039f80-0e4e-11eb-9a43-dd21b28e209b.png)

Hence, `launch` can handle certain types of error messages and provide more useful information:

![image](https://user-images.githubusercontent.com/2480569/96335893-0cb35300-107c-11eb-9871-76e477391202.png)

![image](https://user-images.githubusercontent.com/2480569/96336616-60746b00-1081-11eb-9c1e-a8c06da46e2a.png)

It is also possible to get additional information, e.g., from the package manager:

![image](https://user-images.githubusercontent.com/2480569/96335900-1f2d8c80-107c-11eb-9b30-5925d6d06df0.png)

It would even be conceivable that the dialog just asks the user for confirmation to run the suggested command automatically.
