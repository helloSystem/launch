# launch [![Build Status](https://api.cirrus-ci.com/github/helloSystem/launch.svg)](https://cirrus-ci.com/github/helloSystem/launch)

Command line tools (`launch`, `open`,...) to launch applications and open documents (and protocols). Will first search `$PATH` and then `.app` bundles and `.AppDir` directories using the launch "database", and will show launch errors in the GUI. GUI applications in [helloSystem](https://hellosystem.github.io/) like like Filer, Menu, and other applications use these tools to launch applications and open documents (and protocols). The command line tools can also be invoked directly from the command line.

The use of XDG standards relating to launching applications and opening documents (and protocols) is only supported to a certain extent to provide smooth backward compatibility to legacy applications, but is otherwise discouraged.

While the tools are developed with helloSystem in mind, they are also built and occasionally tested on Linux to ensure platform independence.

## Build

On Alpine Linux:

```
apk add --no-cache qt5-qtbase-dev kwindowsystem-dev git cmake musl-dev alpine-sdk clang
```

```shell
mkdir build
cd build
cmake ..
make
```

## Launch "database"

The tools use a filesystem-based "database" to look up which applications should be launched to open documents (or protocols) of certain (MIME) types.

Currently the implementation is like this:

```
~/.local/share/launch/Applications
~/.local/share/launch/MIME
~/.local/share/launch/MIME/x-scheme-handler_https
# The following entries get populated automatically whenever the system "sees" an application
~/.local/share/launch/MIME/x-scheme-handler_https/Chromium.app
~/.local/share/launch/MIME/x-scheme-handler_https/firefox.desktop
# The following entries do not get populated automatically, but only after the user chooses a default application for a (MIME) type
~/.local/share/launch/MIME/x-scheme-handler_https/Default # Symlink to the default application for this MIME type
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
