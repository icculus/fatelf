CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(FatELF)

INCLUDE_DIRECTORIES(include)
ADD_DEFINITIONS(-c99 -Wall -Werror)

ADD_EXECUTABLE(fatelf-glue utils/fatelf-glue.c utils/fatelf-utils.c)

# end of CMakeLists.txt ...

