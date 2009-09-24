CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(FatELF)

INCLUDE_DIRECTORIES(include)
ADD_DEFINITIONS(-Wall -Werror)

ADD_EXECUTABLE(fatelf-glue utils/fatelf-glue.c utils/fatelf-utils.c)
ADD_EXECUTABLE(fatelf-info utils/fatelf-info.c utils/fatelf-utils.c)
ADD_EXECUTABLE(fatelf-extract utils/fatelf-extract.c utils/fatelf-utils.c)
ADD_EXECUTABLE(fatelf-replace utils/fatelf-replace.c utils/fatelf-utils.c)
ADD_EXECUTABLE(fatelf-remove utils/fatelf-remove.c utils/fatelf-utils.c)

# end of CMakeLists.txt ...

