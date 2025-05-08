ABX Client - Build and Run Instructions
----------------------------------------

This application is a C++ client that reads data from the ABX_Exchange.

Prerequisites:
--------------
Before building and running the application, make sure the following tools are installed on your system:

1. C++ compiler (e.g., g++, clang++)
2. CMake (version 3.10 or higher)
3. Make utility
4. Linux OS is recommended (this was tested on Linux)

You can install the required tools on a Debian/Ubuntu system using:

    sudo apt update
    sudo apt install build-essential cmake

Build Instructions:
-------------------
1. Open a terminal and navigate to the build directory:

    cd ABX_Client/build

2. Run CMake to generate build files:

    cmake ..

3. Build the project using make:

    make

This will generate an executable file named `client_app`.

Run Instructions:
-----------------
Once the build is successful, run the client application with:

    ./client_app

Ensure that the ABX_Exchange is running and accessible before executing the client.

End of Instructions
--------------------
