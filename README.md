Blender for V-Ray
=================

Requirements
------------
 - SVN
 - Git
 - Cmake
 - Ninja (optional)
 - [ZMQ](http://zeromq.org/) version 4.X.X or build from source/install from package manager
 - [LibSodium](https://download.libsodium.org/) version 1.X.X or build from source/install from package manager
 - [libjpeg-turbo](http://libjpeg-turbo.virtualgl.org/) *OS X/Linux ONLY* version 1.X.X or build from source/install from package manager
 - Compiler:
   - Windows: MSVC 2013 / MSVC 2015
   - Linux: GCC 4.8.X
   - OS X: Any
 - Prebuilt libs for:
   - Windows:
     - MSVC 2013: [SVN url](https://svn.blender.org/svnroot/bf-blender/trunk/lib/win64_vc12)
     - MSVC 2015: [SVN url](https://svn.blender.org/svnroot/bf-blender/trunk/lib/win64_vc14)
   - OS X: [SVN url](https://svn.blender.org/svnroot/bf-blender/trunk/lib/darwin-9.x.universal)


Building from source
--------------------
 - Clone this repository ```git clone https://github.com/bdancer/blender-for-vray```
 - Checkout git branch ```git checkout dev/vray_for_blender/vb35``` (from inside of blender-for-vray)
 - Get all submodules ```git subomdule update --init --recursive```
 - Put ZMQ and LibSodium in the following structure
    ```
    blender-for-vray-libs
    └───<Windows/Linux/Darwin>
        ├───jpeg-turbo
        │   ├───include
        │   └───lib
        │       ├───Debug
        │       └───Release
        ├───sodium
        │   └───lib
        │       ├───Debug
        │       └───Release
        └───zmq
            ├───include
            └───lib
                ├───Debug
                └───Release
    ```
 - Assuming all libs are in C:/dev make a build dir C:/dev/build run the following cmake:
    ```
    cmake -G "Visual Studio 2012 Win64"            \
        -DWITH_VRAY_FOR_BLENDER=ON                 \
        -DLIBS_ROOT=C:/dev/blender-for-vray-libs   \
        -DLIBDIR=C:/dev/win64_vc12                 \
        C:/dev/blender-for-vray                    \
    ```
 - Open Visual Studion and build the INSTALL project
 - Inside the build folder (C:/dev/build/bin/Debug/2.78/scripts/addons) run ```git clone https://github.com/bdancer/vb30```
 - Inside the vb30 folder ```git subomdule update --init --recursive```
